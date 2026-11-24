#pragma once

#include <JuceHeader.h>
#include "BinaryData.h"
#include "CustomLookAndFeel.h"
#include <array>
#include <queue>
#include <vector>
#include <cmath>

namespace ShantySonar
{

class ShantyMascotComponent : public juce::Component
{
public:
    ShantyMascotComponent()
    {
        loadSittingParts();
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
    }

    void updateAnimation(float audioLevel, float sonarActivity, float unmaskActivity, bool learning, bool learned,
                         bool sootheMode = false, float sootheReduction = 0.0f,
                         bool bypassed = false, bool deltaMode = false, float /*freqNorm*/ = 0.5f)
    {
        currentAudio  = juce::jlimit(0.0f, 1.0f, audioLevel);
        currentSonar  = juce::jlimit(0.0f, 1.0f, sonarActivity);
        currentUnmask = juce::jlimit(0.0f, 1.0f, unmaskActivity);
        isLearningNow = learning && !sootheMode && !bypassed;
        hasLearnedNow = learned && !sootheMode;
        isSootheMode  = sootheMode;
        currentSoothe = juce::jlimit(0.0f, 1.0f, sootheReduction);
        isBypassed    = bypassed;
        isDeltaMode   = deltaMode && sootheMode && !bypassed;
        animTimeSec += 1.0f / 60.0f;

        sleepTwitchTimer -= 1.0f / 60.0f;
        if (sleepTwitchTimer <= 0.0f)
        {
            sleepTwitchProgress = 1.0f;
            sleepTwitchTimer = 3.2f + 1.8f * std::abs(std::sin(animTimeSec * 0.7f));
        }
        if (sleepTwitchProgress > 0.0f)
            sleepTwitchProgress = juce::jmax(0.0f, sleepTwitchProgress - (1.0f / 60.0f) * 3.5f);

        lickTimer -= 1.0f / 60.0f;
        if (lickTimer <= 0.0f)
        {
            lickProgress = 1.0f;
            lickTimer = 2.8f + 1.6f * std::abs(std::cos(animTimeSec * 0.7f));
        }
        if (lickProgress > 0.0f)
            lickProgress = juce::jmax(0.0f, lickProgress - (1.0f / 60.0f) * 1.75f);

        if (isBeingPetted)
            petHappyTimer = 1.6f;
        else if (petHappyTimer > 0.0f)
            petHappyTimer = juce::jmax(0.0f, petHappyTimer - (1.0f / 60.0f));

        for (auto& h : hearts)
        {
            if (h.life > 0.0f)
            {
                h.life -= (1.0f / 60.0f) * 0.9f;
                h.y -= 0.7f;
                h.x += std::sin(animTimeSec * 5.0f + h.phase) * 0.35f;
            }
        }

        if (isBeingPetted)
        {
            heartSpawnTimer -= 1.0f / 60.0f;
            if (heartSpawnTimer <= 0.0f)
            {
                spawnHeart();
                heartSpawnTimer = 0.22f;
            }
        }

        repaint();
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.mods.isLeftButtonDown())
        {
            isBeingPetted = true;
            petHappyTimer = 1.6f;
            lastPetPos = e.position;
            spawnHeart();
            repaint();
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (isBeingPetted)
        {
            if (e.position.getDistanceFrom(lastPetPos) > 18.0f)
            {
                lastPetPos = e.position;
                spawnHeart();
            }
            petHappyTimer = 1.6f;
        }
    }

    void mouseUp(const juce::MouseEvent&) override
    {
        isBeingPetted = false;
    }

    void paint(juce::Graphics& g) override
    {
        const auto bounds = getLocalBounds().toFloat();
        if (!bodyFrame.isValid())
            return;

        g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);

        const bool isHappy = (petHappyTimer > 0.01f);

