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

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginParameter.h"

//==============================================================================

class PhaserAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    PhaserAudioProcessor();
    ~PhaserAudioProcessor();

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
        "Square",
        "Sawtooth"
    };

    enum waveformIndex {
        waveformSine = 0,
        waveformTriangle,
        waveformSquare,
        waveformSawtooth,
    };

    //======================================

    class Filter : public IIRFilter
    {
    public:
        void updateCoefficients (const double discreteFrequency) noexcept
        {
            jassert (discreteFrequency > 0);

            const double wc = jmin (discreteFrequency, M_PI * 0.99);
            const double tan_half_wc = tan (wc / 2.0);

            coefficients = IIRCoefficients (/* b0 */ tan_half_wc - 1.0,
                                            /* b1 */ tan_half_wc + 1.0,
                                            /* b2 */ 0.0,
                                            /* a0 */ tan_half_wc + 1.0,
                                            /* a1 */ tan_half_wc - 1.0,
                                            /* a2 */ 0.0);

            setCoefficients (coefficients);
        }
    };

    OwnedArray<Filter> filters;
    Array<float> filteredOutputs;
    void updateFilters (double centreFrequency);
    int numFiltersPerChannel;
    unsigned int sampleCountToUpdateFilters;
    unsigned int updateFiltersInterval;

    float lfoPhase;
    float inverseSampleRate;
    float twoPi;

    float lfo (float phase, int waveform);

    //======================================

    PluginParametersManager parameters;

    PluginParameterLinSlider paramDepth;
    PluginParameterLinSlider paramFeedback;
    PluginParameterComboBox paramNumFilters;
    PluginParameterLogSlider paramMinFrequency;
    PluginParameterLogSlider paramSweepWidth;
    PluginParameterLinSlider paramLFOfrequency;
    PluginParameterComboBox paramLFOwaveform;
    PluginParameterToggle paramStereo;

private:
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PhaserAudioProcessor)
};
