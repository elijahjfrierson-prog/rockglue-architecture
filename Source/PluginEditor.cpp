#include "PluginEditor.h"

using namespace rockglue;
using namespace rockglue::ui;

// ---------------------------------------------------------------------------
// Look and feel

RockGlueLookAndFeel::RockGlueLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, kText);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, kDimText);
    setColour(juce::ComboBox::backgroundColourId, kPanel);
    setColour(juce::ComboBox::outlineColourId, kPanelEdge);
    setColour(juce::ComboBox::textColourId, kText);
    setColour(juce::ComboBox::arrowColourId, kAccent);
    setColour(juce::PopupMenu::backgroundColourId, kPanel);
    setColour(juce::PopupMenu::textColourId, kText);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, kAccent.withAlpha(0.35f));
    setColour(juce::ToggleButton::textColourId, kText);
}

void RockGlueLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float startAngle, float endAngle, juce::Slider&)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(w), static_cast<float>(h)).reduced(6.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float lineW = juce::jmax(3.0f, radius * 0.12f);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, endAngle, true);
    g.setColour(kPanelEdge);
    g.strokePath(track, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius - lineW, radius - lineW, 0.0f, startAngle, angle, true);
    g.setColour(kAccent);
    g.strokePath(value, juce::PathStrokeType(lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xff262a31));
    g.fillEllipse(juce::Rectangle<float>(radius * 1.3f, radius * 1.3f).withCentre(centre));

    const juce::Point<float> tip(centre.x + std::sin(angle) * (radius * 0.55f),
                                 centre.y - std::cos(angle) * (radius * 0.55f));
    g.setColour(kText);
    g.drawLine({ centre, tip }, 2.5f);
}

void RockGlueLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float minPos, float maxPos, juce::Slider::SliderStyle style, juce::Slider& s)
{
    if (style != juce::Slider::LinearHorizontal)
    {
        juce::LookAndFeel_V4::drawLinearSlider(g, x, y, w, h, pos, minPos, maxPos, style, s);
        return;
    }
    const float cy = static_cast<float>(y) + h * 0.5f;
    g.setColour(kPanelEdge);
    g.fillRoundedRectangle(static_cast<float>(x), cy - 3.0f, static_cast<float>(w), 6.0f, 3.0f);
    g.setColour(kAccent);
    g.fillRoundedRectangle(static_cast<float>(x), cy - 3.0f, pos - static_cast<float>(x), 6.0f, 3.0f);
    g.setColour(kText);
    g.fillEllipse(pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

void RockGlueLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced(2.0f);
    const bool on = b.getToggleState();
    g.setColour(on ? kAccent.withAlpha(0.25f) : kPanel);
    g.fillRoundedRectangle(r, 6.0f);
    g.setColour(on ? kAccent : (highlighted ? kDimText : kPanelEdge));
    g.drawRoundedRectangle(r, 6.0f, 1.5f);
    g.setColour(on ? kText : kDimText);
    g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::bold)));
    g.drawText(b.getButtonText(), r, juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Glue meter

void GlueMeter::setGainReductionDb(float gr)
{
    // Fast attack, slow fall so the needle reads like a VU.
    display = gr > display ? gr : display * 0.85f + gr * 0.15f;
    repaint();
}

void GlueMeter::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(r, 5.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(r, 5.0f, 1.0f);

    auto bar = r.reduced(6.0f);
    const float frac = juce::jlimit(0.0f, 1.0f, display / range);
    auto fill = bar.withHeight(bar.getHeight() * frac);   // grows downward from the top
    g.setColour(kAccent2.interpolatedWith(kAccent, frac));
    g.fillRoundedRectangle(fill, 3.0f);

    g.setColour(kDimText);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    for (int db = 0; db <= static_cast<int>(range); db += 2)
    {
        const float yy = bar.getY() + bar.getHeight() * (static_cast<float>(db) / range);
        g.drawHorizontalLine(static_cast<int>(yy), bar.getX(), bar.getRight());
        g.drawText("-" + juce::String(db), juce::Rectangle<float>(bar.getX(), yy - 6.0f, bar.getWidth(), 12.0f),
                   juce::Justification::centredRight);
    }
}

// ---------------------------------------------------------------------------
// Visualizer

void FootprintVisualizer::push(const MeterFrame& frame)
{
    latest = frame;
    drumHistory[static_cast<size_t>(writeIndex)] = frame.drumGrDb;
    glueHistory[static_cast<size_t>(writeIndex)] = frame.glueGrDb;
    widthHistory[static_cast<size_t>(writeIndex)] = frame.width;
    writeIndex = (writeIndex + 1) % kHistory;
    repaint();
}

void FootprintVisualizer::paint(juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour(kPanel);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(kPanelEdge);
    g.drawRoundedRectangle(r, 8.0f, 1.0f);

    auto footprint = r.removeFromRight(r.getHeight()).reduced(10.0f);
    auto graph = r.reduced(10.0f);

    // --- gain-reduction history (drums: orange, glue: cyan)
    auto drawHistory = [&](const std::array<float, kHistory>& hist, float maxDb, juce::Colour c)
    {
        juce::Path p;
        for (int i = 0; i < kHistory; ++i)
        {
            const int idx = (writeIndex + i) % kHistory;
            const float x = graph.getX() + graph.getWidth() * static_cast<float>(i) / (kHistory - 1);
            const float v = juce::jlimit(0.0f, 1.0f, hist[static_cast<size_t>(idx)] / maxDb);
            const float y = graph.getY() + graph.getHeight() * v;
            if (i == 0) p.startNewSubPath(x, y); else p.lineTo(x, y);
        }
        g.setColour(c);
        g.strokePath(p, juce::PathStrokeType(1.8f));
    };
    g.setColour(kPanelEdge);
    for (int i = 1; i < 4; ++i)
        g.drawHorizontalLine(static_cast<int>(graph.getY() + graph.getHeight() * i / 4.0f), graph.getX(), graph.getRight());
    drawHistory(drumHistory, 20.0f, kAccent);
    drawHistory(glueHistory, 6.0f, kAccent2);

    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.setColour(kAccent);
    g.drawText("SMASHER GR  " + juce::String(latest.drumGrDb, 1) + " dB", graph.removeFromTop(14.0f), juce::Justification::topLeft);
    g.setColour(kAccent2);
    g.drawText("GLUE GR  " + juce::String(latest.glueGrDb, 1) + " dB", graph.removeFromTop(14.0f), juce::Justification::topLeft);

    // --- stereo footprint: ellipse whose width is the side/mid ratio, tilted
    // by correlation (negative correlation = out of phase, drawn in orange).
    const auto centre = footprint.getCentre();
    const float radius = footprint.getWidth() * 0.5f;
    g.setColour(kPanelEdge);
    g.drawEllipse(footprint, 1.0f);
    g.drawLine(centre.x, footprint.getY(), centre.x, footprint.getBottom(), 1.0f);
    g.drawLine(footprint.getX(), centre.y, footprint.getRight(), centre.y, 1.0f);

    const float w = juce::jlimit(0.05f, 1.0f, latest.width) * radius;
    const float hgt = juce::jlimit(0.05f, 1.0f, 1.0f - latest.width * 0.5f) * radius;
    const bool inPhase = latest.correlation >= 0.0f;
    g.setColour((inPhase ? kGreen : kAccent).withAlpha(0.7f));
    g.fillEllipse(juce::Rectangle<float>(w * 2.0f, hgt * 2.0f).withCentre(centre));

    g.setColour(kDimText);
    g.setFont(juce::Font(juce::FontOptions(10.0f)));
    g.drawText("WIDTH " + juce::String(juce::roundToInt(latest.width * 100.0f)) + "%   CORR "
                   + juce::String(latest.correlation, 2),
               footprint.withY(footprint.getBottom() - 2.0f).withHeight(12.0f), juce::Justification::centred);
}

// ---------------------------------------------------------------------------
// Editor

RockGlueEditor::RockGlueEditor(RockGlueProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);

    nodes[0] = { "DRUM BUS", "Smasher  |  FET 4:1  0.05 ms / 50 ms", {} };
    nodes[1] = { "BASS BUS", "Low-End Anchor  |  M/S", {} };
    nodes[2] = { "GTR / VOX", "The Pocket  |  2.5 kHz Q 1.0  (M/S on master)", {} };
    nodes[3] = { "MASTER", "VCA Glue  |  2:1  30 ms  soft knee", {} };

    inputMode.addItemList({ "Master", "4-Bus Stems" }, 1);
    addAndMakeVisible(inputMode);
    inputModeLabel.setText("INPUT", juce::dontSendNotification);
    inputModeLabel.setJustificationType(juce::Justification::centredRight);
    addAndMakeVisible(inputModeLabel);
    inputModeAtt = std::make_unique<ComboAttachment>(processor.apvts, pid::inputMode, inputMode);

    setupSlider(drumDrive, drumDriveLabel, "DRUM DRIVE", juce::Slider::LinearHorizontal);
    setupSlider(drumMix, drumMixLabel, "PARALLEL MIX", juce::Slider::LinearHorizontal);
    setupSlider(grit, gritLabel, "GRIT", juce::Slider::LinearHorizontal);
    setupSlider(carvePocket, carvePocketLabel, "CARVE POCKET", juce::Slider::LinearHorizontal);
    setupSlider(threshold, thresholdLabel, "THRESHOLD", juce::Slider::RotaryHorizontalVerticalDrag);
    setupSlider(makeup, makeupLabel, "MAKEUP", juce::Slider::RotaryHorizontalVerticalDrag);

    addAndMakeVisible(monoLock);
    addAndMakeVisible(autoRelease);
    addAndMakeVisible(glueMeter);
    addAndMakeVisible(visualizer);

    drumDriveAtt   = std::make_unique<SliderAttachment>(processor.apvts, pid::drumDrive, drumDrive);
    drumMixAtt     = std::make_unique<SliderAttachment>(processor.apvts, pid::drumMix, drumMix);
    gritAtt        = std::make_unique<SliderAttachment>(processor.apvts, pid::grit, grit);
    carvePocketAtt = std::make_unique<SliderAttachment>(processor.apvts, pid::carvePocket, carvePocket);
    thresholdAtt   = std::make_unique<SliderAttachment>(processor.apvts, pid::glueThreshold, threshold);
    makeupAtt      = std::make_unique<SliderAttachment>(processor.apvts, pid::glueMakeup, makeup);
    monoLockAtt    = std::make_unique<ButtonAttachment>(processor.apvts, pid::monoLock, monoLock);
    autoReleaseAtt = std::make_unique<ButtonAttachment>(processor.apvts, pid::glueAutoRel, autoRelease);

    setResizable(true, true);
    setResizeLimits(760, 480, 1600, 1000);
    setSize(920, 560);
    startTimerHz(30);
}

