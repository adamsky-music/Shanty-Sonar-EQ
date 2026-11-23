#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>

namespace ShantySonar
{

inline constexpr int numBands = 12;

enum class FilterType
{
    Bell = 0,
    LowShelf,
    HighShelf,
    LowCut,
    HighCut,
    Notch,
    Bandpass,
    TiltShelf
};

enum class ChannelMode
{
    Stereo = 0,
    Mid,
    Side,
    Left,
    Right
};

struct BandParams
{
    bool enabled = false;
    FilterType type = FilterType::Bell;
    int slopeStages = 2; // 1=12dB, 2=24dB, 3=36dB, 4=48dB (for LowCut/HighCut)
    float freq = 1000.0f;
    float gainDb = 0.0f;
    float q = 1.0f;
    ChannelMode chMode = ChannelMode::Stereo;

    // Dynamic EQ
    bool dynEnabled = false;
    float dynThresholdDb = -24.0f;
    float dynRangeDb = -6.0f; // -24 to +24 dB
    float dynAttackMs = 15.0f;
    float dynReleaseMs = 120.0f;
};

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a1 = 0.0, a2 = 0.0;

    static BiquadCoeffs makeIdentity() noexcept { return {}; }

    static BiquadCoeffs makeFromRaw(double b0, double b1, double b2, double a0, double a1, double a2) noexcept
    {
        BiquadCoeffs c;
        const double invA0 = (std::abs(a0) > 1e-12) ? (1.0 / a0) : 1.0;
        c.b0 = b0 * invA0;
        c.b1 = b1 * invA0;
        c.b2 = b2 * invA0;
        c.a1 = a1 * invA0;
        c.a2 = a2 * invA0;
        return c;
    }

    static BiquadCoeffs makeBell(double sr, double freq, double q, double gainDb) noexcept
    {
        if (std::abs(gainDb) < 0.01)
            return makeIdentity();
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * juce::jmax(0.05, q));
        return makeFromRaw(1.0 + alpha * A, -2.0 * cosW0, 1.0 - alpha * A,
                           1.0 + alpha / A, -2.0 * cosW0, 1.0 - alpha / A);
    }

    static BiquadCoeffs makeLowShelf(double sr, double freq, double q, double gainDb) noexcept
    {
        if (std::abs(gainDb) < 0.01)
            return makeIdentity();
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double slope = juce::jlimit(0.15, 2.0, q);
        const double alpha = (sinW0 * 0.5) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        return makeFromRaw(A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha),
                           2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0),
                           A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha),
                           (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha,
                           -2.0 * ((A - 1.0) + (A + 1.0) * cosW0),
                           (A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha);
    }

    static BiquadCoeffs makeHighShelf(double sr, double freq, double q, double gainDb) noexcept
    {
        if (std::abs(gainDb) < 0.01)
            return makeIdentity();
        const double A = std::pow(10.0, gainDb / 40.0);
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double slope = juce::jlimit(0.15, 2.0, q);
        const double alpha = (sinW0 * 0.5) * std::sqrt((A + 1.0 / A) * (1.0 / slope - 1.0) + 2.0);
        const double twoSqrtAAlpha = 2.0 * std::sqrt(A) * alpha;

        return makeFromRaw(A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha),
                           -2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0),
                           A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha),
                           (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha,
                           2.0 * ((A - 1.0) - (A + 1.0) * cosW0),
                           (A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha);
    }

    static BiquadCoeffs makeLowCut(double sr, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * juce::jmax(0.2, q));
        return makeFromRaw((1.0 + cosW0) * 0.5, -(1.0 + cosW0), (1.0 + cosW0) * 0.5,
                           1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
    }

    static BiquadCoeffs makeHighCut(double sr, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * juce::jmax(0.2, q));
        return makeFromRaw((1.0 - cosW0) * 0.5, 1.0 - cosW0, (1.0 - cosW0) * 0.5,
                           1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
    }

    static BiquadCoeffs makeNotch(double sr, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * juce::jmax(0.2, q));
        return makeFromRaw(1.0, -2.0 * cosW0, 1.0,
                           1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
    }

    static BiquadCoeffs makeBandpass(double sr, double freq, double q) noexcept
    {
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.48, freq) / sr;
        const double cosW0 = std::cos(w0);
        const double sinW0 = std::sin(w0);
        const double alpha = sinW0 / (2.0 * juce::jmax(0.2, q));
        return makeFromRaw(alpha, 0.0, -alpha,
                           1.0 + alpha, -2.0 * cosW0, 1.0 - alpha);
    }

    double getMagnitudeSquared(double w0) const noexcept
    {
        const std::complex<double> z1 = std::polar(1.0, -w0);
        const std::complex<double> z2 = z1 * z1;
        const std::complex<double> num = b0 + b1 * z1 + b2 * z2;
        const std::complex<double> den = 1.0 + a1 * z1 + a2 * z2;
        const double d = std::norm(den);
        return (d > 1e-24) ? (std::norm(num) / d) : 1.0;
    }
};

