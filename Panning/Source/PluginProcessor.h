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

#pragma once

#define _USE_MATH_DEFINES
#include <cmath>

#include "../JuceLibraryCode/JuceHeader.h"
#include "PluginParameter.h"

//==============================================================================

class PanningAudioProcessor : public AudioProcessor
{
public:
    //==============================================================================

    PanningAudioProcessor();
    ~PanningAudioProcessor();

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

    StringArray methodItemsUI = {
        "Panorama + Precedence",
        "ITD + ILD",
    };

    enum methodIndex {
        methodPanoramaPrecedence = 0,
        methodItdIld,
    };

    //======================================

    class DelayLine
    {
    public:
        void setup (const int maxDelayTimeInSamples)
        {
            delayBufferSamples = maxDelayTimeInSamples + 2;
            if (delayBufferSamples < 1)
                delayBufferSamples = 1;

            delayBuffer.setSize (1, delayBufferSamples);
            delayBuffer.clear();

            delayWritePosition = 0;
        }

        void writeSample (const float sampleToWrite)
        {
            delayBuffer.setSample (0, delayWritePosition, sampleToWrite);

            if (++delayWritePosition >= delayBufferSamples)
                delayWritePosition -= delayBufferSamples;
        }

        float readSample (const float delayTime){
            float readPosition =
                fmodf ((float)(delayWritePosition - 1) - delayTime + (float)delayBufferSamples, delayBufferSamples);
            int localReadPosition = floorf (readPosition);

            float fraction = readPosition - (float)localReadPosition;
            float delayed1 = delayBuffer.getSample (0, (localReadPosition + 0));
            float delayed2 = delayBuffer.getSample (0, (localReadPosition + 1) % delayBufferSamples);

            return delayed1 + fraction * (delayed2 - delayed1);
        }

    private:
        AudioSampleBuffer delayBuffer;
        int delayBufferSamples;
        int delayWritePosition;
    };

    DelayLine delayLineL;
    DelayLine delayLineR;
    int maximumDelayInSamples;

    //======================================

    class Filter : public IIRFilter
    {
    public:
        void updateCoefficients (const double angle, const double headFactor) noexcept
        {
            double alpha = 1.0 + cos (angle);

            coefficients = IIRCoefficients (/* b0 */ headFactor + alpha,
                                            /* b1 */ headFactor - alpha,
                                            /* b2 */ 0.0,
                                            /* a0 */ headFactor + 1.0,
                                            /* a1 */ headFactor - 1.0,
                                            /* a2 */ 0.0);

            setCoefficients (coefficients);
        }
    };

    Filter filterL;
    Filter filterR;

    //======================================

    PluginParametersManager parameters;

    PluginParameterComboBox paramMethod;
    PluginParameterLinSlider paramPanning;

private:
    //==============================================================================

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanningAudioProcessor)
};
