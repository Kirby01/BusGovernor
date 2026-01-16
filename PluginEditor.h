/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class BusGovernorAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    BusGovernorAudioProcessorEditor (BusGovernorAudioProcessor&);
    ~BusGovernorAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    BusGovernorAudioProcessor& audioProcessor;

    // Background + needle smoothing value (still named lamp, but now it drives the needle)
    juce::Image backgroundImage;
    float lamp = 0.0f; // 0..1

    //==============================================================================
    // Controls
    juce::Slider pressureSlider;
    juce::Slider driveSlider;
    juce::Slider volumeSlider;   // (replaces makeup)

    juce::Label pressureLabel;
    juce::Label driveLabel;
    juce::Label volumeLabel;     // (replaces makeup)

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SliderAttachment> pressureAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> volumeAttachment; // (replaces makeup)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BusGovernorAudioProcessorEditor)
};
