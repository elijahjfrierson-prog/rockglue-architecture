#pragma once

#include "PluginProcessor.h"

#include <JuceHeader.h>

#include <array>

namespace rockglue::ui
{
inline const juce::Colour kBackground { 0xff15171b };
inline const juce::Colour kPanel      { 0xff1e2127 };
inline const juce::Colour kPanelEdge  { 0xff2c3038 };
inline const juce::Colour kText       { 0xffd6d9de };
inline const juce::Colour kDimText    { 0xff7f8592 };
inline const juce::Colour kAccent     { 0xffff6a3d };   // hot orange
inline const juce::Colour kAccent2    { 0xff3dd6ff };   // cyan
inline const juce::Colour kGreen      { 0xff5ad17a };

class RockGlueLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RockGlueLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                          float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float sliderPos,
                          float minPos, float maxPos, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
};

// Vertical gain-reduction meter, 0 .. -maxDb.
class GlueMeter : public juce::Component
{
public:
    explicit GlueMeter(float maxDb) : range(maxDb) {}
    void setGainReductionDb(float gr);
    void paint(juce::Graphics&) override;

private:
    float range;
    float display = 0.0f;
};

// Responsive visualizer: rolling gain-reduction history for the Smasher and
// the Glue plus a stereo width / correlation footprint.
class FootprintVisualizer : public juce::Component
{
public:
    void push(const MeterFrame& frame);
    void paint(juce::Graphics&) override;

private:
    static constexpr int kHistory = 256;
    std::array<float, kHistory> drumHistory {}, glueHistory {}, widthHistory {};
    int writeIndex = 0;
    MeterFrame latest;
};
} // namespace rockglue::ui

class RockGlueEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit RockGlueEditor(RockGlueProcessor&);
    ~RockGlueEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    struct Node
    {
        juce::String title, subtitle;
        juce::Rectangle<int> bounds;
    };

    void timerCallback() override;
    void setupSlider(juce::Slider&, juce::Label&, const juce::String& name, juce::Slider::SliderStyle);
    void layoutControl(juce::Rectangle<int> area, juce::Component& control, juce::Label& label);

    RockGlueProcessor& processor;
    rockglue::ui::RockGlueLookAndFeel lnf;

    juce::ComboBox inputMode;
    juce::Label inputModeLabel;

    juce::Slider drumDrive, drumMix, grit, carvePocket, threshold, makeup;
    juce::Label drumDriveLabel, drumMixLabel, gritLabel, carvePocketLabel, thresholdLabel, makeupLabel;
    juce::ToggleButton monoLock { "Mono Lock" }, autoRelease { "Auto Release" };

    rockglue::ui::GlueMeter glueMeter { 6.0f };
    rockglue::ui::FootprintVisualizer visualizer;

    std::array<Node, 4> nodes;

    std::unique_ptr<ComboAttachment> inputModeAtt;
    std::unique_ptr<SliderAttachment> drumDriveAtt, drumMixAtt, gritAtt, carvePocketAtt, thresholdAtt, makeupAtt;
    std::unique_ptr<ButtonAttachment> monoLockAtt, autoReleaseAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RockGlueEditor)
};