        // Curled-up sleeping Shanti when bypassed (unless petted)
        if (isBypassed && !isHappy && sleepingFrame.isValid())
        {
            const auto drawArea = bounds.reduced(8.0f, 10.0f);
            const float imgW = static_cast<float>(sleepingFrame.getWidth());
            const float imgH = static_cast<float>(sleepingFrame.getHeight());
            const float scale = juce::jmin(1.0f, juce::jmin(drawArea.getWidth() / imgW, drawArea.getHeight() / imgH));
            const float destW = imgW * scale;
            const float destH = imgH * scale;
            const juce::Rectangle<float> destRect(drawArea.getCentreX() - destW * 0.5f,
                                                  drawArea.getBottom() - destH + 6.0f,
                                                  destW, destH);

            {
                const float bgCx = destRect.getCentreX();
                const float bgCy = destRect.getCentreY() + destRect.getHeight() * 0.08f;
                const float bgR  = destRect.getWidth() * 0.50f;
                juce::ColourGradient stageGlow(juce::Colour(0xFF252A34).withAlpha(0.45f), bgCx, bgCy,
                                               juce::Colours::transparentBlack, bgCx + bgR, bgCy, true);
                g.setGradientFill(stageGlow);
                g.fillEllipse(bgCx - bgR, bgCy - bgR, bgR * 2.0f, bgR * 2.0f);
            }

            {
                const float shY = destRect.getBottom() - destRect.getHeight() * 0.22f;
                const auto shadowRect = juce::Rectangle<float>(destRect.getX() + destRect.getWidth() * 0.08f,
                                                               shY,
                                                               destRect.getWidth() * 0.84f,
                                                               destRect.getHeight() * 0.12f);
                g.setColour(juce::Colours::black.withAlpha(0.50f));
                g.fillEllipse(shadowRect);
            }

            const float breath = std::sin(animTimeSec * 1.55f);
            const float twitch = std::sin(sleepTwitchProgress * juce::MathConstants<float>::pi * 4.0f) * sleepTwitchProgress;
            const float scaleY = 1.0f + 0.018f * breath;
            const float scaleX = 1.0f - 0.008f * breath;
            const float anchorX = destRect.getCentreX();
            const float anchorY = destRect.getBottom() - destRect.getHeight() * 0.18f;

            g.setOpacity(1.0f);
            {
                juce::Graphics::ScopedSaveState ss(g);
                g.addTransform(juce::AffineTransform::scale(scaleX, scaleY, anchorX, anchorY)
                                   .rotated(twitch * 0.025f, destRect.getX() + destRect.getWidth() * 0.72f,
                                            destRect.getY() + destRect.getHeight() * 0.45f));
                g.drawImage(sleepingFrame, destRect);
            }

            for (int k = 0; k < 3; ++k)
            {
                const float cycle = std::fmod(animTimeSec * 0.38f + static_cast<float>(k) * 0.33f, 1.0f);
                const float alpha = std::sin(cycle * juce::MathConstants<float>::pi) * 0.65f;
                const float zx = destRect.getX() + destRect.getWidth() * (0.76f + 0.05f * cycle + 0.02f * std::sin(cycle * 6.0f));
                const float zy = destRect.getY() + destRect.getHeight() * (0.28f - 0.22f * cycle);
                g.setColour(Palette::tailCyan.withAlpha(alpha));
                g.setFont(juce::FontOptions(10.0f + 3.0f * cycle, juce::Font::bold));
                g.drawText(k == 0 ? "z" : "Z", static_cast<int>(zx), static_cast<int>(zy), 14, 14, juce::Justification::centred);
            }
            return;
        }

