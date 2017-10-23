/*
  ==============================================================================

    This code is based on the contents of the book: "Audio Effects: Theory,
    Implementation and Application" by Joshua D. Reiss and Andrew P. McPherson.

    Code by Juan Gil <http://juangil.com/>.
    Copyright (C) 2017 Juan Gil.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameter.h"

//==============================================================================

RingModulationAudioProcessor::RingModulationAudioProcessor():
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
    , paramDepth (parameters, "Depth", "", 0.0f, 1.0f, 0.5f)
    , paramFrequency (parameters, "Carrier frequency", "Hz", 10.0f, 1000.0f, 200.0f)
    , paramWaveform (parameters, "Carrier waveform", waveformItemsUI, waveformSine)
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

RingModulationAudioProcessor::~RingModulationAudioProcessor()
{
}

//==============================================================================

void RingModulationAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramDepth.reset (sampleRate, smoothTime);
    paramFrequency.reset (sampleRate, smoothTime);
    paramWaveform.reset (sampleRate, smoothTime);

    //======================================

    lfoPhase = 0.0f;
    inverseSampleRate = 1.0f / (float)sampleRate;
    twoPi = 2.0f * M_PI;
}

void RingModulationAudioProcessor::releaseResources()
{
}

void RingModulationAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float currentDepth = paramDepth.getNextValue();
    float currentFrequency = paramFrequency.getNextValue();
    float phase;

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);
        phase = lfoPhase;

        for (int sample = 0; sample < numSamples; ++sample) {
            const float in = channelData[sample];
            float carrier = 2.0f * lfo (phase, (int)paramWaveform.getTargetValue()) - 1.0f;
            float out = in * (1 - currentDepth + currentDepth * carrier);

            channelData[sample] = out;

            phase += currentFrequency * inverseSampleRate;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }
    }

    lfoPhase = phase;

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================

float RingModulationAudioProcessor::lfo (float phase, int waveform)
{
    float out = 0.0f;

    switch (waveform) {
        case waveformSine: {
            out = 0.5f + 0.5f * sinf (twoPi * phase);
            break;
        }
        case waveformTriangle: {
            if (phase < 0.25f)
                out = 0.5f + 2.0f * phase;
            else if (phase < 0.75f)
                out = 1.0f - 2.0f * (phase - 0.25f);
            else
                out = 2.0f * (phase - 0.75f);
            break;
        }
        case waveformSawtooth: {
            if (phase < 0.5f)
                out = 0.5f + phase;
            else
                out = phase - 0.5f;
            break;
        }
        case waveformInverseSawtooth: {
            if (phase < 0.5f)
                out = 0.5f - phase;
            else
                out = 1.5f - phase;
            break;
        }
        case waveformSquare: {
            if (phase < 0.5f)
                out = 0.0f;
            else
                out = 1.0f;
            break;
        }
        case waveformSquareSlopedEdges: {
            if (phase < 0.48f)
                out = 1.0f;
            else if (phase < 0.5f)
                out = 1.0f - 50.0f * (phase - 0.48f);
            else if (phase < 0.98f)
                out = 0.0f;
            else
                out = 50.0f * (phase - 0.98f);
            break;
        }
    }

    return out;
}

//==============================================================================






//==============================================================================

void RingModulationAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    ScopedPointer<XmlElement> xml (parameters.valueTreeState.state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RingModulationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.state = ValueTree::fromXml (*xmlState);
}

//==============================================================================

bool RingModulationAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* RingModulationAudioProcessor::createEditor()
{
    return new RingModulationAudioProcessorEditor (*this);
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool RingModulationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String RingModulationAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RingModulationAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RingModulationAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RingModulationAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RingModulationAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int RingModulationAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RingModulationAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RingModulationAudioProcessor::setCurrentProgram (int index)
{
}

const String RingModulationAudioProcessor::getProgramName (int index)
{
    return {};
}

void RingModulationAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RingModulationAudioProcessor();
}

//==============================================================================
