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

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

VibratoAudioProcessorEditor::VibratoAudioProcessorEditor (VibratoAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    const OwnedArray<AudioProcessorParameter>& parameters = processor.getParameters();
    int comboBoxCounter = 0;

    int editorHeight = 2 * editorMargin;
    for (int i = 0; i < parameters.size(); ++i) {
        if (const AudioProcessorParameterWithID* parameter =
                dynamic_cast<AudioProcessorParameterWithID*> (parameters[i])) {

            if (processor.parameters.parameterTypes[i] == "Slider") {
                Slider* aSlider;
                sliders.add (aSlider = new Slider());
                aSlider->setTextValueSuffix (parameter->label);
                aSlider->setTextBoxStyle (Slider::TextBoxLeft,
                                          false,
                                          sliderTextEntryBoxWidth,
                                          sliderTextEntryBoxHeight);

                SliderAttachment* aSliderAttachment;
                sliderAttachments.add (aSliderAttachment =
                    new SliderAttachment (processor.parameters.valueTreeState, parameter->paramID, *aSlider));

                components.add (aSlider);
                editorHeight += sliderHeight;
            }

            //======================================

            else if (processor.parameters.parameterTypes[i] == "ToggleButton") {
                ToggleButton* aButton;
                toggles.add (aButton = new ToggleButton());
                aButton->setToggleState (parameter->getDefaultValue(), dontSendNotification);

                ButtonAttachment* aButtonAttachment;
                buttonAttachments.add (aButtonAttachment =
                    new ButtonAttachment (processor.parameters.valueTreeState, parameter->paramID, *aButton));

                components.add (aButton);
                editorHeight += buttonHeight;
            }

            //======================================

            else if (processor.parameters.parameterTypes[i] == "ComboBox") {
                ComboBox* aComboBox;
                comboBoxes.add (aComboBox = new ComboBox());
                aComboBox->setEditableText (false);
                aComboBox->setJustificationType (Justification::left);
                aComboBox->addItemList (processor.parameters.comboBoxItemLists[comboBoxCounter++], 1);

                ComboBoxAttachment* aComboBoxAttachment;
                comboBoxAttachments.add (aComboBoxAttachment =
                    new ComboBoxAttachment (processor.parameters.valueTreeState, parameter->paramID, *aComboBox));

                components.add (aComboBox);
                editorHeight += comboBoxHeight;
            }

            //======================================

            Label* aLabel;
            labels.add (aLabel = new Label (parameter->name, parameter->name));
            aLabel->attachToComponent (components.getLast(), true);
            addAndMakeVisible (aLabel);

            components.getLast()->setName (parameter->name);
            components.getLast()->setComponentID (parameter->paramID);
            addAndMakeVisible (components.getLast());
        }
    }

    addAndMakeVisible (&pitchShiftLabel);
    editorHeight += 20;

    //======================================

    editorHeight += components.size() * editorPadding;
    setSize (editorWidth, editorHeight);
    startTimer (50);
}

VibratoAudioProcessorEditor::~VibratoAudioProcessorEditor()
{
}

//==============================================================================

void VibratoAudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (ResizableWindow::backgroundColourId));
}

void VibratoAudioProcessorEditor::resized()
{
    Rectangle<int> r = getLocalBounds().reduced (editorMargin);
    r = r.removeFromRight (r.getWidth() - labelWidth);

    for (int i = 0; i < components.size(); ++i) {
        if (Slider* aSlider = dynamic_cast<Slider*> (components[i]))
            components[i]->setBounds (r.removeFromTop (sliderHeight));

        if (ToggleButton* aButton = dynamic_cast<ToggleButton*> (components[i]))
            components[i]->setBounds (r.removeFromTop (buttonHeight));

        if (ComboBox* aComboBox = dynamic_cast<ComboBox*> (components[i]))
            components[i]->setBounds (r.removeFromTop (comboBoxHeight));

        r = r.removeFromBottom (r.getHeight() - editorPadding);
    }

    pitchShiftLabel.setBounds (0, getBottom() - 20, getWidth(), 20);
}

//==============================================================================

void VibratoAudioProcessorEditor::timerCallback()
{
    updateUIcomponents();
}

void VibratoAudioProcessorEditor::updateUIcomponents()
{
    float minPitch = 0.0f;
    float maxPitch = 0.0f;
    float minSpeed = 1.0f;
    float maxSpeed = 1.0f;
    String pitchShiftText = "";

    float width = processor.paramWidth.getTargetValue();
    float frequency = processor.paramFrequency.getTargetValue();
    int waveform = (int)processor.paramWaveform.getTargetValue();

    switch (waveform) {
        case VibratoAudioProcessor::waveformSine: {
            minSpeed = 1.0f - M_PI * width * frequency;
            maxSpeed = 1.0f + M_PI * width * frequency;
            break;
        }
        case VibratoAudioProcessor::waveformTriangle: {
            minSpeed = 1.0f - 2.0f * width * frequency;
            maxSpeed = 1.0f + 2.0f * width * frequency;
            break;
        }
        case VibratoAudioProcessor::waveformSawtooth: {
            minSpeed = 1.0f - width * frequency;
            maxSpeed = 1.0f;
            break;
        }
        case VibratoAudioProcessor::waveformInverseSawtooth: {
            minSpeed = 1.0f;
            maxSpeed = 1.0f + width * frequency;
            break;
        }
    }

    maxPitch = 12.0f * logf (maxSpeed) / logf (2.0f);

    if (minSpeed > 0.0f) {
        minPitch = 12.0f * logf (minSpeed) / logf (2.0f);
        pitchShiftText = String::formatted ("Vibrato range: %+.2f to %+.2f semitones (speed %.3f to %.3f)",
                                            minPitch, maxPitch, minSpeed, maxSpeed);
    } else {
        pitchShiftText = String::formatted ("Vibrato range: ----- to %+.2f semitones (speed %.3f to %.3f)",
                                            minPitch, maxPitch, minSpeed, maxSpeed);
    }

    pitchShiftLabel.setText (pitchShiftText, dontSendNotification);
}

//==============================================================================
