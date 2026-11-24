#include "PluginEditor.h"
#include "BinaryData.h"

ShantySonarEQEditor::ShantySonarEQEditor(ShantySonarEQProcessor& p)
    : AudioProcessorEditor(&p), processor(p), spectrum(p)
{
    setLookAndFeel(&lnf);

    adamskyLogo = juce::ImageCache::getFromMemory(BinaryData::adamsky_logo_png, BinaryData::adamsky_logo_pngSize);

    addAndMakeVisible(spectrum);
    addAndMakeVisible(mascot);

    spectrum.onBandSelected = [this](int idx) { selectBand(idx); };

    // Top bar
    modeSonarBtn.onClick = [this] {
        if (auto* p = processor.apvts.getParameter("engine_mode"))
            p->setValueNotifyingHost(0.0f);
        updateModeVisibility();
    };
    modeSootheBtn.onClick = [this] {
        if (auto* p = processor.apvts.getParameter("engine_mode"))
            p->setValueNotifyingHost(1.0f);
        updateModeVisibility();
    };
    addAndMakeVisible(modeSonarBtn);
    addAndMakeVisible(modeSootheBtn);

    profileCombo.addItemList({ "Universal", "Vocals", "Drums", "Bass", "Guitar", "Keys / Synth", "Speech", "Master" }, 1);
    addAndMakeVisible(profileCombo);
    profileAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "profile", profileCombo);

    sootheQualCombo.addItemList({ "Soft", "Hard" }, 1);
    addAndMakeVisible(sootheQualCombo);
    sootheQualAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "soothe_quality", sootheQualCombo);

    sootheDeltaBtn.setClickingTogglesState(true);
    addAndMakeVisible(sootheDeltaBtn);
    sootheDeltaAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "soothe_delta", sootheDeltaBtn);

    phaseModeCombo.addItemList({ "Zero-Latency", "Linear Phase" }, 1);
    addAndMakeVisible(phaseModeCombo);
    phaseModeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "phase_mode", phaseModeCombo);

    learnBtn.onClick = [this] {
        if (processor.getIsLearning())
            processor.stopLearn();
        else
            processor.startLearn();
    };
    addAndMakeVisible(learnBtn);

    clearLearnBtn.onClick = [this] { processor.clearLearned(); };
    addAndMakeVisible(clearLearnBtn);

    slotABtn.setClickingTogglesState(false);
    slotBBtn.setClickingTogglesState(false);
    slotABtn.onClick = [this] {
        processor.switchAB(0);
        selectBand(currentBand);
    };
    slotBBtn.onClick = [this] {
        processor.switchAB(1);
        selectBand(currentBand);
    };
    addAndMakeVisible(slotABtn);
    addAndMakeVisible(slotBBtn);

    autoGainBtn.setClickingTogglesState(true);
    addAndMakeVisible(autoGainBtn);
    autoGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "auto_gain", autoGainBtn);

    bypassBtn.setClickingTogglesState(true);
    addAndMakeVisible(bypassBtn);
    bypassAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypassBtn);

    manualBtn.setTooltip("Open Manual / Руководство");
    manualBtn.onClick = [this] {
        manualOverlay.setVisible(!manualOverlay.isVisible());
        manualOverlay.toFront(true);
    };
    addAndMakeVisible(manualBtn);

    addChildComponent(manualOverlay);

    // Right sidebar knobs
    setupKnob(impactSlider, impactLabel, "IMPACT", "sonar");
    setupKnob(smoothSlider, smoothLabel, "SMOOTH", "sonar");
    setupKnob(adaptiveSlider, adaptiveLabel, "ADAPTIVE", "tail");
    setupKnob(unmaskSlider, unmaskLabel, "UNMASK", "unmask");

    setupKnob(sootheDepthSlider, sootheDepthLabel, "DEPTH", "amber");
    setupKnob(sootheSharpSlider, sootheSharpLabel, "SHARP", "amber");
    setupKnob(sootheSelectSlider, sootheSelectLabel, "SELECT", "tail");
    setupKnob(sootheAtkSlider, sootheAtkLabel, "ATTACK", "amber");
    setupKnob(sootheRelSlider, sootheRelLabel, "RELEASE", "amber");

    setupKnob(mixSlider, mixLabel, "MIX", "tail");
    setupKnob(outputSlider, outputLabel, "OUTPUT", "sonar", true);

    impactAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sonar_impact", impactSlider);
    smoothAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sonar_smooth", smoothSlider);
    adaptiveAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "sonar_adaptive", adaptiveSlider);
    unmaskAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "unmask_amt", unmaskSlider);
    sootheDepthAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "soothe_depth", sootheDepthSlider);
    sootheSharpAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "soothe_sharpness", sootheSharpSlider);
    sootheSelectAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "soothe_selectivity", sootheSelectSlider);
    sootheAtkAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "soothe_attack", sootheAtkSlider);
    sootheRelAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "soothe_release", sootheRelSlider);
    mixAtt          = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "mix", mixSlider);
    outputAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "output_gain", outputSlider);

    groupCombo.addItemList({ "Group: Off", "Group A", "Group B", "Group C", "Group D" }, 1);
    addAndMakeVisible(groupCombo);
    groupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "unmask_group", groupCombo);

    priorityCombo.addItemList({ "Foreground", "Midground", "Background" }, 1);
    addAndMakeVisible(priorityCombo);
    priorityAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, "unmask_priority", priorityCombo);

    // Bottom Band Inspector
    for (int i = 0; i < ShantySonar::numBands; ++i)
    {
        auto& btn = bandButtons[i];
        btn.setButtonText(juce::String(i + 1));
        btn.onClick = [this, i] { selectBand(i); };
        addAndMakeVisible(btn);
    }

    addAndMakeVisible(bandEnabledBtn);
    bandSoloBtn.setClickingTogglesState(true);
    bandSoloBtn.onClick = [this] {
        processor.setSoloBand(bandSoloBtn.getToggleState() ? currentBand : -1);
    };
    addAndMakeVisible(bandSoloBtn);

    bandTypeCombo.addItemList({ "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch", "Bandpass", "Tilt Shelf" }, 1);
    addAndMakeVisible(bandTypeCombo);

    bandSlopeCombo.addItemList({ "12 dB", "24 dB", "36 dB", "48 dB" }, 1);
    addAndMakeVisible(bandSlopeCombo);

    bandChModeCombo.addItemList({ "Stereo", "Mid", "Side", "Left", "Right" }, 1);
    addAndMakeVisible(bandChModeCombo);

    setupKnob(bandFreqSlider, bandFreqLabel, "FREQ", "sonar");
    setupKnob(bandGainSlider, bandGainLabel, "GAIN", "sonar", true);
    setupKnob(bandQSlider, bandQLabel, "Q", "tail");

    addAndMakeVisible(dynEnabledBtn);
    setupKnob(dynThreshSlider, dynThreshLabel, "THRESH", "dyn");
    setupKnob(dynRangeSlider, dynRangeLabel, "RANGE", "dyn", true);
    setupKnob(dynAtkSlider, dynAtkLabel, "ATTACK", "dyn");
    setupKnob(dynRelSlider, dynRelLabel, "RELEASE", "dyn");

    selectBand(0);
    updateModeVisibility();

    setResizable(true, true);
    setResizeLimits(980, 640, 1600, 1050);
    setSize(1140, 720);
    startTimerHz(45);
}

