#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "CustomLookAndFeel.h"
#include <array>
#include <cmath>
#include <functional>

namespace ShantySonar
{

class SpectrumAnalyzerComponent : public juce::Component
{
public:
    SpectrumAnalyzerComponent(ShantySonarEQProcessor& p) : processor(p)
    {
        setMouseCursor(juce::MouseCursor::CrosshairCursor);
    }

    std::function<void(int)> onBandSelected;

    void setSelectedBand(int idx) noexcept
    {
        selectedBand = juce::jlimit(0, numBands - 1, idx);
        repaint();
    }

    void setFrozen(bool shouldFreeze) noexcept
    {
        frozen = shouldFreeze;
        repaint();
    }

    bool isFrozen() const noexcept { return frozen; }

    void updateData()
    {
        if (!frozen)
            processor.copyDisplayData(preSpec, postSpec, smartCurve, maskingCurve);
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        g.setColour(Palette::bgCanvas);
        g.fillRoundedRectangle(bounds, 6.0f);

        const auto plot = bounds.reduced(8.0f, 10.0f);

        // 1. Grid
        drawGrid(g, plot);

        // 2. Smart Filter Range Shading + Handles
        const float minF = processor.apvts.getRawParameterValue("sonar_min_freq")->load();
        const float maxF = processor.apvts.getRawParameterValue("sonar_max_freq")->load();
        const float xMin = freqToX(minF, plot);
        const float xMax = freqToX(maxF, plot);

        g.setColour(Palette::sonarCyan.withAlpha(0.045f));
        g.fillRect(xMin, plot.getY(), juce::jmax(1.0f, xMax - xMin), plot.getHeight());

        // Range boundary lines
        g.setColour(Palette::sonarCyan.withAlpha(0.45f));
        g.drawVerticalLine(static_cast<int>(xMin), plot.getY(), plot.getBottom());
        g.drawVerticalLine(static_cast<int>(xMax), plot.getY(), plot.getBottom());

        // Range grabber pills at top
        g.setColour(Palette::sonarCyan.withAlpha(0.85f));
        g.fillRoundedRectangle(xMin - 4.0f, plot.getY(), 8.0f, 12.0f, 2.0f);
        g.fillRoundedRectangle(xMax - 4.0f, plot.getY(), 8.0f, 12.0f, 2.0f);

        // 3. Pre & Post FFT Spectrum
        drawSpectrumFill(g, plot, preSpec, Palette::textSecondary.withAlpha(0.14f), Palette::textSecondary.withAlpha(0.28f));
        drawSpectrumFill(g, plot, postSpec, Palette::tailCyan.withAlpha(0.10f), Palette::tailCyan.withAlpha(0.45f));

        // 4. Masking / Collision Spectrum (hanging down from 0 dB)
        drawMaskingOverlay(g, plot);

        // 5. Smart Sonar Curve
        drawSmartCurve(g, plot);

        // 6. Parametric EQ Curve + Combined Curve
        drawEQCurves(g, plot);

        // 7. Band Nodes
        drawBandNodes(g, plot);

        if (frozen)
        {
            g.setColour(Palette::sonarCyan.withAlpha(0.22f));
            g.fillRoundedRectangle(plot.getRight() - 62.0f, plot.getY() + 4.0f, 56.0f, 18.0f, 4.0f);
            g.setColour(Palette::sonarCyan);
            g.setFont(juce::FontOptions(10.0f, juce::Font::bold));
            g.drawText("FROZEN", static_cast<int>(plot.getRight()) - 62, static_cast<int>(plot.getY()) + 4, 56, 18,
                       juce::Justification::centred);
        }

        g.setColour(Palette::borderSubtle);
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        const auto plot = getLocalBounds().toFloat().reduced(8.0f, 10.0f);
        const float minF = processor.apvts.getRawParameterValue("sonar_min_freq")->load();
        const float maxF = processor.apvts.getRawParameterValue("sonar_max_freq")->load();
        const float xMin = freqToX(minF, plot);
        const float xMax = freqToX(maxF, plot);

        // Check range handle grab at top 22px
        if (e.position.y < plot.getY() + 24.0f)
        {
            if (std::abs(e.position.x - xMin) < 12.0f)
            {
                dragTarget = DragTarget::MinFreqHandle;
                return;
            }
            if (std::abs(e.position.x - xMax) < 12.0f)
            {
                dragTarget = DragTarget::MaxFreqHandle;
                return;
            }
        }

        // Hit test active band nodes
        int hitBand = -1;
        float bestDist = 16.0f;
        for (int i = 0; i < numBands; ++i)
        {
            const auto bp = processor.getBandParams(i);
            if (!bp.enabled)
                continue;
            const float bx = freqToX(bp.freq, plot);
            const float by = dbToY(bp.gainDb, plot);
            const float d = e.position.getDistanceFrom({ bx, by });
            if (d < bestDist)
            {
                bestDist = d;
                hitBand = i;
            }
        }

        if (e.mods.isRightButtonDown())
        {
            if (hitBand >= 0)
            {
                if (auto* p = processor.apvts.getParameter(ShantySonarEQProcessor::bandParamID(hitBand, "enabled")))
                    p->setValueNotifyingHost(0.0f);
                repaint();
            }
            return;
        }

        if (hitBand >= 0)
        {
            selectedBand = hitBand;
            if (onBandSelected) onBandSelected(selectedBand);
            dragTarget = DragTarget::BandNode;
            repaint();
            return;
        }

        // Double-click or click on empty space -> activate next free band
        if (e.getNumberOfClicks() >= 1)
        {
            int freeBand = -1;
            for (int i = 0; i < numBands; ++i)
            {
                if (!processor.getBandParams(i).enabled)
                {
                    freeBand = i;
                    break;
                }
            }
            if (freeBand < 0)
                freeBand = selectedBand;

            selectedBand = freeBand;
            if (auto* en = processor.apvts.getParameter(ShantySonarEQProcessor::bandParamID(selectedBand, "enabled")))
                en->setValueNotifyingHost(1.0f);

            setBandFreqGainFromPoint(selectedBand, e.position, plot);
            if (onBandSelected) onBandSelected(selectedBand);
            dragTarget = DragTarget::BandNode;
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        const auto plot = getLocalBounds().toFloat().reduced(8.0f, 10.0f);
        if (dragTarget == DragTarget::MinFreqHandle)
        {
            const float f = juce::jlimit(20.0f, 2000.0f, xToFreq(e.position.x, plot));
            if (auto* p = processor.apvts.getParameter("sonar_min_freq"))
                p->setValueNotifyingHost(p->convertTo0to1(f));
        }
        else if (dragTarget == DragTarget::MaxFreqHandle)
        {
            const float f = juce::jlimit(2000.0f, 20000.0f, xToFreq(e.position.x, plot));
            if (auto* p = processor.apvts.getParameter("sonar_max_freq"))
                p->setValueNotifyingHost(p->convertTo0to1(f));
        }
        else if (dragTarget == DragTarget::BandNode)
        {
            setBandFreqGainFromPoint(selectedBand, e.position, plot);
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        dragTarget = DragTarget::None;
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (auto* p = processor.apvts.getParameter(ShantySonarEQProcessor::bandParamID(selectedBand, "q")))
        {
            const float cur = p->convertFrom0to1(p->getValue());
            const float next = juce::jlimit(0.1f, 18.0f, cur * (1.0f + wheel.deltaY * 0.6f));
            p->setValueNotifyingHost(p->convertTo0to1(next));
        }
    }

private:
    enum class DragTarget { None, BandNode, MinFreqHandle, MaxFreqHandle };
    DragTarget dragTarget = DragTarget::None;

    ShantySonarEQProcessor& processor;
    int selectedBand = 0;
    bool frozen = false;

    std::array<float, SonarEngineDSP::numBins> preSpec {};
    std::array<float, SonarEngineDSP::numBins> postSpec {};
    std::array<float, SonarEngineDSP::numBins> smartCurve {};
    std::array<float, SonarEngineDSP::numBins> maskingCurve {};

    static float freqToX(float freqHz, const juce::Rectangle<float>& plot) noexcept
    {
        const float t = std::log10(juce::jlimit(20.0f, 20000.0f, freqHz) / 20.0f) / 3.0f;
        return plot.getX() + t * plot.getWidth();
    }

    static float xToFreq(float x, const juce::Rectangle<float>& plot) noexcept
    {
        const float t = juce::jlimit(0.0f, 1.0f, (x - plot.getX()) / juce::jmax(1.0f, plot.getWidth()));
        return 20.0f * std::pow(1000.0f, t);
    }

    static float dbToY(float db, const juce::Rectangle<float>& plot) noexcept
    {
        const float norm = (24.0f - juce::jlimit(-24.0f, 24.0f, db)) / 48.0f;
        return plot.getY() + norm * plot.getHeight();
    }

    static float yToDb(float y, const juce::Rectangle<float>& plot) noexcept
    {
        const float norm = juce::jlimit(0.0f, 1.0f, (y - plot.getY()) / juce::jmax(1.0f, plot.getHeight()));
        return 24.0f - norm * 48.0f;
    }

    static float specDbToY(float db, const juce::Rectangle<float>& plot) noexcept
    {
        const float norm = (0.0f - juce::jlimit(-90.0f, 0.0f, db)) / 90.0f;
        return plot.getY() + norm * plot.getHeight();
    }

    void setBandFreqGainFromPoint(int idx, juce::Point<float> pt, const juce::Rectangle<float>& plot)
    {
        const float freq = xToFreq(pt.x, plot);
        const float gain = yToDb(pt.y, plot);
        if (auto* fp = processor.apvts.getParameter(ShantySonarEQProcessor::bandParamID(idx, "freq")))
            fp->setValueNotifyingHost(fp->convertTo0to1(freq));
        if (auto* gp = processor.apvts.getParameter(ShantySonarEQProcessor::bandParamID(idx, "gain")))
            gp->setValueNotifyingHost(gp->convertTo0to1(gain));
    }

    void drawGrid(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        static constexpr float freqs[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        static const char* labels[] = { "30", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };

        g.setFont(10.0f);
        for (int i = 0; i < 10; ++i)
        {
            const float x = freqToX(freqs[i], plot);
            g.setColour(Palette::borderSubtle.withAlpha(0.55f));
            g.drawVerticalLine(static_cast<int>(x), plot.getY(), plot.getBottom());
            g.setColour(Palette::textSecondary.withAlpha(0.75f));
            g.drawText(labels[i], static_cast<int>(x) - 16, static_cast<int>(plot.getBottom()) - 14, 32, 12,
                       juce::Justification::centred);
        }

        for (int db = -18; db <= 18; db += 6)
        {
            const float y = dbToY(static_cast<float>(db), plot);
            g.setColour(db == 0 ? Palette::borderSubtle.brighter(0.35f) : Palette::borderSubtle.withAlpha(0.45f));
            g.drawHorizontalLine(static_cast<int>(y), plot.getX(), plot.getRight());
            if (db != 0)
            {
                g.setColour(Palette::textSecondary.withAlpha(0.65f));
                g.drawText((db > 0 ? "+" : "") + juce::String(db),
                           static_cast<int>(plot.getX()) + 2, static_cast<int>(y) - 12, 28, 12,
                           juce::Justification::left);
            }
        }
    }

    void drawSpectrumFill(juce::Graphics& g, const juce::Rectangle<float>& plot,
                          const std::array<float, SonarEngineDSP::numBins>& spec,
                          juce::Colour fillCol, juce::Colour strokeCol) const
    {
        juce::Path p;
        p.startNewSubPath(plot.getX(), plot.getBottom());
        for (int b = 0; b < SonarEngineDSP::numBins; ++b)
        {
            const float t = static_cast<float>(b) / static_cast<float>(SonarEngineDSP::numBins - 1);
            const float x = plot.getX() + t * plot.getWidth();
            const float y = specDbToY(spec[b], plot);
            p.lineTo(x, y);
        }
        p.lineTo(plot.getRight(), plot.getBottom());
        p.closeSubPath();

        g.setColour(fillCol);
        g.fillPath(p);

        juce::Path line;
        for (int b = 0; b < SonarEngineDSP::numBins; ++b)
        {
            const float t = static_cast<float>(b) / static_cast<float>(SonarEngineDSP::numBins - 1);
            const float x = plot.getX() + t * plot.getWidth();
            const float y = specDbToY(spec[b], plot);
            if (b == 0) line.startNewSubPath(x, y);
            else line.lineTo(x, y);
        }
        g.setColour(strokeCol);
        g.strokePath(line, juce::PathStrokeType(1.2f));
    }

    void drawMaskingOverlay(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        float maxMask = 0.0f;
        for (float v : maskingCurve) maxMask = juce::jmax(maxMask, v);
        if (maxMask < 0.08f) return;

        const float y0 = dbToY(0.0f, plot);
        juce::Path p;
        p.startNewSubPath(plot.getX(), y0);
        for (int b = 0; b < SonarEngineDSP::numBins; ++b)
        {
            const float t = static_cast<float>(b) / static_cast<float>(SonarEngineDSP::numBins - 1);
            const float x = plot.getX() + t * plot.getWidth();
            const float y = dbToY(-maskingCurve[b], plot);
            p.lineTo(x, y);
        }
        p.lineTo(plot.getRight(), y0);
        p.closeSubPath();

        g.setColour(Palette::teethRed.withAlpha(0.24f));
        g.fillPath(p);
    }

    void drawSmartCurve(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        const bool isSoothe = (processor.apvts.getRawParameterValue("engine_mode")->load() > 0.5f);
        const auto curveCol = isSoothe ? Palette::satAmber : Palette::sonarCyan;

        const float y0 = dbToY(0.0f, plot);
        juce::Path fill;
        juce::Path line;
        fill.startNewSubPath(plot.getX(), y0);

        for (int b = 0; b < SonarEngineDSP::numBins; ++b)
        {
            const float t = static_cast<float>(b) / static_cast<float>(SonarEngineDSP::numBins - 1);
            const float x = plot.getX() + t * plot.getWidth();
            const float y = dbToY(smartCurve[b], plot);
            fill.lineTo(x, y);
            if (b == 0) line.startNewSubPath(x, y);
            else line.lineTo(x, y);
        }
        fill.lineTo(plot.getRight(), y0);
        fill.closeSubPath();

        g.setColour(curveCol.withAlpha(isSoothe ? 0.20f : 0.16f));
        g.fillPath(fill);
        g.setColour(curveCol);
        g.strokePath(line, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawEQCurves(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        const double sr = processor.getCurrentSampleRate();
        std::array<BandParams, numBands> bps;
        bool anyActive = false;
        for (int i = 0; i < numBands; ++i)
        {
            bps[i] = processor.getBandParams(i);
            if (bps[i].enabled) anyActive = true;
        }
        if (!anyActive) return;

        constexpr int steps = 160;
        juce::Path totalPath;
        for (int s = 0; s <= steps; ++s)
        {
            const float t = static_cast<float>(s) / static_cast<float>(steps);
            const float freq = 20.0f * std::pow(1000.0f, t);
            double sumDb = 0.0;
            for (int i = 0; i < numBands; ++i)
                sumDb += ParametricBandProcessor::getMagnitudeDB(bps[i], freq, sr);

            // Add smart curve for total response
            const float bPos = t * static_cast<float>(SonarEngineDSP::numBins - 1);
            const int b0 = static_cast<int>(bPos);
            const int b1 = juce::jmin(SonarEngineDSP::numBins - 1, b0 + 1);
            const float frac = bPos - static_cast<float>(b0);
            const float sDb = smartCurve[b0] * (1.0f - frac) + smartCurve[b1] * frac;

            const float x = plot.getX() + t * plot.getWidth();
            const float y = dbToY(static_cast<float>(sumDb) + sDb, plot);
            if (s == 0) totalPath.startNewSubPath(x, y);
            else totalPath.lineTo(x, y);
        }

        g.setColour(Palette::textPrimary.withAlpha(0.88f));
        g.strokePath(totalPath, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void drawBandNodes(juce::Graphics& g, const juce::Rectangle<float>& plot) const
    {
        for (int i = 0; i < numBands; ++i)
        {
            const auto bp = processor.getBandParams(i);
            if (!bp.enabled)
                continue;

            const float bx = freqToX(bp.freq, plot);
            const float by = dbToY(bp.gainDb, plot);
            const auto col = Palette::bandColour(i);
            const bool isSel = (i == selectedBand);

            // Live Dynamic EQ stem
            if (bp.dynEnabled)
            {
                const float dynOffset = processor.getBandLiveDynGain(i);
                const float dynY = dbToY(bp.gainDb + dynOffset, plot);
                g.setColour(Palette::satAmber.withAlpha(0.85f));
                g.drawLine(bx, by, bx, dynY, 3.0f);
                g.fillEllipse(bx - 3.5f, dynY - 3.5f, 7.0f, 7.0f);
            }

            if (isSel)
            {
                g.setColour(col.withAlpha(0.28f));
                g.fillEllipse(bx - 14.0f, by - 14.0f, 28.0f, 28.0f);
            }

            g.setColour(Palette::bgDark);
            g.fillEllipse(bx - 9.0f, by - 9.0f, 18.0f, 18.0f);
            g.setColour(col);
            g.drawEllipse(bx - 9.0f, by - 9.0f, 18.0f, 18.0f, isSel ? 2.2f : 1.5f);

            g.setColour(Palette::textPrimary);
            g.setFont(10.0f);
            g.drawText(juce::String(i + 1), static_cast<int>(bx) - 9, static_cast<int>(by) - 9, 18, 18,
                       juce::Justification::centred);
        }
    }
};

} // namespace ShantySonar
