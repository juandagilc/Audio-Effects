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

WahWahAudioProcessor::WahWahAudioProcessor():
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
    , paramMode (parameters, "Mode", modeItemsUI, modeManual)
    , paramMix (parameters, "Mix", "", 0.0f, 1.0f, 0.5f)
    , paramFrequency (parameters, "Frequency", "Hz", 200.0f, 1300.0f, 300.0f,
                      [this](float value){ paramFrequency.setValue (value); updateFilters(); return value; })
    , paramQfactor (parameters, "Q Factor", "", 0.1f, 20.0f, 10.0f,
                    [this](float value){ paramQfactor.setValue (value); updateFilters(); return value; })
    , paramGain (parameters, "Gain", "dB", 0.0f, 20.0f, 20.0f,
                 [this](float value){ paramGain.setValue (value); updateFilters(); return value; })
    , paramFilterType (parameters, "Filter type", filterTypeItemsUI, filterTypeResonantLowPass,
                       [this](float value){ paramFilterType.setValue (value); updateFilters(); return value; })
    , paramLFOfrequency (parameters, "LFO Frequency", "Hz", 0.0f, 5.0f, 2.0f)
{
    centreFrequency = paramFrequency.getTargetValue();
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

WahWahAudioProcessor::~WahWahAudioProcessor()
{
}

//==============================================================================

void WahWahAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramMode.reset (sampleRate, smoothTime);
    paramMix.reset (sampleRate, smoothTime);
    paramFrequency.reset (sampleRate, smoothTime);
    paramQfactor.reset (sampleRate, smoothTime);
    paramGain.reset (sampleRate, smoothTime);
    paramFilterType.reset (sampleRate, smoothTime);
    paramLFOfrequency.reset (sampleRate, smoothTime);

    //======================================

    filters.clear();
    for (int i = 0; i < getTotalNumInputChannels(); ++i) {
        Filter* filter;
        filters.add (filter = new Filter());
    }
    updateFilters();

    lfoPhase = 0.0f;
    inverseSampleRate = 1.0f / (float)sampleRate;
    twoPi = 2.0f * M_PI;
}

void WahWahAudioProcessor::releaseResources()
{
}

void WahWahAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float phase;

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);
        phase = lfoPhase;

        for (int sample = 0; sample < numSamples; ++sample) {
            float in = channelData[sample];

            if (paramMode.getTargetValue() == modeAutomatic) {
                centreFrequency = 0.5f + 0.5f * sinf (twoPi * phase);
                centreFrequency *= paramFrequency.maxValue - paramFrequency.minValue;
                centreFrequency += paramFrequency.minValue;

                phase += paramLFOfrequency.getNextValue() * inverseSampleRate;
                if (phase >= 1.0f)
                    phase -= 1.0f;

                paramFrequency.setValue (centreFrequency);
                updateFilters();
            }

            float filtered = filters[channel]->processSingleSampleRaw (in);
            float out = in + paramMix.getNextValue() * (filtered - in);
            channelData[sample] = out;
        }
    }

    lfoPhase = phase;

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================

void WahWahAudioProcessor::updateFilters()
{
    double discreteFrequency = 2.0 * M_PI * (double)paramFrequency.getTargetValue() / getSampleRate();
    double qFactor = (double)paramQfactor.getTargetValue();
    double gain = pow (10.0, (double)paramGain.getTargetValue() * 0.05);
    int type = (int)paramFilterType.getTargetValue();

    for (int i = 0; i < filters.size(); ++i)
        filters[i]->updateCoefficients (discreteFrequency, qFactor, gain, type);
}

//==============================================================================






//==============================================================================

void WahWahAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    ScopedPointer<XmlElement> xml (parameters.valueTreeState.state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WahWahAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.state = ValueTree::fromXml (*xmlState);
}

//==============================================================================

bool WahWahAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* WahWahAudioProcessor::createEditor()
{
    return new WahWahAudioProcessorEditor (*this);
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool WahWahAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String WahWahAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool WahWahAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool WahWahAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool WahWahAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double WahWahAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int WahWahAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int WahWahAudioProcessor::getCurrentProgram()
{
    return 0;
}

void WahWahAudioProcessor::setCurrentProgram (int index)
{
}

const String WahWahAudioProcessor::getProgramName (int index)
{
    return {};
}

void WahWahAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WahWahAudioProcessor();
}

//==============================================================================