ShantySonarEQEditor::~ShantySonarEQEditor()
{
    stopTimer();
    processor.setSoloBand(-1);
    setLookAndFeel(nullptr);
}

void ShantySonarEQEditor::setupKnob(juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& role, bool bipolar)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 15);
    s.setColour(juce::Slider::textBoxTextColourId, ShantySonar::Palette::textPrimary);
    s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    s.getProperties().set("role", role);
    s.getProperties().set("bipolar", bipolar);
    addAndMakeVisible(s);

    l.setText(text, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(10.5f, juce::Font::bold));
    l.setColour(juce::Label::textColourId, ShantySonar::Palette::textSecondary);
    addAndMakeVisible(l);
}

void ShantySonarEQEditor::selectBand(int bandIndex)
{
    currentBand = juce::jlimit(0, ShantySonar::numBands - 1, bandIndex);
    spectrum.setSelectedBand(currentBand);

    bandEnabledAtt.reset();
    bandTypeAtt.reset();
    bandSlopeAtt.reset();
    bandChModeAtt.reset();
    bandFreqAtt.reset();
    bandGainAtt.reset();
    bandQAtt.reset();
    dynEnabledAtt.reset();
    dynThreshAtt.reset();
    dynRangeAtt.reset();
    dynAtkAtt.reset();
    dynRelAtt.reset();

    auto id = [&](const char* suffix) { return ShantySonarEQProcessor::bandParamID(currentBand, suffix); };

    bandEnabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, id("enabled"), bandEnabledBtn);
    bandTypeAtt    = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, id("type"), bandTypeCombo);
    bandSlopeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, id("slope"), bandSlopeCombo);
    bandChModeAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processor.apvts, id("ch_mode"), bandChModeCombo);
    bandFreqAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("freq"), bandFreqSlider);
    bandGainAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("gain"), bandGainSlider);
    bandQAtt       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("q"), bandQSlider);

    dynEnabledAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, id("dyn_on"), dynEnabledBtn);
    dynThreshAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("dyn_thresh"), dynThreshSlider);
    dynRangeAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("dyn_range"), dynRangeSlider);
    dynAtkAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("dyn_atk"), dynAtkSlider);
    dynRelAtt     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, id("dyn_rel"), dynRelSlider);

    bandSoloBtn.setToggleState(processor.getSoloBand() == currentBand, juce::dontSendNotification);
    repaint();
}

