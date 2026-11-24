#pragma once

#include <JuceHeader.h>
#include "CustomLookAndFeel.h"

namespace ShantySonar
{

class ManualOverlayComponent : public juce::Component
{
public:
    ManualOverlayComponent()
    {
        addAndMakeVisible(langBtn);
        langBtn.onClick = [this] {
            isRussian = !isRussian;
            langBtn.setButtonText(isRussian ? "EN" : "RU");
            updateManualText();
        };

        addAndMakeVisible(closeBtn);
        closeBtn.onClick = [this] { setVisible(false); };

        editor.setMultiLine(true);
        editor.setReadOnly(true);
        editor.setScrollbarsShown(true);
        editor.setCaretVisible(false);
        editor.setColour(juce::TextEditor::backgroundColourId, Palette::bgCanvas);
        editor.setColour(juce::TextEditor::outlineColourId, Palette::borderSubtle);
        editor.setColour(juce::TextEditor::textColourId, Palette::textPrimary);
        editor.setFont(juce::Font(13.5f));
        addAndMakeVisible(editor);

        updateManualText();
    }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colours::black.withAlpha(0.72f));

        const auto card = getCardBounds().toFloat();
        g.setColour(Palette::bgPanel);
        g.fillRoundedRectangle(card, 10.0f);
        g.setColour(Palette::sonarCyan.withAlpha(0.65f));
        g.drawRoundedRectangle(card, 10.0f, 1.5f);

        g.setColour(Palette::textPrimary);
        g.setFont(juce::Font(16.0f, juce::Font::bold));
        g.drawText(isRussian ? "SHANTY SONAR EQ — РУКОВОДСТВО" : "SHANTY SONAR EQ — MANUAL",
                   static_cast<int>(card.getX()) + 18, static_cast<int>(card.getY()) + 12,
                   static_cast<int>(card.getWidth()) - 140, 26, juce::Justification::left);
    }

    void resized() override
    {
        const auto card = getCardBounds();
        closeBtn.setBounds(card.getRight() - 40, card.getY() + 10, 28, 28);
        langBtn.setBounds(card.getRight() - 84, card.getY() + 10, 38, 28);
        editor.setBounds(card.reduced(14, 14).withTrimmedTop(34));
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (!getCardBounds().contains(e.getPosition()))
            setVisible(false);
    }

