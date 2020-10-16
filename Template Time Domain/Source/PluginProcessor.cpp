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

TemplateTimeDomainAudioProcessor::TemplateTimeDomainAudioProcessor():
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
    , parameter1 (parameters, "Parameter 1", "", 0.0f, 1.0f, 0.5f, [](float value){ return value * 127.0f; })
    , parameter2 (parameters, "Parameter 2", "", 0.0f, 1.0f, 0.5f)
    , parameter3 (parameters, "Parameter 3", false, [](float value){ return value * (-2.0f) + 1.0f; })
    , parameter4 (parameters, "Parameter 4", {"Option A", "Option B"}, 1)
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

TemplateTimeDomainAudioProcessor::~TemplateTimeDomainAudioProcessor()
{
}

//==============================================================================

void TemplateTimeDomainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    parameter1.reset (sampleRate, smoothTime);
    parameter2.reset (sampleRate, smoothTime);
    parameter3.reset (sampleRate, smoothTime);
    parameter4.reset (sampleRate, smoothTime);
}

void TemplateTimeDomainAudioProcessor::releaseResources()
{
}

void TemplateTimeDomainAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float currentParameter2 = parameter2.getNextValue();
    float currentParameter3 = parameter3.getNextValue();
    float currentParameter4 = parameter4.getNextValue();

    float factor = currentParameter2 * currentParameter3 * currentParameter4;

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);

        for (int sample = 0; sample < numSamples; ++sample) {
            const float in = channelData[sample];
            float out = in * factor;

            channelData[sample] = out;
        }
    }

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);

    //======================================

    MidiBuffer processedMidi;
    for (const MidiMessageMetadata meta : midiMessages) {
        MidiMessage message = meta.getMessage();
        int time = meta.samplePosition;

        if (message.isNoteOn()) {
            uint8 newVel = (uint8)(parameter1.getTargetValue());
            message = MidiMessage::noteOn (message.getChannel(), message.getNoteNumber(), newVel);
        }
        processedMidi.addEvent (message, time);
    }

    midiMessages.swapWith (processedMidi);
}

//==============================================================================






//==============================================================================

void TemplateTimeDomainAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.valueTreeState.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TemplateTimeDomainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* TemplateTimeDomainAudioProcessor::createEditor()
{
    return new TemplateTimeDomainAudioProcessorEditor (*this);
}

bool TemplateTimeDomainAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool TemplateTimeDomainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String TemplateTimeDomainAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TemplateTimeDomainAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TemplateTimeDomainAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TemplateTimeDomainAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TemplateTimeDomainAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int TemplateTimeDomainAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TemplateTimeDomainAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TemplateTimeDomainAudioProcessor::setCurrentProgram (int index)
{
}

const String TemplateTimeDomainAudioProcessor::getProgramName (int index)
{
    return {};
}

void TemplateTimeDomainAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TemplateTimeDomainAudioProcessor();
}

//==============================================================================
