/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <atomic>        // MUST be before JuceHeader on MSVC
#include <JuceHeader.h>

//==============================================================================
class BusGovernorAudioProcessor  : public juce::AudioProcessor
{
public:
    // Parameter IDs (keep these stable for preset compatibility)
    static constexpr const char* paramPressureId = "pressure";
    static constexpr const char* paramDriveId    = "drive";
    static constexpr const char* paramVolumeId   = "volume";   // (replaces makeup)

    //==============================================================================
    BusGovernorAudioProcessor();
    ~BusGovernorAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Parameters
    juce::AudioProcessorValueTreeState apvts;
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    //==============================================================================
    // Meter exposed to editor (needle)
    std::atomic<float> bMeter { 0.0f };

private:
    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BusGovernorAudioProcessor)

    // DSP state
    float a = 1.0f;
    float b = 1.0f;

    // UI smoothing for meter
    float bSmooth = 1.0f;
};