private:
    juce::Rectangle<int> getCardBounds() const
    {
        return getLocalBounds().reduced(juce::jmax(40, getWidth() / 10),
                                          juce::jmax(30, getHeight() / 12));
    }

    void updateManualText()
    {
        if (isRussian)
        {
            editor.setText(
                "SHANTY SONAR EQ (Adamsky Music)\n"
                "Двухрежимный адаптивный эквалайзер: SONAR (аналог smart:EQ 4) + SOOTHE (аналог soothe2/3).\n\n"
                "============================================================\n"
                "1. РЕЖИМ SONAR (Адаптивная спектральная коррекция)\n"
                "============================================================\n"
                "• Профиль (Profile): выберите тип сигнала (Universal, Vocals, Drums, Bass, Guitar, Keys/Synth, Speech, Master).\n"
                "• LEARN: включите воспроизведение трека и нажмите LEARN. Шанти просканирует спектр (~1.6 сек) и построит корректирующую кривую. CLR сбрасывает кривую.\n"
                "• IMPACT: глубина применения умной кривой (0–100%).\n"
                "• SMOOTH: сглаживание спектральной кривой (от детальной коррекции резонансов до плавного тонального баланса).\n"
                "• ADAPTIVE: динамическая подстройка спектра в реальном времени под текущий сигнал.\n"
                "• Диапазон (Smart Range): перетаскивайте две голубые шторки сверху графика (Min/Max Freq), чтобы ограничить зону работы движка.\n\n"
                "============================================================\n"
                "2. МЕЖКАНАЛЬНАЯ РАСЧИСТКА (UNMASKING)\n"
                "============================================================\n"
                "• Повесьте Shanty Sonar EQ на несколько дорожек и выберите одну группу (Group A–D).\n"
                "• Задайте приоритет: Foreground (вокал/лид), Midground (гитары/синты), Background (пэды/инструментал).\n"
                "• Поверните UNMASK на более низком приоритете — плагин автоматически прорежет конфликтующие частоты (подсвечиваются красным на графике). Также работает через внешний Sidechain.\n\n"
                "============================================================\n"
                "3. РЕЖИМ SOOTHE (Подавитель резонансов)\n"
                "============================================================\n"
                "• Переключитесь кнопкой SOOTHE в шапке плагина.\n"
                "• DEPTH: глубина подавления острых резонансов (до -18 дБ).\n"
                "• SHARP: ширина детектирования — от мягкого разгребания мути (слева) до узкого хирургического вырезания свиста (справа).\n"
                "• SELECT: порог избирательности (подавлять только явные пики или сглаживать весь спектр).\n"
                "• ATTACK / RELEASE: скорость срабатывания и восстановления.\n"
                "• Soft / Hard: мягкое музыкальное или жёсткое хирургическое подавление.\n"
                "• DELTA: прослушивание ТОЛЬКО вырезаемых резонансов.\n\n"
                "============================================================\n"
                "4. 12-ПОЛОСНЫЙ ПАРАМЕТРИЧЕСКИЙ И ДИНАМИЧЕСКИЙ EQ\n"
                "============================================================\n"
                "• Кликните по пустому месту на графике, чтобы добавить полосу.\n"
                "• Перетаскивайте узел для изменения частоты (Freq) и усиления (Gain).\n"
                "• Колёсико мыши над графиком меняет добротность (Q) выбранной полосы.\n"
                "• Правый клик по узлу отключает полосу.\n"
                "• В нижней панели доступны: тип фильтра, крутизна среза (12–48 dB/oct), режим каналов (Stereo / Mid / Side / Left / Right), кнопка SOLO и полноценный Dynamic EQ (Dynamic, Thresh, Range, Attack, Release).\n\n"
                "============================================================\n"
                "5. ОБЩИЕ ФУНКЦИИ И ШАНТИ\n"
                "============================================================\n"
                "• Zero-Latency / Linear Phase: нулевая задержка для записи или линейно-фазовая STFT-обработка для сведения и мастеринга.\n"
                "• A / B: мгновенное сравнение двух состояний настроек.\n"
                "• AUTO GAIN: автоматическая компенсация громкости.\n"
                "• Шанти-маскот: реагирует на сканирование, громкость и подавление резонансов. Погладьте её мышкой!",
                false);
        }
        else
        {
            editor.setText(
                "SHANTY SONAR EQ (Adamsky Music)\n"
                "Dual-engine adaptive equalizer: SONAR (smart:EQ 4 style) + SOOTHE (soothe2/3 style).\n\n"
                "============================================================\n"
                "1. SONAR MODE (Adaptive Spectral Shaping)\n"
                "============================================================\n"
                "• Profile: choose a target instrument profile (Universal, Vocals, Drums, Bass, Guitar, Keys/Synth, Speech, Master).\n"
                "• LEARN: play audio and click LEARN. Shanty scans the spectrum (~1.6s) and builds a custom correction curve. Click CLR to reset.\n"
                "• IMPACT: scales the Smart Curve depth (0–100%).\n"
                "• SMOOTH: smooths the spectral curve from surgical detail to broad tonal balance.\n"
                "• ADAPTIVE: real-time dynamic spectral tracking.\n"
                "• Smart Range: drag the top cyan handles on the graph (Min/Max Freq) to limit where the engine operates.\n\n"
                "============================================================\n"
                "2. CROSS-CHANNEL UNMASKING\n"
                "============================================================\n"
                "• Load Shanty Sonar EQ on multiple tracks and assign them to the same Group (Group A–D).\n"
                "• Set Track Priority: Foreground (vocals/lead), Midground (guitars/keys), Background (pads/bus).\n"
                "• Increase UNMASK on lower-priority tracks to carve space automatically (collision zones appear in red). External Sidechain is also supported.\n\n"
                "============================================================\n"
                "3. SOOTHE MODE (Dynamic Resonance Suppressor)\n"
                "============================================================\n"
                "• Switch to SOOTHE in the top bar.\n"
                "• DEPTH: suppression depth (up to -18 dB on resonant peaks).\n"
                "• SHARP: detection selectivity width — broad de-muddying (low) to laser-surgical whistle removal (high).\n"
                "• SELECT: threshold selectivity (target only prominent spikes vs broader smoothing).\n"
                "• ATTACK / RELEASE: envelope speed.\n"
                "• Soft / Hard: transparent musical knee vs aggressive surgical suppression.\n"
                "• DELTA: audition ONLY the resonances being removed.\n\n"
                "============================================================\n"
                "4. 12-BAND PARAMETRIC & DYNAMIC EQ\n"
                "============================================================\n"
                "• Click empty graph space to add/enable the next band.\n"
                "• Drag nodes for Frequency & Gain; use mouse wheel to adjust Q; right-click a node to disable it.\n"
                "• Bottom inspector: Filter Type, Slope (12–48 dB/oct), Channel Mode (Stereo / Mid / Side / L / R), SOLO audition, and per-band Dynamic EQ (Dynamic, Thresh, Range, Attack, Release).\n\n"
                "============================================================\n"
                "5. GLOBAL CONTROLS & SHANTY\n"
                "============================================================\n"
                "• Zero-Latency / Linear Phase: zero-latency tracking or linear-phase STFT processing.\n"
                "• A / B: compare two parameter states.\n"
                "• AUTO GAIN: automatic RMS loudness compensation.\n"
                "• Shanty Mascot: reacts to learning, level, and resonance suppression. Pet her with your mouse!",
                false);
        }
        editor.setCaretPosition(0);
    }

    bool isRussian = true;
    juce::TextButton langBtn { "EN" };
    juce::TextButton closeBtn { "X" };
    juce::TextEditor editor;
};

} // namespace ShantySonar
