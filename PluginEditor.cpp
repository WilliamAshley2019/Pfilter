#include "PluginProcessor.h"
#include "PluginEditor.h"

FrequencyResponseDisplay::FrequencyResponseDisplay(DynamicFilterProcessor& p)
    : audioProcessor(p)
{
    startTimerHz(30);
}

FrequencyResponseDisplay::~FrequencyResponseDisplay()
{
    stopTimer();
}

void FrequencyResponseDisplay::timerCallback()
{
    if (!audioProcessor.isVisualizerActive())
    {
        stopTimer();
        repaint();
        return;
    }

    updateResponseCurve();
    repaint();
}

void FrequencyResponseDisplay::drawGrid(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    std::vector<float> freqs = { 20, 50, 80, 100, 250, 500, 1000, 2000, 3000, 4000, 5000, 8000, 10000, 12000, 14000, 15000, 18000, 20000 };

    for (auto freq : freqs)
    {
        float x = juce::jmap(std::log10(freq), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());

        g.setColour(juce::Colours::white.withAlpha(0.15f));

        float dashLength = 4.0f;
        float gapLength = 4.0f;
        float y = bounds.getY();

        while (y < bounds.getBottom())
        {
            float segmentEnd = juce::jmin(y + dashLength, bounds.getBottom());
            g.drawLine(x, y, x, segmentEnd, 1.0f);
            y = segmentEnd + gapLength;
        }
    }

    for (int db = -48; db <= 12; db += 4)
    {
        float y = juce::jmap((float)db, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());

        g.setColour(juce::Colours::white.withAlpha(0.15f));

        float dashLength = 4.0f;
        float gapLength = 4.0f;
        float x = bounds.getX();

        while (x < bounds.getRight())
        {
            float segmentEnd = juce::jmin(x + dashLength, bounds.getRight());
            g.drawLine(x, y, segmentEnd, y, 1.0f);
            x = segmentEnd + gapLength;
        }
    }

    float zeroY = juce::jmap(0.0f, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
    g.setColour(juce::Colours::white.withAlpha(0.3f));

    float dashLength = 6.0f;
    float gapLength = 4.0f;
    float x = bounds.getX();

    while (x < bounds.getRight())
    {
        float segmentEnd = juce::jmin(x + dashLength, bounds.getRight());
        g.drawLine(x, zeroY, segmentEnd, zeroY, 1.5f);
        x = segmentEnd + gapLength;
    }
}

void FrequencyResponseDisplay::drawWaveforms(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(50, 20);

    if (inputWaveformData.empty() || outputWaveformData.empty())
        return;

    float centerY = bounds.getCentreY();
    float heightScale = bounds.getHeight() * 0.15f;

    juce::Path inputPath;
    juce::Path outputPath;

    for (size_t i = 0; i < inputWaveformData.size(); ++i)
    {
        float x = juce::jmap((float)i, 0.0f, (float)inputWaveformData.size(),
            bounds.getX(), bounds.getRight());
        float inputY = centerY - (inputWaveformData[i] * heightScale);
        float outputY = centerY - (outputWaveformData[i] * heightScale);

        if (i == 0)
        {
            inputPath.startNewSubPath(x, inputY);
            outputPath.startNewSubPath(x, outputY);
        }
        else
        {
            inputPath.lineTo(x, inputY);
            outputPath.lineTo(x, outputY);
        }
    }

    g.setColour(juce::Colour(100, 20, 30).withAlpha(0.6f));
    g.strokePath(inputPath, juce::PathStrokeType(1.5f));

    juce::Path outputFillPath = outputPath;
    outputFillPath.lineTo(bounds.getRight(), centerY);
    outputFillPath.lineTo(bounds.getX(), centerY);
    outputFillPath.closeSubPath();

    g.setColour(juce::Colour(180, 40, 60).withAlpha(0.15f));
    g.fillPath(outputFillPath);

    g.setColour(juce::Colour(220, 80, 100).withAlpha(0.8f));
    g.strokePath(outputPath, juce::PathStrokeType(1.8f));
}

void FrequencyResponseDisplay::drawFrequencyLabels(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(10.0f));

    std::vector<std::pair<float, juce::String>> mainLabels = {
        {20.0f, "20"}, {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}, {20000.0f, "20k"}
    };

    for (auto& label : mainLabels)
    {
        float x = juce::jmap(std::log10(label.first), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());
        g.drawText(label.second, static_cast<int>(x - 15.0f), static_cast<int>(bounds.getBottom() - 15.0f),
            30, 15, juce::Justification::centred);
    }

    g.setFont(juce::FontOptions(8.0f));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.7f));

    std::vector<std::pair<float, juce::String>> smallLabels = {
        {50.0f, "50"}, {250.0f, "250"}, {500.0f, "500"},
        {2000.0f, "2k"}, {5000.0f, "5k"}
    };

    for (auto& label : smallLabels)
    {
        float x = juce::jmap(std::log10(label.first), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());
        g.drawText(label.second, static_cast<int>(x - 12.0f), static_cast<int>(bounds.getBottom() - 15.0f),
            24, 15, juce::Justification::centred);
    }
}

