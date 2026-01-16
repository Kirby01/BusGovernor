/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static void setupKnob (juce::Slider& s)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 18);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
}

static void setupLabel (juce::Label& l, const juce::String& text, juce::Component& attachTo)
{
    l.setText (text, juce::dontSendNotification);
    l.setJustificationType (juce::Justification::centred);
    l.attachToComponent (&attachTo, false);
    l.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.9f));
}

//==============================================================================
BusGovernorAudioProcessorEditor::BusGovernorAudioProcessorEditor (BusGovernorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setSize (400, 300);

    // Load background once (cached)
    backgroundImage = juce::ImageCache::getFromMemory (
        BinaryData::bak_png,
        BinaryData::bak_pngSize
    );

    // ---- Controls ----
    setupKnob (pressureSlider);
    setupKnob (driveSlider);
    setupKnob (volumeSlider);

    addAndMakeVisible (pressureSlider);
    addAndMakeVisible (driveSlider);
    addAndMakeVisible (volumeSlider);

    setupLabel (pressureLabel, "Pressure", pressureSlider);
    setupLabel (driveLabel,    "Drive",    driveSlider);
    setupLabel (volumeLabel,   "Volume",   volumeSlider);

    // Attach to APVTS params (defaults come from parameter layout)
    pressureAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                             BusGovernorAudioProcessor::paramPressureId,
                                                             pressureSlider);

    driveAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                          BusGovernorAudioProcessor::paramDriveId,
                                                          driveSlider);

    volumeAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                           BusGovernorAudioProcessor::paramVolumeId,
                                                           volumeSlider);

    startTimerHz (30); // smooth needle, low CPU
}

BusGovernorAudioProcessorEditor::~BusGovernorAudioProcessorEditor() {}

//==============================================================================
void BusGovernorAudioProcessorEditor::timerCallback()
{
    const float b = audioProcessor.bMeter.load (std::memory_order_relaxed);

    // Map b -> needle amount (0..1). b ~ 1 => near zero.
    float target = std::log1p (juce::jmax (0.0f, b - 1.0f));   // 0..inf
    target = juce::jlimit (0.0f, 1.0f, target * 0.85f);       // scale into 0..1

    // Mechanical-ish smoothing
    lamp = 0.92f * lamp + 0.08f * target;

    repaint();
}

void BusGovernorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);

    // Background
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat());

    // Slight dark overlay so text & needle pop
    g.setColour (juce::Colours::black.withAlpha (0.22f));
    g.fillRect (getLocalBounds());

    // Title
    g.setColour (juce::Colours::white.withAlpha (0.9f));
    g.setFont (15.0f);
    g.drawFittedText ("BusGovernor v2.0 - Harmonious Records",
                      getLocalBounds().reduced (8),
                      juce::Justification::topLeft,
                      1);

    // ---- Governor Needle (driven by b) ----
    {
        auto area = getLocalBounds().toFloat();

        // Gauge bounds (top-right)
        auto gauge = area.removeFromTop (90.0f).removeFromRight (120.0f).reduced (10.0f);

        // Make it square-ish
        float size = juce::jmin (gauge.getWidth(), gauge.getHeight());
        gauge = gauge.withSizeKeepingCentre (size, size);

        auto c = gauge.getCentre();
        float r = size * 0.44f;      // arc radius
        float thickness = 3.5f;

        // Arc range
        const float startA = juce::MathConstants<float>::pi * 1.15f;
        const float endA   = juce::MathConstants<float>::pi * 1.85f;

        // Background arc (Path for older JUCE compatibility)
        g.setColour (juce::Colours::white.withAlpha (0.18f));
        juce::Path arcPath;
        arcPath.addArc (c.x - r, c.y - r,
                        r * 2.0f, r * 2.0f,
                        startA, endA,
                        true);
        g.strokePath (arcPath, juce::PathStrokeType (thickness));

        // Tick marks
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        const int ticks = 7;
        for (int i = 0; i < ticks; ++i)
        {
            float t = (float) i / (float) (ticks - 1);
            float a = startA + t * (endA - startA);

            float x1 = c.x + std::cos (a) * (r - 2.0f);
            float y1 = c.y + std::sin (a) * (r - 2.0f);
            float x2 = c.x + std::cos (a) * (r + 7.0f);
            float y2 = c.y + std::sin (a) * (r + 7.0f);

            g.drawLine (x1, y1, x2, y2, 1.0f);
        }

        // Needle angle from smoothed value (lamp is now needle 0..1)
        float needleA = startA + lamp * (endA - startA);

        // Needle shadow
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (c.x + 1.0f, c.y + 1.0f,
                    c.x + 1.0f + std::cos (needleA) * (r - 6.0f),
                    c.y + 1.0f + std::sin (needleA) * (r - 6.0f),
                    3.0f);

        // Needle
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.drawLine (c.x, c.y,
                    c.x + std::cos (needleA) * (r - 6.0f),
                    c.y + std::sin (needleA) * (r - 6.0f),
                    2.6f);

        // Hub
        g.setColour (juce::Colours::white.withAlpha (0.35f));
        g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f, 1.0f);

        // Label
        g.setColour (juce::Colours::white.withAlpha (0.65f));
        g.setFont (11.0f);
        g.drawFittedText ("GOV",
                          gauge.toNearestInt().withTrimmedTop ((int) (size * 0.62f)),
                          juce::Justification::centredTop, 1);
    }
}

void BusGovernorAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();

    // Bottom area for knobs
    auto bottom = bounds.removeFromBottom (120).reduced (18);

    auto knobW = bottom.getWidth() / 3;

    pressureSlider.setBounds (bottom.removeFromLeft (knobW).reduced (10, 10));
    driveSlider.setBounds    (bottom.removeFromLeft (knobW).reduced (10, 10));
    volumeSlider.setBounds   (bottom.removeFromLeft (knobW).reduced (10, 10));
}
