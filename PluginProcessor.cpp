#include "PluginProcessor.h"

namespace
{
    const juce::StringArray rateNames   { "1/16", "1/16T", "1/32", "1/32T", "1/64" };
    const double            rateQN[]    { 0.25, 1.0 / 6.0, 0.125, 1.0 / 12.0, 0.0625 };

    const juce::StringArray patternNames { "Up", "Down", "UpDown", "Random", "Thirds", "Pedal" };

    const juce::StringArray sourceNames { "Held Notes", "Scale" };

    const juce::StringArray scaleNames { "Natural Minor", "Harmonic Minor", "Phrygian Dominant",
                                         "Phrygian", "Major", "Dorian", "Minor Pentatonic" };

    const std::vector<std::vector<int>> scaleIntervals
    {
        { 0, 2, 3, 5, 7, 8, 10 },
        { 0, 2, 3, 5, 7, 8, 11 },
        { 0, 1, 4, 5, 7, 8, 10 },
        { 0, 1, 3, 5, 7, 8, 10 },
        { 0, 2, 4, 5, 7, 9, 11 },
        { 0, 2, 3, 5, 7, 9, 10 },
        { 0, 3, 5, 7, 10 }
    };
}

ShredGenProcessor::ShredGenProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    held.reserve (32);
    pool.reserve (256);
}

juce::AudioProcessorValueTreeState::ParameterLayout ShredGenProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rate", 1 },    "Rate",    rateNames, 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", patternNames, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "source", 1 },  "Source",  sourceNames, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 },   "Scale",   scaleNames, 1));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "octaves", 1 }, "Octaves", 1, 4, 2));
    l.add (std::make_unique<AudioParameterFloat>  (ParameterID { "gate", 1 },    "Gate",    NormalisableRange<float> (0.05f, 1.0f), 0.8f));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "velocity", 1 },"Velocity", 1, 127, 90));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "accent", 1 },  "Accent",  0, 60, 20));
    l.add (std::make_unique<AudioParameterInt>    (ParameterID { "accentEvery", 1 }, "Accent Every", 1, 8, 4));

    return l;
}

void ShredGenProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;
    samplesToNext = 0.0;
    offCountdown = 0.0;
    noteOn = -1;
    stepIndex = 0;
}

bool ShredGenProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::mono();
}

void ShredGenProcessor::buildPool()
{
    pool.clear();
    if (held.empty())
        return;

    std::vector<int> sorted (held);
    std::sort (sorted.begin(), sorted.end());

    const int octs   = (int) *apvts.getRawParameterValue ("octaves");
    const int source = (int) *apvts.getRawParameterValue ("source");
    const int scale  = (int) *apvts.getRawParameterValue ("scale");

    if (source == 0)
    {
        for (int o = 0; o < octs; ++o)
            for (int n : sorted)
                pool.push_back (n + 12 * o);
    }
    else
    {
        const int root = sorted.front();
        const auto& iv = scaleIntervals[(size_t) juce::jlimit (0, (int) scaleIntervals.size() - 1, scale)];

        for (int o = 0; o < octs; ++o)
            for (int i : iv)
                pool.push_back (root + 12 * o + i);

        pool.push_back (root + 12 * octs);
    }

    pool.erase (std::remove_if (pool.begin(), pool.end(), [] (int n) { return n < 0 || n > 127; }), pool.end());
    std::sort (pool.begin(), pool.end());
    pool.erase (std::unique (pool.begin(), pool.end()), pool.end());
}

void ShredGenProcessor::startStep (juce::MidiBuffer& out, int samplePos, double stepSamples)
{
    if (noteOn >= 0)
    {
        out.addEvent (juce::MidiMessage::noteOff (1, noteOn), samplePos);
        noteOn = -1;
    }

    buildPool();
    const int N = (int) pool.size();
    if (N == 0)
        return;

    const int pattern = (int) *apvts.getRawParameterValue ("pattern");
    const int i = stepIndex;
    int idx = 0;

    switch (pattern)
    {
        case 0: idx = i % N; break;                                   // Up
        case 1: idx = N - 1 - (i % N); break;                         // Down
        case 2:                                                       // UpDown
        {
            const int cycle = juce::jmax (1, 2 * N - 2);
            const int k = i % cycle;
            idx = k < N ? k : cycle - k;
            break;
        }
        case 3: idx = rng.nextInt (N); break;                         // Random
        case 4: idx = ((i / 2) + (i & 1) * 2) % N; break;             // Thirds: 0 2 1 3 2 4 ...
        case 5:                                                       // Pedal
            idx = (i % 2 == 0 || N < 2) ? 0 : 1 + ((i / 2) % (N - 1));
            break;
        default: break;
    }

    int vel = (int) *apvts.getRawParameterValue ("velocity");
    const int accent = (int) *apvts.getRawParameterValue ("accent");
    const int every  = juce::jmax (1, (int) *apvts.getRawParameterValue ("accentEvery"));
    if (i % every == 0)
        vel += accent;
    vel = juce::jlimit (1, 127, vel);

    const float gate = *apvts.getRawParameterValue ("gate");

    noteOn = pool[(size_t) idx];
    out.addEvent (juce::MidiMessage::noteOn (1, noteOn, (juce::uint8) vel), samplePos);
    offCountdown = juce::jmax (1.0, stepSamples * (double) gate);

    ++stepIndex;
}

void ShredGenProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    // 1. Читаем входящие ноты
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();

        if (msg.isNoteOn())
        {
            if (std::find (held.begin(), held.end(), msg.getNoteNumber()) == held.end())
                held.push_back (msg.getNoteNumber());
        }
        else if (msg.isNoteOff())
        {
            held.erase (std::remove (held.begin(), held.end(), msg.getNoteNumber()), held.end());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            held.clear();
        }
    }

    juce::MidiBuffer out;   // входящие ноты не пропускаем, только сгенерированные

    // 2. Темп от хоста
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto b = pos->getBpm())
                bpm = juce::jmax (20.0, *b);

    const int rateIdx = juce::jlimit (0, 4, (int) *apvts.getRawParameterValue ("rate"));
    const double stepSamples = rateQN[rateIdx] * 60.0 / bpm * sr;

    const int n = buffer.getNumSamples();

    if (held.empty())
    {
        if (noteOn >= 0)
        {
            out.addEvent (juce::MidiMessage::noteOff (1, noteOn), 0);
            noteOn = -1;
        }
        stepIndex = 0;
        samplesToNext = 0.0;
        midi.swapWith (out);
        return;
    }

    // 3. Генерация шагов
    for (int s = 0; s < n; ++s)
    {
        if (noteOn >= 0)
        {
            offCountdown -= 1.0;
            if (offCountdown <= 0.0)
            {
                out.addEvent (juce::MidiMessage::noteOff (1, noteOn), s);
                noteOn = -1;
            }
        }

        if (samplesToNext <= 0.0)
        {
            startStep (out, s, stepSamples);
            samplesToNext += stepSamples;
        }

        samplesToNext -= 1.0;
    }

    midi.swapWith (out);
}

juce::AudioProcessorEditor* ShredGenProcessor::createEditor()
{
    return new juce::GenericAudioProcessorEditor (*this);
}

void ShredGenProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void ShredGenProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ShredGenProcessor();
}