void FrequencyResponseDisplay::drawMagnitudeLabels(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colours::lightgrey);
    g.setFont(juce::FontOptions(10.0f));

    for (int db = -48; db <= 12; db += 12)
    {
        float y = juce::jmap((float)db, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
        juce::String text = juce::String(db) + " dB";
        g.drawText(text, 5, static_cast<int>(y - 7.0f), 40, 14, juce::Justification::left);
    }

    g.setFont(juce::FontOptions(8.0f));
    g.setColour(juce::Colours::lightgrey.withAlpha(0.7f));

    for (int db = -44; db <= 8; db += 4)
    {
        if (db % 12 != 0)
        {
            float y = juce::jmap((float)db, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
            juce::String text = juce::String(db);
            g.drawText(text, 5, static_cast<int>(y - 5.0f), 30, 10, juce::Justification::left);
        }
    }
}

void FrequencyResponseDisplay::updateResponseCurve()
{
    audioProcessor.getFrequencyResponse(magnitudeData);
    audioProcessor.getInputWaveform(inputWaveformData);
    audioProcessor.getOutputWaveform(outputWaveformData);

    if (magnitudeData.empty())
        return;

    auto bounds = getLocalBounds().toFloat().reduced(50, 20);

    responseCurve.clear();

    for (size_t i = 0; i < magnitudeData.size(); ++i)
    {
        float freq = 20.0f * std::pow(1000.0f, i / 511.0f);
        float x = juce::jmap(std::log10(freq), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());

        float mag = juce::jlimit(-48.0f, 12.0f, magnitudeData[i]);
        float y = juce::jmap(mag, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());

        if (i == 0)
            responseCurve.startNewSubPath(x, y);
        else
            responseCurve.lineTo(x, y);
    }
}

void FrequencyResponseDisplay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black);

    if (!audioProcessor.isVisualizerActive())
    {
        g.setColour(juce::Colours::grey);
        g.setFont(juce::FontOptions(18.0f));
        g.drawText("Visualizer Disabled - Click LED to Enable", getLocalBounds(), juce::Justification::centred);
        return;
    }

    drawGrid(g);
    drawWaveforms(g);

    auto bounds = getLocalBounds().toFloat().reduced(50, 20);

    if (!responseCurve.isEmpty())
    {
        juce::Path fillPath = responseCurve;
        float zeroY = juce::jmap(0.0f, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());

        fillPath.lineTo(bounds.getRight(), zeroY);
        fillPath.lineTo(bounds.getX(), zeroY);
        fillPath.closeSubPath();

        g.setColour(juce::Colour(0, 255, 255).withAlpha(0.2f));
        g.fillPath(fillPath);

        g.setColour(juce::Colour(0, 255, 255).withAlpha(0.9f));
        g.strokePath(responseCurve, juce::PathStrokeType(2.5f));
    }

    drawFrequencyLabels(g);
    drawMagnitudeLabels(g);
}

void FrequencyResponseDisplay::resized()
{
    updateResponseCurve();
}

// ADDED: Missing method implementations
void FrequencyResponseDisplay::startVisualizerTimer()
{
    if (!isTimerRunning())
        startTimerHz(30);
}

void FrequencyResponseDisplay::stopVisualizerTimer()
{
    stopTimer();
    repaint();
}

