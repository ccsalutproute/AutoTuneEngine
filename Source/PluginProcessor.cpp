#include "PluginProcessor.h"
#include "PluginEditor.h"

// Chaînes accentuées : toujours passer par fromUTF8 (portable MSVC / clang)
static juce::String toU8 (const char* s) { return juce::String::fromUTF8 (s); }

static juce::StringArray u8List (std::initializer_list<const char*> items)
{
    juce::StringArray a;
    for (auto* i : items) a.add (toU8 (i));
    return a;
}

static const juce::StringArray kKeys = u8List ({ "Do (C)", "Do# (C#)", "Ré (D)", "Ré# (D#)", "Mi (E)", "Fa (F)",
                                                 "Fa# (F#)", "Sol (G)", "Sol# (G#)", "La (A)", "La# (A#)", "Si (B)" });

static const juce::StringArray kVoices = u8List ({ "Soprano", "Alto / Ténor", "Baryton", "Basse", "Instrument" });

static void voiceRange (int v, float& lo, float& hi)
{
    switch (v)
    {
        case 0:  lo = 160.0f; hi = 1200.0f; break;
        case 1:  lo = 85.0f;  hi = 900.0f;  break;
        case 2:  lo = 70.0f;  hi = 600.0f;  break;
        case 3:  lo = 55.0f;  hi = 400.0f;  break;
        default: lo = 50.0f;  hi = 1500.0f; break;
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout VoxTuneProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray scales;
    for (int i = 0; i < vt::NumScales; ++i) scales.add (String::fromUTF8 (vt::getScaleNames()[i]));

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, toU8 ("Tonalité"), kKeys, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Gamme", scales, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, "Voix", kVoices, 1));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "retune", 1 }, "Retune Speed",
              NormalisableRange<float> (0.0f, 400.0f, 0.1f, 0.4f), 20.0f,
              AudioParameterFloatAttributes().withLabel ("ms")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Correction",
              NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
              AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "stability", 1 }, toU8 ("Stabilité"),
              NormalisableRange<float> (0.0f, 50.0f, 0.1f), 15.0f,
              AudioParameterFloatAttributes().withLabel ("cents")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "transpose", 1 }, "Transpose",
              NormalisableRange<float> (-12.0f, 12.0f, 1.0f), 0.0f,
              AudioParameterFloatAttributes().withLabel ("st")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
              NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
              AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "refA", 1 }, "Diapason",
              NormalisableRange<float> (430.0f, 450.0f, 0.1f), 440.0f,
              AudioParameterFloatAttributes().withLabel ("Hz")));

    static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "note" + String (i), 1 },
                                                     String ("Note ") + noteNames[i], true));
    return l;
}

VoxTuneProcessor::VoxTuneProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VoxTuneState", createLayout())
{
    keyParam   = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("key"));
    scaleParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("scale"));
    voiceParam = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter ("voice"));
    retune     = apvts.getRawParameterValue ("retune");
    amount     = apvts.getRawParameterValue ("amount");
    stability  = apvts.getRawParameterValue ("stability");
    transpose  = apvts.getRawParameterValue ("transpose");
    mix        = apvts.getRawParameterValue ("mix");
    refA       = apvts.getRawParameterValue ("refA");
    for (int i = 0; i < 12; ++i)
        noteParams[i] = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter ("note" + juce::String (i)));
}

bool VoxTuneProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void VoxTuneProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
    setLatencySamples (engine.getLatency());   // FL Studio compense automatiquement
}

uint16_t VoxTuneProcessor::getEffectiveMask() const
{
    const int scale = scaleParam->getIndex();
    if (scale == vt::Custom)
    {
        uint16_t m = 0;
        for (int i = 0; i < 12; ++i)
            if (noteParams[i]->get()) m |= (uint16_t) (1u << i);
        return m;
    }
    return vt::absoluteScaleMask (keyParam->getIndex(), scale);
}

void VoxTuneProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn  = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int ch = numIn; ch < numOut; ++ch)
        buffer.clear (ch, 0, n);

    vt::EngineParams p;
    p.noteMask   = getEffectiveMask();
    p.retuneMs   = retune->load();
    p.amount     = amount->load() * 0.01f;
    p.hysteresis = stability->load() * 0.01f;
    p.transpose  = transpose->load();
    p.mix        = mix->load() * 0.01f;
    p.referenceA = refA->load();
    voiceRange (voiceParam->getIndex(), p.minHz, p.maxHz);
    engine.setParams (p);

    // La voix est traitée en mono (somme des canaux), puis renvoyée sur toutes les sorties.
    const int chans = juce::jmax (1, numIn);
    const float norm = 1.0f / (float) chans;
    float* out0 = buffer.getWritePointer (0);

    for (int i = 0; i < n; ++i)
    {
        float x = 0.0f;
        for (int ch = 0; ch < numIn; ++ch) x += buffer.getReadPointer (ch)[i];
        out0[i] = engine.processSample (x * norm);
    }
    for (int ch = 1; ch < numOut; ++ch)
        buffer.copyFrom (ch, 0, out0, n);
}

juce::AudioProcessorEditor* VoxTuneProcessor::createEditor()
{
    return new VoxTuneEditor (*this);
}

void VoxTuneProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VoxTuneProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VoxTuneProcessor();
}
