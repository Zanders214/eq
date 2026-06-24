#include "NeonLookAndFeel.h"
#include <BinaryData.h>

namespace zeq
{

static juce::Typeface::Ptr loadFace (const char* data, int size)
{
    return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
}

juce::Font Fonts::grotesk (float height, Weight w)
{
    static auto med = loadFace (BinaryData::SpaceGroteskMedium_ttf,   BinaryData::SpaceGroteskMedium_ttfSize);
    static auto sem = loadFace (BinaryData::SpaceGroteskSemiBold_ttf, BinaryData::SpaceGroteskSemiBold_ttfSize);
    static auto bld = loadFace (BinaryData::SpaceGroteskBold_ttf,     BinaryData::SpaceGroteskBold_ttfSize);

    juce::Typeface::Ptr tf = sem;
    if (w == bold)
        tf = bld;
    else if (w == medium)
        tf = med;
    return juce::Font (juce::FontOptions().withTypeface (tf).withHeight (height));
}

juce::Font Fonts::mono (float height, bool mediumWeight)
{
    static auto reg = loadFace (BinaryData::JetBrainsMonoRegular_ttf, BinaryData::JetBrainsMonoRegular_ttfSize);
    static auto med = loadFace (BinaryData::JetBrainsMonoMedium_ttf,  BinaryData::JetBrainsMonoMedium_ttfSize);

    return juce::Font (juce::FontOptions().withTypeface (mediumWeight ? med : reg).withHeight (height));
}

// -----------------------------------------------------------------------------
NeonLookAndFeel::NeonLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, theme::text2);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);

    // Dark, on-brand styling for the (otherwise default-V4) transient windows used
    // by user presets: the load PopupMenu, the save AlertWindow and its TextEditor.
    setColour (juce::PopupMenu::backgroundColourId,          theme::panelBase);
    setColour (juce::PopupMenu::textColourId,                theme::text1);
    setColour (juce::PopupMenu::headerTextColourId,          theme::textLabel);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::accent.withAlpha (0.30f));
    setColour (juce::PopupMenu::highlightedTextColourId,     juce::Colours::white);

    setColour (juce::TextEditor::backgroundColourId,         theme::well);
    setColour (juce::TextEditor::textColourId,               theme::text1);
    setColour (juce::TextEditor::outlineColourId,            theme::whiteAlpha (0.12f));
    setColour (juce::TextEditor::focusedOutlineColourId,     theme::accent);
    setColour (juce::TextEditor::highlightColourId,          theme::accent.withAlpha (0.30f));
    setColour (juce::CaretComponent::caretColourId,          theme::accent);

    setColour (juce::AlertWindow::backgroundColourId,        theme::panelTop);
    setColour (juce::AlertWindow::textColourId,              theme::text1);
    setColour (juce::AlertWindow::outlineColourId,           theme::whiteAlpha (0.10f));
}

void NeonLookAndFeel::glow (juce::Graphics& g, juce::Rectangle<float> b, juce::Colour c,
                            float radius, float alpha)
{
    auto centre = b.getCentre();
    const float r = juce::jmax (b.getWidth(), b.getHeight()) * 0.5f + radius;
    juce::ColourGradient grad (c.withAlpha (alpha), centre.x, centre.y,
                               c.withAlpha (0.0f), centre.x + r, centre.y, true);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);
}

static juce::ColourGradient sliderGradient (const juce::String& kind, float x0, float x1, float y)
{
    juce::ColourGradient grad (theme::cyan, x0, y, theme::violet, x1, y, false);
    grad.clearColours();
    if (kind == "warm")
    {
        grad.addColour (0.0, theme::pink);  grad.addColour (1.0, theme::amber);
    }
    else if (kind == "spectrum")
    {
        grad.addColour (0.0, theme::cyan);  grad.addColour (0.34, theme::violet);
        grad.addColour (0.67, theme::pink); grad.addColour (1.0, theme::amber);
    }
    else if (kind == "accent")
    {
        grad.addColour (0.0, theme::accent); grad.addColour (1.0, theme::accentVio);
    }
    else // cool
    {
        grad.addColour (0.0, theme::cyan);  grad.addColour (1.0, theme::violet);
    }
    return grad;
}

void NeonLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float sliderPos, float, float,
                                        juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cy = (float) y + (float) h * 0.5f;
    const float trackH = 5.0f;
    const auto left = (float) x;
    const auto right = (float) (x + w);
    auto track = juce::Rectangle<float> (left, cy - trackH * 0.5f, (float) w, trackH);

    // unfilled track
    g.setColour (theme::whiteAlpha (0.10f));
    g.fillRoundedRectangle (track, trackH * 0.5f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawRoundedRectangle (track.reduced (0.5f), trackH * 0.5f, 1.0f);

    const bool enabled = s.isEnabled();
    const juce::String kind = s.getProperties().getWithDefault ("gradient", "cool").toString();

    // filled portion
    auto fill = track.withWidth (juce::jmax (trackH, sliderPos - left));
    auto grad = sliderGradient (kind, left, right, cy);
    if (! enabled)
        grad.multiplyOpacity (0.25f);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (fill, trackH * 0.5f);

    if (! enabled)
        return;

    // round handle
    const float hr = 7.0f;
    juce::Point thumb { juce::jlimit (left + hr, right - hr, sliderPos), cy };
    auto thumbCol = theme::rampColour ((thumb.x - left) / juce::jmax (1.0f, right - left));

    glow (g, { thumb.x - hr, thumb.y - hr, hr * 2.0f, hr * 2.0f }, thumbCol, 5.0f, 0.45f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (thumb.x - hr, thumb.y - hr + 1.0f, hr * 2.0f, hr * 2.0f);
    g.setColour (theme::text1);
    g.fillEllipse (thumb.x - hr, thumb.y - hr, hr * 2.0f, hr * 2.0f);
    g.setColour (thumbCol);
    g.drawEllipse (thumb.x - hr, thumb.y - hr, hr * 2.0f, hr * 2.0f, 1.5f);
}

void NeonLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                        float pos, float startAngle, float endAngle,
                                        juce::Slider&)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto area = juce::Rectangle<float> (size, size).withCentre (bounds.getCentre());
    const float cx = area.getCentreX();
    const float cyc = area.getCentreY();
    const float radius = size * 0.5f;
    const float arcR = radius - 5.0f;
    const float toAngle = startAngle + pos * (endAngle - startAngle);

    // knob face
    juce::ColourGradient face (juce::Colour (0xff222731), cx - radius * 0.2f, cyc - radius * 0.3f,
                               juce::Colour (0xff0b0d12), cx + radius * 0.5f, cyc + radius * 0.6f, true);
    g.setGradientFill (face);
    g.fillEllipse (area.reduced (5.0f));
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawEllipse (area.reduced (5.0f), 1.0f);

    // track arc
    juce::Path track;
    track.addCentredArc (cx, cyc, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (theme::whiteAlpha (0.10f));
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // value arc + glow
    juce::Path val;
    val.addCentredArc (cx, cyc, arcR, arcR, 0.0f, startAngle, toAngle, true);
    g.setColour (theme::accent.withAlpha (0.30f));
    g.strokePath (val, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::accent);
    g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // indicator
    juce::Point tip { cx + std::sin (toAngle) * (arcR - 3.0f),
                      cyc - std::cos (toAngle) * (arcR - 3.0f) };
    juce::Point root { cx + std::sin (toAngle) * (arcR * 0.4f),
                       cyc - std::cos (toAngle) * (arcR * 0.4f) };
    g.setColour (theme::textBright);
    g.drawLine ({ root, tip }, 2.0f);
    g.fillEllipse (tip.x - 2.5f, tip.y - 2.5f, 5.0f, 5.0f);
}

juce::Label* NeonLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

} // namespace zeq