void ShantySonarEQEditor::updateModeVisibility()
{
    const bool isSoothe = (processor.apvts.getRawParameterValue("engine_mode")->load() > 0.5f);

    profileCombo.setVisible(!isSoothe);
    learnBtn.setVisible(!isSoothe);
    clearLearnBtn.setVisible(!isSoothe);
    impactSlider.setVisible(!isSoothe);   impactLabel.setVisible(!isSoothe);
    smoothSlider.setVisible(!isSoothe);   smoothLabel.setVisible(!isSoothe);
    adaptiveSlider.setVisible(!isSoothe); adaptiveLabel.setVisible(!isSoothe);
    unmaskSlider.setVisible(!isSoothe);   unmaskLabel.setVisible(!isSoothe);
    groupCombo.setVisible(!isSoothe);
    priorityCombo.setVisible(!isSoothe);

    sootheQualCombo.setVisible(isSoothe);
    sootheDeltaBtn.setVisible(isSoothe);
    sootheDepthSlider.setVisible(isSoothe);  sootheDepthLabel.setVisible(isSoothe);
    sootheSharpSlider.setVisible(isSoothe);  sootheSharpLabel.setVisible(isSoothe);
    sootheSelectSlider.setVisible(isSoothe); sootheSelectLabel.setVisible(isSoothe);
    sootheAtkSlider.setVisible(isSoothe);    sootheAtkLabel.setVisible(isSoothe);
    sootheRelSlider.setVisible(isSoothe);    sootheRelLabel.setVisible(isSoothe);

    modeSonarBtn.setColour(juce::TextButton::buttonColourId,
                           !isSoothe ? ShantySonar::Palette::sonarCyan.withAlpha(0.32f) : ShantySonar::Palette::bgPanel);
    modeSootheBtn.setColour(juce::TextButton::buttonColourId,
                            isSoothe ? ShantySonar::Palette::satAmber.withAlpha(0.32f) : ShantySonar::Palette::bgPanel);
    repaint();
}

void ShantySonarEQEditor::timerCallback()
{
    spectrum.updateData();

    const bool isSoothe = (processor.apvts.getRawParameterValue("engine_mode")->load() > 0.5f);
    if (profileCombo.isVisible() == isSoothe)
        updateModeVisibility();

    const bool learning = processor.getIsLearning();
    const bool learned  = processor.getHasLearned();
    const float prog    = processor.getLearnProgress();

    if (learning)
    {
        learnBtn.setButtonText("SCAN " + juce::String(static_cast<int>(prog * 100.0f)) + "%");
        learnBtn.setColour(juce::TextButton::buttonColourId, ShantySonar::Palette::sonarCyan.withAlpha(0.38f));
    }
    else if (learned)
    {
        learnBtn.setButtonText("LEARNED");
        learnBtn.setColour(juce::TextButton::buttonColourId, ShantySonar::Palette::emeraldOk.withAlpha(0.26f));
    }
    else
    {
        learnBtn.setButtonText("LEARN");
        learnBtn.setColour(juce::TextButton::buttonColourId, ShantySonar::Palette::bgPanel);
    }

    const int ab = processor.getCurrentABSlot();
    slotABtn.setColour(juce::TextButton::buttonColourId,
                       ab == 0 ? ShantySonar::Palette::sonarCyan.withAlpha(0.3f) : ShantySonar::Palette::bgPanel);
    slotBBtn.setColour(juce::TextButton::buttonColourId,
                       ab == 1 ? ShantySonar::Palette::sonarCyan.withAlpha(0.3f) : ShantySonar::Palette::bgPanel);

    for (int i = 0; i < ShantySonar::numBands; ++i)
    {
        const auto bp = processor.getBandParams(i);
        auto& btn = bandButtons[i];
        const auto col = ShantySonar::Palette::bandColour(i);
        if (i == currentBand)
            btn.setColour(juce::TextButton::buttonColourId, col.withAlpha(0.35f));
        else if (bp.enabled)
            btn.setColour(juce::TextButton::buttonColourId, col.withAlpha(0.16f));
        else
            btn.setColour(juce::TextButton::buttonColourId, ShantySonar::Palette::bgPanel);
    }

    mascot.updateAnimation(processor.outputLevelMeter.load(std::memory_order_relaxed),
                           processor.sonarActivityMeter.load(std::memory_order_relaxed),
                           processor.unmaskActivityMeter.load(std::memory_order_relaxed),
                           learning,
                           learned,
                           isSoothe,
                           processor.sootheReductionMeter.load(std::memory_order_relaxed));
}

