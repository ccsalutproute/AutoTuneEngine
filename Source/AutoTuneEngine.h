/*
  ==============================================================================
    VoxTune - AutoTuneEngine.h
    Moteur DSP de correction de hauteur, indépendant de JUCE (C++17 pur).

    Chaîne de traitement :
      1. Détection de hauteur  : YIN (décimé ~11 kHz) + raffinement pleine résolution
      2. Quantification        : note la plus proche dans la gamme, avec hystérésis
      3. Lissage (Retune Speed): filtre à un pôle sur la correction en demi-tons
      4. Transposition         : TD-PSOLA (préserve naturellement les formants)
  ==============================================================================
*/
#pragma once

#include <vector>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <atomic>

namespace vt
{

constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Biquad passe-bas (RBJ)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;

    void setLowpass (double sr, double fc, double q = 0.7071)
    {
        const double w0 = 2.0 * kPi * fc / sr, c = std::cos (w0), s = std::sin (w0);
        const double alpha = s / (2.0 * q), a0 = 1.0 + alpha;
        b0 = (float) ((1.0 - c) * 0.5 / a0);
        b1 = (float) ((1.0 - c) / a0);
        b2 = b0;
        a1 = (float) (-2.0 * c / a0);
        a2 = (float) ((1.0 - alpha) / a0);
    }
    inline float process (float x) noexcept
    {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
    void reset() noexcept { z1 = z2 = 0; }
};

//==============================================================================
// Détecteur de hauteur YIN
class PitchDetector
{
public:
    void prepare (double sampleRate)
    {
        sr    = sampleRate;
        decim = std::max (1, (int) std::lround (sr / 11025.0));
        dsr   = sr / decim;

        lp1.setLowpass (sr, std::min (2200.0, dsr * 0.4));
        lp2.setLowpass (sr, std::min (2200.0, dsr * 0.4));

        win        = (int) std::ceil (dsr * 0.025);           // fenêtre ~25 ms
        tauMaxLim  = (int) std::ceil (dsr / 50.0) + 2;        // jusqu'à 50 Hz
        hop        = std::max (1, (int) std::lround (dsr * 0.004)); // ~4 ms

        dbuf.assign (kDSize, 0.0f);
        fbuf.assign (kFSize, 0.0f);
        frame.assign ((size_t) (win + tauMaxLim + 4), 0.0f);
        diff.assign ((size_t) (tauMaxLim + 4), 0.0f);
        cmnd.assign ((size_t) (tauMaxLim + 4), 1.0f);

        setRange (70.0f, 1000.0f);
        reset();
    }

    void setRange (float minHz, float maxHz)
    {
        tauMin = std::max (2, (int) std::floor (dsr / maxHz));
        tauMax = std::min (tauMaxLim - 2, (int) std::ceil (dsr / minHz));
    }

    void reset()
    {
        std::fill (dbuf.begin(), dbuf.end(), 0.0f);
        std::fill (fbuf.begin(), fbuf.end(), 0.0f);
        lp1.reset(); lp2.reset();
        dpos = fpos = 0; acc = 0; accCount = 0; hopCount = 0;
        hist[0] = hist[1] = hist[2] = 0;
        freq = 0; confidence = 0;
    }

    /** Retourne true quand une nouvelle estimation est disponible. */
    inline bool push (float x) noexcept
    {
        fbuf[(size_t) (fpos & kFMask)] = x;
        ++fpos;

        acc += lp2.process (lp1.process (x));
        if (++accCount >= decim)
        {
            dbuf[(size_t) (dpos & kDMask)] = acc / (float) decim;
            ++dpos; acc = 0; accCount = 0;

            if (++hopCount >= hop)
            {
                hopCount = 0;
                analyse();
                return true;
            }
        }
        return false;
    }

