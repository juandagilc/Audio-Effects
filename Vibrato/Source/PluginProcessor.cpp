/*
  ==============================================================================

    This code is based on the contents of the book: "Audio Effects: Theory,
    Implementation and Application" by Joshua D. Reiss and Andrew P. McPherson.

    Code by Juan Gil <https://juangil.com/>.
    Copyright (C) 2017-2019 Juan Gil.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PluginParameter.h"

//==============================================================================

VibratoAudioProcessor::VibratoAudioProcessor():
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
    , paramWidth (parameters, "Width", "ms", 1.0f, 50.0f, 10.0f, [](float value){ return value * 0.001f; })
    , paramFrequency (parameters, "LFO Frequency", "Hz", 0.0f, 10.0f, 2.0f)
    , paramWaveform (parameters, "LFO Waveform", waveformItemsUI, waveformSine)
    , paramInterpolation (parameters, "Interpolation", interpolationItemsUI, interpolationLinear)
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

VibratoAudioProcessor::~VibratoAudioProcessor()
{
}

//==============================================================================

void VibratoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramWidth.reset (sampleRate, smoothTime);
    paramFrequency.reset (sampleRate, smoothTime);
    paramWaveform.reset (sampleRate, smoothTime);
    paramInterpolation.reset (sampleRate, smoothTime);

    //======================================

    float maxDelayTime = paramWidth.maxValue;
    delayBufferSamples = (int)(maxDelayTime * (float)sampleRate) + 1;
    if (delayBufferSamples < 1)
        delayBufferSamples = 1;

    delayBufferChannels = getTotalNumInputChannels();
    delayBuffer.setSize (delayBufferChannels, delayBufferSamples);
    delayBuffer.clear();

    delayWritePosition = 0;
    lfoPhase = 0.0f;
    inverseSampleRate = 1.0f / (float)sampleRate;
    twoPi = 2.0f * M_PI;
}

void VibratoAudioProcessor::releaseResources()
{
}

void VibratoAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float currentWidth = paramWidth.getNextValue();
    float currentFrequency = paramFrequency.getNextValue();

    int localWritePosition;
    float phase;

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);
        float* delayData = delayBuffer.getWritePointer (channel);
        localWritePosition = delayWritePosition;
        phase = lfoPhase;

        for (int sample = 0; sample < numSamples; ++sample) {
            const float in = channelData[sample];
            float out = 0.0f;

            float localDelayTime = currentWidth * lfo (phase, (int)paramWaveform.getTargetValue()) * (float)getSampleRate();

            float readPosition =
                fmodf ((float)localWritePosition - localDelayTime + (float)delayBufferSamples - 1.0f, delayBufferSamples);
            int localReadPosition = floorf (readPosition);

            switch ((int)paramInterpolation.getTargetValue()) {
                case interpolationNearestNeighbour: {
                    float closestSample = delayData[localReadPosition % delayBufferSamples];
                    out = closestSample;
                    break;
                }
                case interpolationLinear: {
                    float fraction = readPosition - (float)localReadPosition;
                    float delayed0 = delayData[(localReadPosition + 0)];
                    float delayed1 = delayData[(localReadPosition + 1) % delayBufferSamples];
                    out = delayed0 + fraction * (delayed1 - delayed0);
                    break;
                }
                case interpolationCubic: {
                    float fraction = readPosition - (float)localReadPosition;
                    float fractionSqrt = fraction * fraction;
                    float fractionCube = fractionSqrt * fraction;

                    float sample0 = delayData[(localReadPosition - 1 + delayBufferSamples) % delayBufferSamples];
                    float sample1 = delayData[(localReadPosition + 0)];
                    float sample2 = delayData[(localReadPosition + 1) % delayBufferSamples];
                    float sample3 = delayData[(localReadPosition + 2) % delayBufferSamples];

                    float a0 = - 0.5f * sample0 + 1.5f * sample1 - 1.5f * sample2 + 0.5f * sample3;
                    float a1 = sample0 - 2.5f * sample1 + 2.0f * sample2 - 0.5f * sample3;
                    float a2 = - 0.5f * sample0 + 0.5f * sample2;
                    float a3 = sample1;
                    out = a0 * fractionCube + a1 * fractionSqrt + a2 * fraction + a3;
                    break;
                }
            }

            channelData[sample] = out;
            delayData[localWritePosition] = in;

            if (++localWritePosition >= delayBufferSamples)
                localWritePosition -= delayBufferSamples;

            phase += currentFrequency * inverseSampleRate;
            if (phase >= 1.0f)
                phase -= 1.0f;
        }
    }

    delayWritePosition = localWritePosition;
    lfoPhase = phase;

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================

float VibratoAudioProcessor::lfo (float phase, int waveform)
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
    }

    return out;
}

//==============================================================================






//==============================================================================

void VibratoAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    ScopedPointer<XmlElement> xml (parameters.valueTreeState.state.createXml());
    copyXmlToBinary (*xml, destData);
}

void VibratoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.state = ValueTree::fromXml (*xmlState);
}

//==============================================================================

AudioProcessorEditor* VibratoAudioProcessor::createEditor()
{
    return new VibratoAudioProcessorEditor (*this);
}

bool VibratoAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool VibratoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String VibratoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool VibratoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool VibratoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool VibratoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double VibratoAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int VibratoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int VibratoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void VibratoAudioProcessor::setCurrentProgram (int index)
{
}

const String VibratoAudioProcessor::getProgramName (int index)
{
    return {};
}

void VibratoAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VibratoAudioProcessor();
}

//==============================================================================
