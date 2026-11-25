#include "PluginProcessor.h"
#include "PluginEditor.h"

ShantySonarEQProcessor::ShantySonarEQProcessor()
    : AudioProcessor(BusesProperties()
                     .withInput("Input", juce::AudioChannelSet::stereo(), true)
                     .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                     .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts(*this, &undoManager, "PARAMETERS", createParameterLayout())
{
    engineModePtr   = apvts.getRawParameterValue("engine_mode");
    profilePtr      = apvts.getRawParameterValue("profile");
    phaseModePtr    = apvts.getRawParameterValue("phase_mode");
    impactPtr       = apvts.getRawParameterValue("sonar_impact");
    smoothPtr       = apvts.getRawParameterValue("sonar_smooth");
    adaptivePtr     = apvts.getRawParameterValue("sonar_adaptive");
    minFreqPtr      = apvts.getRawParameterValue("sonar_min_freq");
    maxFreqPtr      = apvts.getRawParameterValue("sonar_max_freq");
    unmaskPtr       = apvts.getRawParameterValue("unmask_amt");
    groupPtr        = apvts.getRawParameterValue("unmask_group");
    priorityPtr     = apvts.getRawParameterValue("unmask_priority");
    sootheDepthPtr  = apvts.getRawParameterValue("soothe_depth");
    sootheSharpPtr  = apvts.getRawParameterValue("soothe_sharpness");
    sootheSelectPtr = apvts.getRawParameterValue("soothe_selectivity");
    sootheAtkPtr    = apvts.getRawParameterValue("soothe_attack");
    sootheRelPtr    = apvts.getRawParameterValue("soothe_release");
    sootheMsBalPtr  = apvts.getRawParameterValue("soothe_ms_balance");
    sootheQualPtr   = apvts.getRawParameterValue("soothe_quality");
    sootheDeltaPtr  = apvts.getRawParameterValue("soothe_delta");
    autoGainPtr     = apvts.getRawParameterValue("auto_gain");
    mixPtr          = apvts.getRawParameterValue("mix");
    outputGainPtr   = apvts.getRawParameterValue("output_gain");
    bypassPtr       = apvts.getRawParameterValue("bypass");

    for (int i = 0; i < ShantySonar::numBands; ++i)
    {
        auto& p = bandPtrs[i];
        p.enabled   = apvts.getRawParameterValue(bandParamID(i, "enabled"));
        p.type      = apvts.getRawParameterValue(bandParamID(i, "type"));
        p.slope     = apvts.getRawParameterValue(bandParamID(i, "slope"));
        p.freq      = apvts.getRawParameterValue(bandParamID(i, "freq"));
        p.gain      = apvts.getRawParameterValue(bandParamID(i, "gain"));
        p.q         = apvts.getRawParameterValue(bandParamID(i, "q"));
        p.chMode    = apvts.getRawParameterValue(bandParamID(i, "ch_mode"));
        p.dynOn     = apvts.getRawParameterValue(bandParamID(i, "dyn_on"));
        p.dynThresh = apvts.getRawParameterValue(bandParamID(i, "dyn_thresh"));
        p.dynRange  = apvts.getRawParameterValue(bandParamID(i, "dyn_range"));
        p.dynAtk    = apvts.getRawParameterValue(bandParamID(i, "dyn_atk"));
        p.dynRel    = apvts.getRawParameterValue(bandParamID(i, "dyn_rel"));
    }

    abSlots[0] = apvts.copyState();
    abSlots[1] = apvts.copyState();
}

juce::AudioProcessorValueTreeState::ParameterLayout ShantySonarEQProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "engine_mode", "Engine Mode",
        juce::StringArray { "Sonar (smart:EQ)", "Smooth (Resonance)" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "profile", "Source Profile",
        juce::StringArray { "Universal", "Vocals", "Drums", "Bass", "Guitar", "Keys / Synth", "Speech", "Master" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "phase_mode", "Phase Mode",
        juce::StringArray { "Zero-Latency", "Linear Phase" }, 0));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sonar_impact", "Impact", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 75.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sonar_smooth", "Smoothing", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sonar_adaptive", "Adaptive", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 25.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sonar_min_freq", "Sonar Min Freq",
        juce::NormalisableRange<float>(20.0f, 2000.0f, 1.0f, 0.35f), 30.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "sonar_max_freq", "Sonar Max Freq",
        juce::NormalisableRange<float>(2000.0f, 20000.0f, 1.0f, 0.35f), 18000.0f));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "unmask_amt", "Unmask", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "unmask_group", "Unmask Group",
        juce::StringArray { "Off", "Group A", "Group B", "Group C", "Group D" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "unmask_priority", "Track Priority",
        juce::StringArray { "Foreground", "Midground", "Background" }, 1));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_depth", "Smooth Depth", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 60.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_sharpness", "Smooth Sharpness", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 50.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_selectivity", "Smooth Selectivity", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 45.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_attack", "Smooth Attack", juce::NormalisableRange<float>(0.5f, 50.0f, 0.5f, 0.45f), 5.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_release", "Smooth Release", juce::NormalisableRange<float>(10.0f, 500.0f, 1.0f, 0.45f), 80.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "soothe_ms_balance", "Smooth M/S Balance", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "soothe_quality", "Smooth Mode",
        juce::StringArray { "Soft", "Hard" }, 0));
    params.push_back(std::make_unique<juce::AudioParameterBool>("soothe_delta", "Delta Listen", false));

    params.push_back(std::make_unique<juce::AudioParameterBool>("auto_gain", "Auto-Gain", false));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 0.5f), 100.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "output_gain", "Output", juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterBool>("bypass", "Bypass", false));

    static constexpr float defaultFreqs[ShantySonar::numBands] = {
        60.0f, 120.0f, 250.0f, 500.0f, 1000.0f, 2000.0f,
        3500.0f, 5000.0f, 8000.0f, 12000.0f, 16000.0f, 1000.0f
    };

    for (int i = 0; i < ShantySonar::numBands; ++i)
    {
        params.push_back(std::make_unique<juce::AudioParameterBool>(
            bandParamID(i, "enabled"), "Band " + juce::String(i + 1) + " Active", false));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandParamID(i, "type"), "Band " + juce::String(i + 1) + " Type",
            juce::StringArray { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Bandpass", "Tilt Shelf" }, 0));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandParamID(i, "slope"), "Band " + juce::String(i + 1) + " Slope",
            juce::StringArray { "12 dB", "24 dB", "36 dB", "48 dB" }, 1));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "freq"), "Band " + juce::String(i + 1) + " Freq",
            juce::NormalisableRange<float>(20.0f, 20000.0f, 0.1f, 0.25f), defaultFreqs[i]));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "gain"), "Band " + juce::String(i + 1) + " Gain",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "q"), "Band " + juce::String(i + 1) + " Q",
            juce::NormalisableRange<float>(0.1f, 18.0f, 0.01f, 0.35f), 1.0f));
        params.push_back(std::make_unique<juce::AudioParameterChoice>(
            bandParamID(i, "ch_mode"), "Band " + juce::String(i + 1) + " Mode",
            juce::StringArray { "Stereo", "Mid", "Side", "Left", "Right" }, 0));

        params.push_back(std::make_unique<juce::AudioParameterBool>(
            bandParamID(i, "dyn_on"), "Band " + juce::String(i + 1) + " Dyn", false));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "dyn_thresh"), "Band " + juce::String(i + 1) + " Threshold",
            juce::NormalisableRange<float>(-60.0f, 0.0f, 0.5f), -24.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "dyn_range"), "Band " + juce::String(i + 1) + " Dyn Range",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), -6.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "dyn_atk"), "Band " + juce::String(i + 1) + " Attack",
            juce::NormalisableRange<float>(0.5f, 200.0f, 0.5f, 0.4f), 15.0f));
        params.push_back(std::make_unique<juce::AudioParameterFloat>(
            bandParamID(i, "dyn_rel"), "Band " + juce::String(i + 1) + " Release",
            juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.4f), 120.0f));
    }

    return { params.begin(), params.end() };
}

void ShantySonarEQProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = juce::jmax(8000.0, sampleRate);
    sonarEngine.prepare(currentSampleRate, samplesPerBlock);
    for (auto& b : eqBands)
        b.prepare(currentSampleRate);
    for (auto& s : soloFilterState)
        s.reset();

    const int delayCap = ShantySonar::SonarEngineDSP::fftSize + samplesPerBlock + 256;
    dryDelayBuffer.setSize(2, delayCap, false, true, true);
    dryDelayWritePos = 0;

    autoGainEnvIn = 0.0f;
    autoGainEnvOut = 0.0f;
    autoGainLinear = 1.0f;
}

void ShantySonarEQProcessor::releaseResources()
{
    sonarEngine.resetBuffers();
}

bool ShantySonarEQProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut)
        return false;
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.inputBuses[1];
        if (!sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

ShantySonar::BandParams ShantySonarEQProcessor::getBandParams(int i) const
{
    ShantySonar::BandParams bp;
    if (i < 0 || i >= ShantySonar::numBands)
        return bp;
    const auto& p = bandPtrs[i];
    bp.enabled       = p.enabled->load() > 0.5f;
    bp.type          = static_cast<ShantySonar::FilterType>(static_cast<int>(p.type->load()));
    bp.slopeStages   = static_cast<int>(p.slope->load()) + 1;
    bp.freq          = p.freq->load();
    bp.gainDb        = p.gain->load();
    bp.q             = p.q->load();
    bp.chMode        = static_cast<ShantySonar::ChannelMode>(static_cast<int>(p.chMode->load()));
    bp.dynEnabled    = p.dynOn->load() > 0.5f;
    bp.dynThresholdDb = p.dynThresh->load();
    bp.dynRangeDb    = p.dynRange->load();
    bp.dynAttackMs   = p.dynAtk->load();
    bp.dynReleaseMs  = p.dynRel->load();
    return bp;
}

float ShantySonarEQProcessor::getBandLiveDynGain(int bandIndex) const noexcept
{
    if (bandIndex < 0 || bandIndex >= ShantySonar::numBands)
        return 0.0f;
    return eqBands[bandIndex].getLiveDynGainDb();
}

void ShantySonarEQProcessor::switchAB(int targetSlot)
{
    if (targetSlot < 0 || targetSlot > 1 || targetSlot == currentABSlot)
        return;
    abSlots[currentABSlot] = apvts.copyState();
    currentABSlot = targetSlot;
    apvts.replaceState(abSlots[currentABSlot].createCopy());
}

void ShantySonarEQProcessor::loadFactoryPreset(int presetIndex)
{
    undoManager.beginNewTransaction("Load Preset");

    auto setParam = [this](const juce::String& id, float realVal) {
        if (auto* p = apvts.getParameter(id))
            p->setValueNotifyingHost(p->convertTo0to1(realVal));
    };

    // Reset all bands first
    for (int i = 0; i < ShantySonar::numBands; ++i)
    {
        setParam(bandParamID(i, "enabled"), 0.0f);
        setParam(bandParamID(i, "type"), 0.0f);
        setParam(bandParamID(i, "gain"), 0.0f);
        setParam(bandParamID(i, "q"), 1.0f);
        setParam(bandParamID(i, "ch_mode"), 0.0f);
        setParam(bandParamID(i, "dyn_on"), 0.0f);
    }

    setParam("bypass", 0.0f);
    setParam("soothe_delta", 0.0f);
    setParam("mix", 100.0f);
    setParam("output_gain", 0.0f);
    setParam("auto_gain", 0.0f);
    setParam("phase_mode", 0.0f);
    setParam("sonar_min_freq", 30.0f);
    setParam("sonar_max_freq", 18000.0f);
    setParam("unmask_amt", 0.0f);
    setParam("unmask_group", 0.0f);
    setParam("soothe_ms_balance", 0.0f);

    switch (presetIndex)
    {
        case 0: // Default (Init)
            setParam("engine_mode", 0.0f);
            setParam("profile", 0.0f);
            setParam("sonar_impact", 75.0f);
            setParam("sonar_smooth", 50.0f);
            setParam("sonar_adaptive", 25.0f);
            setParam("soothe_depth", 60.0f);
            setParam("soothe_sharpness", 50.0f);
            setParam("soothe_selectivity", 45.0f);
            setParam("soothe_attack", 5.0f);
            setParam("soothe_release", 80.0f);
            setParam("soothe_quality", 0.0f);
            break;

        case 1: // Vocal Polish (SONAR)
            setParam("engine_mode", 0.0f);
            setParam("profile", 1.0f); // Vocals
            setParam("sonar_impact", 80.0f);
            setParam("sonar_smooth", 55.0f);
            setParam("sonar_adaptive", 35.0f);
            setParam("sonar_min_freq", 80.0f);
            setParam(bandParamID(0, "enabled"), 1.0f);
            setParam(bandParamID(0, "type"), 3.0f); // Low Cut
            setParam(bandParamID(0, "freq"), 80.0f);
            break;

        case 2: // Vocal De-Harsh (SMOOTH)
            setParam("engine_mode", 1.0f);
            setParam("soothe_depth", 65.0f);
            setParam("soothe_sharpness", 60.0f);
            setParam("soothe_selectivity", 45.0f);
            setParam("soothe_attack", 3.0f);
            setParam("soothe_release", 60.0f);
            setParam("soothe_ms_balance", -35.0f); // Focus on Mid
            setParam("soothe_quality", 0.0f);
            setParam("sonar_min_freq", 1800.0f);
            setParam("sonar_max_freq", 16000.0f);
            break;

        case 3: // Kick & Bass Unmask
            setParam("engine_mode", 0.0f);
            setParam("profile", 3.0f); // Bass
            setParam("sonar_impact", 70.0f);
            setParam("sonar_adaptive", 40.0f);
            setParam("unmask_group", 1.0f); // Group A
            setParam("unmask_priority", 1.0f); // Midground
            setParam("unmask_amt", 65.0f);
            break;

        case 4: // Master Silk
            setParam("engine_mode", 1.0f);
            setParam("phase_mode", 1.0f); // Linear Phase
            setParam("auto_gain", 1.0f);
            setParam("soothe_depth", 40.0f);
            setParam("soothe_sharpness", 35.0f);
            setParam("soothe_selectivity", 55.0f);
            setParam("soothe_attack", 8.0f);
            setParam("soothe_release", 110.0f);
            setParam("soothe_quality", 0.0f);
            break;

        case 5: // Mud Cleanup
            setParam("engine_mode", 1.0f);
            setParam("soothe_depth", 70.0f);
            setParam("soothe_sharpness", 25.0f);
            setParam("soothe_selectivity", 35.0f);
            setParam("soothe_attack", 10.0f);
            setParam("soothe_release", 120.0f);
            setParam("sonar_min_freq", 120.0f);
            setParam("sonar_max_freq", 2000.0f);
            break;

        case 6: // Harsh Guitar Tamer
            setParam("engine_mode", 1.0f);
            setParam("soothe_depth", 60.0f);
            setParam("soothe_sharpness", 65.0f);
            setParam("soothe_selectivity", 50.0f);
            setParam("soothe_attack", 4.0f);
            setParam("soothe_release", 75.0f);
            setParam("soothe_ms_balance", 25.0f); // Side-biased
            setParam("sonar_min_freq", 1500.0f);
            setParam("sonar_max_freq", 9000.0f);
            break;
    }
}

void ShantySonarEQProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    hostBypassed.store(true, std::memory_order_relaxed);
    AudioProcessor::processBlockBypassed(buffer, midiMessages);
}

void ShantySonarEQProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    hostBypassed.store(false, std::memory_order_relaxed);

    auto mainBus = getBusBuffer(buffer, true, 0);
    const int numSamples = mainBus.getNumSamples();
    const int numCh = mainBus.getNumChannels();
    if (numSamples <= 0 || numCh <= 0)
        return;

    const juce::AudioBuffer<float>* sidechainBuf = nullptr;
    if (getBusCount(true) > 1 && getBus(true, 1)->isEnabled())
    {
        auto sc = getBusBuffer(buffer, true, 1);
        if (sc.getNumChannels() > 0 && sc.getNumSamples() == numSamples)
            sidechainBuf = &sc;
    }

    // Update Sonar / Soothe settings
    ShantySonar::SonarEngineDSP::Settings sSettings;
    sSettings.engineMode        = static_cast<ShantySonar::EngineMode>(static_cast<int>(engineModePtr->load()));
    sSettings.profile           = static_cast<ShantySonar::SourceProfile>(static_cast<int>(profilePtr->load()));
    sSettings.phaseMode         = static_cast<ShantySonar::PhaseMode>(static_cast<int>(phaseModePtr->load()));
    sSettings.impact            = impactPtr->load() * 0.01f;
    sSettings.smoothing         = smoothPtr->load() * 0.01f;
    sSettings.adaptive          = adaptivePtr->load() * 0.01f;
    sSettings.minFreq           = minFreqPtr->load();
    sSettings.maxFreq           = juce::jmax(sSettings.minFreq + 50.0f, maxFreqPtr->load());
    sSettings.unmask            = unmaskPtr->load() * 0.01f;
    sSettings.group             = static_cast<int>(groupPtr->load());
    sSettings.priority          = static_cast<int>(priorityPtr->load());
    sSettings.sootheDepth       = sootheDepthPtr->load() * 0.01f;
    sSettings.sootheSharpness   = sootheSharpPtr->load() * 0.01f;
    sSettings.sootheSelectivity = sootheSelectPtr->load() * 0.01f;
    sSettings.sootheAttackMs    = sootheAtkPtr->load();
    sSettings.sootheReleaseMs   = sootheRelPtr->load();
    sSettings.sootheMsBalance   = sootheMsBalPtr->load() * 0.01f;
    sSettings.sootheQuality     = static_cast<ShantySonar::SootheQuality>(static_cast<int>(sootheQualPtr->load()));
    sonarEngine.updateSettings(sSettings);

    const int targetLatency = sonarEngine.getLatencySamples();
    if (getLatencySamples() != targetLatency)
        setLatencySamples(targetLatency);

    // Input RMS meter
    float inRms = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inRms += mainBus.getRMSLevel(ch, 0, numSamples);
    inRms /= static_cast<float>(numCh);
    inputLevelMeter.store(juce::jlimit(0.0f, 1.0f, inRms * 2.5f), std::memory_order_relaxed);

    // Store dry signal for Mix / Bypass
    const int delayCap = dryDelayBuffer.getNumSamples();
    const int dryDelaySamples = targetLatency;
    for (int i = 0; i < numSamples; ++i)
    {
        const int w = (dryDelayWritePos + i) % delayCap;
        dryDelayBuffer.setSample(0, w, mainBus.getSample(0, i));
        dryDelayBuffer.setSample(1, w, mainBus.getSample(numCh > 1 ? 1 : 0, i));
    }

    if (bypassPtr->load() > 0.5f)
    {
        // Still feed analyzer so UI spectrum stays alive
        juce::AudioBuffer<float> scratch;
        scratch.makeCopyOf(mainBus);
        sonarEngine.processBlock(scratch, sidechainBuf);

        for (int i = 0; i < numSamples; ++i)
        {
            const int r = (dryDelayWritePos + i - dryDelaySamples + delayCap) % delayCap;
            mainBus.setSample(0, i, dryDelayBuffer.getSample(0, r));
            if (numCh > 1)
                mainBus.setSample(1, i, dryDelayBuffer.getSample(1, r));
        }
        dryDelayWritePos = (dryDelayWritePos + numSamples) % delayCap;
        outputLevelMeter.store(inputLevelMeter.load(std::memory_order_relaxed), std::memory_order_relaxed);
        return;
    }

    // 1. Smart Sonar Engine
    sonarEngine.processBlock(mainBus, sidechainBuf);

    // 2. 12-band Parametric + Dynamic EQ
    float* left = mainBus.getWritePointer(0);
    float* right = (numCh > 1) ? mainBus.getWritePointer(1) : nullptr;
    for (int i = 0; i < ShantySonar::numBands; ++i)
        eqBands[i].processBlock(left, right, numSamples, getBandParams(i));

    // 3. Band Solo audition (if active)
    const int soloIdx = soloBandIndex.load(std::memory_order_relaxed);
    if (soloIdx >= 0 && soloIdx < ShantySonar::numBands)
    {
        const auto bp = getBandParams(soloIdx);
        const auto soloCoeffs = ShantySonar::BiquadCoeffs::makeBandpass(
            currentSampleRate, bp.freq, juce::jlimit(0.5f, 8.0f, bp.q));
        for (int i = 0; i < numSamples; ++i)
        {
            left[i] = soloFilterState[0].process(left[i], soloCoeffs) * 1.4f;
            if (right != nullptr)
                right[i] = soloFilterState[1].process(right[i], soloCoeffs) * 1.4f;
        }
    }

    // 4. Auto-Gain Compensation
    const bool useAutoGain = autoGainPtr->load() > 0.5f;
    float outRms = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        outRms += mainBus.getRMSLevel(ch, 0, numSamples);
    outRms /= static_cast<float>(numCh);

    if (useAutoGain && inRms > 1e-4f && outRms > 1e-4f)
    {
        autoGainEnvIn = 0.92f * autoGainEnvIn + 0.08f * inRms;
        autoGainEnvOut = 0.92f * autoGainEnvOut + 0.08f * outRms;
        const float targetRatio = juce::jlimit(0.25f, 4.0f, autoGainEnvIn / juce::jmax(1e-5f, autoGainEnvOut));
        autoGainLinear = 0.90f * autoGainLinear + 0.10f * targetRatio;
    }
    else
    {
        autoGainLinear = 0.90f * autoGainLinear + 0.10f * 1.0f;
    }

    // 5. Mix + Output Gain (or Delta Listen in Soothe mode)
    const float mix = mixPtr->load() * 0.01f;
    const float outGainLin = juce::Decibels::decibelsToGain(outputGainPtr->load()) * autoGainLinear;
    const bool isDelta = (sSettings.engineMode == ShantySonar::EngineMode::Soothe) && (sootheDeltaPtr->load() > 0.5f);

    for (int i = 0; i < numSamples; ++i)
    {
        const int r = (dryDelayWritePos + i - dryDelaySamples + delayCap) % delayCap;
        const float dryL = dryDelayBuffer.getSample(0, r);
        const float wetL = isDelta ? (dryL - left[i]) : (left[i] * outGainLin);
        float outL = isDelta ? wetL : (dryL * (1.0f - mix) + wetL * mix);
        if (!std::isfinite(outL)) outL = 0.0f;
        left[i] = juce::jlimit(-4.0f, 4.0f, outL);

        if (right != nullptr)
        {
            const float dryR = dryDelayBuffer.getSample(1, r);
            const float wetR = isDelta ? (dryR - right[i]) : (right[i] * outGainLin);
            float outR = isDelta ? wetR : (dryR * (1.0f - mix) + wetR * mix);
            if (!std::isfinite(outR)) outR = 0.0f;
            right[i] = juce::jlimit(-4.0f, 4.0f, outR);
        }
    }
    dryDelayWritePos = (dryDelayWritePos + numSamples) % delayCap;

    // Meters for Shanty Mascot
    float finalRms = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        finalRms += mainBus.getRMSLevel(ch, 0, numSamples);
    finalRms /= static_cast<float>(numCh);
    outputLevelMeter.store(juce::jlimit(0.0f, 1.0f, finalRms * 2.5f), std::memory_order_relaxed);
    sonarActivityMeter.store(sSettings.impact * (sonarEngine.getHasLearned() ? 1.0f : sSettings.adaptive), std::memory_order_relaxed);
    unmaskActivityMeter.store(sSettings.unmask, std::memory_order_relaxed);
    sootheReductionMeter.store(sonarEngine.getSootheReductionMeter(), std::memory_order_relaxed);
}

void ShantySonarEQProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void ShantySonarEQProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* ShantySonarEQProcessor::createEditor()
{
    return new ShantySonarEQEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShantySonarEQProcessor();
}
