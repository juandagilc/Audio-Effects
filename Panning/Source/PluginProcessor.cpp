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

PanningAudioProcessor::PanningAudioProcessor():
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
    , paramMethod (parameters, "Method", methodItemsUI, methodItdIld)
    , paramPanning (parameters, "Panning", "", -1.0f, 1.0f, 0.5f)
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

PanningAudioProcessor::~PanningAudioProcessor()
{
}

//==============================================================================

void PanningAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramMethod.reset (sampleRate, smoothTime);
    paramPanning.reset (sampleRate, smoothTime);

    //======================================

    maximumDelayInSamples = (int)(1e-3f * (float)getSampleRate());
    delayLineL.setup (maximumDelayInSamples);
    delayLineR.setup (maximumDelayInSamples);
}

void PanningAudioProcessor::releaseResources()
{
}

void PanningAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    float currentPanning = paramPanning.getNextValue();

    float* channelDataL = buffer.getWritePointer (0);
    float* channelDataR = buffer.getWritePointer (1);

    switch ((int)paramMethod.getTargetValue()) {

        //======================================

        case methodPanoramaPrecedence: {
            // Panorama
            float theta = degreesToRadians (30.0f);
            float phi = -currentPanning * theta;
            float cos_theta = cosf (theta);
            float cos_phi = cosf (phi);
            float sin_theta = sinf (theta);
            float sin_phi = sinf (phi);
            float gainL = (cos_phi * sin_theta + sin_phi * cos_theta);
            float gainR = (cos_phi * sin_theta - sin_phi * cos_theta);
            float norm = 1.0f / sqrtf (gainL * gainL + gainR * gainR);

            // Precedence
            float delayFactor = (currentPanning + 1.0f) / 2.0f;
            float delayTimeL = (float)maximumDelayInSamples * (delayFactor);
            float delayTimeR = (float)maximumDelayInSamples * (1.0f - delayFactor);
            for (int sample = 0; sample < numSamples; ++sample) {
                const float in = channelDataL[sample];
                delayLineL.writeSample (in);
                delayLineR.writeSample (in);
                channelDataL[sample] = delayLineL.readSample (delayTimeL) * gainL * norm;
                channelDataR[sample] = delayLineR.readSample (delayTimeR) * gainR * norm;
            }
            break;
        }

        //======================================

        case methodItdIld: {
            float headRadius = 8.5e-2f;
            float speedOfSound = 340.0f;
            float headFactor = (float)getSampleRate() * headRadius / speedOfSound;

            // Interaural Time Difference (ITD)
            auto Td = [headFactor](const float angle){
                if (abs (angle) < (float)M_PI_2)
                    return headFactor * (1.0f - cosf (angle));
                else
                    return headFactor * (abs (angle) + 1.0f - (float)M_PI_2);
            };
            float theta = degreesToRadians (90.0f);
            float phi = currentPanning * theta;
            float currentDelayTimeL = Td (phi + (float)M_PI_2);
            float currentDelayTimeR = Td (phi - (float)M_PI_2);
            for (int sample = 0; sample < numSamples; ++sample) {
                const float in = channelDataL[sample];
                delayLineL.writeSample (in);
                delayLineR.writeSample (in);
                channelDataL[sample] = delayLineL.readSample (currentDelayTimeL);
                channelDataR[sample] = delayLineR.readSample (currentDelayTimeR);
            }

            // Interaural Level Difference (ILD)
            filterL.updateCoefficients ((double)phi + M_PI_2, (double)(headRadius / speedOfSound));
            filterR.updateCoefficients ((double)phi - M_PI_2, (double)(headRadius / speedOfSound));
            filterL.processSamples (channelDataL, numSamples);
            filterR.processSamples (channelDataR, numSamples);
            break;
        }
    }

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================






//==============================================================================

void PanningAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    auto state = parameters.valueTreeState.copyState();
    std::unique_ptr<XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void PanningAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.replaceState (ValueTree::fromXml (*xmlState));
}

//==============================================================================

AudioProcessorEditor* PanningAudioProcessor::createEditor()
{
    return new PanningAudioProcessorEditor (*this);
}

bool PanningAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool PanningAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String PanningAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool PanningAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool PanningAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool PanningAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double PanningAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

//==============================================================================

int PanningAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int PanningAudioProcessor::getCurrentProgram()
{
    return 0;
}

void PanningAudioProcessor::setCurrentProgram (int index)
{
}

const String PanningAudioProcessor::getProgramName (int index)
{
    return {};
}

void PanningAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PanningAudioProcessor();
}

//==============================================================================
