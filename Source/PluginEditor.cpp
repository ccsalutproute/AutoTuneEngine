#include "PluginEditor.h"

namespace
{
    const juce::Colour kBg      { 0xff12141a };
    const juce::Colour kPanel   { 0xff1b1e27 };
    const juce::Colour kLine    { 0xff2a2f3c };
    const juce::Colour kText    { 0xffd8dce6 };
    const juce::Colour kDim     { 0xff7c8496 };
    const juce::Colour kAccent  { 0xff38d9a9 };
    const juce::Colour kWarm    { 0xffffb454 };

    const char* kNoteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

    juce::String toU8 (const char* s) { return juce::String::fromUTF8 (s); }
}

//==============================================================================
VoxLookAndFeel::VoxLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kDim);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextButton::textColourOffId, kDim);
    setColour (juce::TextButton::textColourOnId, kBg);
}

void VoxLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                       float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float track = juce::jmax (3.0f, r * 0.12f);

    juce::Path bg;
    bg.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, startAngle, endAngle, true);
    g.setColour (kLine);
    g.strokePath (bg, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path val;
    val.addCentredArc (c.x, c.y, r - track, r - track, 0.0f, startAngle, angle, true);
    g.setColour (kAccent);
    g.strokePath (val, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float knobR = r - track * 2.6f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2c313e), c.x, c.y - knobR,
                                             juce::Colour (0xff1a1d25), c.x, c.y + knobR, false));
    g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);

    juce::Path ptr;
    ptr.addRoundedRectangle (-1.5f, -knobR + 4.0f, 3.0f, knobR * 0.45f, 1.5f);
    g.setColour (kText);
    g.fillPath (ptr, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void VoxLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (kPanel.brighter (0.05f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (kLine);
    g.drawRoundedRectangle (r, 5.0f, 1.0f);

    juce::Path arrow;
    const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
    arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
    g.setColour (kDim);
    g.fillPath (arrow);
}

void VoxLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.5f);
    const bool on = b.getToggleState();
    g.setColour (on ? kAccent : kPanel.brighter (over ? 0.12f : 0.04f));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? kAccent.brighter (0.3f) : kLine);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

//==============================================================================
void PitchGraph::push (float detected, float corrected, uint16_t m)
{
    det[write] = detected;
    cor[write] = corrected;
    write = (write + 1) % kLen;
    mask = m;
    if (corrected > 0) centre += (corrected - centre) * 0.05f;
}

