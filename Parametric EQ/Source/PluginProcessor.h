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

class ParametricEQAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    ParametricEQAudioProcessor();
    ~ParametricEQAudioProcessor();

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

    StringArray filterTypeItemsUI = {
        "Low-pass",
        "High-pass",
        "Low-shelf",
        "High-shelf",
        "Band-pass",
        "Band-stop",
        "Peaking/Notch"
    };

    enum filterTypeIndex {
        filterTypeLowPass = 0,
        filterTypeHighPass,
        filterTypeLowShelf,
        filterTypeHighShelf,
        filterTypeBandPass,
        filterTypeBandStop,
        filterTypePeakingNotch,
    };

    //======================================

    class Filter : public IIRFilter
    {
    public:
        void updateCoefficients (const double discreteFrequency,
                                 const double qFactor,
                                 const double gain,
                                 const int filterType) noexcept
        {
            jassert (discreteFrequency > 0);
            jassert (qFactor > 0);

            double bandwidth = jmin (discreteFrequency / qFactor, M_PI * 0.99);
            double two_cos_wc = -2.0 * cos (discreteFrequency);
            double tan_half_bw = tan (bandwidth / 2.0);
            double tan_half_wc = tan (discreteFrequency / 2.0);
            double sqrt_gain = sqrt (gain);

            switch (filterType) {
                case filterTypeLowPass: {
                    coefficients = IIRCoefficients (/* b0 */ tan_half_wc,
                                                    /* b1 */ tan_half_wc,
                                                    /* b2 */ 0.0,
                                                    /* a0 */ tan_half_wc + 1.0,
                                                    /* a1 */ tan_half_wc - 1.0,
                                                    /* a2 */ 0.0);
                    break;
                }
                case filterTypeHighPass: {
                    coefficients = IIRCoefficients (/* b0 */ 1.0,
                                                    /* b1 */ -1.0,
                                                    /* b2 */ 0.0,
                                                    /* a0 */ tan_half_wc + 1.0,
                                                    /* a1 */ tan_half_wc - 1.0,
                                                    /* a2 */ 0.0);
                    break;
                }
                case filterTypeLowShelf: {
                    coefficients = IIRCoefficients (/* b0 */ gain * tan_half_wc + sqrt_gain,
                                                    /* b1 */ gain * tan_half_wc - sqrt_gain,
                                                    /* b2 */ 0.0,
                                                    /* a0 */ tan_half_wc + sqrt_gain,
                                                    /* a1 */ tan_half_wc - sqrt_gain,
                                                    /* a2 */ 0.0);
                    break;
                }
                case filterTypeHighShelf: {
                    coefficients = IIRCoefficients (/* b0 */ sqrt_gain * tan_half_wc + gain,
                                                    /* b1 */ sqrt_gain * tan_half_wc - gain,
                                                    /* b2 */ 0.0,
                                                    /* a0 */ sqrt_gain * tan_half_wc + 1.0,
                                                    /* a1 */ sqrt_gain * tan_half_wc - 1.0,
                                                    /* a2 */ 0.0);
                    break;
                }
                case filterTypeBandPass: {
                    coefficients = IIRCoefficients (/* b0 */ tan_half_bw,
                                                    /* b1 */ 0.0,
                                                    /* b2 */ -tan_half_bw,
                                                    /* a0 */ 1.0 + tan_half_bw,
                                                    /* a1 */ two_cos_wc,
                                                    /* a2 */ 1.0 - tan_half_bw);
                    break;
                }
                case filterTypeBandStop: {
                    coefficients = IIRCoefficients (/* b0 */ 1.0,
                                                    /* b1 */ two_cos_wc,
                                                    /* b2 */ 1.0,
                                                    /* a0 */ 1.0 + tan_half_bw,
                                                    /* a1 */ two_cos_wc,
                                                    /* a2 */ 1.0 - tan_half_bw);
                    break;
                }
                case filterTypePeakingNotch: {
                    coefficients = IIRCoefficients (/* b0 */ sqrt_gain + gain * tan_half_bw,
                                                    /* b1 */ sqrt_gain * two_cos_wc,
                                                    /* b2 */ sqrt_gain - gain * tan_half_bw,
                                                    /* a0 */ sqrt_gain + tan_half_bw,
                                                    /* a1 */ sqrt_gain * two_cos_wc,
                                                    /* a2 */ sqrt_gain - tan_half_bw);
                    break;
                }
            }

            setCoefficients (coefficients);
        }
    };

    OwnedArray<Filter> filters;
    void updateFilters();

    //======================================

    PluginParametersManager parameters;

    PluginParameterLogSlider paramFrequency;
    PluginParameterLinSlider paramQfactor;
    PluginParameterLinSlider paramGain;
    PluginParameterComboBox paramFilterType;

private:
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParametricEQAudioProcessor)
};
