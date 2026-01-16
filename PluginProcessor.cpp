/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>
#include <atomic>
#include <cmath>

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout BusGovernorAudioProcessor::createParameterLayout()
{
    using APVTS = juce::AudioProcessorValueTreeState;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramPressureId, 1 },
        "Pressure",
        juce::NormalisableRange<float> (0.0f, 1.0f, 0.001f),
        0.27f));

    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramDriveId, 1 },
        "Detector Drive",
        juce::NormalisableRange<float> (1.0f, 24.0f, 0.01f),
        5.8f));

    // Post output volume (pure trim at the very end)
    params.push_back (std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { paramVolumeId, 1 },
        "Volume",
        juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f),
        1.0f));

    return { params.begin(), params.end() };
}

//==============================================================================
BusGovernorAudioProcessor::BusGovernorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor (BusesProperties()
    #if ! JucePlugin_IsMidiEffect
     #if ! JucePlugin_IsSynth
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
     #endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
    #endif
      )
    , apvts (*this, nullptr, "Parameters", createParameterLayout())
#else
    : apvts (*this, nullptr, "Parameters", createParameterLayout())
#endif
{
}

BusGovernorAudioProcessor::~BusGovernorAudioProcessor() {}

//==============================================================================
const juce::String BusGovernorAudioProcessor::getName() const { return JucePlugin_Name; }

bool BusGovernorAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool BusGovernorAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool BusGovernorAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double BusGovernorAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int BusGovernorAudioProcessor::getNumPrograms() { return 1; }
int BusGovernorAudioProcessor::getCurrentProgram() { return 0; }
void BusGovernorAudioProcessor::setCurrentProgram (int) {}
const juce::String BusGovernorAudioProcessor::getProgramName (int) { return {}; }
void BusGovernorAudioProcessor::changeProgramName (int, const juce::String&) {}

//==============================================================================
void BusGovernorAudioProcessor::prepareToPlay (double, int)
{
    a = 1.0f;
    b = 1.0f;

    bSmooth = 1.0f;
    bMeter.store (0.0f, std::memory_order_relaxed);
}

void BusGovernorAudioProcessor::releaseResources() {}

#ifndef JucePlugin_PreferredChannelConfigurations
bool BusGovernorAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void BusGovernorAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    float* ch0 = buffer.getWritePointer (0);
    float* ch1 = (numChannels > 1) ? buffer.getWritePointer (1) : nullptr;

    // Read params once per block
    const float pressure = apvts.getRawParameterValue (paramPressureId)->load();
    const float drive    = apvts.getRawParameterValue (paramDriveId)->load();
    const float volume   = apvts.getRawParameterValue (paramVolumeId)->load();

    constexpr float eps = 1.0e-12f;
    constexpr float shapeK = 6.0f;

    for (int s = 0; s < numSamples; ++s)
    {
        const float l = ch0[s];
        const float r = (ch1 != nullptr) ? ch1[s] : l;

        // Detector from INPUT only
        const float det = std::abs (l + r);

        // ---- BusGovernor core ----
        const float aSafe = std::max (a, eps);
        a = (1.0f - 0.012f) * (a + std::abs (b - a))
          + 0.012f * std::abs (b * det * det * drive) / (aSafe * aSafe);

        const float bSafe = std::max (b, eps);
        const float base  = std::max (b + std::abs (a - b), eps);
        const float expo  = a / bSafe;
        b = (1.0f - 0.008f) * (a + std::abs (b - a))
          + 0.008f * std::abs (std::pow (base, expo));

        // ---- Base output: NO makeup inside ----
        const float invb = 1.0f / std::max (b, eps);
        float outL = l * invb;   // out = in/b
        float outR = r * invb;

        // ---- Pressure A: shaped delta between out and out/b ----
        const float pressedL = outL * invb;   // out/b
        const float pressedR = outR * invb;

        const float dL = pressedL - outL;
        const float dR = pressedR - outR;

        const float dLs = dL / (1.0f + shapeK * std::abs (dL));
        const float dRs = dR / (1.0f + shapeK * std::abs (dR));

        outL += pressure * dLs;
        outR += pressure * dRs;

        // ---- Post output Volume (pure trim) ----
        outL *= volume;
        outR *= volume;

        ch0[s] = outL;
        if (ch1 != nullptr)
            ch1[s] = outR;

        // ---- UI meter (smoothed) ----
        bSmooth = 0.95f * bSmooth + 0.05f * b;
        bMeter.store (bSmooth, std::memory_order_relaxed);
    }

    for (int ch = 2; ch < numChannels; ++ch)
        buffer.clear (ch, 0, numSamples);
}

//==============================================================================
bool BusGovernorAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* BusGovernorAudioProcessor::createEditor()
{
    return new BusGovernorAudioProcessorEditor (*this);
}

//==============================================================================
void BusGovernorAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState().createXml())
        copyXmlToBinary (*state, destData);
}

void BusGovernorAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BusGovernorAudioProcessor();
}
