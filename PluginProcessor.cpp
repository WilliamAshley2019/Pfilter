#include "PluginProcessor.h"
#include "PluginEditor.h"

DynamicFilterProcessor::DynamicFilterProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    )
#endif
    , apvts(*this, nullptr, "Parameters", createParameterLayout())
{
}

DynamicFilterProcessor::~DynamicFilterProcessor()
{
}

const juce::String DynamicFilterProcessor::getName() const { return JucePlugin_Name; }
bool DynamicFilterProcessor::acceptsMidi() const { return false; }
bool DynamicFilterProcessor::producesMidi() const { return false; }
bool DynamicFilterProcessor::isMidiEffect() const { return false; }
double DynamicFilterProcessor::getTailLengthSeconds() const { return 0.1; }
int DynamicFilterProcessor::getNumPrograms() { return 1; }
int DynamicFilterProcessor::getCurrentProgram() { return 0; }
void DynamicFilterProcessor::setCurrentProgram(int) {}
const juce::String DynamicFilterProcessor::getProgramName(int) { return "Default"; }
void DynamicFilterProcessor::changeProgramName(int, const juce::String&) {}

void DynamicFilterProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels = 1;

    filterChainL.prepare(spec);
    filterChainR.prepare(spec);

    filterChainL.reset();
    filterChainR.reset();

    smoothedCutoff.reset(sampleRate, 0.02);
    smoothedQ.reset(sampleRate, 0.02);

    float initCutoff = *apvts.getRawParameterValue("cutoff");
    float initQ = *apvts.getRawParameterValue("q");

    smoothedCutoff.setCurrentAndTargetValue(initCutoff);
    smoothedQ.setCurrentAndTargetValue(initQ);

    currentCutoff = initCutoff;
    currentQ = initQ;

    updateFilterCoefficients();

    inputLevel.store(0.0f, std::memory_order_relaxed);
    outputLevel.store(0.0f, std::memory_order_relaxed);
    gainReduction.store(0.0f, std::memory_order_relaxed);
    inputLevelSum = 0.0f;
    outputLevelSum = 0.0f;
    levelSampleCount = 0;
}

void DynamicFilterProcessor::releaseResources()
{
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool DynamicFilterProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
}
#endif

void DynamicFilterProcessor::updateFilterCoefficients()
{
    float cutoff = currentCutoff;
    float q = currentQ;
    int type = currentType;
    int slopeIndex = currentSlope / 12 - 1;
    int characteristic = currentCharacteristic;

    int slope = (slopeIndex + 1) * 12;

    int numStages = slope / 12;
    numStages = juce::jmin(numStages, 4);

    float stageQ = q;
    if (characteristic == BUTTERWORTH && numStages > 1)
    {
        stageQ = 0.707f / std::sqrt(static_cast<float>(numStages));
    }
    else if (characteristic == LINKWITZ_RILEY)
    {
        stageQ = 0.5f;
    }
    else if (characteristic == BESSEL)
    {
        stageQ = 0.577f / std::sqrt(static_cast<float>(numStages));
    }

    juce::ScopedLock lock(coefficientLock);
    currentCoefficients.clear();

    for (int stage = 0; stage < numStages; ++stage)
    {
        FilterCoefs::Ptr coefs;

        switch (type)
        {
        case HIGHPASS:
            coefs = FilterCoefs::makeHighPass(currentSampleRate, cutoff, stageQ);
            break;
        case LOWPASS:
            coefs = FilterCoefs::makeLowPass(currentSampleRate, cutoff, stageQ);
            break;
        case BANDPASS:
            coefs = FilterCoefs::makeBandPass(currentSampleRate, cutoff, stageQ);
            break;
        case NOTCH:
            coefs = FilterCoefs::makeNotch(currentSampleRate, cutoff, stageQ);
            break;
        default:
            coefs = FilterCoefs::makeHighPass(currentSampleRate, cutoff, stageQ);
            break;
        }

        currentCoefficients.push_back(coefs);

        if (stage == 0)
        {
            *filterChainL.get<0>().coefficients = *coefs;
            *filterChainR.get<0>().coefficients = *coefs;
        }
        else if (stage == 1)
        {
            *filterChainL.get<1>().coefficients = *coefs;
            *filterChainR.get<1>().coefficients = *coefs;
        }
        else if (stage == 2)
        {
            *filterChainL.get<2>().coefficients = *coefs;
            *filterChainR.get<2>().coefficients = *coefs;
        }
        else if (stage == 3)
        {
            *filterChainL.get<3>().coefficients = *coefs;
            *filterChainR.get<3>().coefficients = *coefs;
        }
    }

    for (int stage = numStages; stage < 4; ++stage)
    {
        auto unity = FilterCoefs::makeAllPass(currentSampleRate, 1000.0f);
        if (stage == 0)
        {
            *filterChainL.get<0>().coefficients = *unity;
            *filterChainR.get<0>().coefficients = *unity;
        }
        else if (stage == 1)
        {
            *filterChainL.get<1>().coefficients = *unity;
            *filterChainR.get<1>().coefficients = *unity;
        }
        else if (stage == 2)
        {
            *filterChainL.get<2>().coefficients = *unity;
            *filterChainR.get<2>().coefficients = *unity;
        }
        else if (stage == 3)
        {
            *filterChainL.get<3>().coefficients = *unity;
            *filterChainR.get<3>().coefficients = *unity;
        }
    }
}

