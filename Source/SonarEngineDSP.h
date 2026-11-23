#pragma once

#include <JuceHeader.h>
#include "ParametricEQDSP.h"
#include <array>
#include <atomic>
#include <cmath>
#include <mutex>
#include <vector>

namespace ShantySonar
{

enum class SourceProfile
{
    Universal = 0,
    Vocals,
    Drums,
    Bass,
    Guitar,
    KeysSynth,
    Speech,
    Master
};

enum class PhaseMode
{
    ZeroLatency = 0,
    LinearPhase
};

enum class TrackPriority
{
    Foreground = 0,
    Midground,
    Background
};

enum class EngineMode
{
    Sonar = 0,
    Soothe
};

enum class SootheQuality
{
    Soft = 0,
    Hard
};

// Process-wide lock-free-friendly hub for Cross-Channel / Track Unmasking across plugin instances
class SharedSonarHub
{
public:
    static constexpr int numGroups = 4; // Group A, B, C, D
    static constexpr int numBins = 64;

    struct Slot
    {
        void* owner = nullptr;
        int group = 0; // 0 = Off, 1..4 = A..D
        int priority = 1; // 0=FG, 1=MG, 2=BG
        double lastSeenTimeMs = 0.0;
        std::array<float, numBins> spectrumDb {};
    };

    static SharedSonarHub& getInstance()
    {
        static SharedSonarHub hub;
        return hub;
    }

    void publish(void* owner, int group, int priority, const std::array<float, numBins>& specDb)
    {
        const std::lock_guard<std::mutex> lock(mtx);
        const double now = juce::Time::getMillisecondCounterHiRes();

        // Remove if group == 0
        if (group <= 0)
        {
            for (auto& s : slots)
                if (s.owner == owner)
                    s.owner = nullptr;
            return;
        }

        Slot* target = nullptr;
        for (auto& s : slots)
        {
            if (s.owner == owner)
            {
                target = &s;
                break;
            }
            if (s.owner == nullptr && target == nullptr)
                target = &s;
        }
        if (target == nullptr && !slots.empty())
            target = &slots[0];

        if (target != nullptr)
        {
            target->owner = owner;
            target->group = group;
            target->priority = priority;
            target->lastSeenTimeMs = now;
            target->spectrumDb = specDb;
        }
    }

    void unregister(void* owner)
    {
        const std::lock_guard<std::mutex> lock(mtx);
        for (auto& s : slots)
            if (s.owner == owner)
                s.owner = nullptr;
    }

    bool fetchHigherPrioritySpectrum(void* owner, int group, int myPriority, std::array<float, numBins>& outMaxDb)
    {
        outMaxDb.fill(-100.0f);
        if (group <= 0 || myPriority <= 0)
            return false;

        const std::lock_guard<std::mutex> lock(mtx);
        const double now = juce::Time::getMillisecondCounterHiRes();
        bool found = false;

        for (auto& s : slots)
        {
            if (s.owner == nullptr || s.owner == owner)
                continue;
            if ((now - s.lastSeenTimeMs) > 500.0)
            {
                s.owner = nullptr;
                continue;
            }
            if (s.group == group && s.priority < myPriority)
            {
                found = true;
                for (int b = 0; b < numBins; ++b)
                    outMaxDb[b] = juce::jmax(outMaxDb[b], s.spectrumDb[b]);
            }
        }
        return found;
    }

private:
    SharedSonarHub() = default;
    std::mutex mtx;
    std::array<Slot, 32> slots {};
};

class SonarEngineDSP
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder; // 2048
    static constexpr int hopSize = fftSize / 4;   // 512
    static constexpr int numBins = 64;
    static constexpr int numMinPhaseBands = 16;