DynamicFilterProcessorEditor::DynamicFilterProcessorEditor(DynamicFilterProcessor& p)
    : juce::AudioProcessorEditor(&p), audioProcessor(p), responseDisplay(p)
{
    setSize(850, 580);
    setLookAndFeel(&customLookAndFeel);

    addAndMakeVisible(cutoffSlider);
    cutoffSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    cutoffSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    cutoffSlider.setTextValueSuffix(" Hz");
    cutoffAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "cutoff", cutoffSlider);

    addAndMakeVisible(cutoffLabel);
    cutoffLabel.setText("Cutoff", juce::dontSendNotification);
    cutoffLabel.setJustificationType(juce::Justification::centred);
    cutoffLabel.attachToComponent(&cutoffSlider, false);
    cutoffLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(cutoffBypassButton);
    cutoffBypassButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    cutoffBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "cutoffBypass", cutoffBypassButton);

    addAndMakeVisible(qSlider);
    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "q", qSlider);

    addAndMakeVisible(qLabel);
    qLabel.setText("Q Factor", juce::dontSendNotification);
    qLabel.setJustificationType(juce::Justification::centred);
    qLabel.attachToComponent(&qSlider, false);
    qLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(qBypassButton);
    qBypassButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    qBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "qBypass", qBypassButton);

    addAndMakeVisible(resonanceSlider);
    resonanceSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    resonanceSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    resonanceSlider.setTextValueSuffix(" dB");
    resonanceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "resonance", resonanceSlider);

    addAndMakeVisible(resonanceLabel);
    resonanceLabel.setText("Resonance", juce::dontSendNotification);
    resonanceLabel.setJustificationType(juce::Justification::centred);
    resonanceLabel.attachToComponent(&resonanceSlider, false);
    resonanceLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(resonanceBypassButton);
    resonanceBypassButton.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    resonanceBypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "resonanceBypass", resonanceBypassButton);

    addAndMakeVisible(typeComboBox);
    typeComboBox.addItem("High-Pass", 1);
    typeComboBox.addItem("Low-Pass", 2);
    typeComboBox.addItem("Band-Pass", 3);
    typeComboBox.addItem("Notch", 4);
    typeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "type", typeComboBox);

    addAndMakeVisible(typeLabel);
    typeLabel.setText("Type", juce::dontSendNotification);
    typeLabel.setJustificationType(juce::Justification::centredLeft);
    typeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(slopeComboBox);
    slopeComboBox.addItem("12 dB/oct", 1);
    slopeComboBox.addItem("24 dB/oct", 2);
    slopeComboBox.addItem("36 dB/oct", 3);
    slopeComboBox.addItem("48 dB/oct", 4);
    slopeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "slope", slopeComboBox);

    addAndMakeVisible(slopeLabel);
    slopeLabel.setText("Slope", juce::dontSendNotification);
    slopeLabel.setJustificationType(juce::Justification::centredLeft);
    slopeLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(characteristicComboBox);
    characteristicComboBox.addItem("Butterworth", 1);
    characteristicComboBox.addItem("Linkwitz-Riley", 2);
    characteristicComboBox.addItem("Bessel", 3);
    characteristicAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        audioProcessor.apvts, "characteristic", characteristicComboBox);

    addAndMakeVisible(characteristicLabel);
    characteristicLabel.setText("Character", juce::dontSendNotification);
    characteristicLabel.setJustificationType(juce::Justification::centredLeft);
    characteristicLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    addAndMakeVisible(bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "bypass", bypassButton);

    addAndMakeVisible(visualizerButton);
    visualizerButton.setClickingTogglesState(true);
    visualizerButton.setToggleState(true, juce::dontSendNotification);
    visualizerAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        audioProcessor.apvts, "visualizerEnabled", visualizerButton);

    visualizerButton.onClick = [this]()
        {
            bool isActive = visualizerButton.getToggleState();
            audioProcessor.setVisualizerState(isActive);

            if (isActive)
                responseDisplay.startVisualizerTimer();
            else
                responseDisplay.stopVisualizerTimer();
        };

    audioProcessor.setVisualizerState(true);
    responseDisplay.startVisualizerTimer();

    addAndMakeVisible(responseDisplay);

    addAndMakeVisible(inputMeterLabel);
    inputMeterLabel.setJustificationType(juce::Justification::centredLeft);
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    inputMeterLabel.setFont(juce::FontOptions(12.0f));

    addAndMakeVisible(outputMeterLabel);
    outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    outputMeterLabel.setFont(juce::FontOptions(12.0f));

    addAndMakeVisible(gainReductionLabel);
    gainReductionLabel.setJustificationType(juce::Justification::centredLeft);
    gainReductionLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    gainReductionLabel.setFont(juce::FontOptions(12.0f));

    startTimerHz(15);
}

