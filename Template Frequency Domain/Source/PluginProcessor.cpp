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

TemplateFrequencyDomainAudioProcessor::TemplateFrequencyDomainAudioProcessor():
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
    , paramFftSize (parameters, "FFT size", fftSizeItemsUI, fftSize512,
                    [this](float value){
                        const ScopedLock sl (lock);
                        value = (float)(1 << ((int)value + 5));
                        paramFftSize.setValue (value);
                        updateFftSize();
                        updateHopSize();
                        updateWindow();
                        updateWindowScaleFactor();
                        return value;
                    })
    , paramHopSize (parameters, "Hop size", hopSizeItemsUI, hopSize8,
                    [this](float value){
                        const ScopedLock sl (lock);
                        value = (float)(1 << ((int)value + 1));
                        paramHopSize.setValue (value);
                        updateFftSize();
                        updateHopSize();
                        updateWindow();
                        updateWindowScaleFactor();
                        return value;
                    })
    , paramWindowType (parameters, "Window type", windowTypeItemsUI, windowTypeHann,
                       [this](float value){
                           const ScopedLock sl (lock);
                           paramWindowType.setValue (value);
                           updateFftSize();
                           updateHopSize();
                           updateWindow();
                           updateWindowScaleFactor();
                           return value;
                       })
{
    parameters.valueTreeState.state = ValueTree (Identifier (getName().removeCharacters ("- ")));
}

TemplateFrequencyDomainAudioProcessor::~TemplateFrequencyDomainAudioProcessor()
{
}

//==============================================================================

void TemplateFrequencyDomainAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const double smoothTime = 1e-3;
    paramFftSize.reset (sampleRate, smoothTime);
    paramHopSize.reset (sampleRate, smoothTime);
    paramWindowType.reset (sampleRate, smoothTime);
}

void TemplateFrequencyDomainAudioProcessor::releaseResources()
{
}

void TemplateFrequencyDomainAudioProcessor::processBlock (AudioSampleBuffer& buffer, MidiBuffer& midiMessages)
{
    const ScopedLock sl (lock);

    ScopedNoDenormals noDenormals;

    const int numInputChannels = getTotalNumInputChannels();
    const int numOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    //======================================

    int currentInputBufferWritePosition;
    int currentOutputBufferWritePosition;
    int currentOutputBufferReadPosition;
    int currentSamplesSinceLastFFT;

    for (int channel = 0; channel < numInputChannels; ++channel) {
        float* channelData = buffer.getWritePointer (channel);

        currentInputBufferWritePosition = inputBufferWritePosition;
        currentOutputBufferWritePosition = outputBufferWritePosition;
        currentOutputBufferReadPosition = outputBufferReadPosition;
        currentSamplesSinceLastFFT = samplesSinceLastFFT;

        for (int sample = 0; sample < numSamples; ++sample) {

            //======================================

            const float in = channelData[sample];
            channelData[sample] = outputBuffer.getSample (channel, currentOutputBufferReadPosition);

            //======================================

            outputBuffer.setSample (channel, currentOutputBufferReadPosition, 0.0f);
            if (++currentOutputBufferReadPosition >= outputBufferLength)
                currentOutputBufferReadPosition = 0;

            //======================================

            inputBuffer.setSample (channel, currentInputBufferWritePosition, in);
            if (++currentInputBufferWritePosition >= inputBufferLength)
                currentInputBufferWritePosition = 0;

            //======================================

            if (++currentSamplesSinceLastFFT >= hopSize) {
                currentSamplesSinceLastFFT = 0;

                //======================================

                int inputBufferIndex = currentInputBufferWritePosition;
                for (int index = 0; index < fftSize; ++index) {
                    fftTimeDomain[index].real (fftWindow[index] * inputBuffer.getSample (channel, inputBufferIndex));
                    fftTimeDomain[index].imag (0.0f);

                    if (++inputBufferIndex >= inputBufferLength)
                        inputBufferIndex = 0;
                }

                //======================================

                fft->perform (fftTimeDomain, fftFrequencyDomain, false);

                for (int index = 0; index < fftSize / 2 + 1; ++index) {
                    float magnitude = abs (fftFrequencyDomain[index]);
                    float phase = arg (fftFrequencyDomain[index]);

                    fftFrequencyDomain[index].real (magnitude * cosf (phase));
                    fftFrequencyDomain[index].imag (magnitude * sinf (phase));
                    if (index > 0 && index < fftSize / 2) {
                        fftFrequencyDomain[fftSize - index].real (magnitude * cosf (phase));
                        fftFrequencyDomain[fftSize - index].imag (magnitude * sinf (-phase));
                    }
                }

                fft->perform (fftFrequencyDomain, fftTimeDomain, true);

                //======================================

                int outputBufferIndex = currentOutputBufferWritePosition;
                for (int index = 0; index < fftSize; ++index) {
                    float out = outputBuffer.getSample (channel, outputBufferIndex);
                    out += fftTimeDomain[index].real() * windowScaleFactor;
                    outputBuffer.setSample (channel, outputBufferIndex, out);

                    if (++outputBufferIndex >= outputBufferLength)
                        outputBufferIndex = 0;
                }

                //======================================

                currentOutputBufferWritePosition += hopSize;
                if (currentOutputBufferWritePosition >= outputBufferLength)
                    currentOutputBufferWritePosition = 0;
            }

            //======================================
        }
    }

    inputBufferWritePosition = currentInputBufferWritePosition;
    outputBufferWritePosition = currentOutputBufferWritePosition;
    outputBufferReadPosition = currentOutputBufferReadPosition;
    samplesSinceLastFFT = currentSamplesSinceLastFFT;

    //======================================

    for (int channel = numInputChannels; channel < numOutputChannels; ++channel)
        buffer.clear (channel, 0, numSamples);
}