    float getFrequency() const noexcept   { return freq; }       // 0 = non voisé
    float getConfidence() const noexcept  { return confidence; }
    double getHopSeconds() const noexcept { return (double) (hop * decim) / sr; }
    void setGate (float linear) noexcept  { gate = linear; }
    void setThreshold (float t) noexcept  { threshold = t; }

private:
    void analyse() noexcept
    {
        const int N = win + tauMax + 1;
        if (dpos < N) { pushResult (0); return; }

        for (int i = 0; i < N; ++i)
            frame[(size_t) i] = dbuf[(size_t) ((dpos - N + i) & kDMask)];

        // Gate de niveau
        double e = 0;
        for (int i = N - win; i < N; ++i) e += (double) frame[(size_t) i] * frame[(size_t) i];
        if (std::sqrt (e / win) < gate) { confidence = 0; pushResult (0); return; }

        // Fonction de différence + moyenne cumulative normalisée
        cmnd[0] = 1.0f;
        double running = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double s = 0;
            const float* a = frame.data();
            const float* b = frame.data() + tau;
            for (int j = 0; j < win; ++j) { const float d = a[j] - b[j]; s += (double) d * d; }
            diff[(size_t) tau] = (float) s;
            running += s;
            cmnd[(size_t) tau] = running > 0 ? (float) (s * tau / running) : 1.0f;
        }

        // Premier minimum sous le seuil
        int best = -1;
        for (int tau = tauMin; tau <= tauMax; ++tau)
        {
            if (cmnd[(size_t) tau] < threshold)
            {
                while (tau + 1 <= tauMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau]) ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
        {
            float mn = 1e9f;
            for (int tau = tauMin; tau <= tauMax; ++tau)
                if (cmnd[(size_t) tau] < mn) { mn = cmnd[(size_t) tau]; best = tau; }
            if (mn > 0.35f) { confidence = 1.0f - mn; pushResult (0); return; }
        }
        confidence = 1.0f - cmnd[(size_t) best];

        // Interpolation parabolique (résolution décimée)
        double tauF = best;
        if (best > 1 && best < tauMax)
        {
            const double y0 = cmnd[(size_t) best - 1], y1 = cmnd[(size_t) best], y2 = cmnd[(size_t) best + 1];
            const double den = y0 - 2 * y1 + y2;
            if (std::abs (den) > 1e-12) tauF += 0.5 * (y0 - y2) / den;
        }

        // Raffinement à pleine résolution autour de tauF*decim
        double period = tauF * decim;
        if (decim > 1)
        {
            const int Wf  = win * decim;
            const int c   = (int) std::lround (period);
            const int lo  = std::max (2, c - decim), hi = c + decim;
            if (fpos > (int64_t) (Wf + hi + 2))
            {
                const int64_t start = fpos - Wf - hi - 1;
                double dPrev = 0, dCur = 0, dBest = 1e300;
                int lBest = c;
                double dl[64];
                const int count = hi - lo + 1;
                if (count <= 62)
                {
                    for (int L = lo; L <= hi; ++L)
                    {
                        double s = 0;
                        for (int j = 0; j < Wf; ++j)
                        {
                            const float d = fbuf[(size_t) ((start + j) & kFMask)]
                                          - fbuf[(size_t) ((start + j + L) & kFMask)];
                            s += (double) d * d;
                        }
                        dl[L - lo] = s;
                        if (s < dBest) { dBest = s; lBest = L; }
                    }
                    (void) dPrev; (void) dCur;
                    double p = lBest;
                    if (lBest > lo && lBest < hi)
                    {
                        const double y0 = dl[lBest - lo - 1], y1 = dl[lBest - lo], y2 = dl[lBest - lo + 1];
                        const double den = y0 - 2 * y1 + y2;
                        if (std::abs (den) > 1e-20) p += 0.5 * (y0 - y2) / den;
                    }
                    period = p;
                }
            }
        }

        pushResult ((float) (sr / period));
    }

    // Médiane sur 3 estimations pour éliminer les erreurs d'octave isolées
    void pushResult (float f) noexcept
    {
        hist[0] = hist[1]; hist[1] = hist[2]; hist[2] = f;
        int voiced = (hist[0] > 0) + (hist[1] > 0) + (hist[2] > 0);
        if (hist[2] <= 0 || voiced < 2) { freq = 0; return; }
        float a = hist[0] > 0 ? hist[0] : hist[2];
        float b = hist[1] > 0 ? hist[1] : hist[2];
        float c = hist[2];
        freq = std::max (std::min (a, b), std::min (std::max (a, b), c));
    }