RockGlueEditor::~RockGlueEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void RockGlueEditor::setupSlider(juce::Slider& s, juce::Label& l, const juce::String& name,
                                 juce::Slider::SliderStyle style)
{
    s.setSliderStyle(style);
    s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 18);
    addAndMakeVisible(s);
    l.setText(name, juce::dontSendNotification);
    l.setJustificationType(juce::Justification::centred);
    l.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
    addAndMakeVisible(l);
}

void RockGlueEditor::layoutControl(juce::Rectangle<int> area, juce::Component& control, juce::Label& label)
{
    label.setBounds(area.removeFromTop(16));
    control.setBounds(area);
}

void RockGlueEditor::timerCallback()
{
    const auto frame = processor.getMeterFrame();
    glueMeter.setGainReductionDb(frame.glueGrDb);
    visualizer.push(frame);
    if (lastFourBusShown != processor.isFourBusSelected())
    {
        lastFourBusShown = processor.isFourBusSelected();
        repaint();
    }
}

void RockGlueEditor::paint(juce::Graphics& g)
{
    g.fillAll(kBackground);

    auto header = getLocalBounds().removeFromTop(48).reduced(16, 8);
    g.setColour(kText);
    g.setFont(juce::Font(juce::FontOptions(20.0f, juce::Font::bold)));
    g.drawText("ROCKGLUE", header.removeFromLeft(120), juce::Justification::centredLeft);
    g.setColour(kAccent);
    g.drawText("ARCHITECTURE", header.removeFromLeft(150), juce::Justification::centredLeft);
    g.setColour(kDimText);
    g.setFont(juce::Font(juce::FontOptions(11.0f)));
    g.drawText(processor.isFourBusSelected() ? "4-BUS STEMS  |  0 samples latency" : "MASTER BUS INSERT  |  0 samples latency",
               header.removeFromLeft(220), juce::Justification::centredLeft);

    for (const auto& n : nodes)
    {
        auto r = n.bounds.toFloat();
        g.setColour(kPanel);
        g.fillRoundedRectangle(r, 8.0f);
        g.setColour(kPanelEdge);
        g.drawRoundedRectangle(r, 8.0f, 1.0f);

        auto t = n.bounds.reduced(12, 10);
        g.setColour(kText);
        g.setFont(juce::Font(juce::FontOptions(14.0f, juce::Font::bold)));
        g.drawText(n.title, t.removeFromTop(18), juce::Justification::centredLeft);
        g.setColour(kDimText);
        g.setFont(juce::Font(juce::FontOptions(10.5f)));
        g.drawText(n.subtitle, t.removeFromTop(14), juce::Justification::centredLeft);
    }
}

