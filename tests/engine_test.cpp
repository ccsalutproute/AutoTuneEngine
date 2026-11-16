// Test du moteur DSP (sans JUCE) : un La faux de +40 cents avec vibrato
// doit ressortir verrouillé sur La (MIDI 57) en Do majeur, Retune Speed = 0.
#include "../Source/AutoTuneEngine.h"
#include <cstdio>
#include <vector>

int main()
{
    const double sr = 44100;
    vt::AutoTuneEngine e;
    e.prepare (sr);
    vt::EngineParams p;
    p.noteMask = vt::absoluteScaleMask (0, vt::Major);
    p.retuneMs = 0;
    e.setParams (p);

    const int N = (int) (sr * 1.5);
    std::vector<float> out ((size_t) N);
    double ph = 0;
    for (int i = 0; i < N; ++i)
    {
        const double t = i / sr;
        const double midi = 57.4 + 0.3 * std::sin (2 * vt::kPi * 5 * t);
        ph += 2 * vt::kPi * 440.0 * std::pow (2.0, (midi - 69) / 12) / sr;
        float x = 0;
        for (int h = 1; h <= 8; ++h) x += (float) (0.3 / h * std::sin (h * ph));
        out[(size_t) i] = e.processSample (x);
    }

    vt::PitchDetector d;
    d.prepare (sr);
    double worst = 0;
    int count = 0;
    for (int i = 0; i < N; ++i)
        if (d.push (out[(size_t) i]) && i > e.getLatency() + (int) (sr * 0.2))
        {
            const float f = d.getFrequency();
            if (f <= 0) continue;
            const double err = std::abs (69 + 12 * std::log2 (f / 440.0) - 57.0) * 100.0;
            worst = std::max (worst, err);
            ++count;
        }

    std::printf ("mesures: %d, erreur max: %.1f cents\n", count, worst);
    if (count < 100 || worst > 8.0) { std::puts ("ECHEC"); return 1; }
    std::puts ("OK");
    return 0;
}