    static constexpr int kDSize = 4096,  kDMask = kDSize - 1;
    static constexpr int kFSize = 32768, kFMask = kFSize - 1;

    double sr = 44100, dsr = 11025;
    int decim = 4, win = 256, tauMin = 10, tauMax = 160, tauMaxLim = 224, hop = 44;
    float gate = 0.0025f, threshold = 0.15f;
    Biquad lp1, lp2;
    std::vector<float> dbuf, fbuf, frame, diff, cmnd;
    int64_t dpos = 0, fpos = 0;
    float acc = 0; int accCount = 0, hopCount = 0;
    float hist[3] {};
    float freq = 0, confidence = 0;
};

//==============================================================================
// Gammes
enum ScaleType
{
    Chromatic = 0, Major, NaturalMinor, HarmonicMinor, MelodicMinor,
    MajorPentatonic, MinorPentatonic, Blues, Dorian, Phrygian, Lydian, Mixolydian,
    Custom, NumScales
};

inline const char* const* getScaleNames()
{
    static const char* names[] = { "Chromatique", "Majeure", "Mineure naturelle", "Mineure harmonique",
                                   "Mineure mélodique", "Pentatonique majeure", "Pentatonique mineure",
                                   "Blues", "Dorien", "Phrygien", "Lydien", "Mixolydien", "Personnalisée" };
    return names;
}

/** Masque relatif à la tonique (bit i = demi-ton i). */
inline uint16_t relativeScaleMask (int type)
{
    auto m = [] (std::initializer_list<int> notes) { uint16_t r = 0; for (int n : notes) r |= (uint16_t) (1u << n); return r; };
    switch (type)
    {
        case Major:            return m ({ 0, 2, 4, 5, 7, 9, 11 });
        case NaturalMinor:     return m ({ 0, 2, 3, 5, 7, 8, 10 });
        case HarmonicMinor:    return m ({ 0, 2, 3, 5, 7, 8, 11 });
        case MelodicMinor:     return m ({ 0, 2, 3, 5, 7, 9, 11 });
        case MajorPentatonic:  return m ({ 0, 2, 4, 7, 9 });
        case MinorPentatonic:  return m ({ 0, 3, 5, 7, 10 });
        case Blues:            return m ({ 0, 3, 5, 6, 7, 10 });
        case Dorian:           return m ({ 0, 2, 3, 5, 7, 9, 10 });
        case Phrygian:         return m ({ 0, 1, 3, 5, 7, 8, 10 });
        case Lydian:           return m ({ 0, 2, 4, 6, 7, 9, 11 });
        case Mixolydian:       return m ({ 0, 2, 4, 5, 7, 9, 10 });
        default:               return 0x0FFF;
    }
}

/** Masque absolu (bit 0 = Do, bit 9 = La...). */
inline uint16_t absoluteScaleMask (int key, int type)
{
    const uint16_t rel = relativeScaleMask (type);
    uint16_t abs = 0;
    for (int i = 0; i < 12; ++i)
        if (rel & (1u << i)) abs |= (uint16_t) (1u << ((i + key) % 12));
    return abs;
}

//==============================================================================
struct EngineParams
{
    uint16_t noteMask   = 0x0FFF; // notes autorisées (masque absolu)
    float retuneMs      = 10.0f;  // 0 = effet robotique instantané
    float amount        = 1.0f;   // 0..1
    float hysteresis    = 0.15f;  // demi-tons
    float transpose     = 0.0f;   // demi-tons
    float mix           = 1.0f;   // 0..1
    float minHz         = 70.0f;
    float maxHz         = 1000.0f;
    float referenceA    = 440.0f;
};

//==============================================================================
class AutoTuneEngine
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        detector.prepare (sr);
        maxPeriod     = (int) std::ceil (sr / 50.0);
        minPeriod     = (int) std::floor (sr / 1500.0);
        unvoicedStep  = std::max (32, (int) std::lround (sr * 0.006));
        latency       = 3 * maxPeriod + 16;