        const float breathY = std::sin(animTimeSec * 2.2f) * 1.0f;
        const auto drawArea = bounds.reduced(4.0f, 4.0f).translated(0.0f, breathY - 2.0f);
        const float imgW = static_cast<float>(bodyFrame.getWidth());
        const float imgH = static_cast<float>(bodyFrame.getHeight());
        const float scale = juce::jmin(1.0f, juce::jmin(drawArea.getWidth() / imgW, drawArea.getHeight() / imgH));
        const float destW = imgW * scale;
        const float destH = imgH * scale;
        const juce::Rectangle<float> destRect(drawArea.getCentreX() - destW * 0.5f,
                                              drawArea.getCentreY() - destH * 0.5f,
                                              destW, destH);

        // Stage glow reacting to Sonar / Soothe & Audio
        {
            const float bgCx = destRect.getCentreX();
            const float bgCy = destRect.getCentreY() + destRect.getHeight() * 0.04f;
            const float bgR  = destRect.getWidth() * 0.58f;
            const auto baseCol = isSootheMode
                ? Palette::satAmber.interpolatedWith(Palette::tailCyan, 0.25f)
                : (isLearningNow
                    ? Palette::sonarCyan
                    : (hasLearnedNow ? Palette::emeraldOk.interpolatedWith(Palette::sonarCyan, 0.5f) : Palette::tailCyan));
            const float activity = isSootheMode ? juce::jmax(currentAudio, currentSoothe) : juce::jmax(currentAudio, currentSonar);
            const auto glowCol = baseCol.interpolatedWith(Palette::teethRed, currentUnmask * 0.45f)
                                        .withAlpha(0.14f + 0.26f * activity);
            juce::ColourGradient stageGlow(glowCol, bgCx, bgCy,
                                           juce::Colours::transparentBlack, bgCx + bgR, bgCy, true);
            g.setGradientFill(stageGlow);
            g.fillEllipse(bgCx - bgR, bgCy - bgR, bgR * 2.0f, bgR * 2.0f);
        }

        // Sonar radar rings when Learning or Soothe Delta
        if (isLearningNow || isDeltaMode)
        {
            const float earCx = destRect.getX() + destRect.getWidth() * 0.68f;
            const float earCy = destRect.getY() + destRect.getHeight() * 0.16f;
            const auto ringCol = isDeltaMode ? Palette::satAmber : Palette::sonarCyan;
            const float speed = isDeltaMode ? 2.1f : 1.4f;
            for (int rIdx = 0; rIdx < 3; ++rIdx)
            {
                const float phase = std::fmod(animTimeSec * speed + static_cast<float>(rIdx) * 0.33f, 1.0f);
                const float rad = 10.0f + phase * destRect.getWidth() * 0.55f;
                const float alpha = (1.0f - phase) * 0.65f;
                g.setColour(ringCol.withAlpha(alpha));
                g.drawEllipse(earCx - rad, earCy - rad, rad * 2.0f, rad * 2.0f, 1.6f);
            }
        }

        // Ground shadow
        {
            const float shY = destRect.getBottom() - destRect.getHeight() * 0.14f;
            const auto shadowRect = juce::Rectangle<float>(destRect.getCentreX() - destRect.getWidth() * 0.25f,
                                                           shY,
                                                           destRect.getWidth() * 0.52f,
                                                           destRect.getHeight() * 0.06f);
            g.setColour(juce::Colours::black.withAlpha(0.50f));
            g.fillEllipse(shadowRect);
        }

        g.setOpacity(1.0f);

        // 1. Tail wags smoothly like a pendulum
        if (tailFrame.isValid())
        {
            float wagAngle = 0.0f;
            if (isHappy)
                wagAngle = std::sin(animTimeSec * 18.0f) * 0.24f;
            else
            {
                const float energy = isSootheMode ? juce::jmax(currentAudio, currentSoothe) : juce::jmax(currentAudio, currentSonar);
                wagAngle = std::sin(animTimeSec * (2.0f + energy * 6.5f)) * (0.03f + 0.14f * energy);
            }

            const float pivotX = destRect.getX() + 0.325f * destRect.getWidth();
            const float pivotY = destRect.getY() + 0.835f * destRect.getHeight();

            juce::Graphics::ScopedSaveState ss(g);
            g.addTransform(juce::AffineTransform::rotation(wagAngle, pivotX, pivotY));
            g.drawImage(tailFrame, destRect);
        }

