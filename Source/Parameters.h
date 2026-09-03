#pragma once

#include <JuceHeader.h>

namespace rockglue::pid
{
inline constexpr const char* inputMode    = "inputMode";     // 0 = Stereo Mix, 1 = 4-Bus
inline constexpr const char* drumDrive    = "drumDrive";     // dB
inline constexpr const char* drumMix      = "drumMix";       // %
inline constexpr const char* monoLock     = "monoLock";      // bool
inline constexpr const char* grit         = "grit";          // %
inline constexpr const char* carvePocket  = "carvePocket";   // %
inline constexpr const char* glueThreshold= "glueThreshold"; // dB
inline constexpr const char* glueMakeup   = "glueMakeup";    // dB
inline constexpr const char* glueAutoRel  = "glueAutoRelease";
} // namespace rockglue::pid

namespace rockglue
{
inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct = [](const char* id, const char* name, float def)
    {
        return std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, NormalisableRange<float> { 0.0f, 100.0f, 0.1f }, def,
            AudioParameterFloatAttributes().withLabel("%"));
    };

    p.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { pid::inputMode, 1 }, "Input Mode",
        StringArray { "Stereo Mix", "4-Bus" }, 0));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { pid::drumDrive, 1 }, "Drum Drive",
        NormalisableRange<float> { 0.0f, 30.0f, 0.1f }, 8.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(pct(pid::drumMix, "Parallel Mix", 50.0f));

    p.push_back(std::make_unique<AudioParameterBool>(ParameterID { pid::monoLock, 1 }, "Mono Lock", true));
    p.push_back(pct(pid::grit, "Grit", 25.0f));

    p.push_back(pct(pid::carvePocket, "Carve Pocket", 100.0f));

    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { pid::glueThreshold, 1 }, "Threshold",
        NormalisableRange<float> { -20.0f, 10.0f, 0.1f }, -10.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { pid::glueMakeup, 1 }, "Makeup Gain",
        NormalisableRange<float> { 0.0f, 12.0f, 0.1f }, 2.0f,
        AudioParameterFloatAttributes().withLabel("dB")));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID { pid::glueAutoRel, 1 }, "Auto Release", true));

    return { p.begin(), p.end() };
}
} // namespace rockglue