    SonarEngineDSP() : fft(fftOrder), sidechainFft(fftOrder)
    {
        for (int i = 0; i < fftSize; ++i)
            window[i] = 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i) / static_cast<float>(fftSize)));
        resetAll();
    }

    ~SonarEngineDSP()
    {
        SharedSonarHub::getInstance().unregister(this);
    }

    void prepare(double sr, int /*maxBlockSize*/)
    {
        sampleRate = juce::jmax(8000.0, sr);
        initBinFrequencies();
        resetBuffers();
    }

    void resetBuffers()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            inRing[ch].fill(0.0f);
            outRing[ch].fill(0.0f);
            scRing[ch].fill(0.0f);
            for (int b = 0; b < numMinPhaseBands; ++b)
                minPhaseStates[ch][b].reset();
        }
        ringWritePos = 0;
        hopCounter = 0;
        preSpectrumDb.fill(-90.0f);
        postSpectrumDb.fill(-90.0f);
        maskingSpectrumDb.fill(0.0f);
        scSpectrumDb.fill(-100.0f);
        sootheEnvDb.fill(0.0f);
        sootheReductionMeter.store(0.0f, std::memory_order_relaxed);
        hasSidechainSignal = false;
    }

    void resetAll()
    {
        learnedCurveDb.fill(0.0f);
        adaptiveCurveDb.fill(0.0f);
        finalSmartCurveDb.fill(0.0f);
        sootheEnvDb.fill(0.0f);
        learnAccumDb.fill(0.0f);
        learnFrameCount = 0;
        isLearning.store(false, std::memory_order_relaxed);
        hasLearned.store(false, std::memory_order_relaxed);
        learnProgress.store(0.0f, std::memory_order_relaxed);
        sootheReductionMeter.store(0.0f, std::memory_order_relaxed);
        resetBuffers();
    }

    void startLearn()
    {
        learnAccumDb.fill(0.0f);
        learnFrameCount = 0;
        learnProgress.store(0.0f, std::memory_order_relaxed);
        isLearning.store(true, std::memory_order_release);
    }

    void stopLearn()
    {
        if (isLearning.load(std::memory_order_acquire))
        {
            finalizeLearn();
            isLearning.store(false, std::memory_order_release);
        }
    }

    void clearLearned()
    {
        isLearning.store(false, std::memory_order_relaxed);
        hasLearned.store(false, std::memory_order_relaxed);
        learnProgress.store(0.0f, std::memory_order_relaxed);
        learnedCurveDb.fill(0.0f);
        adaptiveCurveDb.fill(0.0f);
        finalSmartCurveDb.fill(0.0f);
    }

    struct Settings
    {
        EngineMode engineMode = EngineMode::Sonar;
        SourceProfile profile = SourceProfile::Universal;
        PhaseMode phaseMode = PhaseMode::ZeroLatency;
        float impact = 0.75f;     // 0..1
        float smoothing = 0.5f;   // 0..1
        float adaptive = 0.25f;   // 0..1
        float minFreq = 30.0f;    // Hz
        float maxFreq = 18000.0f; // Hz
        float unmask = 0.0f;      // 0..1
        int group = 0;            // 0=Off, 1..4
        int priority = 1;         // 0=FG, 1=MG, 2=BG

        // Soothe3-style Dynamic Resonance Suppressor
        float sootheDepth = 0.60f;       // 0..1
        float sootheSharpness = 0.50f;   // 0..1
        float sootheSelectivity = 0.45f; // 0..1
        float sootheAttackMs = 5.0f;     // ms
        float sootheReleaseMs = 80.0f;   // ms
        SootheQuality sootheQuality = SootheQuality::Soft;
    };

    void updateSettings(const Settings& s)
    {
        settings = s;
    }

    int getLatencySamples() const noexcept
    {
        return (settings.phaseMode == PhaseMode::LinearPhase) ? fftSize : 0;
    }

    void processBlock(juce::AudioBuffer<float>& mainBuffer, const juce::AudioBuffer<float>* sidechainBuffer)
    {
        const int numSamples = mainBuffer.getNumSamples();
        const int numCh = juce::jmin(2, mainBuffer.getNumChannels());
        if (numCh <= 0 || numSamples <= 0)
            return;

        // Check sidechain energy
        if (sidechainBuffer != nullptr && sidechainBuffer->getNumChannels() > 0 && sidechainBuffer->getNumSamples() >= numSamples)
        {
            float scPeak = 0.0f;
            for (int ch = 0; ch < juce::jmin(2, sidechainBuffer->getNumChannels()); ++ch)
                scPeak = juce::jmax(scPeak, sidechainBuffer->getMagnitude(ch, 0, numSamples));
            hasSidechainSignal = (scPeak > 1e-4f);
        }
        else
        {
            hasSidechainSignal = false;
        }

        // Feed STFT analysis (and synthesis if LinearPhase)
        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                const float x = mainBuffer.getSample(ch, i);
                inRing[ch][ringWritePos] = x;

                if (hasSidechainSignal)
                {
                    const int scCh = juce::jmin(ch, sidechainBuffer->getNumChannels() - 1);
                    scRing[ch][ringWritePos] = sidechainBuffer->getSample(scCh, i);
                }

                if (settings.phaseMode == PhaseMode::LinearPhase)
                {
                    const float y = outRing[ch][ringWritePos];
                    outRing[ch][ringWritePos] = 0.0f;
                    mainBuffer.setSample(ch, i, y);
                }
            }

            if (numCh == 1)
                inRing[1][ringWritePos] = inRing[0][ringWritePos];

            ringWritePos = (ringWritePos + 1) & (fftSize - 1);
            if (++hopCounter >= hopSize)
            {
                hopCounter = 0;
                processHop(numCh);
            }
        }

        // If ZeroLatency mode, apply the Smart Curve via 16-band minimum-phase biquad bank
        if (settings.phaseMode == PhaseMode::ZeroLatency)
            applyMinPhaseBank(mainBuffer, numCh, numSamples);
    }

    // UI getters (thread-safe snapshot)
    void copyDisplayData(std::array<float, numBins>& preOut,
                         std::array<float, numBins>& postOut,
                         std::array<float, numBins>& smartCurveOut,
                         std::array<float, numBins>& maskingOut) const
    {
        const std::lock_guard<std::mutex> lock(uiMutex);
        preOut = preSpectrumDb;
        postOut = postSpectrumDb;
        smartCurveOut = finalSmartCurveDb;
        maskingOut = maskingSpectrumDb;
    }

    bool getIsLearning() const noexcept { return isLearning.load(std::memory_order_relaxed); }
    bool getHasLearned() const noexcept { return hasLearned.load(std::memory_order_relaxed); }
    float getLearnProgress() const noexcept { return learnProgress.load(std::memory_order_relaxed); }
    float getSootheReductionMeter() const noexcept { return sootheReductionMeter.load(std::memory_order_relaxed); }

    double getBinFreq(int b) const noexcept { return binFreqs[juce::jlimit(0, numBins - 1, b)]; }

    float getSmartCurveAtFreq(float freqHz) const noexcept
    {
        if (freqHz <= binFreqs[0]) return finalSmartCurveDb[0];
        if (freqHz >= binFreqs[numBins - 1]) return finalSmartCurveDb[numBins - 1];
        const float logF = std::log2(freqHz);
        const float logMin = std::log2(20.0f);
        const float logMax = std::log2(20000.0f);
        const float pos = juce::jlimit(0.0f, static_cast<float>(numBins - 1),
                                       (logF - logMin) / (logMax - logMin) * static_cast<float>(numBins - 1));
        const int idx = static_cast<int>(pos);
        const int next = juce::jmin(numBins - 1, idx + 1);
        const float frac = pos - static_cast<float>(idx);
        return finalSmartCurveDb[idx] * (1.0f - frac) + finalSmartCurveDb[next] * frac;
    }

