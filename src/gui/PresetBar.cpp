#include "PresetBar.h"
#include "NeonLookAndFeel.h"
#include "Theme.h"
#include "../Presets.h"

namespace zeq
{
using namespace theme;

PresetBar::PresetBar (ZandersEqAudioProcessor& p) : proc (p), apvts (p.getApvts()) {}

// The six built-in chips fill the row left of the SAVE / LOAD buttons.
juce::Rectangle<float> PresetBar::chipBounds (int i) const
{
    const int n = (int) presets().size();
    const float gap = 6.0f;
    const float total = (float) getWidth() - 120.0f;     // reserve the right edge for the buttons
    const float w = (total - gap * (float) (n - 1)) / (float) n;
    return { (w + gap) * (float) i, 0.0f, w, (float) getHeight() };
}

juce::Rectangle<float> PresetBar::saveBtnBounds() const
{
    return { (float) getWidth() - 112.0f, 0.0f, 50.0f, (float) getHeight() };
}

juce::Rectangle<float> PresetBar::menuBtnBounds() const
{
    return { (float) getWidth() - 56.0f, 0.0f, 56.0f, (float) getHeight() };
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto& list = presets();
    const juce::Colour violetText (0xffb6abff);

    for (int i = 0; i < (int) list.size(); ++i)
    {
        auto chip = chipBounds (i);
        const bool active = matchesPreset (apvts, list[(size_t) i]);

        g.setColour (active ? accentVio.withAlpha (0.16f) : whiteAlpha (0.05f));
        g.fillRoundedRectangle (chip, 8.0f);
        g.setColour (active ? accentVio.withAlpha (0.45f) : whiteAlpha (0.08f));
        g.drawRoundedRectangle (chip.reduced (0.5f), 8.0f, 1.0f);

        g.setColour (active ? violetText : text2);
        g.setFont (Fonts::grotesk (10.0f, Fonts::semibold).withExtraKerningFactor (0.03f));
        g.drawText (list[(size_t) i].name, chip, juce::Justification::centred);
    }

    auto drawBtn = [&] (juce::Rectangle<float> r, const juce::String& label)
    {
        g.setColour (accent.withAlpha (0.10f));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (accent.withAlpha (0.28f));
        g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
        g.setColour (accent);
        g.setFont (Fonts::grotesk (9.5f, Fonts::semibold).withExtraKerningFactor (0.1f));
        g.drawText (label, r, juce::Justification::centred);
    };
    drawBtn (saveBtnBounds(), "SAVE");
    drawBtn (menuBtnBounds(), "LOAD");
}

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    if (saveBtnBounds().contains (e.position)) { showSaveDialog(); return; }
    if (menuBtnBounds().contains (e.position)) { showPresetMenu(); return; }

    const auto& list = presets();
    for (int i = 0; i < (int) list.size(); ++i)
    {
        if (chipBounds (i).contains (e.position))
        {
            proc.recordUndoableEdit ([&] { applyPreset (apvts, list[(size_t) i]); });
            if (onPresetApplied) onPresetApplied();
            repaint();
            return;
        }
    }
}

void PresetBar::showSaveDialog()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name this preset:",
                                      juce::MessageBoxIconType::NoIcon, this);
    aw->setLookAndFeel (&getLookAndFeel());
    aw->addTextEditor ("name", "My Preset");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int result)
    {
        if (result == 1 && proc.saveUserPreset (aw->getTextEditorContents ("name")))
            if (onPresetApplied) onPresetApplied();
        aw->setLookAndFeel (nullptr);
    }), true);   // delete the window when dismissed
}

void PresetBar::showPresetMenu()
{
    const auto files = proc.listUserPresets();

    juce::PopupMenu menu;
    menu.setLookAndFeel (&getLookAndFeel());
    if (files.isEmpty())
    {
        menu.addItem (-1, "No saved presets", false);
    }
    else
    {
        for (int i = 0; i < files.size(); ++i)
            menu.addItem (i + 1, files[i].getFileNameWithoutExtension());

        juce::PopupMenu del;
        for (int i = 0; i < files.size(); ++i)
            del.addItem (1000 + i + 1, files[i].getFileNameWithoutExtension());
        menu.addSeparator();
        menu.addSubMenu ("Delete", del);
    }
    menu.addSeparator();
    menu.addItem (2000, "Reveal presets folder");

    const auto target = localAreaToGlobal (menuBtnBounds().toNearestInt());
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (target),
        [this, files] (int result)
        {
            if (result <= 0)
                return;
            if (result == 2000)            { proc.userPresetsDir().revealToUser(); }
            else if (result >= 1000)       { proc.deleteUserPreset (files[result - 1000 - 1]); }
            else if (result - 1 < files.size())
            {
                proc.loadPresetFromFile (files[result - 1]);
                if (onPresetApplied) onPresetApplied();
                repaint();
            }
        });
}

} // namespace zeq
