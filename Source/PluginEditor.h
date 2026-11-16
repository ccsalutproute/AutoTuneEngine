#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class VoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VoxLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh,
                       juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool over, bool down) override;
};

//==============================================================================
/** Graphique défilant : hauteur détectée (gris) et corrigée (couleur). */
class PitchGraph : public juce::Component
{
public:
    void push (float detected, float corrected, uint16_t mask);
    void paint (juce::Graphics&) override;

private:
    static constexpr int kLen = 240;
    float det[kLen] {}, cor[kLen] {};
    int write = 0;
    uint16_t mask = 0x0FFF;
    float centre = 60.0f;
};

//==============================================================================
class VoxTuneEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VoxTuneEditor (VoxTuneProcessor&);
    ~VoxTuneEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void noteClicked (int pc);

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    VoxTuneProcessor& proc;
    VoxLookAndFeel lnf;

    Knob retune, amount, stability, transpose, mix;
    juce::ComboBox keyBox, scaleBox, voiceBox;
    juce::Label keyLabel, scaleLabel, voiceLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt, scaleAtt, voiceAtt;

    juce::TextButton noteButtons[12];
    PitchGraph graph;

    juce::String noteText = "--", centsText;
    float centsValue = 0.0f;
    bool hasPitch = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxTuneEditor)
};
