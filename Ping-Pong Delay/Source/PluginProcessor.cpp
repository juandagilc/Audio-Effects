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

PingPongDelayAudioProcessor::PingPongDelayAudioProcessor():
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
    , paramBalance (parameters, "Balance input", "", 0.0f, 1.0f, 0.25f)
    , paramDelayTime (parameters, "Delay time", "s", 0.0f, 5.0f, 0.1f)
    , paramFeedback (parameters, "Feedback", "", 0.0f, 0.9f, 0.7f)
    , paramMix (parameters, "Mix", "", 0.0f, 1.0f, 1.0f)
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

PingPongDelayAudioProcessor::~PingPongDelayAudioProcessor()
{
}

//==============================================================================

void PingPongDelayAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramBalance.reset (sampleRate, smoothTime);
    paramDelayTime.reset (sampleRate, smoothTime);
    paramFeedback.reset (sampleRate, smoothTime);
    paramMix.reset (sampleRate, smoothTime);

    //======================================

    float maxDelayTime = paramDelayTime.maxValue;
    delayBufferSamples = (int)(maxDelayTime * (float)sampleRate) + 1;
    if (delayBufferSamples < 1)
        delayBufferSamples = 1;

    delayBufferChannels = getTotalNumInputChannels();
    delayBuffer.setSize (delayBufferChannels, delayBufferSamples);
    delayBuffer.clear();

    delayWritePosition = 0;
}

void PingPongDelayAudioProcessor::releaseResources()
{
}

void PingPongDelayAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float currentBalance = paramBalance.getNextValue();
    float currentDelayTime = paramDelayTime.getTargetValue() * (float)getSampleRate();
    float currentFeedback = paramFeedback.getNextValue();
    float currentMix = paramMix.getNextValue();

    int localWritePosition = delayWritePosition;

    float* channelDataL = buffer.getWritePointer (0);
    float* channelDataR = buffer.getWritePointer (1);
    float* delayDataL = delayBuffer.getWritePointer (0);
    float* delayDataR = delayBuffer.getWritePointer (1);

    for (int sample = 0; sample < numSamples; ++sample) {
        const float inL = (1.0f - currentBalance) * channelDataL[sample];
        const float inR = currentBalance * channelDataR[sample];
        float outL = 0.0f;
        float outR = 0.0f;

        float readPosition =
            fmodf ((float)localWritePosition - currentDelayTime + (float)delayBufferSamples, delayBufferSamples);
        int localReadPosition = floorf (readPosition);

        if (localReadPosition != localWritePosition) {
            float fraction = readPosition - (float)localReadPosition;
            float delayed1L = delayDataL[(localReadPosition + 0)];
            float delayed1R = delayDataR[(localReadPosition + 0)];
            float delayed2L = delayDataL[(localReadPosition + 1) % delayBufferSamples];
            float delayed2R = delayDataR[(localReadPosition + 1) % delayBufferSamples];
            outL = delayed1L + fraction * (delayed2L - delayed1L);
            outR = delayed1R + fraction * (delayed2R - delayed1R);

            channelDataL[sample] = inL + currentMix * (outL - inL);
            channelDataR[sample] = inR + currentMix * (outR - inR);
            delayDataL[localWritePosition] = inL + outR * currentFeedback;
            delayDataR[localWritePosition] = inR + outL * currentFeedback;
        }

        if (++localWritePosition >= delayBufferSamples)
            localWritePosition -= delayBufferSamples;
    }

    delayWritePosition = localWritePosition;

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================






//==============================================================================

void PingPongDelayAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.valueTreeState.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PingPongDelayAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* PingPongDelayAudioProcessor::createEditor()
{
    return new PingPongDelayAudioProcessorEditor (*this);
}

bool PingPongDelayAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool PingPongDelayAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String PingPongDelayAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PingPongDelayAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PingPongDelayAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PingPongDelayAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PingPongDelayAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int PingPongDelayAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PingPongDelayAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PingPongDelayAudioProcessor::setCurrentProgram (int index)
{
}

const String PingPongDelayAudioProcessor::getProgramName (int index)
{
    return {};
}

void PingPongDelayAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PingPongDelayAudioProcessor();
}

//==============================================================================