        // Listening head/body tilt during Learn, Delta, or Soothe
        float tiltAngle = 0.0f;
        if (isLearningNow)
            tiltAngle = std::sin(animTimeSec * 3.2f) * 0.045f;
        else if (isDeltaMode)
            tiltAngle = std::sin(animTimeSec * 5.0f) * 0.03f;
        else if (isSootheMode)
            tiltAngle = std::sin(animTimeSec * 1.8f) * 0.015f * currentSoothe;

        const float neckX = destRect.getX() + 0.62f * destRect.getWidth();
        const float neckY = destRect.getY() + 0.45f * destRect.getHeight();
        const auto bodyTransform = juce::AffineTransform::rotation(tiltAngle, neckX, neckY);

        juce::Graphics::ScopedSaveState bodyScope(g);
        g.addTransform(bodyTransform);

        // 2. Seated Shanti body (new crisp sprite from Shanty Pawmper)
        g.drawImage(bodyFrame, destRect);

        // 3. Happy eyes only when petted
        if (isHappy)
        {
            const float happyAlpha = juce::jmin(1.0f, petHappyTimer * 1.5f);
            auto drawHappyEye = [&](float cxNorm, float cyNorm, float rx) {
                const float cx = destRect.getX() + cxNorm * destRect.getWidth();
                const float cy = destRect.getY() + cyNorm * destRect.getHeight();
                g.setColour(juce::Colour(0xFFF6F8FA).withAlpha(happyAlpha));
                g.fillEllipse(cx - rx * 1.35f, cy - rx * 1.15f, rx * 2.7f, rx * 2.3f);

                juce::Path arc;
                arc.startNewSubPath(cx - rx * 1.1f, cy + rx * 0.25f);
                arc.quadraticTo(cx, cy - rx * 0.95f, cx + rx * 1.1f, cy + rx * 0.25f);
                g.setColour(juce::Colour(0xFF22252A).withAlpha(happyAlpha));
                g.strokePath(arc, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            };

            const float eyeR = destRect.getWidth() * 0.014f;
            drawHappyEye(0.663f, 0.193f, eyeR);
            drawHappyEye(0.742f, 0.184f, eyeR * 0.85f);
        }

        // 4. Floating hearts
        for (const auto& h : hearts)
        {
            if (h.life > 0.01f)
            {
                const float alpha = juce::jmin(1.0f, h.life * 1.4f);
                const float s = 5.5f;
                juce::Path heart;
                heart.startNewSubPath(h.x, h.y + s * 0.35f);
                heart.cubicTo(h.x - s, h.y - s * 0.4f, h.x - s * 0.5f, h.y - s * 1.1f, h.x, h.y - s * 0.45f);
                heart.cubicTo(h.x + s * 0.5f, h.y - s * 1.1f, h.x + s, h.y - s * 0.4f, h.x, h.y + s * 0.35f);
                heart.closeSubPath();

                g.setColour(Palette::teethRed.withAlpha(alpha));
                g.fillPath(heart);
            }
        }

        // 5. Collar glow in sync with Sonar / Soothe / Unmasking
        const float collarIntensity = juce::jlimit(0.0f, 1.0f,
            currentSonar * 0.65f + currentUnmask * 0.7f + currentSoothe * 0.65f + (isLearningNow ? 0.5f : 0.0f));
        if (collarIntensity > 0.02f && collarGlowImage.isValid())
        {
            const float cx = destRect.getX() + collarNormCenter.x * destRect.getWidth();
            const float cy = destRect.getY() + collarNormCenter.y * destRect.getHeight();
            const float haloRadius = destRect.getWidth() * (0.14f + 0.09f * collarIntensity);

            const auto baseHalo = isSootheMode ? Palette::satAmber : Palette::sonarCyan;
            const auto haloCol = baseHalo.interpolatedWith(Palette::teethRed, currentUnmask);
            juce::ColourGradient halo(haloCol.withAlpha(0.72f * collarIntensity), cx, cy,
                                      juce::Colours::transparentBlack, cx + haloRadius, cy, true);
            g.setGradientFill(halo);
            g.fillEllipse(cx - haloRadius, cy - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);

            g.setOpacity(collarIntensity);
            g.drawImage(collarGlowImage, destRect);
            g.setOpacity(1.0f);
        }
    }

private:
    struct FloatingHeart
    {
        float x = 0.0f, y = 0.0f, life = 0.0f, phase = 0.0f;
    };