//==============================================================================

void TemplateFrequencyDomainAudioProcessor::updateFftSize()
{
    fftSize = (int)paramFftSize.getTargetValue();
    fft = new dsp::FFT (log2 (fftSize));

    inputBufferLength = fftSize;
    inputBufferWritePosition = 0;
    inputBuffer.clear();
    inputBuffer.setSize (getTotalNumInputChannels(), inputBufferLength);

    outputBufferLength = fftSize;
    outputBufferWritePosition = 0;
    outputBufferReadPosition = 0;
    outputBuffer.clear();
    outputBuffer.setSize (getTotalNumInputChannels(), outputBufferLength);

    fftWindow.realloc (fftSize);
    fftWindow.clear (fftSize);

    fftTimeDomain.realloc (fftSize);
    fftTimeDomain.clear (fftSize);

    fftFrequencyDomain.realloc (fftSize);
    fftFrequencyDomain.clear (fftSize);

    samplesSinceLastFFT = 0;
}

void TemplateFrequencyDomainAudioProcessor::updateHopSize()
{
    overlap = (int)paramHopSize.getTargetValue();
    if (overlap != 0) {
        hopSize = fftSize / overlap;
        outputBufferWritePosition = hopSize % outputBufferLength;
    }
}

void TemplateFrequencyDomainAudioProcessor::updateWindow()
{
    switch ((int)paramWindowType.getTargetValue()) {
        case windowTypeRectangular: {
            for (int sample = 0; sample < fftSize; ++sample)
                fftWindow[sample] = 1.0f;
            break;
        }
        case windowTypeBartlett: {
            for (int sample = 0; sample < fftSize; ++sample)
                fftWindow[sample] = 1.0f - fabs (2.0f * (float)sample / (float)(fftSize - 1) - 1.0f);
            break;
        }
        case windowTypeHann: {
            for (int sample = 0; sample < fftSize; ++sample)
                fftWindow[sample] = 0.5f - 0.5f * cosf (2.0f * M_PI * (float)sample / (float)(fftSize - 1));
            break;
        }
        case windowTypeHamming: {
            for (int sample = 0; sample < fftSize; ++sample)
                fftWindow[sample] = 0.54f - 0.46f * cosf (2.0f * M_PI * (float)sample / (float)(fftSize - 1));
            break;
        }
    }
}

void TemplateFrequencyDomainAudioProcessor::updateWindowScaleFactor()
{
    float windowSum = 0.0f;
    for (int sample = 0; sample < fftSize; ++sample)
        windowSum += fftWindow[sample];

    windowScaleFactor = 0.0f;
    if (overlap != 0 && windowSum != 0.0f)
        windowScaleFactor = 1.0f / (float)overlap / windowSum * (float)fftSize;
}

//==============================================================================






//==============================================================================

void TemplateFrequencyDomainAudioProcessor::getStateInformation (MemoryBlock& destData)
{
    ScopedPointer<XmlElement> xml (parameters.valueTreeState.state.createXml());
    copyXmlToBinary (*xml, destData);
}

void TemplateFrequencyDomainAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    ScopedPointer<XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr)
        if (xmlState->hasTagName (parameters.valueTreeState.state.getType()))
            parameters.valueTreeState.state = ValueTree::fromXml (*xmlState);
}

//==============================================================================

bool TemplateFrequencyDomainAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

AudioProcessorEditor* TemplateFrequencyDomainAudioProcessor::createEditor()
{
    return new TemplateFrequencyDomainAudioProcessorEditor (*this);
}

//==============================================================================

#ifndef JucePlugin_PreferredChannelConfigurations
bool TemplateFrequencyDomainAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
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

const String TemplateFrequencyDomainAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool TemplateFrequencyDomainAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool TemplateFrequencyDomainAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool TemplateFrequencyDomainAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double TemplateFrequencyDomainAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int TemplateFrequencyDomainAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int TemplateFrequencyDomainAudioProcessor::getCurrentProgram()
{
    return 0;
}

void TemplateFrequencyDomainAudioProcessor::setCurrentProgram (int index)
{
}

const String TemplateFrequencyDomainAudioProcessor::getProgramName (int index)
{
    return {};
}

void TemplateFrequencyDomainAudioProcessor::changeProgramName (int index, const String& newName)
{
}

//==============================================================================

// This creates new instances of the plugin..
AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new TemplateFrequencyDomainAudioProcessor();
}

//==============================================================================
