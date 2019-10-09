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
                      [this](float value){ paramFrequency.setCurrentAndTargetValue (value); updateFilters(); return value; })
    , paramQfactor (parameters, "Q Factor", "", 0.1f, 20.0f, 10.0f,
                    [this](float value){ paramQfactor.setCurrentAndTargetValue (value); updateFilters(); return value; })
    , paramGain (parameters, "Gain", "dB", 0.0f, 20.0f, 20.0f,
                 [this](float value){ paramGain.setCurrentAndTargetValue (value); updateFilters(); return value; })
    , paramFilterType (parameters, "Filter type", filterTypeItemsUI, filterTypeResonantLowPass,
                       [this](float value){ paramFilterType.setCurrentAndTargetValue (value); updateFilters(); return value; })
    , paramLFOfrequency (parameters, "LFO Frequency", "Hz", 0.0f, 5.0f, 2.0f)
    , paramMixLFOandEnvelope (parameters, "LFO/Env", "", 0.0f, 1.0f, 0.8f)
    , paramEnvelopeAttack (parameters, "Env. Attack", "ms", 0.1f, 100.0f, 2.0f, [](float value){ return value * 0.001f; })
    , paramEnvelopeRelease (parameters, "Env. Release", "ms", 10.0f, 1000.0f, 300.0f, [](float value){ return value * 0.001f; })
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
    paramMixLFOandEnvelope.reset (sampleRate, smoothTime);
    paramEnvelopeAttack.reset (sampleRate, smoothTime);
    paramEnvelopeRelease.reset (sampleRate, smoothTime);

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

    for (int i = 0; i < getTotalNumInputChannels(); ++i)
        envelopes.add (0.0f);
    inverseE = 1.0f / M_E;
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

            float absIn = fabs (in);
            float envelope;
            float attack = calculateAttackOrRelease (paramEnvelopeAttack.getNextValue());
            float release = calculateAttackOrRelease (paramEnvelopeRelease.getNextValue());

            if (absIn > envelopes[channel])
                envelope = attack * envelopes[channel] + (1.0f - attack) * absIn;
            else
                envelope = release * envelopes[channel] + (1.0f - release) * absIn;

            envelopes.set (channel, envelope);

            if (paramMode.getTargetValue() == modeAutomatic) {
                float centreFrequencyLFO = 0.5f + 0.5f * sinf (twoPi * phase);
                float centreFrequencyEnv = envelopes[channel];
                centreFrequency =
                    centreFrequencyLFO + paramMixLFOandEnvelope.getNextValue() * (centreFrequencyEnv - centreFrequencyLFO);

                centreFrequency *= paramFrequency.maxValue - paramFrequency.minValue;
                centreFrequency += paramFrequency.minValue;

                phase += paramLFOfrequency.getNextValue() * inverseSampleRate;
                if (phase >= 1.0f)
                    phase -= 1.0f;

                paramFrequency.setCurrentAndTargetValue (centreFrequency);
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

float WahWahAudioProcessor::calculateAttackOrRelease (float value)
{
    if (value == 0.0f)
        return 0.0f;
    else
        return pow (inverseE, inverseSampleRate / value);
}

//==============================================================================






//==============================================================================

void WahWahAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.valueTreeState.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void WahWahAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* WahWahAudioProcessor::createEditor()
{
    return new WahWahAudioProcessorEditor (*this);
}

bool WahWahAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
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

//==============================================================================

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