void PitchGraph::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    g.setColour (kPanel);
    g.fillRoundedRectangle (b, 8.0f);

    const float range = 14.0f; // demi-tons visibles
    const float lo = centre - range * 0.5f;
    auto yOf = [&] (float midi) { return b.getBottom() - (midi - lo) / range * b.getHeight(); };

    g.saveState();
    g.reduceClipRegion (b.toNearestInt());

    // Lignes de notes
    g.setFont (juce::FontOptions (11.0f));
    for (int n = (int) std::floor (lo); n <= (int) std::ceil (lo + range); ++n)
    {
        const int pc = ((n % 12) + 12) % 12;
        const bool allowed = (mask >> pc) & 1;
        const float y = yOf ((float) n);
        g.setColour (allowed ? kLine.brighter (0.25f) : kLine.withAlpha (0.4f));
        g.drawHorizontalLine ((int) y, b.getX() + 34.0f, b.getRight());
        g.setColour (allowed ? kText.withAlpha (0.8f) : kDim.withAlpha (0.4f));
        g.drawText (juce::String (kNoteNames[pc]) + juce::String (n / 12 - 1),
                    (int) b.getX() + 4, (int) y - 7, 30, 14, juce::Justification::left);
    }

    auto drawCurve = [&] (const float* data, juce::Colour col, float thick)
    {
        juce::Path p;
        bool started = false;
        const float dx = (b.getWidth() - 36.0f) / (float) (kLen - 1);
        for (int i = 0; i < kLen; ++i)
        {
            const float v = data[(write + i) % kLen];
            const float x = b.getX() + 36.0f + i * dx;
            if (v <= 0) { started = false; continue; }
            const float y = yOf (v);
            if (! started) { p.startNewSubPath (x, y); started = true; }
            else p.lineTo (x, y);
        }
        g.setColour (col);
        g.strokePath (p, juce::PathStrokeType (thick, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    };

    drawCurve (det, kDim.withAlpha (0.8f), 1.5f);
    drawCurve (cor, kAccent, 2.2f);
    g.restoreState();

    g.setColour (kLine);
    g.drawRoundedRectangle (b.reduced (0.5f), 8.0f, 1.0f);
}

//==============================================================================
VoxTuneEditor::VoxTuneEditor (VoxTuneProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);

    auto setupKnob = [this] (Knob& k, const juce::String& id, const juce::String& name)
    {
        k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 80, 18);
        k.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                      juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (k.slider);
        k.label.setText (name, juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.label.setColour (juce::Label::textColourId, kDim);
        k.label.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (k.label);
        k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    };

    setupKnob (retune,    "retune",    "RETUNE SPEED");
    setupKnob (amount,    "amount",    "CORRECTION");
    setupKnob (stability, "stability", toU8 ("STABILITÉ"));
    setupKnob (transpose, "transpose", "TRANSPOSE");
    setupKnob (mix,       "mix",       "MIX");
    retune.slider.setTextValueSuffix (" ms");
    amount.slider.setTextValueSuffix (" %");
    stability.slider.setTextValueSuffix (" c");
    transpose.slider.setTextValueSuffix (" st");
    mix.slider.setTextValueSuffix (" %");

    auto setupCombo = [this] (juce::ComboBox& box, juce::Label& lab, const juce::String& id, const juce::String& name,
                              std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>& att)
    {
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
            box.addItemList (c->choices, 1);
        addAndMakeVisible (box);
        lab.setText (name, juce::dontSendNotification);
        lab.setColour (juce::Label::textColourId, kDim);
        lab.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        addAndMakeVisible (lab);
        att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, box);
    };

    setupCombo (keyBox,   keyLabel,   "key",   toU8 ("TONALITÉ"), keyAtt);
    setupCombo (scaleBox, scaleLabel, "scale", "GAMME",            scaleAtt);
    setupCombo (voiceBox, voiceLabel, "voice", "TYPE DE VOIX",     voiceAtt);

    for (int i = 0; i < 12; ++i)
    {
        noteButtons[i].setButtonText (kNoteNames[i]);
        noteButtons[i].setClickingTogglesState (false);
        noteButtons[i].onClick = [this, i] { noteClicked (i); };
        addAndMakeVisible (noteButtons[i]);
    }

    addAndMakeVisible (graph);

    setResizable (true, true);
    setResizeLimits (720, 460, 1400, 900);
    setSize (820, 500);
    startTimerHz (30);
}

VoxTuneEditor::~VoxTuneEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VoxTuneEditor::noteClicked (int pc)
{
    // Cliquer une note passe en gamme personnalisée en partant de la gamme actuelle
    const uint16_t current = proc.getEffectiveMask();
    auto* scale = proc.apvts.getParameter ("scale");

    if (dynamic_cast<juce::AudioParameterChoice*> (scale)->getIndex() != vt::Custom)
    {
        for (int i = 0; i < 12; ++i)
        {
            auto* np = proc.apvts.getParameter ("note" + juce::String (i));
            np->beginChangeGesture();
            np->setValueNotifyingHost ((current >> i) & 1 ? 1.0f : 0.0f);
            np->endChangeGesture();
        }
        scale->beginChangeGesture();
        scale->setValueNotifyingHost (scale->convertTo0to1 ((float) vt::Custom));
        scale->endChangeGesture();
    }

    auto* np = proc.apvts.getParameter ("note" + juce::String (pc));
    np->beginChangeGesture();
    np->setValueNotifyingHost (np->getValue() > 0.5f ? 0.0f : 1.0f);
    np->endChangeGesture();
}

