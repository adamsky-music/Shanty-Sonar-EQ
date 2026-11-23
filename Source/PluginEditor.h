#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include "ShantyMascot.h"
#include "SpectrumAnalyzerComponent.h"
#include <array>
#include <memory>

class ShantySonarEQEditor : public juce::AudioProcessorEditor,
                            private juce::Timer
{
public:
    explicit ShantySonarEQEditor(ShantySonarEQProcessor&);
    ~ShantySonarEQEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void selectBand(int bandIndex);
    void setupKnob(juce::Slider& s, juce::Label& l, const juce::String& text, const juce::String& role, bool bipolar = false);

    ShantySonarEQProcessor& processor;
    ShantySonar::ShantyLookAndFeel lnf;

    juce::Image adamskyLogo;
    ShantySonar::SpectrumAnalyzerComponent spectrum;
    ShantySonar::ShantyMascotComponent mascot;

    // Top bar
    juce::ComboBox profileCombo, phaseModeCombo;
    juce::TextButton learnBtn { "LEARN" };
    juce::TextButton clearLearnBtn { "CLR" };
    juce::TextButton slotABtn { "A" };
    juce::TextButton slotBBtn { "B" };
    juce::TextButton autoGainBtn { "AUTO GAIN" };
    juce::TextButton bypassBtn { "BYPASS" };

    // Right sidebar: Smart Sonar & Unmasking & Output
    juce::Slider impactSlider, smoothSlider, adaptiveSlider, unmaskSlider;
    juce::Label impactLabel, smoothLabel, adaptiveLabel, unmaskLabel;
    juce::ComboBox groupCombo, priorityCombo;
    juce::Label groupLabel, priorityLabel;
    juce::Slider mixSlider, outputSlider;
    juce::Label mixLabel, outputLabel;

    // Bottom Band Inspector
    int currentBand = 0;
    std::array<juce::TextButton, ShantySonar::numBands> bandButtons;

    juce::ToggleButton bandEnabledBtn { "Active" };
    juce::TextButton bandSoloBtn { "SOLO" };
    juce::ComboBox bandTypeCombo, bandSlopeCombo, bandChModeCombo;
    juce::Slider bandFreqSlider, bandGainSlider, bandQSlider;
    juce::Label bandFreqLabel, bandGainLabel, bandQLabel;

    juce::ToggleButton dynEnabledBtn { "Dynamic" };
    juce::Slider dynThreshSlider, dynRangeSlider, dynAtkSlider, dynRelSlider;
    juce::Label dynThreshLabel, dynRangeLabel, dynAtkLabel, dynRelLabel;

    // Global APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> profileAtt, phaseModeAtt, groupAtt, priorityAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> impactAtt, smoothAtt, adaptiveAtt, unmaskAtt, mixAtt, outputAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAtt, bypassAtt;

    // Per-band dynamic APVTS Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bandEnabledAtt, dynEnabledAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> bandTypeAtt, bandSlopeAtt, bandChModeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bandFreqAtt, bandGainAtt, bandQAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dynThreshAtt, dynRangeAtt, dynAtkAtt, dynRelAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShantySonarEQEditor)
};