    juce::Image bodyFrame;
    juce::Image tailFrame;
    juce::Image sleepingFrame;
    juce::Image collarGlowImage;
    juce::Point<float> collarNormCenter { 0.63f, 0.35f };

    float currentAudio  = 0.0f;
    float currentSonar  = 0.0f;
    float currentUnmask = 0.0f;
    float currentSoothe = 0.0f;
    bool isLearningNow  = false;
    bool hasLearnedNow  = false;
    bool isSootheMode   = false;
    bool isBypassed     = false;
    bool isDeltaMode    = false;
    float animTimeSec   = 0.0f;

    float sleepTwitchTimer = 2.0f;
    float sleepTwitchProgress = 0.0f;
    float lickTimer = 1.2f;
    float lickProgress = 0.0f;
    bool isBeingPetted = false;
    float petHappyTimer = 0.0f;
    float heartSpawnTimer = 0.0f;
    juce::Point<float> lastPetPos;
    std::array<FloatingHeart, 8> hearts {};
    int nextHeartIdx = 0;

    void spawnHeart()
    {
        auto& h = hearts[static_cast<size_t>(nextHeartIdx)];
        nextHeartIdx = (nextHeartIdx + 1) % static_cast<int>(hearts.size());
        h.x = static_cast<float>(getWidth()) * 0.62f + std::sin(animTimeSec * 7.3f + nextHeartIdx) * 14.0f;
        h.y = static_cast<float>(getHeight()) * 0.22f;
        h.life = 1.0f;
        h.phase = animTimeSec;
    }