        int size = 1;
        while (size < latency * 4 + 4 * maxPeriod) size <<= 1;
        bufSize = size; bufMask = size - 1;
        inBuf.assign ((size_t) size, 0.0f);
        outBuf.assign ((size_t) size, 0.0f);
        reset();
    }

    void reset()
    {
        std::fill (inBuf.begin(), inBuf.end(), 0.0f);
        std::fill (outBuf.begin(), outBuf.end(), 0.0f);
        detector.reset();
        inPos = 0; lastMark = 0; lastMarkVoiced = false;
        nextSynth = (double) unvoicedStep;
        markHead = markCount = 0;
        voiced = false; period = unvoicedStep; ratio = 1.0;
        smoothedCorr = 0; currentNote = -1000;
        detectedMidi.store (-1.0f); targetMidi.store (-1.0f);
    }

    void setParams (const EngineParams& p)
    {
        params = p;
        if (params.noteMask == 0) params.noteMask = 0x0FFF;
        detector.setRange (std::max (50.0f, p.minHz), std::min (1500.0f, p.maxHz));
    }

    int getLatency() const noexcept { return latency; }

    inline float processSample (float x) noexcept
    {
        const int64_t n = inPos;
        inBuf[(size_t) (n & bufMask)] = x;
        ++inPos;

        if (detector.push (x)) updatePitch();
        placeMarks();
        synthesise (n);

        const size_t oi = (size_t) (n & bufMask);
        const float wet = outBuf[oi];
        outBuf[oi] = 0.0f;
        const float dry = inBuf[(size_t) ((n - latency) & bufMask)];
        return params.mix * wet + (1.0f - params.mix) * dry;
    }

    // Lus par l'interface (thread GUI)
    std::atomic<float> detectedMidi { -1.0f }, targetMidi { -1.0f };