void ShantySonarEQEditor::paint(juce::Graphics& g)
{
    g.fillAll(ShantySonar::Palette::bgDark);

    const auto bounds = getLocalBounds();

    // Header bar
    const auto header = bounds.withHeight(52);
    g.setColour(ShantySonar::Palette::bgPanel);
    g.fillRect(header);
    g.setColour(ShantySonar::Palette::borderSubtle);
    g.drawHorizontalLine(header.getBottom() - 1, 0.0f, static_cast<float>(getWidth()));

    if (adamskyLogo.isValid())
        g.drawImageWithin(adamskyLogo, 12, 8, 36, 36, juce::RectanglePlacement::centred);

    const bool isSoothe = (processor.apvts.getRawParameterValue("engine_mode")->load() > 0.5f);
    g.setColour(ShantySonar::Palette::textPrimary);
    g.setFont(juce::Font(17.0f, juce::Font::bold));
    g.drawText("SHANTY SONAR EQ", 56, 8, 200, 22, juce::Justification::left);
    g.setColour(isSoothe ? ShantySonar::Palette::satAmber : ShantySonar::Palette::sonarCyan);
    g.setFont(juce::Font(10.5f, juce::Font::bold));
    g.drawText(isSoothe ? "DYNAMIC RESONANCE SUPPRESSOR" : "ADAPTIVE SPECTRAL & UNMASKING EQ",
               56, 29, 230, 14, juce::Justification::left);

    // Right sidebar panel background
    const int sideW = 248;
    const int bottomH = 166;
    const auto sideRect = juce::Rectangle<int>(getWidth() - sideW - 8, 60, sideW, getHeight() - 60 - bottomH - 14);
    g.setColour(ShantySonar::Palette::bgPanel);
    g.fillRoundedRectangle(sideRect.toFloat(), 6.0f);
    g.setColour(ShantySonar::Palette::borderSubtle);
    g.drawRoundedRectangle(sideRect.toFloat(), 6.0f, 1.0f);

    // Bottom inspector panel background
    const auto botRect = juce::Rectangle<int>(8, getHeight() - bottomH - 8, getWidth() - 16, bottomH);
    g.setColour(ShantySonar::Palette::bgPanel);
    g.fillRoundedRectangle(botRect.toFloat(), 6.0f);
    g.setColour(ShantySonar::Palette::borderSubtle);
    g.drawRoundedRectangle(botRect.toFloat(), 6.0f, 1.0f);

    // Dynamic EQ section divider in bottom inspector
    const int dynX = botRect.getX() + static_cast<int>(botRect.getWidth() * 0.50f);
    g.setColour(ShantySonar::Palette::borderSubtle);
    g.drawVerticalLine(dynX, static_cast<float>(botRect.getY() + 36), static_cast<float>(botRect.getBottom() - 8));
}