    static juce::Image stripOuterWhiteBackground(const juce::Image& src,
                                                 std::initializer_list<juce::Point<float>> extraSeeds = {})
    {
        const int w = src.getWidth();
        const int h = src.getHeight();
        juce::Image out(juce::Image::ARGB, w, h, true);

        juce::Image::BitmapData srcData(src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData dstData(out, juce::Image::BitmapData::writeOnly);

        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                dstData.setPixelColour(x, y, srcData.getPixelColour(x, y));

        auto isNearWhite = [&](int x, int y) -> bool {
            const auto c = srcData.getPixelColour(x, y);
            const int minC = juce::jmin(static_cast<int>(c.getRed()),
                                        juce::jmin(static_cast<int>(c.getGreen()),
                                                   static_cast<int>(c.getBlue())));
            return minC >= 220;
        };

        std::vector<uint8_t> visited(static_cast<size_t>(w * h), 0);
        std::queue<std::pair<int, int>> q;

        for (int x = 0; x < w; ++x)
        {
            if (isNearWhite(x, 0)) { visited[static_cast<size_t>(x)] = 1; q.push({ x, 0 }); }
            if (isNearWhite(x, h - 1)) { visited[static_cast<size_t>((h - 1) * w + x)] = 1; q.push({ x, h - 1 }); }
        }
        for (int y = 1; y < h - 1; ++y)
        {
            if (isNearWhite(0, y)) { visited[static_cast<size_t>(y * w)] = 1; q.push({ 0, y }); }
            if (isNearWhite(w - 1, y)) { visited[static_cast<size_t>(y * w + w - 1)] = 1; q.push({ w - 1, y }); }
        }

        for (const auto& seed : extraSeeds)
        {
            const int sx = juce::jlimit(0, w - 1, static_cast<int>(seed.x * static_cast<float>(w)));
            const int sy = juce::jlimit(0, h - 1, static_cast<int>(seed.y * static_cast<float>(h)));
            const size_t idx = static_cast<size_t>(sy * w + sx);
            if (!visited[idx] && isNearWhite(sx, sy))
            {
                visited[idx] = 1;
                q.push({ sx, sy });
            }
        }

        const int dx[4] = { 1, -1, 0, 0 };
        const int dy[4] = { 0, 0, 1, -1 };

        while (!q.empty())
        {
            auto [cx, cy] = q.front();
            q.pop();
            dstData.setPixelColour(cx, cy, juce::Colours::transparentBlack);

            for (int d = 0; d < 4; ++d)
            {
                const int nx = cx + dx[d];
                const int ny = cy + dy[d];
                if (nx >= 0 && nx < w && ny >= 0 && ny < h)
                {
                    const size_t idx = static_cast<size_t>(ny * w + nx);
                    if (!visited[idx] && isNearWhite(nx, ny))
                    {
                        visited[idx] = 1;
                        q.push({ nx, ny });
                    }
                }
            }
        }

        // Smooth anti-aliased edge feathering + white-halo removal (2 passes)
        for (int pass = 0; pass < 2; ++pass)
        {
            std::vector<juce::Colour> updates;
            std::vector<std::pair<int, int>> coords;

            for (int y = 1; y < h - 1; ++y)
            {
                for (int x = 1; x < w - 1; ++x)
                {
                    const auto c = dstData.getPixelColour(x, y);
                    if (c.getAlpha() == 0)
                        continue;

                    int transparentNeighbors = 0;
                    for (int d = 0; d < 4; ++d)
                    {
                        if (dstData.getPixelColour(x + dx[d], y + dy[d]).getAlpha() < 64)
                            ++transparentNeighbors;
                    }

                    if (transparentNeighbors > 0)
                    {
                        const float luma = (static_cast<float>(c.getRed()) + c.getGreen() + c.getBlue()) / 3.0f;
                        const float edgeAlpha = juce::jlimit(0.15f, 1.0f, (235.0f - luma) / 145.0f);
                        const float newAlpha = (static_cast<float>(c.getAlpha()) / 255.0f) * edgeAlpha;
                        const float rgbScale = (luma > 130.0f) ? 0.35f : 0.85f;
                        const auto nc = juce::Colour::fromRGBA(
                            static_cast<uint8_t>(c.getRed() * rgbScale),
                            static_cast<uint8_t>(c.getGreen() * rgbScale),
                            static_cast<uint8_t>(c.getBlue() * rgbScale),
                            static_cast<uint8_t>(juce::jlimit(0.0f, 255.0f, newAlpha * 255.0f)));
                        updates.push_back(nc);
                        coords.push_back({ x, y });
                    }
                }
            }

            for (size_t i = 0; i < coords.size(); ++i)
                dstData.setPixelColour(coords[i].first, coords[i].second, updates[i]);
        }

        return out;
    }

    static juce::Colour brightenFurPixel(juce::Colour c)
    {
        if (c.getAlpha() < 245)
            return c;

        const int r = c.getRed();
        const int g = c.getGreen();
        const int b = c.getBlue();
        const int maxC = juce::jmax(r, juce::jmax(g, b));
        const int minC = juce::jmin(r, juce::jmin(g, b));
        const int sat = maxC - minC;

        if (sat < 38 && maxC >= 135)
        {
            const float norm = static_cast<float>(maxC - 135) / 120.0f;
            const float boosted = std::pow(juce::jlimit(0.0f, 1.0f, norm), 0.55f);
            const uint8_t nr = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(185 + boosted * 70.0f)));
            const uint8_t ng = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(188 + boosted * 67.0f)));
            const uint8_t nb = static_cast<uint8_t>(juce::jlimit(0, 255, static_cast<int>(194 + boosted * 61.0f)));
            return juce::Colour::fromRGBA(nr, ng, nb, c.getAlpha());
        }
        return c;
    }

    void loadSittingParts()
    {
        auto rawSit = juce::ImageCache::getFromMemory(BinaryData::shanti_sitting_png,
                                                      BinaryData::shanti_sitting_pngSize);
        if (!rawSit.isValid())
            return;

        auto fullSit = stripOuterWhiteBackground(rawSit, { { 0.595f, 0.780f } });

        const int w = fullSit.getWidth();
        const int h = fullSit.getHeight();

        bodyFrame = juce::Image(juce::Image::ARGB, w, h, true);
        tailFrame = juce::Image(juce::Image::ARGB, w, h, true);
        collarGlowImage = juce::Image(juce::Image::ARGB, w, h, true);

        juce::Image::BitmapData srcData(fullSit, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData bodyData(bodyFrame, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData tailData(tailFrame, juce::Image::BitmapData::writeOnly);
        juce::Image::BitmapData glowData(collarGlowImage, juce::Image::BitmapData::writeOnly);

        const int tailCutX = static_cast<int>(w * 0.320f);
        const int tailCutY = static_cast<int>(h * 0.560f);
        const int tailRootMinY = static_cast<int>(h * 0.780f);
        const int tailRootMaxY = static_cast<int>(h * 0.875f);
        const int tailOverlapPx = 14;

        double sumX = 0.0, sumY = 0.0;
        int count = 0;

        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                const auto raw = srcData.getPixelColour(x, y);
                if (raw.getAlpha() == 0)
                    continue;

                const auto c = brightenFurPixel(raw);

                if (x < tailCutX && y > tailCutY)
                {
                    tailData.setPixelColour(x, y, c);
                }
                else
                {
                    bodyData.setPixelColour(x, y, c);
                    if (y >= tailRootMinY && y <= tailRootMaxY && x >= tailCutX && x < tailCutX + tailOverlapPx)
                        tailData.setPixelColour(x, y, c);
                }

                if (raw.getAlpha() > 128)
                {
                    const int r = raw.getRed();
                    const int g = raw.getGreen();
                    const int b = raw.getBlue();
                    if (r >= 120 && r > static_cast<int>(g * 1.55f) && r > static_cast<int>(b * 1.55f))
                    {
                        glowData.setPixelColour(x, y, juce::Colour(0xFFFF424E));
                        sumX += x;
                        sumY += y;
                        ++count;
                    }
                }
            }
        }

        if (count > 0)
            collarNormCenter = { static_cast<float>(sumX / (count * w)),
                                 static_cast<float>(sumY / (count * h)) };

        auto rawSleep = juce::ImageCache::getFromMemory(BinaryData::shanti_sleeping_png,
                                                        BinaryData::shanti_sleeping_pngSize);
        if (rawSleep.isValid())
        {
            auto cleanSleep = stripOuterWhiteBackground(rawSleep);
            const int sw = cleanSleep.getWidth();
            const int sh = cleanSleep.getHeight();
            sleepingFrame = juce::Image(juce::Image::ARGB, sw, sh, true);
            juce::Image::BitmapData sSrc(cleanSleep, juce::Image::BitmapData::readOnly);
            juce::Image::BitmapData sDst(sleepingFrame, juce::Image::BitmapData::writeOnly);
            for (int y = 0; y < sh; ++y)
            {
                for (int x = 0; x < sw; ++x)
                {
                    const auto c = sSrc.getPixelColour(x, y);
                    if (c.getAlpha() > 0)
                        sDst.setPixelColour(x, y, brightenFurPixel(c));
                }
            }
        }
    }
};

} // namespace ShantySonar
