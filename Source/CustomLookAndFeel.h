#pragma once

#include <JuceHeader.h>

namespace ShantySonar
{

namespace Palette
{
    inline const juce::Colour bgDark        = juce::Colour(0xFF141619);
    inline const juce::Colour bgPanel       = juce::Colour(0xFF1D2025);
    inline const juce::Colour bgCanvas      = juce::Colour(0xFF0F1114);
    inline const juce::Colour borderSubtle  = juce::Colour(0xFF2B3038);
    inline const juce::Colour sonarCyan     = juce::Colour(0xFF38D4F0); // Smart Sonar AI curve
    inline const juce::Colour tailCyan      = juce::Colour(0xFF7CB8D9); // Shanti silver-blue
    inline const juce::Colour teethRed      = juce::Colour(0xFFE03E46); // Shanti red collar / Unmasking
    inline const juce::Colour satAmber      = juce::Colour(0xFFF29F4B); // Dynamic EQ / Warning
    inline const juce::Colour emeraldOk     = juce::Colour(0xFF3BD689); // Balanced / Learned
    inline const juce::Colour textPrimary   = juce::Colour(0xFFF2F4F7);
    inline const juce::Colour textSecondary = juce::Colour(0xFF8B94A3);
    inline const juce::Colour knobBody      = juce::Colour(0xFF21242B);
    inline const juce::Colour knobHighlight = juce::Colour(0xFF323742);

    inline juce::Colour bandColour(int idx)
    {
        static const juce::Colour colours[] = {
            juce::Colour(0xFF38D4F0),
            juce::Colour(0xFF7CB8D9),
            juce::Colour(0xFF3BD689),
            juce::Colour(0xFFF29F4B),
            juce::Colour(0xFFE03E46),
            juce::Colour(0xFFB388FF),
            juce::Colour(0xFF4DD0E1),
            juce::Colour(0xFFFFD54F),
            juce::Colour(0xFFFF7043),
            juce::Colour(0xFF81C784),
            juce::Colour(0xFF64B5F6),
            juce::Colour(0xFFF06292)
        };
        return colours[idx % 12];
    }
}

class ShantyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ShantyLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, Palette::bgDark);
        setColour(juce::ComboBox::backgroundColourId, Palette::bgPanel);
        setColour(juce::ComboBox::outlineColourId, Palette::borderSubtle);
        setColour(juce::ComboBox::textColourId, Palette::textPrimary);
        setColour(juce::ComboBox::arrowColourId, Palette::sonarCyan);
        setColour(juce::PopupMenu::backgroundColourId, Palette::bgPanel);
        setColour(juce::PopupMenu::textColourId, Palette::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, Palette::sonarCyan.withAlpha(0.30f));
        setColour(juce::TextButton::buttonColourId, Palette::bgPanel);
        setColour(juce::TextButton::buttonOnColourId, Palette::sonarCyan.withAlpha(0.26f));
        setColour(juce::TextButton::textColourOffId, Palette::textSecondary);
        setColour(juce::TextButton::textColourOnId, Palette::textPrimary);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                                   static_cast<float>(width), static_cast<float>(height)).reduced(3.0f);
        const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float cx = bounds.getCentreX();
        const float cy = bounds.getCentreY();
        const float angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

        const float trackRadius = radius - 3.0f;
        const float trackThickness = 2.8f;

        juce::Path bgArc;
        bgArc.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(Palette::borderSubtle);
        g.strokePath(bgArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool isBipolar = slider.getProperties().getWithDefault("bipolar", false);
        const float startValAngle = isBipolar
            ? rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle)
            : rotaryStartAngle;

        juce::Colour accent = Palette::sonarCyan;
        const auto role = slider.getProperties().getWithDefault("role", juce::String("sonar")).toString();
        if (role == "unmask" || role == "red")
            accent = Palette::teethRed;
        else if (role == "dyn" || role == "amber")
            accent = Palette::satAmber;
        else if (role == "tail")
            accent = Palette::tailCyan;
        else if (role == "emerald")
            accent = Palette::emeraldOk;

        juce::Path valArc;
        valArc.addCentredArc(cx, cy, trackRadius, trackRadius, 0.0f,
                             juce::jmin(startValAngle, angle), juce::jmax(startValAngle, angle), true);
        g.setColour(accent);
        g.strokePath(valArc, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const float bodyRadius = trackRadius - 4.0f;
        if (bodyRadius > 2.0f)
        {
            juce::ColourGradient grad(Palette::knobHighlight, cx - bodyRadius * 0.4f, cy - bodyRadius * 0.5f,
                                      Palette::knobBody, cx + bodyRadius * 0.5f, cy + bodyRadius * 0.6f, true);
            g.setGradientFill(grad);
            g.fillEllipse(cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f);

            g.setColour(Palette::borderSubtle.brighter(0.15f));
            g.drawEllipse(cx - bodyRadius, cy - bodyRadius, bodyRadius * 2.0f, bodyRadius * 2.0f, 1.0f);

            const float ptrLen = bodyRadius * 0.65f;
            const float px = cx + std::sin(angle) * ptrLen;
            const float py = cy - std::cos(angle) * ptrLen;
            g.setColour(Palette::textPrimary);
            g.drawLine(cx + std::sin(angle) * (bodyRadius * 0.22f),
                       cy - std::cos(angle) * (bodyRadius * 0.22f),
                       px, py, 2.0f);
        }
    }
};

} // namespace ShantySonar