void ShantySonarEQEditor::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Top bar layout
    int tx = 272;
    modeSonarBtn.setBounds(tx, 12, 64, 28); tx += 66;
    modeSootheBtn.setBounds(tx, 12, 68, 28); tx += 76;

    profileCombo.setBounds(tx, 12, 120, 28);
    learnBtn.setBounds(tx + 126, 12, 84, 28);
    clearLearnBtn.setBounds(tx + 214, 12, 38, 28);

    sootheQualCombo.setBounds(tx, 12, 92, 28);
    sootheDeltaBtn.setBounds(tx + 98, 12, 70, 28);

    phaseModeCombo.setBounds(w - 414, 12, 114, 28);
    slotABtn.setBounds(w - 292, 12, 30, 28);
    slotBBtn.setBounds(w - 260, 12, 30, 28);
    autoGainBtn.setBounds(w - 222, 12, 84, 28);
    bypassBtn.setBounds(w - 130, 12, 78, 28);
    manualBtn.setBounds(w - 44, 12, 32, 28);

    const int sideW = 248;
    const int bottomH = 166;
    const int mainH = h - 60 - bottomH - 14;

    spectrum.setBounds(8, 60, w - sideW - 24, mainH);

    // Right sidebar
    const int sx = w - sideW - 8;
    const int mascotH = juce::jlimit(120, 175, mainH / 3);
    mascot.setBounds(sx + 6, 64, sideW - 12, mascotH);

    int ky = 64 + mascotH + 4;
    const int kw = (sideW - 18) / 2;
    const int kh = 68;

    auto placeKnob = [&](juce::Slider& s, juce::Label& l, int x, int y, int width, int height) {
        l.setBounds(x, y, width, 13);
        s.setBounds(x, y + 12, width, height - 12);
    };

    const int sonarStartKy = ky;
    placeKnob(impactSlider, impactLabel, sx + 6, ky, kw, kh);
    placeKnob(smoothSlider, smoothLabel, sx + 12 + kw, ky, kw, kh);
    ky += kh + 2;

    placeKnob(adaptiveSlider, adaptiveLabel, sx + 6, ky, kw, kh);
    placeKnob(unmaskSlider, unmaskLabel, sx + 12 + kw, ky, kw, kh);
    ky += kh + 6;

    groupCombo.setBounds(sx + 8, ky, (sideW - 22) / 2, 24);
    priorityCombo.setBounds(sx + 14 + (sideW - 22) / 2, ky, (sideW - 22) / 2, 24);
    ky += 30;

    // Soothe knobs occupy the same sidebar region when Soothe mode is active
    int sky = sonarStartKy;
    const int kw3 = (sideW - 24) / 3;
    placeKnob(sootheDepthSlider, sootheDepthLabel, sx + 6, sky, kw, kh);
    placeKnob(sootheSharpSlider, sootheSharpLabel, sx + 12 + kw, sky, kw, kh);
    sky += kh + 2;
    placeKnob(sootheSelectSlider, sootheSelectLabel, sx + 6, sky, kw3, kh);
    placeKnob(sootheAtkSlider, sootheAtkLabel, sx + 12 + kw3, sky, kw3, kh);
    placeKnob(sootheRelSlider, sootheRelLabel, sx + 18 + kw3 * 2, sky, kw3, kh);

    const int remH = juce::jmax(58, (60 + mainH) - ky - 6);
    placeKnob(mixSlider, mixLabel, sx + 6, ky, kw, juce::jmin(72, remH));
    placeKnob(outputSlider, outputLabel, sx + 12 + kw, ky, kw, juce::jmin(72, remH));

    // Bottom Inspector
    const int bx = 16;
    const int by = h - bottomH - 2;
    const int bw = w - 32;

    // Band 1..12 pills
    const int pillW = 30;
    for (int i = 0; i < ShantySonar::numBands; ++i)
        bandButtons[i].setBounds(bx + i * (pillW + 3), by + 4, pillW, 24);

    int cx = bx + ShantySonar::numBands * (pillW + 3) + 12;
    bandEnabledBtn.setBounds(cx, by + 4, 68, 24); cx += 70;
    bandSoloBtn.setBounds(cx, by + 4, 52, 24); cx += 58;
    bandTypeCombo.setBounds(cx, by + 4, 108, 24); cx += 114;
    bandSlopeCombo.setBounds(cx, by + 4, 76, 24); cx += 82;
    bandChModeCombo.setBounds(cx, by + 4, 82, 24);

    // Parametric knobs (left half)
    const int knobY = by + 36;
    const int knobH = bottomH - 46;
    const int leftW = bw / 2 - 12;
    const int pKw = juce::jmin(110, leftW / 3);
    placeKnob(bandFreqSlider, bandFreqLabel, bx + 12, knobY, pKw, knobH);
    placeKnob(bandGainSlider, bandGainLabel, bx + 20 + pKw, knobY, pKw, knobH);
    placeKnob(bandQSlider, bandQLabel, bx + 28 + pKw * 2, knobY, pKw, knobH);

    // Dynamic EQ knobs (right half)
    const int rx = bx + bw / 2 + 12;
    dynEnabledBtn.setBounds(rx, knobY + 2, 86, 22);
    const int dKw = juce::jmin(92, (bw / 2 - 108) / 4);
    const int dStart = rx + 92;
    placeKnob(dynThreshSlider, dynThreshLabel, dStart, knobY, dKw, knobH);
    placeKnob(dynRangeSlider, dynRangeLabel, dStart + dKw + 4, knobY, dKw, knobH);
    placeKnob(dynAtkSlider, dynAtkLabel, dStart + (dKw + 4) * 2, knobY, dKw, knobH);
    placeKnob(dynRelSlider, dynRelLabel, dStart + (dKw + 4) * 3, knobY, dKw, knobH);

    manualOverlay.setBounds(getLocalBounds());
}
