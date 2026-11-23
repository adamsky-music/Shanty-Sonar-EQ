#pragma once

#include <JuceHeader.h>
#include "SonarEngineDSP.h"
#include "ParametricEQDSP.h"
#include <array>
#include <atomic>

class ShantySonarEQProcessor : public juce::AudioProcessor
{
public:
    ShantySonarEQProcessor();
    ~ShantySonarEQProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.05; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // Smart Sonar controls
    void startLearn() { sonarEngine.startLearn(); }
    void stopLearn() { sonarEngine.stopLearn(); }
    void clearLearned() { sonarEngine.clearLearned(); }
    bool getIsLearning() const noexcept { return sonarEngine.getIsLearning(); }
    bool getHasLearned() const noexcept { return sonarEngine.getHasLearned(); }
    float getLearnProgress() const noexcept { return sonarEngine.getLearnProgress(); }

    void copyDisplayData(std::array<float, ShantySonar::SonarEngineDSP::numBins>& preOut,
                         std::array<float, ShantySonar::SonarEngineDSP::numBins>& postOut,
                         std::array<float, ShantySonar::SonarEngineDSP::numBins>& smartOut,
                         std::array<float, ShantySonar::SonarEngineDSP::numBins>& maskOut) const
    {
        sonarEngine.copyDisplayData(preOut, postOut, smartOut, maskOut);
    }

    ShantySonar::BandParams getBandParams(int bandIndex) const;
    float getBandLiveDynGain(int bandIndex) const noexcept;
    double getCurrentSampleRate() const noexcept { return currentSampleRate; }

    void setSoloBand(int bandIndex) noexcept { soloBandIndex.store(bandIndex, std::memory_order_relaxed); }
    int getSoloBand() const noexcept { return soloBandIndex.load(std::memory_order_relaxed); }

    // A/B state comparison
    void switchAB(int targetSlot);
    int getCurrentABSlot() const noexcept { return currentABSlot; }

    std::atomic<float> inputLevelMeter { 0.0f };
    std::atomic<float> outputLevelMeter { 0.0f };
    std::atomic<float> sonarActivityMeter { 0.0f };
    std::atomic<float> unmaskActivityMeter { 0.0f };
    std::atomic<float> sootheReductionMeter { 0.0f };

    static juce::String bandParamID(int bandIndex, const char* suffix)
    {
        return "b" + juce::String(bandIndex + 1) + "_" + suffix;
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    ShantySonar::SonarEngineDSP sonarEngine;
    std::array<ShantySonar::ParametricBandProcessor, ShantySonar::numBands> eqBands;
    std::array<ShantySonar::BiquadState, 2> soloFilterState;

    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWritePos = 0;
    double currentSampleRate = 44100.0;
    float autoGainEnvIn = 0.0f;
    float autoGainEnvOut = 0.0f;
    float autoGainLinear = 1.0f;

    std::atomic<int> soloBandIndex { -1 };

    juce::ValueTree abSlots[2];
    int currentABSlot = 0;

    struct BandParamPtrs
    {
        std::atomic<float>* enabled = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* chMode = nullptr;
        std::atomic<float>* dynOn = nullptr;
        std::atomic<float>* dynThresh = nullptr;
        std::atomic<float>* dynRange = nullptr;
        std::atomic<float>* dynAtk = nullptr;
        std::atomic<float>* dynRel = nullptr;
    };

    std::array<BandParamPtrs, ShantySonar::numBands> bandPtrs;

    std::atomic<float>* engineModePtr = nullptr;
    std::atomic<float>* profilePtr = nullptr;
    std::atomic<float>* phaseModePtr = nullptr;
    std::atomic<float>* impactPtr = nullptr;
    std::atomic<float>* smoothPtr = nullptr;
    std::atomic<float>* adaptivePtr = nullptr;
    std::atomic<float>* minFreqPtr = nullptr;
    std::atomic<float>* maxFreqPtr = nullptr;
    std::atomic<float>* unmaskPtr = nullptr;
    std::atomic<float>* groupPtr = nullptr;
    std::atomic<float>* priorityPtr = nullptr;
    std::atomic<float>* sootheDepthPtr = nullptr;
    std::atomic<float>* sootheSharpPtr = nullptr;
    std::atomic<float>* sootheSelectPtr = nullptr;
    std::atomic<float>* sootheAtkPtr = nullptr;
    std::atomic<float>* sootheRelPtr = nullptr;
    std::atomic<float>* sootheQualPtr = nullptr;
    std::atomic<float>* sootheDeltaPtr = nullptr;
    std::atomic<float>* autoGainPtr = nullptr;
    std::atomic<float>* mixPtr = nullptr;
    std::atomic<float>* outputGainPtr = nullptr;
    std::atomic<float>* bypassPtr = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShantySonarEQProcessor)
};
