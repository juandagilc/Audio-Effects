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

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginParameter.h"

//==============================================================================

class RingModulationAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    RingModulationAudioProcessor();
    ~RingModulationAudioProcessor();

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

    StringArray waveformItemsUI = {
        "Sine",
        "Triangle",
        "Sawtooth (rising)",
        "Sawtooth (falling)",
        "Square",
        "Square with sloped edges"
    };

    enum waveformIndex {
        waveformSine = 0,
        waveformTriangle,
        waveformSawtooth,
        waveformInverseSawtooth,
        waveformSquare,
        waveformSquareSlopedEdges,
    };

    //======================================

    float lfoPhase;
    float inverseSampleRate;
    float twoPi;

    float lfo (float phase, int waveform);

    //======================================

    PluginParametersManager parameters;

    PluginParameterLinSlider paramDepth;
    PluginParameterLinSlider paramFrequency;
    PluginParameterComboBox paramWaveform;

private:
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RingModulationAudioProcessor)
};
