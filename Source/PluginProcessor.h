#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "AutoTuneEngine.h"

class VoxTuneProcessor : public juce::AudioProcessor
{
public:
    VoxTuneProcessor();
    ~VoxTuneProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "VoxTune"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** Masque des notes autorisées (bit 0 = Do) selon la tonalité/gamme courante. */
    uint16_t getEffectiveMask() const;

    juce::AudioProcessorValueTreeState apvts;
    vt::AutoTuneEngine engine;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

private:
    juce::AudioParameterChoice* keyParam = nullptr;
    juce::AudioParameterChoice* scaleParam = nullptr;
    juce::AudioParameterChoice* voiceParam = nullptr;
    std::atomic<float>* retune = nullptr;
    std::atomic<float>* amount = nullptr;
    std::atomic<float>* stability = nullptr;
    std::atomic<float>* transpose = nullptr;
    std::atomic<float>* mix = nullptr;
    std::atomic<float>* refA = nullptr;
    juce::AudioParameterBool* noteParams[12] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VoxTuneProcessor)
};
