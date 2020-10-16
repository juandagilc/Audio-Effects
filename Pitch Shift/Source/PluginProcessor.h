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

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginParameter.h"

//==============================================================================

class PitchShiftAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    PitchShiftAudioProcessor();
    ~PitchShiftAudioProcessor();

    //==============================================================================

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (AudioSampleBuffer&, MidiBuffer&) override;

    //==============================================================================






    //==============================================================================

    void getStateInformation (MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================

    AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    //==============================================================================

    const String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect () const override;
    double getTailLengthSeconds() const override;

    //==============================================================================

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const String getProgramName (int index) override;
    void changeProgramName (int index, const String& newName) override;

    //==============================================================================






    //==============================================================================

    StringArray fftSizeItemsUI = {
        "32",
        "64",
        "128",
        "256",
        "512",
        "1024",
        "2048",
        "4096",
        "8192"
    };

    enum fftSizeIndex {
        fftSize32 = 0,
        fftSize64,
        fftSize128,
        fftSize256,
        fftSize512,
        fftSize1024,
        fftSize2048,
        fftSize4096,
        fftSize8192,
    };

    //======================================

    StringArray hopSizeItemsUI = {
        "1/2 Window",
        "1/4 Window",
        "1/8 Window",
    };

    enum hopSizeIndex {
        hopSize2 = 0,
        hopSize4,
        hopSize8,
    };

    //======================================

    StringArray windowTypeItemsUI = {
        "Bartlett",
        "Hann",
        "Hamming",
    };

    enum windowTypeIndex {
        windowTypeBartlett = 0,
        windowTypeHann,
        windowTypeHamming,
    };

    //======================================

    void updateFftSize();
    void updateHopSize();
    void updateAnalysisWindow();
    void updateWindow (const HeapBlock<float>& window, const int windowLength);
    void updateWindowScaleFactor();

    float princArg (const float phase);

    //======================================

    CriticalSection lock;

    int fftSize;
    std::unique_ptr<dsp::FFT> fft;

    int inputBufferLength;
    int inputBufferWritePosition;
    AudioSampleBuffer inputBuffer;

    int outputBufferLength;
    int outputBufferWritePosition;
    int outputBufferReadPosition;
    AudioSampleBuffer outputBuffer;

    HeapBlock<float> fftWindow;
    HeapBlock<dsp::Complex<float>> fftTimeDomain;
    HeapBlock<dsp::Complex<float>> fftFrequencyDomain;

    int samplesSinceLastFFT;

    int overlap;
    int hopSize;
    float windowScaleFactor;

    //======================================

    HeapBlock<float> omega;
    AudioSampleBuffer inputPhase;
    AudioSampleBuffer outputPhase;
    bool needToResetPhases;

    //======================================

    PluginParametersManager parameters;

    PluginParameterLinSlider paramShift;
    PluginParameterComboBox paramFftSize;
    PluginParameterComboBox paramHopSize;
    PluginParameterComboBox paramWindowType;

private:
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftAudioProcessor)
};
