#pragma once

#include "DSP/RockGlueEngine.h"
#include "Parameters.h"

#include <JuceHeader.h>

#include <atomic>

class RockGlueProcessor : public juce::AudioProcessor
{
public:
    // Bus indices. Bus 0 is the main input: the full mix in Stereo Mix mode,
    // the drums in 4-Bus mode. Buses 1..3 are optional sidechain-style inputs.
    enum Bus { drums = 0, bass = 1, guitars = 2, vocals = 3, numInputBuses = 4 };

    RockGlueProcessor();
    ~RockGlueProcessor() override = default;

    // --- AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- for the editor
    juce::AudioProcessorValueTreeState apvts;
    rockglue::MeterFrame getMeterFrame() const noexcept;
    bool isFourBusActive() const noexcept { return fourBusActive.load(std::memory_order_relaxed); }

private:
    static BusesProperties makeBusesProperties();
    void pushParameters();

    rockglue::RockGlueEngine engine;

    std::atomic<float>* pInputMode = nullptr;
    std::atomic<float>* pDrumDrive = nullptr;
    std::atomic<float>* pDrumMix = nullptr;
    std::atomic<float>* pMonoLock = nullptr;
    std::atomic<float>* pGrit = nullptr;
    std::atomic<float>* pCarvePocket = nullptr;
    std::atomic<float>* pGlueThreshold = nullptr;
    std::atomic<float>* pGlueMakeup = nullptr;
    std::atomic<float>* pGlueAutoRel = nullptr;

    // Meter snapshot for the UI thread, published once per block.
    std::atomic<float> mDrumGr { 0.0f }, mGlueGr { 0.0f }, mWidth { 0.0f }, mCorr { 1.0f }, mPeak { -100.0f };
    std::atomic<bool> fourBusActive { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RockGlueProcessor)
};