struct BiquadState
{
    double z1 = 0.0, z2 = 0.0;

    void reset() noexcept { z1 = 0.0; z2 = 0.0; }

    inline float process(float x, const BiquadCoeffs& c) noexcept
    {
        const double xd = static_cast<double>(x);
        const double y = c.b0 * xd + z1;
        z1 = c.b1 * xd - c.a1 * y + z2;
        z2 = c.b2 * xd - c.a2 * y;
        return static_cast<float>(y);
    }
};

class ParametricBandProcessor
{
public:
    static constexpr int maxStages = 4; // up to 48 dB/oct for cuts, or 2 stages for TiltShelf

    void prepare(double sr) noexcept
    {
        sampleRate = juce::jmax(8000.0, sr);
        reset();
    }

    void reset() noexcept
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            for (int s = 0; s < maxStages; ++s)
                stages[ch][s].reset();
            detFilter[ch].reset();
        }
        envState = 0.0f;
        currentDynGainDb.store(0.0f, std::memory_order_relaxed);
    }

    static int computeNumStages(const BandParams& p) noexcept
    {
        if (p.type == FilterType::LowCut || p.type == FilterType::HighCut)
            return juce::jlimit(1, maxStages, p.slopeStages);
        if (p.type == FilterType::TiltShelf)
            return 2;
        return 1;
    }

    static std::array<BiquadCoeffs, maxStages> computeCoeffs(const BandParams& p, double sr, float effectiveGainDb) noexcept
    {
        std::array<BiquadCoeffs, maxStages> out;
        for (auto& c : out) c = BiquadCoeffs::makeIdentity();

        if (!p.enabled)
            return out;

        switch (p.type)
        {
            case FilterType::Bell:
                out[0] = BiquadCoeffs::makeBell(sr, p.freq, p.q, effectiveGainDb);
                break;
            case FilterType::LowShelf:
                out[0] = BiquadCoeffs::makeLowShelf(sr, p.freq, p.q, effectiveGainDb);
                break;
            case FilterType::HighShelf:
                out[0] = BiquadCoeffs::makeHighShelf(sr, p.freq, p.q, effectiveGainDb);
                break;
            case FilterType::LowCut:
            {
                const int n = juce::jlimit(1, maxStages, p.slopeStages);
                for (int i = 0; i < n; ++i)
                    out[i] = BiquadCoeffs::makeLowCut(sr, p.freq, (i == 0) ? juce::jmax(0.5f, p.q * 0.707f) : 0.7071f);
                break;
            }
            case FilterType::HighCut:
            {
                const int n = juce::jlimit(1, maxStages, p.slopeStages);
                for (int i = 0; i < n; ++i)
                    out[i] = BiquadCoeffs::makeHighCut(sr, p.freq, (i == 0) ? juce::jmax(0.5f, p.q * 0.707f) : 0.7071f);
                break;
            }
            case FilterType::Notch:
                out[0] = BiquadCoeffs::makeNotch(sr, p.freq, p.q);
                break;
            case FilterType::Bandpass:
                out[0] = BiquadCoeffs::makeBandpass(sr, p.freq, p.q);
                break;
            case FilterType::TiltShelf:
                out[0] = BiquadCoeffs::makeLowShelf(sr, p.freq, 0.5, -effectiveGainDb * 0.5);
                out[1] = BiquadCoeffs::makeHighShelf(sr, p.freq, 0.5, effectiveGainDb * 0.5);
                break;
        }
        return out;
    }

    static double getMagnitudeDB(const BandParams& p, double freqHz, double sr) noexcept
    {
        if (!p.enabled)
            return 0.0;
        const auto coeffs = computeCoeffs(p, sr, p.gainDb);
        const int n = computeNumStages(p);
        const double w0 = juce::MathConstants<double>::twoPi * juce::jlimit(10.0, sr * 0.49, freqHz) / sr;
        double mag2 = 1.0;
        for (int i = 0; i < n; ++i)
            mag2 *= coeffs[i].getMagnitudeSquared(w0);
        return 10.0 * std::log10(juce::jmax(1e-12, mag2));
    }

    void processBlock(float* left, float* right, int numSamples, const BandParams& p) noexcept
    {
        if (!p.enabled)
        {
            currentDynGainDb.store(0.0f, std::memory_order_relaxed);
            return;
        }

        const int numStages = computeNumStages(p);
        float dynOffsetDb = 0.0f;

        if (p.dynEnabled && std::abs(p.dynRangeDb) > 0.05f &&
            p.type != FilterType::LowCut && p.type != FilterType::HighCut &&
            p.type != FilterType::Notch && p.type != FilterType::Bandpass)
        {
            const auto detCoeffs = BiquadCoeffs::makeBandpass(sampleRate, p.freq, juce::jlimit(0.6f, 4.0f, p.q));
            const float atkCoeff = std::exp(-1.0f / (0.001f * juce::jmax(0.5f, p.dynAttackMs) * static_cast<float>(sampleRate)));
            const float relCoeff = std::exp(-1.0f / (0.001f * juce::jmax(5.0f, p.dynReleaseMs) * static_cast<float>(sampleRate)));

            for (int i = 0; i < numSamples; ++i)
            {
                float detIn = 0.0f;
                const float l = left[i];
                const float r = right ? right[i] : l;

                switch (p.chMode)
                {
                    case ChannelMode::Stereo: detIn = 0.5f * (l + r); break;
                    case ChannelMode::Mid:    detIn = 0.7071f * (l + r); break;
                    case ChannelMode::Side:   detIn = 0.7071f * (l - r); break;
                    case ChannelMode::Left:   detIn = l; break;
                    case ChannelMode::Right:  detIn = r; break;
                }

                const float bp = detFilter[0].process(detIn, detCoeffs);
                const float rect = std::abs(bp);
                const float coeff = (rect > envState) ? atkCoeff : relCoeff;
                envState = coeff * envState + (1.0f - coeff) * rect;
            }

            const float envDb = 20.0f * std::log10(juce::jmax(1e-6f, envState));
            const float overDb = envDb - p.dynThresholdDb;
            const float ratio = juce::jlimit(0.0f, 1.0f, (overDb + 3.0f) / 15.0f);
            dynOffsetDb = p.dynRangeDb * ratio;
        }

        currentDynGainDb.store(dynOffsetDb, std::memory_order_relaxed);
        const float effectiveGain = juce::jlimit(-24.0f, 24.0f, p.gainDb + dynOffsetDb);
        const auto coeffs = computeCoeffs(p, sampleRate, effectiveGain);

        if (right == nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float x = left[i];
                for (int s = 0; s < numStages; ++s)
                    x = stages[0][s].process(x, coeffs[s]);
                left[i] = x;
            }
            return;
        }

        switch (p.chMode)
        {
            case ChannelMode::Stereo:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float l = left[i];
                    float r = right[i];
                    for (int s = 0; s < numStages; ++s)
                    {
                        l = stages[0][s].process(l, coeffs[s]);
                        r = stages[1][s].process(r, coeffs[s]);
                    }
                    left[i] = l;
                    right[i] = r;
                }
                break;
            }
            case ChannelMode::Mid:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float m = 0.5f * (left[i] + right[i]);
                    const float side = 0.5f * (left[i] - right[i]);
                    for (int s = 0; s < numStages; ++s)
                        m = stages[0][s].process(m, coeffs[s]);
                    left[i] = m + side;
                    right[i] = m - side;
                }
                break;
            }
            case ChannelMode::Side:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    const float m = 0.5f * (left[i] + right[i]);
                    float side = 0.5f * (left[i] - right[i]);
                    for (int s = 0; s < numStages; ++s)
                        side = stages[1][s].process(side, coeffs[s]);
                    left[i] = m + side;
                    right[i] = m - side;
                }
                break;
            }
            case ChannelMode::Left:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float l = left[i];
                    for (int s = 0; s < numStages; ++s)
                        l = stages[0][s].process(l, coeffs[s]);
                    left[i] = l;
                }
                break;
            }
            case ChannelMode::Right:
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    float r = right[i];
                    for (int s = 0; s < numStages; ++s)
                        r = stages[1][s].process(r, coeffs[s]);
                    right[i] = r;
                }
                break;
            }
        }
    }

    float getLiveDynGainDb() const noexcept { return currentDynGainDb.load(std::memory_order_relaxed); }

private:
    double sampleRate = 44100.0;
    std::array<std::array<BiquadState, maxStages>, 2> stages;
    std::array<BiquadState, 2> detFilter;
    float envState = 0.0f;
    std::atomic<float> currentDynGainDb { 0.0f };
};

} // namespace ShantySonar
