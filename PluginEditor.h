#pragma once

#include "PluginProcessor.h"

class FrequencyResponseDisplay : public juce::Component, public juce::Timer
{
public:
    FrequencyResponseDisplay(DynamicFilterProcessor& p);
    ~FrequencyResponseDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

private:
    DynamicFilterProcessor& audioProcessor;
    std::vector<float> magnitudeData;
    juce::Path responseCurve;

    void drawGrid(juce::Graphics& g);
    void drawFrequencyLabels(juce::Graphics& g);
    void drawMagnitudeLabels(juce::Graphics& g);
    void updateResponseCurve();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FrequencyResponseDisplay)
};

class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel()
    {
        setColour(juce::Slider::thumbColourId, juce::Colours::cyan);
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::cyan);
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::darkgrey);
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
        setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::black);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::grey);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
        juce::Slider&) override
    {
        auto radius = juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = x + width * 0.5f;
        auto centreY = y + height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        g.setColour(juce::Colours::darkgrey);
        g.fillEllipse(rx, ry, rw, rw);

        juce::Path arcPath;
        arcPath.addCentredArc(centreX, centreY, radius, radius, 0.0f,
            rotaryStartAngle, angle, true);
        g.setColour(findColour(juce::Slider::rotarySliderFillColourId));
        g.strokePath(arcPath, juce::PathStrokeType(3.0f));

        juce::Path pointer;
        auto pointerLength = radius * 0.7f;
        auto pointerThickness = 3.0f;
        pointer.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));
        g.setColour(juce::Colours::white);
        g.fillPath(pointer);
    }
};

class DynamicFilterProcessorEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    DynamicFilterProcessorEditor(DynamicFilterProcessor&);
    ~DynamicFilterProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    DynamicFilterProcessor& audioProcessor;
    CustomLookAndFeel customLookAndFeel;

    juce::Slider cutoffSlider;
    juce::Label cutoffLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> cutoffAttachment;

    juce::Slider qSlider;
    juce::Label qLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> qAttachment;

    juce::ComboBox typeComboBox;
    juce::Label typeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> typeAttachment;

    juce::ComboBox slopeComboBox;
    juce::Label slopeLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAttachment;

    juce::ComboBox characteristicComboBox;
    juce::Label characteristicLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> characteristicAttachment;

    juce::ToggleButton bypassButton{ "Bypass" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    FrequencyResponseDisplay responseDisplay;

    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;
    juce::Label gainReductionLabel;

    void updateMeters();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DynamicFilterProcessorEditor)
};