private:
    struct Mark { int64_t pos; int period; float ratio; bool voiced; };

    //--------------------------------------------------------------------------
    int nearestAllowed (float midi) const noexcept
    {
        const int r = (int) std::lround (midi);
        int best = r; float bestD = 1e9f;
        for (int k = -7; k <= 7; ++k)
        {
            const int note = r + k;
            const int pc = ((note % 12) + 12) % 12;
            if (params.noteMask & (1u << pc))
            {
                const float d = std::abs ((float) note - midi);
                if (d < bestD) { bestD = d; best = note; }
            }
        }
        return best;
    }

    bool isAllowed (int note) const noexcept
    {
        return (params.noteMask & (1u << (((note % 12) + 12) % 12))) != 0;
    }

    void updatePitch() noexcept
    {
        const float f = detector.getFrequency();
        if (f < params.minHz || f > params.maxHz || f <= 0.0f)
        {
            voiced = false;
            ratio = 1.0;
            detectedMidi.store (-1.0f); targetMidi.store (-1.0f);
            return;
        }

        const float midi = 69.0f + 12.0f * std::log2 (f / params.referenceA);

        // Quantification avec hystérésis
        int cand = nearestAllowed (midi);
        if (voiced && isAllowed (currentNote) && cand != currentNote
            && std::abs (midi - (float) currentNote) - std::abs (midi - (float) cand) < params.hysteresis
            && std::abs (midi - (float) currentNote) < 1.5f)
            cand = currentNote;
        currentNote = cand;

        const double rawCorr = (double) cand - midi;
        if (! voiced)
            smoothedCorr = params.retuneMs < 1.0f ? rawCorr : 0.0;   // départ naturel
        else
        {
            const double tau = params.retuneMs * 0.001;
            const double coef = tau <= 0.0005 ? 1.0 : 1.0 - std::exp (-detector.getHopSeconds() / tau);
            smoothedCorr += (rawCorr - smoothedCorr) * coef;
        }

        const double shift = params.amount * smoothedCorr + params.transpose;
        ratio = std::clamp (std::pow (2.0, shift / 12.0), 0.5, 2.0);
        period = std::clamp ((int) std::lround (sr / f), minPeriod, maxPeriod);
        voiced = true;

        detectedMidi.store (midi);
        targetMidi.store ((float) (midi + shift));
    }

    //--------------------------------------------------------------------------
    // Marques d'analyse : une par période, alignées par corrélation
    void placeMarks() noexcept
    {
        for (;;)
        {
            const bool v = voiced;
            const int T = v ? period : unvoicedStep;
            const int search = (v && lastMarkVoiced) ? std::max (1, T / 4) : 0;
            const int half = T / 2;
            const int64_t cand = lastMark + T;
            if (cand + search + half + 2 >= inPos) break;

            int64_t m = cand;
            if (search > 0)
            {
                double bestC = -1e300;
                for (int off = -search; off <= search; ++off)
                {
                    double c = 0;
                    for (int i = -half; i < half; ++i)
                        c += (double) inBuf[(size_t) ((lastMark + i) & bufMask)]
                           * inBuf[(size_t) ((cand + off + i) & bufMask)];
                    if (c > bestC) { bestC = c; m = cand + off; }
                }
            }

            if (markCount == kMaxMarks) { markHead = (markHead + 1) % kMaxMarks; --markCount; }
            marks[(markHead + markCount) % kMaxMarks] = { m, T, (float) ratio, v };
            ++markCount;
            lastMark = m;
            lastMarkVoiced = v;
        }
    }

    inline float readInterp (double pos) const noexcept
    {
        const double fl = std::floor (pos);
        const int64_t i = (int64_t) fl;
        const float fr = (float) (pos - fl);
        const float a = inBuf[(size_t) (i & bufMask)], b = inBuf[(size_t) ((i + 1) & bufMask)];
        return a + fr * (b - a);
    }

    // Synthèse TD-PSOLA
    void synthesise (int64_t n) noexcept
    {
        while (nextSynth <= (double) (n + maxPeriod + 2))
        {
            const double mapped = nextSynth - latency;

            while (markCount >= 2)
            {
                const Mark& a = marks[markHead];
                const Mark& b = marks[(markHead + 1) % kMaxMarks];
                if (std::abs ((double) b.pos - mapped) <= std::abs ((double) a.pos - mapped))
                { markHead = (markHead + 1) % kMaxMarks; --markCount; }
                else break;
            }
            if (markCount == 0) { nextSynth += unvoicedStep; continue; }

            const Mark& mk = marks[markHead];
            const int T = std::min (mk.period, maxPeriod);
            const double r = mk.voiced ? (double) mk.ratio : 1.0;
            const double Ts = T / r;
            const float gain = mk.voiced ? (float) std::clamp (Ts / T, 0.5, 2.0) : 1.0f;

            const double s = nextSynth;
            const int64_t first = (int64_t) std::ceil (s - T);
            const int64_t last  = (int64_t) std::floor (s + T);
            const double invT = kPi / T;
            for (int64_t k = first; k <= last; ++k)
            {
                const double u = (double) k - s;
                const float w = (float) (0.5 + 0.5 * std::cos (u * invT));
                outBuf[(size_t) (k & bufMask)] += gain * w * readInterp ((double) mk.pos + u);
            }
            nextSynth += Ts;
        }
    }

    static constexpr int kMaxMarks = 2048;

    double sr = 44100;
    EngineParams params;
    PitchDetector detector;

    std::vector<float> inBuf, outBuf;
    int bufSize = 0, bufMask = 0;
    int maxPeriod = 882, minPeriod = 29, unvoicedStep = 256, latency = 2662;

    int64_t inPos = 0, lastMark = 0;
    bool lastMarkVoiced = false;
    double nextSynth = 0;
    Mark marks[kMaxMarks] {};
    int markHead = 0, markCount = 0;

    bool voiced = false;
    int period = 256;
    double ratio = 1.0, smoothedCorr = 0;
    int currentNote = -1000;
};

} // namespace vt
