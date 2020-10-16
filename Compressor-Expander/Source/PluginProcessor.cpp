/*
  ==============================================================================

    This code is based on the contents of the book: "Audio Effects: Theory,
    Implementation and Application" by Joshua D. Reiss and Andrew P. McPherson.

    Code by Juan Gil <https://juangil.com/>.
    Copyright (C) 2017-2020 Juan Gil.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameter.h"

//==============================================================================

CompressorExpanderAudioProcessor::CompressorExpanderAudioProcessor():
#ifndef JucePlugin_PreferredChannelConfigurations
    AudioProcessor (BusesProperties()
                    #if ! JucePlugin_IsMidiEffect
                     #if ! JucePlugin_IsSynth
                      .withInput  ("Input",  AudioChannelSet::stereo(), true)
                     #endif
                      .withOutput ("Output", AudioChannelSet::stereo(), true)
                    #endif
                   ),
#endif
    parameters (*this)
    , paramMode (parameters, "Mode", {"Compressor / Limiter", "Expander / Noise gate"}, 1)
    , paramThreshold (parameters, "Threshold", "dB", -60.0f, 0.0f, -24.0f)
    , paramRatio (parameters, "Ratio", ":1", 1.0f, 100.0f, 50.0f)
    , paramAttack (parameters, "Attack", "ms", 0.1f, 100.0f, 2.0f, [](float value){ return value * 0.001f; })
    , paramRelease (parameters, "Release", "ms", 10.0f, 1000.0f, 300.0f, [](float value){ return value * 0.001f; })
    , paramMakeupGain (parameters, "Makeup gain", "dB", -12.0f, 12.0f, 0.0f)
    , paramBypass (parameters, "Bypass")
{
    parameters.apvts.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

CompressorExpanderAudioProcessor::~CompressorExpanderAudioProcessor()
{
}

//==============================================================================

void CompressorExpanderAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramThreshold.reset (sampleRate, smoothTime);
    paramRatio.reset (sampleRate, smoothTime);
    paramAttack.reset (sampleRate, smoothTime);
    paramRelease.reset (sampleRate, smoothTime);
    paramMakeupGain.reset (sampleRate, smoothTime);
    paramBypass.reset (sampleRate, smoothTime);

    //======================================

    mixedDownInput.setSize (1, samplesPerBlock);

    inputLevel = 0.0f;
    ylPrev = 0.0f;

    inverseSampleRate = 1.0f / (float)getSampleRate();
    inverseE = 1.0f / M_E;
}

void CompressorExpanderAudioProcessor::releaseResources()
{
}

void CompressorExpanderAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    if ((bool)paramBypass.getTargetValue())
        return;

    //======================================

    mixedDownInput.clear();
    for (int channel = 0; channel < numInputChannels; ++channel)
        mixedDownInput.addFrom (0, 0, buffer, channel, 0, numSamples, 1.0f / numInputChannels);

    for (int sample = 0; sample < numSamples; ++sample) {
        bool expander = (bool)paramMode.getTargetValue();
        float T = paramThreshold.getNextValue();
        float R = paramRatio.getNextValue();
        float alphaA = calculateAttackOrRelease (paramAttack.getNextValue());
        float alphaR = calculateAttackOrRelease (paramRelease.getNextValue());
        float makeupGain = paramMakeupGain.getNextValue();

        float inputSquared = powf (mixedDownInput.getSample (0, sample), 2.0f);
        if (expander) {
            const float averageFactor = 0.9999f;
            inputLevel = averageFactor * inputLevel + (1.0f - averageFactor) * inputSquared;
        } else {
            inputLevel = inputSquared;
        }
        xg = (inputLevel <= 1e-6f) ? -60.0f : 10.0f * log10f (inputLevel);

        // Expander
        if (expander) {
            if (xg > T)
                yg = xg;
            else
                yg = T + (xg - T) * R;

            xl = xg - yg;

            if (xl < ylPrev)
                yl = alphaA * ylPrev + (1.0f - alphaA) * xl;
            else
                yl = alphaR * ylPrev + (1.0f - alphaR) * xl;

        // Compressor
        } else {
            if (xg < T)
                yg = xg;
            else
                yg = T + (xg - T) / R;

            xl = xg - yg;

            if (xl > ylPrev)
                yl = alphaA * ylPrev + (1.0f - alphaA) * xl;
            else
                yl = alphaR * ylPrev + (1.0f - alphaR) * xl;
        }

        control = powf (10.0f, (makeupGain - yl) * 0.05f);
        ylPrev = yl;

        for (int channel = 0; channel < numInputChannels; ++channel) {
            float newValue = buffer.getSample (channel, sample) * control;
            buffer.setSample (channel, sample, newValue);
        }
    }

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================

float CompressorExpanderAudioProcessor::calculateAttackOrRelease (float value)
{
    if (value == 0.0f)
        return 0.0f;
    else
        return pow (inverseE, inverseSampleRate / value);
}

//==============================================================================






//==============================================================================

void CompressorExpanderAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.apvts.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void CompressorExpanderAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.apvts.state.getType()))
            parameters.apvts.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* CompressorExpanderAudioProcessor::createEditor()
{
    return new CompressorExpanderAudioProcessorEditor (*this);
}

bool CompressorExpanderAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool CompressorExpanderAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

//==============================================================================

const String CompressorExpanderAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool CompressorExpanderAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool CompressorExpanderAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool CompressorExpanderAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double CompressorExpanderAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int CompressorExpanderAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int CompressorExpanderAudioProcessor::getCurrentProgram()
{
    return 0;
}

void CompressorExpanderAudioProcessor::setCurrentProgram (int index)
{
}

const String CompressorExpanderAudioProcessor::getProgramName (int index)
{
    return {};
}

void CompressorExpanderAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CompressorExpanderAudioProcessor();
}

//==============================================================================
