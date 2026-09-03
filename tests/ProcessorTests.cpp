// Host-side tests against the real RockGlueProcessor: bus layouts, reported
// latency, four-bus routing through the JUCE buffer and state round-trip.

#include "../Source/PluginProcessor.h"

#include <cstdio>

namespace
{
int failures = 0;

void check(bool ok, const juce::String& what, const juce::String& detail = {})
{
    std::printf("%s  %s%s%s\n", ok ? "PASS" : "FAIL", what.toRawUTF8(),
                detail.isEmpty() ? "" : " -- ", detail.toRawUTF8());
    if (! ok)
        ++failures;
}

void setParam(RockGlueProcessor& p, const char* id, float value)
{
    auto* param = p.apvts.getParameter(id);
    param->setValueNotifyingHost(param->convertTo0to1(value));
}

void neutralise(RockGlueProcessor& p)
{
    setParam(p, rockglue::pid::drumDrive, 0.0f);
    setParam(p, rockglue::pid::drumMix, 0.0f);
    setParam(p, rockglue::pid::monoLock, 0.0f);
    setParam(p, rockglue::pid::grit, 0.0f);
    setParam(p, rockglue::pid::carvePocket, 0.0f);
    setParam(p, rockglue::pid::glueThreshold, 10.0f);
    setParam(p, rockglue::pid::glueMakeup, 0.0f);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    // --- latency & layout
    {
        RockGlueProcessor p;
        check(p.getLatencySamples() == 0, "processor reports 0 samples latency",
              juce::String(p.getLatencySamples()));
        check(p.getBusCount(true) == 4, "four input buses declared", juce::String(p.getBusCount(true)));
        check(p.getBusCount(false) == 1, "single output bus");

        auto stereoOnly = p.getBusesLayout();
        check(p.checkBusesLayoutSupported(stereoOnly), "default stereo-in/stereo-out layout supported");

        auto fourBus = stereoOnly;
        for (int i = 1; i < 4; ++i)
            fourBus.inputBuses.set(i, juce::AudioChannelSet::stereo());
        check(p.checkBusesLayoutSupported(fourBus), "4 x stereo input layout supported");

        auto bad = stereoOnly;
        bad.inputBuses.set(0, juce::AudioChannelSet::mono());
        check(! p.checkBusesLayoutSupported(bad), "mono main input rejected");
    }

    // --- four-bus routing through the JUCE buffer
    {
        RockGlueProcessor p;
        auto layout = p.getBusesLayout();
        for (int i = 1; i < 4; ++i)
            layout.inputBuses.set(i, juce::AudioChannelSet::stereo());
        check(p.setBusesLayout(layout), "setBusesLayout(4 x stereo) succeeds");
        neutralise(p);
        setParam(p, rockglue::pid::inputMode, 1.0f);

        p.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> buf(p.getTotalNumInputChannels(), 256);
        check(buf.getNumChannels() == 8, "host buffer carries 8 input channels", juce::String(buf.getNumChannels()));
        for (int bus = 0; bus < 4; ++bus)
            for (int ch = 0; ch < 2; ++ch)
                juce::FloatVectorOperations::fill(buf.getWritePointer(bus * 2 + ch), 0.1f * (bus + 1), 256);

        juce::MidiBuffer midi;
        p.processBlock(buf, midi);
        const float out = buf.getSample(0, 255);
        check(std::abs(out - 1.0f) < 1e-4f, "neutral 4-bus routing sums all four buses to the output",
              juce::String(out));
        check(std::abs(buf.getSample(2, 255)) < 1e-9f, "aux bus channels are cleared after processing");
        check(p.isFourBusActive(), "4-bus mode is reported to the editor");
    }

    // --- state round-trip
    {
        RockGlueProcessor a;
        setParam(a, rockglue::pid::drumDrive, 17.5f);
        setParam(a, rockglue::pid::glueThreshold, -4.0f);
        setParam(a, rockglue::pid::monoLock, 0.0f);
        juce::MemoryBlock state;
        a.getStateInformation(state);

        RockGlueProcessor b;
        b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        check(std::abs(b.apvts.getRawParameterValue(rockglue::pid::drumDrive)->load() - 17.5f) < 1e-3f,
              "drum drive survives a state round-trip");
        check(std::abs(b.apvts.getRawParameterValue(rockglue::pid::glueThreshold)->load() + 4.0f) < 1e-3f,
              "threshold survives a state round-trip");
        check(b.apvts.getRawParameterValue(rockglue::pid::monoLock)->load() < 0.5f,
              "mono lock survives a state round-trip");
    }

    // --- editor constructs and lays out without asserting
    {
        RockGlueProcessor p;
        std::unique_ptr<juce::AudioProcessorEditor> ed(p.createEditor());
        check(ed != nullptr, "editor is created");
        ed->setSize(920, 560);
        check(ed->getWidth() == 920, "editor accepts its default size");
    }

    std::printf("\n%d failure(s)\n", failures);
    return failures == 0 ? 0 : 1;
}
