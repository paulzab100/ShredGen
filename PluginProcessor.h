#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class ShredGenProcessor : public juce::AudioProcessor
{
public:
    ShredGenProcessor();
    ~ShredGenProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "ShredGen"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void buildPool();
    void startStep (juce::MidiBuffer& out, int samplePos, double stepSamples);

    std::vector<int> held;      // удерживаемые ноты
    std::vector<int> pool;      // ноты, по которым бежит паттерн
    juce::Random rng;

    double sr = 44100.0;
    double samplesToNext = 0.0;
    double offCountdown = 0.0;
    int noteOn = -1;
    int stepIndex = 0;
};
