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

RobotizationWhisperizationAudioProcessor::RobotizationWhisperizationAudioProcessor():
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
    stft (*this), parameters (*this)
    , paramEffect (parameters, "Effect", effectItemsUI, effectPassThrough)
    , paramFftSize (parameters, "FFT size", fftSizeItemsUI, fftSize512,
                    [this](float value){
                        const ScopedLock sl (lock);
                        value = (float)(1 << ((int)value + 5));
                        paramFftSize.setCurrentAndTargetValue (value);
                        stft.updateParameters((int)paramFftSize.getTargetValue(),
                                              (int)paramHopSize.getTargetValue(),
                                              (int)paramWindowType.getTargetValue());
                        return value;
                    })
    , paramHopSize (parameters, "Hop size", hopSizeItemsUI, hopSize8,
                    [this](float value){
                        const ScopedLock sl (lock);
                        value = (float)(1 << ((int)value + 1));
                        paramHopSize.setCurrentAndTargetValue (value);
                        stft.updateParameters((int)paramFftSize.getTargetValue(),
                                              (int)paramHopSize.getTargetValue(),
                                              (int)paramWindowType.getTargetValue());
                        return value;
                    })
    , paramWindowType (parameters, "Window type", windowTypeItemsUI, STFT::windowTypeHann,
                       [this](float value){
                           const ScopedLock sl (lock);
                           paramWindowType.setCurrentAndTargetValue (value);
                           stft.updateParameters((int)paramFftSize.getTargetValue(),
                                                 (int)paramHopSize.getTargetValue(),
                                                 (int)paramWindowType.getTargetValue());
                           return value;
                       })
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

RobotizationWhisperizationAudioProcessor::~RobotizationWhisperizationAudioProcessor()
{
}

//==============================================================================

void RobotizationWhisperizationAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramEffect.reset (sampleRate, smoothTime);
    paramFftSize.reset (sampleRate, smoothTime);
    paramHopSize.reset (sampleRate, smoothTime);
    paramWindowType.reset (sampleRate, smoothTime);

    //======================================

    stft.setup (getTotalNumInputChannels());
    stft.updateParameters((int)paramFftSize.getTargetValue(),
                          (int)paramHopSize.getTargetValue(),
                          (int)paramWindowType.getTargetValue());
}

void RobotizationWhisperizationAudioProcessor::releaseResources()
{
}

void RobotizationWhisperizationAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    const ScopedLock sl (lock);

    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    stft.processBlock (buffer);

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================






//==============================================================================

void RobotizationWhisperizationAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.valueTreeState.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void RobotizationWhisperizationAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* RobotizationWhisperizationAudioProcessor::createEditor()
{
    return new RobotizationWhisperizationAudioProcessorEditor (*this);
}

bool RobotizationWhisperizationAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool RobotizationWhisperizationAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String RobotizationWhisperizationAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool RobotizationWhisperizationAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool RobotizationWhisperizationAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool RobotizationWhisperizationAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double RobotizationWhisperizationAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int RobotizationWhisperizationAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int RobotizationWhisperizationAudioProcessor::getCurrentProgram()
{
    return 0;
}

void RobotizationWhisperizationAudioProcessor::setCurrentProgram (int index)
{
}

const String RobotizationWhisperizationAudioProcessor::getProgramName (int index)
{
    return {};
}

void RobotizationWhisperizationAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RobotizationWhisperizationAudioProcessor();
}

//==============================================================================