DynamicFilterProcessorEditor::~DynamicFilterProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void DynamicFilterProcessorEditor::updateMeters()
{
    float inputLevel = audioProcessor.getInputLevel();
    float outputLevel = audioProcessor.getOutputLevel();
    float gainReduction = audioProcessor.getGainReduction();

    float inputDB = inputLevel > 0.00001f ? 20.0f * std::log10(inputLevel) : -100.0f;
    float outputDB = outputLevel > 0.00001f ? 20.0f * std::log10(outputLevel) : -100.0f;

    inputMeterLabel.setText("Input: " + juce::String(inputDB, 1) + " dB", juce::dontSendNotification);
    outputMeterLabel.setText("Output: " + juce::String(outputDB, 1) + " dB", juce::dontSendNotification);
    gainReductionLabel.setText("Gain: " + juce::String(gainReduction, 1) + " dB", juce::dontSendNotification);
}

void DynamicFilterProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xFF1E1E1E));

    auto headerArea = getLocalBounds().removeFromTop(50);
    g.setGradientFill(juce::ColourGradient(
        juce::Colour(0xFF2A2A2A), 0, 0,
        juce::Colour(0xFF1E1E1E), 0, (float)headerArea.getHeight(), false));
    g.fillRect(headerArea);

    g.setColour(juce::Colours::white);
    g.setFont(juce::FontOptions(24.0f));
    g.drawText("Professional Dynamic Filter", headerArea, juce::Justification::centred);
}

void DynamicFilterProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(50);

    auto mainArea = bounds.reduced(10);

    auto displayArea = mainArea.removeFromTop(static_cast<int>(mainArea.getHeight() * 0.55f));
    responseDisplay.setBounds(displayArea.reduced(5));

    auto controlsArea = mainArea.reduced(5);

    auto rotaryArea = controlsArea.removeFromTop(130);
    int rotaryWidth = rotaryArea.getWidth() / 3;

    auto cutoffArea = rotaryArea.removeFromLeft(rotaryWidth).reduced(10);
    cutoffBypassButton.setBounds(cutoffArea.removeFromTop(20).removeFromRight(50));
    cutoffSlider.setBounds(cutoffArea.removeFromTop(100));

    auto qArea = rotaryArea.removeFromLeft(rotaryWidth).reduced(10);
    qBypassButton.setBounds(qArea.removeFromTop(20).removeFromRight(50));
    qSlider.setBounds(qArea.removeFromTop(100));

    auto resonanceArea = rotaryArea.reduced(10);
    resonanceBypassButton.setBounds(resonanceArea.removeFromTop(20).removeFromRight(50));
    resonanceSlider.setBounds(resonanceArea.removeFromTop(100));

    controlsArea.removeFromTop(10);

    auto comboArea = controlsArea.removeFromTop(60);
    int comboWidth = comboArea.getWidth() / 3;

    auto typeArea = comboArea.removeFromLeft(comboWidth).reduced(5);
    typeLabel.setBounds(typeArea.removeFromTop(20));
    typeComboBox.setBounds(typeArea);

    auto slopeArea = comboArea.removeFromLeft(comboWidth).reduced(5);
    slopeLabel.setBounds(slopeArea.removeFromTop(20));
    slopeComboBox.setBounds(slopeArea);

    auto charArea = comboArea.reduced(5);
    characteristicLabel.setBounds(charArea.removeFromTop(20));
    characteristicComboBox.setBounds(charArea);

    controlsArea.removeFromTop(10);

    auto bottomArea = controlsArea.removeFromTop(40);

    bypassButton.setBounds(bottomArea.removeFromRight(100).reduced(5));
    visualizerButton.setBounds(bottomArea.removeFromRight(100).reduced(5));

    auto metersArea = bottomArea.reduced(5);
    int meterWidth = metersArea.getWidth() / 3;

    inputMeterLabel.setBounds(metersArea.removeFromLeft(meterWidth));
    outputMeterLabel.setBounds(metersArea.removeFromLeft(meterWidth));
    gainReductionLabel.setBounds(metersArea);
}

void DynamicFilterProcessorEditor::timerCallback()
{
    updateMeters();
}