private:
    void initBinFrequencies()
    {
        for (int b = 0; b < numBins; ++b)
        {
            const float t = static_cast<float>(b) / static_cast<float>(numBins - 1);
            binFreqs[b] = 20.0f * std::pow(1000.0f, t); // 20 Hz .. 20 kHz
        }
        for (int m = 0; m < numMinPhaseBands; ++m)
        {
            const float t = static_cast<float>(m) / static_cast<float>(numMinPhaseBands - 1);
            minPhaseFreqs[m] = 35.0f * std::pow(16000.0f / 35.0f, t);
        }
    }

    float getProfileTargetDb(SourceProfile prof, float freqHz) const noexcept
    {
        // Base pink-like perceptual slope (-3.2 dB/oct relative to 1 kHz)
        const float octFrom1k = std::log2(juce::jmax(20.0f, freqHz) / 1000.0f);
        float target = -3.2f * octFrom1k;

        auto gauss = [](float f, float center, float widthOct, float amp) {
            const float d = std::log2(juce::jmax(20.0f, f) / center) / widthOct;
            return amp * std::exp(-0.5f * d * d);
        };

        switch (prof)
        {
            case SourceProfile::Universal:
                target += gauss(freqHz, 80.0f, 0.8f, 1.0f) + gauss(freqHz, 3500.0f, 1.0f, 0.8f);
                break;
            case SourceProfile::Vocals:
                if (freqHz < 85.0f) target -= 8.0f * std::log2(85.0f / juce::jmax(20.0f, freqHz));
                target += gauss(freqHz, 260.0f, 0.6f, -2.0f)
                        + gauss(freqHz, 3200.0f, 0.9f, 3.0f)
                        + gauss(freqHz, 11500.0f, 0.8f, 2.2f);
                break;
            case SourceProfile::Drums:
                target += gauss(freqHz, 65.0f, 0.6f, 2.5f)
                        + gauss(freqHz, 340.0f, 0.7f, -2.2f)
                        + gauss(freqHz, 4800.0f, 0.9f, 2.4f);
                break;
            case SourceProfile::Bass:
                if (freqHz < 32.0f) target -= 6.0f * std::log2(32.0f / juce::jmax(20.0f, freqHz));
                target += gauss(freqHz, 65.0f, 0.7f, 3.0f)
                        + gauss(freqHz, 220.0f, 0.6f, -1.5f)
                        + gauss(freqHz, 850.0f, 0.7f, 1.8f);
                if (freqHz > 4000.0f) target -= 3.5f * std::log2(freqHz / 4000.0f);
                break;
            case SourceProfile::Guitar:
                if (freqHz < 80.0f) target -= 7.0f * std::log2(80.0f / juce::jmax(20.0f, freqHz));
                target += gauss(freqHz, 180.0f, 0.6f, 1.2f)
                        + gauss(freqHz, 380.0f, 0.6f, -1.6f)
                        + gauss(freqHz, 2600.0f, 0.8f, 2.2f);
                if (freqHz > 7500.0f) target -= 4.0f * std::log2(freqHz / 7500.0f);
                break;
            case SourceProfile::KeysSynth:
                target += gauss(freqHz, 300.0f, 0.7f, -1.4f)
                        + gauss(freqHz, 5500.0f, 1.0f, 1.4f);
                break;
            case SourceProfile::Speech:
                if (freqHz < 100.0f) target -= 9.0f * std::log2(100.0f / juce::jmax(20.0f, freqHz));
                target += gauss(freqHz, 240.0f, 0.6f, -2.5f)
                        + gauss(freqHz, 2400.0f, 0.85f, 3.2f)
                        + gauss(freqHz, 7000.0f, 0.7f, -1.2f);
                break;
            case SourceProfile::Master:
                target += gauss(freqHz, 55.0f, 0.7f, 1.0f)
                        + gauss(freqHz, 280.0f, 0.7f, -0.8f)
                        + gauss(freqHz, 12000.0f, 0.9f, 1.2f);
                break;
        }
        return target;
    }

    void extractLogBands(const float* fftMag, std::array<float, numBins>& outDb) const noexcept
    {
        const float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
        for (int b = 0; b < numBins; ++b)
        {
            const float fLow = (b == 0) ? 20.0f : std::sqrt(binFreqs[b - 1] * binFreqs[b]);
            const float fHigh = (b == numBins - 1) ? 20000.0f : std::sqrt(binFreqs[b] * binFreqs[b + 1]);
            const int k0 = juce::jlimit(1, fftSize / 2 - 1, static_cast<int>(std::floor(fLow / binHz)));
            const int k1 = juce::jlimit(k0, fftSize / 2 - 1, static_cast<int>(std::ceil(fHigh / binHz)));

            double sumSq = 0.0;
            for (int k = k0; k <= k1; ++k)
                sumSq += static_cast<double>(fftMag[k]) * static_cast<double>(fftMag[k]);
            const double rms = std::sqrt(sumSq / static_cast<double>(k1 - k0 + 1));
            outDb[b] = static_cast<float>(20.0 * std::log10(juce::jmax(1e-5, rms)));
        }
    }

    void smoothCurve(std::array<float, numBins>& curve, int passes) const noexcept
    {
        std::array<float, numBins> tmp {};
        for (int p = 0; p < passes; ++p)
        {
            for (int b = 0; b < numBins; ++b)
            {
                const int b0 = juce::jmax(0, b - 1);
                const int b2 = juce::jmin(numBins - 1, b + 1);
                tmp[b] = 0.25f * curve[b0] + 0.5f * curve[b] + 0.25f * curve[b2];
            }
            curve = tmp;
        }
    }

    void finalizeLearn()
    {
        if (learnFrameCount < 4)
            return;

        std::array<float, numBins> avgSpec {};
        float meanSpec = 0.0f;
        float meanTarget = 0.0f;
        int validCount = 0;

        for (int b = 0; b < numBins; ++b)
        {
            avgSpec[b] = learnAccumDb[b] / static_cast<float>(learnFrameCount);
            if (binFreqs[b] >= 60.0f && binFreqs[b] <= 14000.0f)
            {
                meanSpec += avgSpec[b];
                meanTarget += getProfileTargetDb(settings.profile, binFreqs[b]);
                ++validCount;
            }
        }

        const float offset = (validCount > 0) ? ((meanSpec - meanTarget) / static_cast<float>(validCount)) : 0.0f;

        for (int b = 0; b < numBins; ++b)
        {
            const float target = getProfileTargetDb(settings.profile, binFreqs[b]) + offset;
            const float diff = target - avgSpec[b];
            learnedCurveDb[b] = juce::jlimit(-12.0f, 12.0f, diff * 0.72f);
        }

        smoothCurve(learnedCurveDb, 3);
        hasLearned.store(true, std::memory_order_release);
    }

    void processHop(int numCh)
    {
        // Build mono analysis FFT from ring buffer
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (ringWritePos + i) & (fftSize - 1);
            const float mono = 0.5f * (inRing[0][idx] + inRing[1][idx]);
            fftData[i] = mono * window[i];
        }
        std::fill(fftData.begin() + fftSize, fftData.end(), 0.0f);
        fft.performFrequencyOnlyForwardTransform(fftData.data());

        // Normalize FFT magnitude
        const float norm = 1.0f / (static_cast<float>(fftSize) * 0.25f);
        for (int k = 0; k <= fftSize / 2; ++k)
            fftMag[k] = fftData[k] * norm;

        std::array<float, numBins> instantDb {};
        extractLogBands(fftMag.data(), instantDb);

        // Learn accumulation
        if (isLearning.load(std::memory_order_relaxed))
        {
            constexpr int targetFrames = 140; // ~1.6s at 44.1kHz
            for (int b = 0; b < numBins; ++b)
                learnAccumDb[b] += instantDb[b];
            ++learnFrameCount;
            learnProgress.store(juce::jlimit(0.0f, 1.0f, static_cast<float>(learnFrameCount) / static_cast<float>(targetFrames)),
                                std::memory_order_relaxed);
            if (learnFrameCount >= targetFrames)
            {
                finalizeLearn();
                isLearning.store(false, std::memory_order_release);
            }
        }

        // Publish to SharedSonarHub for cross-channel unmasking
        SharedSonarHub::getInstance().publish(this, settings.group, settings.priority, instantDb);

        // Sidechain FFT if active
        if (hasSidechainSignal)
        {
            for (int i = 0; i < fftSize; ++i)
            {
                const int idx = (ringWritePos + i) & (fftSize - 1);
                const float scMono = 0.5f * (scRing[0][idx] + scRing[1][idx]);
                scFftData[i] = scMono * window[i];
            }
            std::fill(scFftData.begin() + fftSize, scFftData.end(), 0.0f);
            sidechainFft.performFrequencyOnlyForwardTransform(scFftData.data());
            for (int k = 0; k <= fftSize / 2; ++k)
                scFftMag[k] = scFftData[k] * norm;
            extractLogBands(scFftMag.data(), scSpectrumDb);
        }

        // Fetch higher-priority hub spectrum
        std::array<float, numBins> hubMaskDb {};
        const bool hasHubMask = SharedSonarHub::getInstance().fetchHigherPrioritySpectrum(
            this, settings.group, settings.priority, hubMaskDb);

        // Compute Adaptive + Unmasking + Final Smart Curve
        float meanInstant = 0.0f;
        float meanTarget = 0.0f;
        int cnt = 0;
        for (int b = 0; b < numBins; ++b)
        {
            if (binFreqs[b] >= 80.0f && binFreqs[b] <= 12000.0f)
            {
                meanInstant += instantDb[b];
                meanTarget += getProfileTargetDb(settings.profile, binFreqs[b]);
                ++cnt;
            }
        }
        const float dynOffset = (cnt > 0) ? ((meanInstant - meanTarget) / static_cast<float>(cnt)) : 0.0f;

        std::array<float, numBins> workingSmart {};
        std::array<float, numBins> localMasking {};
        localMasking.fill(0.0f);

        if (settings.engineMode == EngineMode::Soothe)
        {
            // oeksound soothe2/3-style Dynamic Spectral Resonance Suppressor
            const int baseRadius = juce::jlimit(3, 14, static_cast<int>(std::round(14.0f - settings.sootheSharpness * 10.0f)));
            const float threshDb = -1.5f + settings.sootheSelectivity * 9.5f;
            const float hopSec = static_cast<float>(hopSize) / static_cast<float>(sampleRate);
            const float atkCoeff = std::exp(-hopSec / juce::jmax(0.0005f, settings.sootheAttackMs * 0.001f));
            const float relCoeff = std::exp(-hopSec / juce::jmax(0.005f, settings.sootheReleaseMs * 0.001f));

            float sumCut = 0.0f;
            for (int b = 0; b < numBins; ++b)
            {
                // Local smoothed spectral baseline around band b
                float sumW = 0.0f;
                float sumVal = 0.0f;
                const int bStart = juce::jmax(0, b - baseRadius);
                const int bEnd   = juce::jmin(numBins - 1, b + baseRadius);
                for (int j = bStart; j <= bEnd; ++j)
                {
                    const float dist = static_cast<float>(std::abs(j - b)) / static_cast<float>(baseRadius + 1);
                    const float w = 1.0f - dist;
                    sumVal += instantDb[j] * w;
                    sumW += w;
                }
                const float localBaseline = (sumW > 0.0f) ? (sumVal / sumW) : instantDb[b];
                const float prominence = instantDb[b] - localBaseline;
                const float excess = juce::jmax(0.0f, prominence - threshDb);

                float targetCutDb = 0.0f;
                if (excess > 0.0f && instantDb[b] > -75.0f)
                {
                    if (settings.sootheQuality == SootheQuality::Soft)
                        targetCutDb = -settings.sootheDepth * 18.0f * (excess / (excess + 5.5f));
                    else
                        targetCutDb = -settings.sootheDepth * juce::jmin(18.0f, excess * 1.65f);
                }

                float rangeWeight = 1.0f;
                if (binFreqs[b] < settings.minFreq)
                    rangeWeight = juce::jlimit(0.0f, 1.0f, 1.0f - std::log2(settings.minFreq / binFreqs[b]) * 1.5f);
                else if (binFreqs[b] > settings.maxFreq)
                    rangeWeight = juce::jlimit(0.0f, 1.0f, 1.0f - std::log2(binFreqs[b] / settings.maxFreq) * 1.5f);

                targetCutDb *= rangeWeight;

                // Envelope smoothing (more negative = deeper cut -> attack; recovering toward 0 -> release)
                const float c = (targetCutDb < sootheEnvDb[b]) ? atkCoeff : relCoeff;
                sootheEnvDb[b] = c * sootheEnvDb[b] + (1.0f - c) * targetCutDb;
                workingSmart[b] = juce::jlimit(-18.0f, 0.0f, sootheEnvDb[b]);
                sumCut += -workingSmart[b];
            }

            // Sharpness also controls curve smoothing (sharper = fewer smoothing passes)
            const int sootheSmooth = juce::jlimit(0, 3, static_cast<int>(std::round((1.0f - settings.sootheSharpness) * 3.0f)));
            if (sootheSmooth > 0)
                smoothCurve(workingSmart, sootheSmooth);

            sootheReductionMeter.store(juce::jlimit(0.0f, 1.0f, (sumCut / static_cast<float>(numBins)) * 0.25f),
                                       std::memory_order_relaxed);
        }
        else
        {
            sootheReductionMeter.store(0.0f, std::memory_order_relaxed);

            for (int b = 0; b < numBins; ++b)
            {
                const float target = getProfileTargetDb(settings.profile, binFreqs[b]) + dynOffset;
                const float dynDiff = juce::jlimit(-10.0f, 10.0f, (target - instantDb[b]) * 0.55f);
                adaptiveCurveDb[b] = 0.90f * adaptiveCurveDb[b] + 0.10f * dynDiff;

                float base = hasLearned.load(std::memory_order_relaxed) ? learnedCurveDb[b] : 0.0f;
                float combined = base + settings.adaptive * adaptiveCurveDb[b];

                // Range mask (smooth 1/3-oct rolloff outside minFreq..maxFreq)
                float rangeWeight = 1.0f;
                if (binFreqs[b] < settings.minFreq)
                    rangeWeight = juce::jlimit(0.0f, 1.0f, 1.0f - std::log2(settings.minFreq / binFreqs[b]) * 1.5f);
                else if (binFreqs[b] > settings.maxFreq)
                    rangeWeight = juce::jlimit(0.0f, 1.0f, 1.0f - std::log2(binFreqs[b] / settings.maxFreq) * 1.5f);

                combined *= settings.impact * rangeWeight;

                // Unmasking dip
                float maskEnergy = -100.0f;
                if (hasHubMask)
                    maskEnergy = juce::jmax(maskEnergy, hubMaskDb[b]);
                if (hasSidechainSignal)
                    maskEnergy = juce::jmax(maskEnergy, scSpectrumDb[b]);

                if (maskEnergy > -70.0f && settings.unmask > 0.001f)
                {
                    const float collision = juce::jlimit(0.0f, 12.0f, (maskEnergy - instantDb[b] + 10.0f) * 0.35f);
                    localMasking[b] = collision * settings.unmask;
                    combined -= localMasking[b] * rangeWeight;
                }

                workingSmart[b] = juce::jlimit(-15.0f, 15.0f, combined);
            }

            const int smoothPasses = static_cast<int>(std::round(settings.smoothing * 6.0f));
            if (smoothPasses > 0)
                smoothCurve(workingSmart, smoothPasses);
        }

        // Linear Phase STFT filtering if active
        if (settings.phaseMode == PhaseMode::LinearPhase)
        {
            const float binHz = static_cast<float>(sampleRate) / static_cast<float>(fftSize);
            const float synthScale = 1.0f / 1.5f; // exact Hann 75% overlap reconstruction

            for (int ch = 0; ch < numCh; ++ch)
            {
                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = (ringWritePos + i) & (fftSize - 1);
                    stftWorkspace[i] = inRing[ch][idx] * window[i];
                }
                std::fill(stftWorkspace.begin() + fftSize, stftWorkspace.end(), 0.0f);
                fft.performRealOnlyForwardTransform(stftWorkspace.data());

                // Apply smart curve magnitude to complex bins
                for (int k = 0; k <= fftSize / 2; ++k)
                {
                    const float freq = juce::jmax(20.0f, static_cast<float>(k) * binHz);
                    const float gainDb = interpCurve(workingSmart, freq);
                    const float gainLin = std::pow(10.0f, gainDb / 20.0f);
                    stftWorkspace[2 * k]     *= gainLin;
                    stftWorkspace[2 * k + 1] *= gainLin;
                }

                fft.performRealOnlyInverseTransform(stftWorkspace.data());

                for (int i = 0; i < fftSize; ++i)
                {
                    const int idx = (ringWritePos + i) & (fftSize - 1);
                    outRing[ch][idx] += stftWorkspace[i] * window[i] * synthScale;
                }
            }
        }

        // Update UI snapshot
        {
            const std::lock_guard<std::mutex> lock(uiMutex);
            finalSmartCurveDb = workingSmart;
            maskingSpectrumDb = localMasking;
            for (int b = 0; b < numBins; ++b)
            {
                preSpectrumDb[b] = 0.65f * preSpectrumDb[b] + 0.35f * instantDb[b];
                postSpectrumDb[b] = 0.65f * postSpectrumDb[b] + 0.35f * (instantDb[b] + workingSmart[b]);
            }
        }
    }

    float interpCurve(const std::array<float, numBins>& curve, float freqHz) const noexcept
    {
        if (freqHz <= binFreqs[0]) return curve[0];
        if (freqHz >= binFreqs[numBins - 1]) return curve[numBins - 1];
        const float logF = std::log2(freqHz);
        const float logMin = std::log2(20.0f);
        const float logMax = std::log2(20000.0f);
        const float pos = juce::jlimit(0.0f, static_cast<float>(numBins - 1),
                                       (logF - logMin) / (logMax - logMin) * static_cast<float>(numBins - 1));
        const int idx = static_cast<int>(pos);
        const int next = juce::jmin(numBins - 1, idx + 1);
        const float frac = pos - static_cast<float>(idx);
        return curve[idx] * (1.0f - frac) + curve[next] * frac;
    }

    void applyMinPhaseBank(juce::AudioBuffer<float>& buffer, int numCh, int numSamples)
    {
        std::array<float, numBins> curveCopy {};
        {
            const std::lock_guard<std::mutex> lock(uiMutex);
            curveCopy = finalSmartCurveDb;
        }

        // Check if curve is effectively flat to save CPU
        float maxAbs = 0.0f;
        for (float v : curveCopy)
            maxAbs = juce::jmax(maxAbs, std::abs(v));
        if (maxAbs < 0.05f)
            return;

        std::array<BiquadCoeffs, numMinPhaseBands> coeffs {};
        for (int m = 0; m < numMinPhaseBands; ++m)
        {
            const float gDb = interpCurve(curveCopy, minPhaseFreqs[m]) * 0.52f; // overlap compensation
            if (m == 0)
                coeffs[m] = BiquadCoeffs::makeLowShelf(sampleRate, minPhaseFreqs[m], 0.7, gDb);
            else if (m == numMinPhaseBands - 1)
                coeffs[m] = BiquadCoeffs::makeHighShelf(sampleRate, minPhaseFreqs[m], 0.7, gDb);
            else
                coeffs[m] = BiquadCoeffs::makeBell(sampleRate, minPhaseFreqs[m], 2.2, gDb);
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                for (int m = 0; m < numMinPhaseBands; ++m)
                    x = minPhaseStates[ch][m].process(x, coeffs[m]);
                data[i] = x;
            }
        }
    }

    double sampleRate = 44100.0;
    Settings settings;

    juce::dsp::FFT fft;
    juce::dsp::FFT sidechainFft;
    std::array<float, fftSize> window {};
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize * 2> scFftData {};
    std::array<float, fftSize * 2> stftWorkspace {};
    std::array<float, fftSize / 2 + 1> fftMag {};
    std::array<float, fftSize / 2 + 1> scFftMag {};

    std::array<std::array<float, fftSize>, 2> inRing {};
    std::array<std::array<float, fftSize>, 2> outRing {};
    std::array<std::array<float, fftSize>, 2> scRing {};
    int ringWritePos = 0;
    int hopCounter = 0;
    bool hasSidechainSignal = false;

    std::array<float, numBins> binFreqs {};
    std::array<float, numMinPhaseBands> minPhaseFreqs {};
    std::array<std::array<BiquadState, numMinPhaseBands>, 2> minPhaseStates {};

    std::array<float, numBins> learnAccumDb {};
    int learnFrameCount = 0;
    std::atomic<bool> isLearning { false };
    std::atomic<bool> hasLearned { false };
    std::atomic<float> learnProgress { 0.0f };

    std::array<float, numBins> learnedCurveDb {};
    std::array<float, numBins> adaptiveCurveDb {};
    std::array<float, numBins> scSpectrumDb {};
    std::array<float, numBins> sootheEnvDb {};
    std::atomic<float> sootheReductionMeter { 0.0f };

    mutable std::mutex uiMutex;
    std::array<float, numBins> preSpectrumDb {};
    std::array<float, numBins> postSpectrumDb {};
    std::array<float, numBins> finalSmartCurveDb {};
    std::array<float, numBins> maskingSpectrumDb {};
};

} // namespace ShantySonar
