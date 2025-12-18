#pragma once

#include <JuceHeader.h>

class DynamicFilterProcessor : public juce::AudioProcessor
{
public:
    DynamicFilterProcessor();
    ~DynamicFilterProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputLevel() const { return inputLevel.load(std::memory_order_relaxed); }
    float getOutputLevel() const { return outputLevel.load(std::memory_order_relaxed); }
    float getGainReduction() const { return gainReduction.load(std::memory_order_relaxed); }

    void getFrequencyResponse(std::vector<float>& magnitudes);
    void getInputWaveform(std::vector<float>& waveform);
    void getOutputWaveform(std::vector<float>& waveform);

    void setVisualizerState(bool active) { visualizerActive.store(active, std::memory_order_relaxed); }
    bool isVisualizerActive() const { return visualizerActive.load(std::memory_order_relaxed); }

private:
    using Filter = juce::dsp::IIR::Filter<float>;
    using FilterCoefs = juce::dsp::IIR::Coefficients<float>;

    enum FilterType {
        HIGHPASS = 0,
        LOWPASS = 1,
        BANDPASS = 2,
        NOTCH = 3
    };

    enum FilterCharacteristic {
        BUTTERWORTH = 0,
        LINKWITZ_RILEY = 1,
        BESSEL = 2
    };

    juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter> filterChainL;
    juce::dsp::ProcessorChain<Filter, Filter, Filter, Filter> filterChainR;

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    bool bypassState{ false };

    juce::LinearSmoothedValue<float> smoothedCutoff;
    juce::LinearSmoothedValue<float> smoothedQ;
    juce::LinearSmoothedValue<float> smoothedResonance;

    float currentCutoff{ 1000.0f };
    float currentQ{ 0.707f };
    float currentResonance{ 0.0f };
    int currentType{ HIGHPASS };
    int currentSlope{ 24 };
    int currentCharacteristic{ BUTTERWORTH };
    int currentNumStages{ 2 };

    int previousType{ HIGHPASS };
    int previousSlope{ 24 };
    int previousCharacteristic{ BUTTERWORTH };

    double currentSampleRate{ 44100.0 };

    std::atomic<float> inputLevel{ 0.0f };
    std::atomic<float> outputLevel{ 0.0f };
    std::atomic<float> gainReduction{ 0.0f };

    // ADDED: Missing visualizerActive member
    std::atomic<bool> visualizerActive{ true };

    float inputLevelSum{ 0.0f };
    float outputLevelSum{ 0.0f };
    int levelSampleCount{ 0 };
    static constexpr int levelUpdateInterval = 2048;

    static constexpr int waveformSize = 512;
    std::vector<float> inputWaveformData;
    std::vector<float> outputWaveformData;
    juce::CriticalSection waveformLock;
    int waveformWritePos{ 0 };

    void updateFilterCoefficients();
    void updateMetrics(const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output);
    void captureWaveforms(const juce::AudioBuffer<float>& input, const juce::AudioBuffer<float>& output);

    juce::CriticalSection coefficientLock;
    std::vector<FilterCoefs::Ptr> currentCoefficients;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicFilterProcessor)
};