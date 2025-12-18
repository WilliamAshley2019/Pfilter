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

void FrequencyResponseDisplay::drawGrid(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    std::vector<float> freqs = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto freq : freqs)
    {
        float x = juce::jmap(std::log10(freq), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());
        g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
        g.drawVerticalLine(juce::roundToInt(x), bounds.getY(), bounds.getBottom());
    }

    for (int db = -48; db <= 12; db += 12)
    {
        float y = juce::jmap((float)db, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
        g.setColour(juce::Colours::darkgrey.withAlpha(0.3f));
        g.drawHorizontalLine(juce::roundToInt(y), bounds.getX(), bounds.getRight());
    }

    float zeroY = juce::jmap(0.0f, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
    g.setColour(juce::Colours::darkgrey.withAlpha(0.6f));
    g.drawHorizontalLine(juce::roundToInt(zeroY), bounds.getX(), bounds.getRight());
}

void FrequencyResponseDisplay::drawFrequencyLabels(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::lightgrey);
    g.setFont(10.0f);

    std::vector<std::pair<float, juce::String>> labels = {
    {20.0f, "20"}, {100.0f, "100"}, {1000.0f, "1k"}, {10000.0f, "10k"}, {20000.0f, "20k"}
    };

    for (auto& label : labels)
    {
        float x = juce::jmap(std::log10(label.first), std::log10(20.0f), std::log10(20000.0f),
            bounds.getX(), bounds.getRight());
        g.drawText(label.second, x - 15, bounds.getBottom() - 15, 30, 15,
            juce::Justification::centred);
    }
}

void FrequencyResponseDisplay::drawMagnitudeLabels(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colours::lightgrey);
    g.setFont(10.0f);

    for (int db = -48; db <= 12; db += 12)
    {
        float y = juce::jmap((float)db, -48.0f, 12.0f, bounds.getBottom(), bounds.getY());
        juce::String text = juce::String(db) + " dB";
        g.drawText(text, 5, y - 7, 40, 14, juce::Justification::left);
    }
}

void FrequencyResponseDisplay::updateResponseCurve()
{
    audioProcessor.getFrequencyResponse(magnitudeData);

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

    drawGrid(g);

    if (!responseCurve.isEmpty())
    {
        g.setColour(juce::Colours::cyan.withAlpha(0.9f));
        g.strokePath(responseCurve, juce::PathStrokeType(2.0f));
    }

    drawFrequencyLabels(g);
    drawMagnitudeLabels(g);
}

void FrequencyResponseDisplay::resized()
{
    updateResponseCurve();
}

void FrequencyResponseDisplay::timerCallback()
{
    updateResponseCurve();
    repaint();
}

DynamicFilterProcessorEditor::DynamicFilterProcessorEditor(DynamicFilterProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p), responseDisplay(p)
{
    setSize(700, 500);
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

    addAndMakeVisible(qSlider);
    qSlider.setSliderStyle(juce::Slider::RotaryVerticalDrag);
    qSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    qAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        audioProcessor.apvts, "q", qSlider);

    addAndMakeVisible(qLabel);
    qLabel.setText("Q / Resonance", juce::dontSendNotification);
    qLabel.setJustificationType(juce::Justification::centred);
    qLabel.attachToComponent(&qSlider, false);
    qLabel.setColour(juce::Label::textColourId, juce::Colours::white);

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

    addAndMakeVisible(responseDisplay);

    addAndMakeVisible(inputMeterLabel);
    inputMeterLabel.setJustificationType(juce::Justification::centredLeft);
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    inputMeterLabel.setFont(12.0f);

    addAndMakeVisible(outputMeterLabel);
    outputMeterLabel.setJustificationType(juce::Justification::centredLeft);
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colours::cyan);
    outputMeterLabel.setFont(12.0f);

    addAndMakeVisible(gainReductionLabel);
    gainReductionLabel.setJustificationType(juce::Justification::centredLeft);
    gainReductionLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    gainReductionLabel.setFont(12.0f);

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
    g.setFont(24.0f);
    g.drawText("Professional Dynamic Filter", headerArea, juce::Justification::centred);
}

void DynamicFilterProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    bounds.removeFromTop(50);

    auto mainArea = bounds.reduced(10);

    auto displayArea = mainArea.removeFromTop(mainArea.getHeight() * 0.55f);
    responseDisplay.setBounds(displayArea.reduced(5));

    auto controlsArea = mainArea.reduced(5);

    auto rotaryArea = controlsArea.removeFromTop(120);
    int rotaryWidth = rotaryArea.getWidth() / 2;

    auto cutoffArea = rotaryArea.removeFromLeft(rotaryWidth).reduced(10);
    cutoffSlider.setBounds(cutoffArea.removeFromTop(100));

    auto qArea = rotaryArea.reduced(10);
    qSlider.setBounds(qArea.removeFromTop(100));

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