void RockGlueEditor::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto header = area.removeFromTop(32);
    inputMode.setBounds(header.removeFromRight(130));
    inputModeLabel.setBounds(header.removeFromRight(60));
    area.removeFromTop(8);

    visualizer.setBounds(area.removeFromBottom(juce::jmax(120, area.getHeight() / 4)));
    area.removeFromBottom(10);

    // Four node panels across.
    const int gap = 10;
    const int colW = (area.getWidth() - gap * 3) / 4;
    for (int i = 0; i < 4; ++i)
        nodes[static_cast<size_t>(i)].bounds = area.withX(area.getX() + i * (colW + gap)).withWidth(colW);

    auto content = [&](int i) { return nodes[static_cast<size_t>(i)].bounds.reduced(12).withTrimmedTop(44); };

    // Drums
    {
        auto c = content(0);
        const int h = c.getHeight() / 2;
        layoutControl(c.removeFromTop(h).reduced(0, 8), drumDrive, drumDriveLabel);
        layoutControl(c.reduced(0, 8), drumMix, drumMixLabel);
    }
    // Bass
    {
        auto c = content(1);
        monoLock.setBounds(c.removeFromTop(36).reduced(10, 2));
        layoutControl(c.reduced(0, 12), grit, gritLabel);
    }
    // Pocket
    {
        auto c = content(2);
        layoutControl(c.withSizeKeepingCentre(c.getWidth(), 80), carvePocket, carvePocketLabel);
    }
    // Master
    {
        auto c = content(3);
        auto meterCol = c.removeFromRight(juce::jmax(44, c.getWidth() / 4));
        glueMeter.setBounds(meterCol.reduced(4, 0));
        c.removeFromRight(6);
        autoRelease.setBounds(c.removeFromBottom(30).reduced(4, 2));
        const int h = c.getHeight() / 2;
        layoutControl(c.removeFromTop(h), threshold, thresholdLabel);
        layoutControl(c, makeup, makeupLabel);
    }
}
