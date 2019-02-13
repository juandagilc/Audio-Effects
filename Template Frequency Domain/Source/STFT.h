/*
 ==============================================================================

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

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================

class STFT
{
public:
    enum windowTypeIndex {
        windowTypeRectangular = 0,
        windowTypeBartlett,
        windowTypeHann,
        windowTypeHamming,
    };

    //======================================

    STFT() : numChannels (1)
    {
    }

    virtual ~STFT()
    {
    }

    //======================================

    void setup (const int numInputChannels)
    {
        numChannels = (numInputChannels > 0) ? numInputChannels : 1;
    }

    void updateParameters (const int newFftSize, const int newOverlap, const int newWindowType)
    {
        updateFftSize (newFftSize);
        updateHopSize (newOverlap);
        updateWindow (newWindowType);
    }

    //======================================

    void processBlock (AudioSampleBuffer& block)
    {
        numSamples = block.getNumSamples();

        for (int channel = 0; channel < numChannels; ++channel) {
            float* channelData = block.getWritePointer (channel);

            currentInputBufferWritePosition = inputBufferWritePosition;
            currentOutputBufferWritePosition = outputBufferWritePosition;
            currentOutputBufferReadPosition = outputBufferReadPosition;
            currentSamplesSinceLastFFT = samplesSinceLastFFT;

            for (int sample = 0; sample < numSamples; ++sample) {
                const float inputSample = channelData[sample];
                inputBuffer.setSample (channel, currentInputBufferWritePosition, inputSample);
                if (++currentInputBufferWritePosition >= inputBufferLength)
                    currentInputBufferWritePosition = 0;

                channelData[sample] = outputBuffer.getSample (channel, currentOutputBufferReadPosition);
                outputBuffer.setSample (channel, currentOutputBufferReadPosition, 0.0f);
                if (++currentOutputBufferReadPosition >= outputBufferLength)
                    currentOutputBufferReadPosition = 0;

                if (++currentSamplesSinceLastFFT >= hopSize) {
                    currentSamplesSinceLastFFT = 0;

                    analysis (channel);
                    modification();
                    synthesis (channel);
                }
            }
        }

        inputBufferWritePosition = currentInputBufferWritePosition;
        outputBufferWritePosition = currentOutputBufferWritePosition;
        outputBufferReadPosition = currentOutputBufferReadPosition;
        samplesSinceLastFFT = currentSamplesSinceLastFFT;
    }

private:
    //======================================

    void updateFftSize (const int newFftSize)
    {
        fftSize = newFftSize;
        fft = new dsp::FFT (log2 (fftSize));

        inputBufferLength = fftSize;
        inputBuffer.clear();
        inputBuffer.setSize (numChannels, inputBufferLength);

        outputBufferLength = fftSize;
        outputBuffer.clear();
        outputBuffer.setSize (numChannels, outputBufferLength);

        fftWindow.realloc (fftSize);
        fftWindow.clear (fftSize);

        timeDomainBuffer.realloc (fftSize);
        timeDomainBuffer.clear (fftSize);

        frequencyDomainBuffer.realloc (fftSize);
        frequencyDomainBuffer.clear (fftSize);

        inputBufferWritePosition = 0;
        outputBufferWritePosition = 0;
        outputBufferReadPosition = 0;
        samplesSinceLastFFT = 0;
    }

    void updateHopSize (const int newOverlap)
    {
        overlap = newOverlap;
        if (overlap != 0) {
            hopSize = fftSize / overlap;
            outputBufferWritePosition = hopSize % outputBufferLength;
        }
    }

    void updateWindow (const int newWindowType)
    {
        switch (newWindowType) {
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

        float windowSum = 0.0f;
        for (int sample = 0; sample < fftSize; ++sample)
            windowSum += fftWindow[sample];

        windowScaleFactor = 0.0f;
        if (overlap != 0 && windowSum != 0.0f)
            windowScaleFactor = 1.0f / (float)overlap / windowSum * (float)fftSize;
    }

    //======================================

    void analysis (const int channel)
    {
        int inputBufferIndex = currentInputBufferWritePosition;
        for (int index = 0; index < fftSize; ++index) {
            timeDomainBuffer[index].real (fftWindow[index] * inputBuffer.getSample (channel, inputBufferIndex));
            timeDomainBuffer[index].imag (0.0f);

            if (++inputBufferIndex >= inputBufferLength)
                inputBufferIndex = 0;
        }
    }

    virtual void modification()
    {
        fft->perform (timeDomainBuffer, frequencyDomainBuffer, false);

        for (int index = 0; index < fftSize / 2 + 1; ++index) {
            float magnitude = abs (frequencyDomainBuffer[index]);
            float phase = arg (frequencyDomainBuffer[index]);

            frequencyDomainBuffer[index].real (magnitude * cosf (phase));
            frequencyDomainBuffer[index].imag (magnitude * sinf (phase));
            if (index > 0 && index < fftSize / 2) {
                frequencyDomainBuffer[fftSize - index].real (magnitude * cosf (phase));
                frequencyDomainBuffer[fftSize - index].imag (magnitude * sinf (-phase));
            }
        }

        fft->perform (frequencyDomainBuffer, timeDomainBuffer, true);
    }

    void synthesis (const int channel)
    {
        int outputBufferIndex = currentOutputBufferWritePosition;
        for (int index = 0; index < fftSize; ++index) {
            float outputSample = outputBuffer.getSample (channel, outputBufferIndex);
            outputSample += timeDomainBuffer[index].real() * windowScaleFactor;
            outputBuffer.setSample (channel, outputBufferIndex, outputSample);

            if (++outputBufferIndex >= outputBufferLength)
                outputBufferIndex = 0;
        }

        currentOutputBufferWritePosition += hopSize;
        if (currentOutputBufferWritePosition >= outputBufferLength)
            currentOutputBufferWritePosition = 0;
    }

protected:
    //======================================
    int numChannels;
    int numSamples;

    int fftSize;
    ScopedPointer<dsp::FFT> fft;

    int inputBufferLength;
    AudioSampleBuffer inputBuffer;

    int outputBufferLength;
    AudioSampleBuffer outputBuffer;

    HeapBlock<float> fftWindow;
    HeapBlock<dsp::Complex<float>> timeDomainBuffer;
    HeapBlock<dsp::Complex<float>> frequencyDomainBuffer;

    int overlap;
    int hopSize;
    float windowScaleFactor;

    int inputBufferWritePosition;
    int outputBufferWritePosition;
    int outputBufferReadPosition;
    int samplesSinceLastFFT;

    int currentInputBufferWritePosition;
    int currentOutputBufferWritePosition;
    int currentOutputBufferReadPosition;
    int currentSamplesSinceLastFFT;
};

//==============================================================================
