#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace rockglue;

juce::AudioProcessor::BusesProperties RockGlueProcessor::makeBusesProperties()
{
    // One stereo main input (the master 2-bus, or drums in stems mode) plus three optional stereo inputs
    // the host can feed as sidechains. Only the main output exists: all four
    // lines sum into the glue bus.
    return BusesProperties()
        .withInput("Drums / Mix", juce::AudioChannelSet::stereo(), true)
        .withInput("Bass",        juce::AudioChannelSet::stereo(), false)
        .withInput("Guitars",     juce::AudioChannelSet::stereo(), false)
        .withInput("Vocals",      juce::AudioChannelSet::stereo(), false)
        .withOutput("Output",     juce::AudioChannelSet::stereo(), true);
}

RockGlueProcessor::RockGlueProcessor()
    : AudioProcessor(makeBusesProperties()),
      apvts(*this, nullptr, "RockGlueState", createParameterLayout())
{
    pInputMode     = apvts.getRawParameterValue(pid::inputMode);
    pDrumDrive     = apvts.getRawParameterValue(pid::drumDrive);
    pDrumMix       = apvts.getRawParameterValue(pid::drumMix);
    pMonoLock      = apvts.getRawParameterValue(pid::monoLock);
    pGrit          = apvts.getRawParameterValue(pid::grit);
    pCarvePocket   = apvts.getRawParameterValue(pid::carvePocket);
    pGlueThreshold = apvts.getRawParameterValue(pid::glueThreshold);
    pGlueMakeup    = apvts.getRawParameterValue(pid::glueMakeup);
    pGlueAutoRel   = apvts.getRawParameterValue(pid::glueAutoRel);

    setLatencySamples(RockGlueEngine::kLatencySamples);
}

bool RockGlueProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto stereo = juce::AudioChannelSet::stereo();
    if (layouts.getMainOutputChannelSet() != stereo || layouts.getMainInputChannelSet() != stereo)
        return false;

    // Aux inputs may be stereo or switched off; anything else is refused.
    for (int i = 1; i < layouts.inputBuses.size(); ++i)
    {
        const auto& set = layouts.inputBuses.getReference(i);
        if (! set.isDisabled() && set != stereo)
            return false;
    }
    return true;
}

void RockGlueProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    pushParameters();
    setLatencySamples(RockGlueEngine::kLatencySamples);
}

void RockGlueProcessor::pushParameters()
{
    engine.setDrumDriveDb(pDrumDrive->load());
    engine.setDrumParallelMix(pDrumMix->load() * 0.01f);
    engine.setMonoLock(pMonoLock->load() > 0.5f);
    engine.setGrit(pGrit->load() * 0.01f);
    engine.setCarvePocket(pCarvePocket->load() * 0.01f);
    engine.setGlueThresholdDb(pGlueThreshold->load());
    engine.setGlueMakeupDb(pGlueMakeup->load());
    engine.setGlueAutoRelease(pGlueAutoRel->load() > 0.5f);
}

void RockGlueProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    pushParameters();

    const int numSamples = buffer.getNumSamples();

    // The main output aliases the main input channels in the host buffer.
    auto main = getBusBuffer(buffer, true, Bus::drums);
    if (main.getNumChannels() < 2 || numSamples == 0)
        return;

    StereoBlock out { main.getWritePointer(0), main.getWritePointer(1) };

    const bool wantFourBus = pInputMode->load() > 0.5f;

    auto busBlock = [&](int busIndex) -> ConstStereoBlock
    {
        if (busIndex >= getBusCount(true) || ! getBus(true, busIndex)->isEnabled())
            return {};
        auto b = getBusBuffer(buffer, true, busIndex);
        if (b.getNumChannels() < 2)
            return {};
        return { b.getReadPointer(0), b.getReadPointer(1) };
    };

    if (wantFourBus)
    {
        fourBusActive.store(true, std::memory_order_relaxed);
        engine.processFourBus(ConstStereoBlock { out.left, out.right },
                              busBlock(Bus::bass), busBlock(Bus::guitars), busBlock(Bus::vocals),
                              out, numSamples);
    }
    else
    {
        fourBusActive.store(false, std::memory_order_relaxed);
        engine.processMaster(out, numSamples);
    }

    // Anything beyond the stereo output pair (aux bus channels the host laid
    // out after the mains) must not leak garbage.
    for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);

    const auto& m = engine.getMeterFrame();
    mDrumGr.store(m.drumGrDb, std::memory_order_relaxed);
    mGlueGr.store(m.glueGrDb, std::memory_order_relaxed);
    mWidth.store(m.width, std::memory_order_relaxed);
    mCorr.store(m.correlation, std::memory_order_relaxed);
    mPeak.store(m.outPeakDb, std::memory_order_relaxed);
}

MeterFrame RockGlueProcessor::getMeterFrame() const noexcept
{
    MeterFrame f;
    f.drumGrDb    = mDrumGr.load(std::memory_order_relaxed);
    f.glueGrDb    = mGlueGr.load(std::memory_order_relaxed);
    f.width       = mWidth.load(std::memory_order_relaxed);
    f.correlation = mCorr.load(std::memory_order_relaxed);
    f.outPeakDb   = mPeak.load(std::memory_order_relaxed);
    return f;
}

juce::AudioProcessorEditor* RockGlueProcessor::createEditor()
{
    return new RockGlueEditor(*this);
}

void RockGlueProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void RockGlueProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RockGlueProcessor();
}