void VoxTuneEditor::timerCallback()
{
    const float det = proc.engine.detectedMidi.load();
    const float cor = proc.engine.targetMidi.load();
    const uint16_t mask = proc.getEffectiveMask();

    graph.push (det, cor, mask);
    graph.repaint();

    for (int i = 0; i < 12; ++i)
        noteButtons[i].setToggleState ((mask >> i) & 1, juce::dontSendNotification);

    hasPitch = det > 0;
    if (hasPitch)
    {
        const int nearest = (int) std::lround (det);
        noteText = juce::String (kNoteNames[((nearest % 12) + 12) % 12]) + juce::String (nearest / 12 - 1);
        centsValue = (det - (float) nearest) * 100.0f;
        centsText = (centsValue >= 0 ? "+" : "") + juce::String ((int) std::lround (centsValue)) + " cents";
    }
    repaint (0, 0, getWidth(), 70);
}

void VoxTuneEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // En-tête
    g.setColour (kText);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("VOX", 20, 18, 60, 30, juce::Justification::left);
    g.setColour (kAccent);
    g.drawText ("TUNE", 70, 18, 80, 30, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("PITCH CORRECTION", 20, 44, 200, 14, juce::Justification::left);

    // Note détectée + indicateur de justesse
    const int cx = getWidth() - 250;
    g.setColour (hasPitch ? kText : kDim);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawText (hasPitch ? noteText : "--", cx, 14, 70, 34, juce::Justification::centredLeft);

    auto meter = juce::Rectangle<float> ((float) cx + 75, 26.0f, 150.0f, 8.0f);
    g.setColour (kPanel);
    g.fillRoundedRectangle (meter, 4.0f);
    g.setColour (kLine.brighter (0.4f));
    g.fillRect (meter.getCentreX() - 0.5f, meter.getY() - 3, 1.0f, meter.getHeight() + 6);
    if (hasPitch)
    {
        const float px = meter.getCentreX() + juce::jlimit (-50.0f, 50.0f, centsValue) / 50.0f * meter.getWidth() * 0.5f;
        g.setColour (std::abs (centsValue) < 10.0f ? kAccent : kWarm);
        g.fillEllipse (px - 6, meter.getCentreY() - 6, 12, 12);
        g.setColour (kDim);
        g.setFont (juce::FontOptions (11.0f));
        g.drawText (centsText, (int) meter.getX(), 38, (int) meter.getWidth(), 14, juce::Justification::centred);
    }
}

void VoxTuneEditor::resized()
{
    auto area = getLocalBounds().reduced (18);
    area.removeFromTop (60);

    // Colonne de gauche : sélecteurs
    auto left = area.removeFromLeft (200);
    auto placeCombo = [&] (juce::Label& lab, juce::ComboBox& box)
    {
        lab.setBounds (left.removeFromTop (18));
        box.setBounds (left.removeFromTop (28));
        left.removeFromTop (12);
    };
    placeCombo (keyLabel, keyBox);
    placeCombo (scaleLabel, scaleBox);
    placeCombo (voiceLabel, voiceBox);

    area.removeFromLeft (16);

    // Bas : potentiomètres
    auto knobs = area.removeFromBottom (130);
    const int kw = knobs.getWidth() / 5;
    for (auto* k : { &retune, &amount, &stability, &transpose, &mix })
    {
        auto col = knobs.removeFromLeft (kw);
        k->label.setBounds (col.removeFromTop (18));
        k->slider.setBounds (col.reduced (4, 0));
    }

    area.removeFromBottom (12);

    // Notes de la gamme
    auto notes = area.removeFromBottom (32);
    const int nw = notes.getWidth() / 12;
    for (int i = 0; i < 12; ++i)
        noteButtons[i].setBounds (notes.removeFromLeft (nw).reduced (2, 0));

    area.removeFromBottom (12);
    graph.setBounds (area);

    // Le graphique occupe aussi la hauteur sous les sélecteurs à gauche : rien d'autre à placer.
    juce::ignoreUnused (left);
}