void DynamicFilterProcessor::updateMetrics(const juce::AudioBuffer<float>& input,
    const juce::AudioBuffer<float>& output)
{
    int numSamples = input.getNumSamples();
    int numChannels = juce::jmin(input.getNumChannels(), output.getNumChannels());

    if (numChannels == 0 || numSamples == 0)
        return;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* inputData = input.getReadPointer(ch);
        auto* outputData = output.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            inputLevelSum += inputData[i] * inputData[i];
            outputLevelSum += outputData[i] * outputData[i];
        }
    }

    levelSampleCount += numSamples * numChannels;

    if (levelSampleCount >= levelUpdateInterval)
    {
        float inputRMS = std::sqrt(inputLevelSum / static_cast<float>(levelSampleCount));
        float outputRMS = std::sqrt(outputLevelSum / static_cast<float>(levelSampleCount));

        inputLevel.store(inputRMS, std::memory_order_release);
        outputLevel.store(outputRMS, std::memory_order_release);

        if (inputRMS > 0.0001f && outputRMS > 0.0001f)
        {
            float reduction = 20.0f * std::log10(outputRMS / inputRMS);
            gainReduction.store(reduction, std::memory_order_release);
        }
        else
        {
            gainReduction.store(0.0f, std::memory_order_release);
        }

        inputLevelSum = 0.0f;
        outputLevelSum = 0.0f;
        levelSampleCount = 0;
    }
}

void DynamicFilterProcessor::getFrequencyResponse(std::vector<float>& magnitudes)
{
    juce::ScopedLock lock(coefficientLock);

    magnitudes.clear();
    magnitudes.resize(512, 0.0f);

    if (currentCoefficients.empty())
        return;

    for (int i = 0; i < 512; ++i)
    {
        float freq = 20.0f * std::pow(1000.0f, i / 511.0f);

        std::complex<float> totalResponse(1.0f, 0.0f);

        for (auto& coef : currentCoefficients)
        {
            auto response = coef->getMagnitudeForFrequency(static_cast<double>(freq), currentSampleRate);
            totalResponse *= std::complex<float>(static_cast<float>(response), 0.0f);
        }

        float magnitude = std::abs(totalResponse);
        magnitudes[i] = 20.0f * std::log10(juce::jmax(0.00001f, magnitude));
    }
}

void DynamicFilterProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (buffer.getNumSamples() == 0)
        return;

    juce::AudioBuffer<float> inputCopy;
    inputCopy.makeCopyOf(buffer);

    bool bypass = *apvts.getRawParameterValue("bypass") > 0.5f;
    bypassState = bypass;

    if (!bypass)
    {
        float targetCutoff = *apvts.getRawParameterValue("cutoff");
        float targetQ = *apvts.getRawParameterValue("q");
        int newType = static_cast<int>(*apvts.getRawParameterValue("type"));
        int newSlope = (static_cast<int>(*apvts.getRawParameterValue("slope")) + 1) * 12;
        int newChar = static_cast<int>(*apvts.getRawParameterValue("characteristic"));

        smoothedCutoff.setTargetValue(targetCutoff);
        smoothedQ.setTargetValue(targetQ);

        bool structuralChange = (newType != previousType) ||
            (newSlope != previousSlope) ||
            (newChar != previousCharacteristic);

        if (structuralChange)
        {
            filterChainL.reset();
            filterChainR.reset();

            previousType = newType;
            previousSlope = newSlope;
            previousCharacteristic = newChar;

            currentType = newType;
            currentSlope = newSlope;
            currentCharacteristic = newChar;

            currentCutoff = smoothedCutoff.getNextValue();
            currentQ = smoothedQ.getNextValue();

            updateFilterCoefficients();
        }

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        for (int sample = 0; sample < numSamples; ++sample)
        {
            float smoothCutoff = smoothedCutoff.getNextValue();
            float smoothQ = smoothedQ.getNextValue();

            bool needsUpdate = std::abs(smoothCutoff - currentCutoff) > 0.1f ||
                std::abs(smoothQ - currentQ) > 0.001f;

            if (needsUpdate)
            {
                currentCutoff = smoothCutoff;
                currentQ = smoothQ;
                updateFilterCoefficients();
            }

            if (numChannels >= 1)
            {
                float inputSample = buffer.getSample(0, sample);
                juce::AudioBuffer<float> tempBuffer(1, 1);
                tempBuffer.setSample(0, 0, inputSample);

                juce::dsp::AudioBlock<float> block(tempBuffer);
                juce::dsp::ProcessContextReplacing<float> context(block);
                filterChainL.process(context);

                buffer.setSample(0, sample, tempBuffer.getSample(0, 0));
            }

            if (numChannels >= 2)
            {
                float inputSample = buffer.getSample(1, sample);
                juce::AudioBuffer<float> tempBuffer(1, 1);
                tempBuffer.setSample(0, 0, inputSample);

                juce::dsp::AudioBlock<float> block(tempBuffer);
                juce::dsp::ProcessContextReplacing<float> context(block);
                filterChainR.process(context);

                buffer.setSample(1, sample, tempBuffer.getSample(0, 0));
            }
        }
    }

    updateMetrics(inputCopy, buffer);
}

bool DynamicFilterProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* DynamicFilterProcessor::createEditor()
{
    return new DynamicFilterProcessorEditor(*this);
}

void DynamicFilterProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void DynamicFilterProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorValueTreeState::ParameterLayout DynamicFilterProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID("bypass", 1), "Bypass", false));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("cutoff", 1), "Cutoff Frequency",
        juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.3f),
        1000.0f,
        juce::String(),
        juce::AudioProcessorParameter::genericParameter,
        [](float value, int) { return juce::String(static_cast<int>(value)) + " Hz"; }));

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("q", 1), "Q / Resonance",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f),
        0.707f));

    juce::StringArray filterTypes;
    filterTypes.add("High-Pass");
    filterTypes.add("Low-Pass");
    filterTypes.add("Band-Pass");
    filterTypes.add("Notch");
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("type", 1), "Filter Type", filterTypes, 0));

    juce::StringArray slopes;
    slopes.add("12 dB/oct");
    slopes.add("24 dB/oct");
    slopes.add("36 dB/oct");
    slopes.add("48 dB/oct");
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("slope", 1), "Slope", slopes, 1));

    juce::StringArray characteristics;
    characteristics.add("Butterworth");
    characteristics.add("Linkwitz-Riley");
    characteristics.add("Bessel");
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("characteristic", 1), "Characteristic", characteristics, 0));

    return layout;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicFilterProcessor();
}