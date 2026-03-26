/**
 *
 * Copyright (c) 2013-2024 Pascal Gauthier.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */
 
#include <inttypes.h>
#include <stdint.h>

#include "DXLookNFeel.h"
#include "DXComponents.h"
#include "Dexed.h"
#include "PluginProcessor.h"

#define REG_COLOUR(id, value) setColour(id, value); colourMap.set(#id, id)

/*
// Loads an image from the file specified by ``path`` and 
// returns it. 
// Otherwise it returns ``nullptr``, if the image was not loaded successfully.
//
// I propose it for deprecation.
//
Image findImage(String path) {
    Image img;
    if ( path.length() <= 3 ) 
        return img;
    //File imgFile(path); // it might cause an assertion in juce_File.cpp
    File imgFile = File::getCurrentWorkingDirectory().getChildFile(path);
    img = ImageCache::getFromFile(imgFile);
    if (img.isNull()) {
        TRACE("img.isNull() == true: cannot load image from the path=%s", path.toRawUTF8());
    }
    return img;
}
*/


// Loads an image from the file specified by ``path`` and 
// strore it in variable ``image``, only if the image was 
// loaded successfully from the file.
// Otherwise, content of variable ``image`` is not altered.
void replaceImage(String path, Image &image) {
    Image img;
    if (path.length() <= 3) {
        return;
    }
    File imgFile = File::getCurrentWorkingDirectory().getChildFile(path);
    img = ImageCache::getFromFile(imgFile);
    if (!img.isNull()) {
        // check sizes
        int hnew = img.getHeight();
        int wnew = img.getWidth();        
        int horig = image.getHeight();
        int worig = image.getWidth();
        if (horig != hnew || worig != wnew) {
            TRACE("WARNING: new image sizes (h=%d,w=%d) differ from original ones (h=%d, w=%d)", hnew, wnew, horig, worig);
        }

        // store the new image
        // 
        // TODO: if we really do not want allow to replace the original image
        // with a new one having different size, then the  previous ``if`` statement 
        // should be supplemented with an ``else`` branch, and the following 
        // assignment should be moved into this ``else`` branch.
        image = img;
    }
    else {
        TRACE("ERROR: img.isNull() == true: cannot load image from the path=%s", path.toRawUTF8());
    }
}

DXLookNFeel::DXLookNFeel() {
    Colour ctrlBackground;
    
    DexedAudioProcessor::dexedAppDir.setAsCurrentWorkingDirectory();
    ctrlBackground = Colour(0xff101722);

    // WARNING! If you modify the colour IDs here, please actualize the file ''DexedTheme.md'' 
    // in the subdirectory ``~/dexed/Documentation/``    

    REG_COLOUR(TextButton::buttonColourId, fillColour);
    REG_COLOUR(TextButton::textColourOnId, Colour(0xfff8f2e7));
    REG_COLOUR(TextButton::textColourOffId, Colour(0xfff8f2e7));
    REG_COLOUR(Slider::rotarySliderOutlineColourId, fillColour);
    REG_COLOUR(Slider::rotarySliderFillColourId, Colour(0xffffd28c));
    REG_COLOUR(AlertWindow::backgroundColourId,lightBackground);
    REG_COLOUR(AlertWindow::textColourId, Colours::white);
    REG_COLOUR(TextEditor::backgroundColourId,ctrlBackground);
    REG_COLOUR(TextEditor::textColourId, Colours::white);
    REG_COLOUR(TextEditor::highlightColourId, fillColour);
    REG_COLOUR(TextEditor::outlineColourId, Colours::transparentBlack);
    REG_COLOUR(ComboBox::backgroundColourId, ctrlBackground);
    REG_COLOUR(ComboBox::textColourId, Colours::white);
    REG_COLOUR(ComboBox::buttonColourId, fillColour);
    REG_COLOUR(PopupMenu::backgroundColourId, background);
    REG_COLOUR(PopupMenu::textColourId, Colours::white);
    REG_COLOUR(PopupMenu::highlightedTextColourId, Colours::white);
    REG_COLOUR(PopupMenu::highlightedBackgroundColourId, fillColour);
    REG_COLOUR(TreeView::backgroundColourId, background);
    REG_COLOUR(DirectoryContentsDisplayComponent::highlightColourId, fillColour);
    REG_COLOUR(DirectoryContentsDisplayComponent::textColourId, Colours::white);
    REG_COLOUR(DialogWindow::backgroundColourId, background);
    REG_COLOUR(ListBox::backgroundColourId, ctrlBackground);

    // Register ``Scrollbar::thumbColourId`` to allow its redefinion in ``DexedTheme.xml``.
    REG_COLOUR(ScrollBar::thumbColourId, background.darker());

    // WARNING! If you modify the images here, please actualize the file ''DexedTheme.md'' 
    // in the subdirectory ``~/dexed/Documentation/``
    imageKnob = ImageCache::getFromMemory(BinaryData::Knob_68x68_png, BinaryData::Knob_68x68_pngSize); // 2x
    imageSwitch = ImageCache::getFromMemory(BinaryData::Switch_96x52_png, BinaryData::Switch_96x52_pngSize); // 2x
    imageSwitchLighted = ImageCache::getFromMemory(BinaryData::SwitchLighted_48x26_png, BinaryData::SwitchLighted_48x26_pngSize);
    imageSwitchOperator = ImageCache::getFromMemory(BinaryData::Switch_64x64_png, BinaryData::Switch_64x64_pngSize); // 2x
    imageButton = ImageCache::getFromMemory(BinaryData::ButtonUnlabeled_50x30_png, BinaryData::ButtonUnlabeled_50x30_pngSize);
    imageSlider = ImageCache::getFromMemory(BinaryData::Slider_52x52_png, BinaryData::Slider_52x52_pngSize); // 2x
    imageScaling = ImageCache::getFromMemory(BinaryData::Scaling_36_26_png, BinaryData::Scaling_36_26_pngSize); // 2x
    imageLight = ImageCache::getFromMemory(BinaryData::Light_28x28_png, BinaryData::Light_28x28_pngSize); // 2x
    imageLFO = ImageCache::getFromMemory(BinaryData::LFO_36_26_png, BinaryData::LFO_36_26_pngSize);
    imageOperator =  ImageCache::getFromMemory(BinaryData::OperatorEditor_574x436_png, BinaryData::OperatorEditor_574x436_pngSize); // 2x
    imageGlobal = ImageCache::getFromMemory (BinaryData::GlobalEditor_1728x288_png, BinaryData::GlobalEditor_1728x288_pngSize); // 2x

    //---
    // load and parse the file ``DexedTheme.xml``
    //---

    File dexedTheme = DexedAudioProcessor::dexedAppDir.getChildFile("DexedTheme.xml");

    if ( ! dexedTheme.existsAsFile() ) {
        TRACE("file \"%s\" does not exists", dexedTheme.getFullPathName().toRawUTF8());
        return;
    }

    std::unique_ptr<XmlElement> root = XmlDocument::parse(dexedTheme);
    if ( root == NULL )
    {
        TRACE("ERROR: XmlDocument::parse(): failed");
        return;
    }

    //--- 
    // Get custom ARGB color codes specified by the
    // ``colour id`` - ``value`` pairs in ``DexedTheme.xml`` file.
    //---

    //forEachXmlChildElementWithTagName(*root, colour, "colour") { // deprecated!
    for (XmlElement* colour = root->getFirstChildElement(); colour != nullptr; colour = colour->getNextElement()) {
        if (colour->hasTagName("colour")) {
            String name = colour->getStringAttribute("id", "");
            if ( name == "" )
                continue;
            String value = colour->getStringAttribute("value", "");
            if ( value == "" )
                continue;
            if ( value.length() < 8 ) {
                TRACE("ERROR: illegal value=\"%s\" at color id=\"%s\"; skipped", value.toRawUTF8(), name.toRawUTF8());
                continue;
            }
            //int conv = strtol(value.toRawUTF8(), NULL, 16); // as alpha (MSB) could be above 0x7F, hence ``strtol()`` is inappropiate to convert values exceeding ``0x7FFFFFFF`` 
            char* endptr = NULL;
            uint64_t conv = strtoull(value.toRawUTF8(), &endptr, 16);
            //TRACE("color id=\"%s\" value=\"%s\": conv=0x%" PRIx64 "", name.toRawUTF8(), value.toRawUTF8(), conv);
            if (endptr != nullptr && *endptr != '\0') {
                TRACE("ERROR: illegal char #%d in value=\"%s\" at color id=\"%s\"; skipped", (int)(*endptr), value.toRawUTF8(), name.toRawUTF8());
                continue;
            }
            if (conv > 0xFFFFFFFFULL) {
                TRACE("ERROR: value 0x%" PRIx64 " exceeded the limit at color id=\"%s\"; skipped", conv, name.toRawUTF8());
                continue;
            }
            if ( colourMap.contains(name) ) {
                setColour(colourMap[name], Colour((uint32_t)conv));
            } else {
                if ( name == "Dexed::backgroundId" ) {
                    background = Colour((uint32_t)conv);
                    continue;
                }
                else if ( name == "Dexed::fillColourId" ) {
                    fillColour = Colour((uint32_t)conv);
                    continue;
                }
                TRACE("ERROR: color id=\"%s\" not found in colourMap; skipped.", name.toRawUTF8());
            }
        }        
    }

    //---
    // Get the custom images specified by the 
    // ``image`` - ``path`` pairs in the "Dexed.xml" file.
    //---

    //forEachXmlChildElementWithTagName(*root, image, "image") { // deprecated!
    for (XmlElement* image = root->getFirstChildElement(); image != nullptr; image = image->getNextElement()) {
        if (image->hasTagName("image")) {
            String name = image->getStringAttribute("id", "");
            String path = image->getStringAttribute("path", "");
            //TRACE("image id=\'%s\' path=\'%s\'", name.toRawUTF8(), path.toRawUTF8());
            if (name == /*"Knob_34x34.png"*/"Knob_68x68.png") { // 2x                
                //imageKnob = findImage(path); // if the new image was not loaded successfully, ``findImage()`` returns ``nullptr``, so the existing ``imageKnob`` is lost
                replaceImage(path, imageKnob);
                continue;
            }
            if (name == /*"Switch_48x26.png"*/ "Switch_96x52.png") { // 2x
                //imageSwitch = findImage(path);
                replaceImage(path, imageSwitch);
                continue;
            }
            if (name == "SwitchLighted_48x26.png") {
                //imageSwitchLighted = findImage(path);
                replaceImage(path, imageSwitchLighted);
                continue;
            }
            if (name == /*"Switch_32x64.png"*/ "Switch_64x64.png") { // 2x
                //imageSwitchOperator = findImage(path);
                replaceImage(path, imageSwitchOperator);
                continue;
            }
            if (name == "ButtonUnlabeled_50x30.png") {
                //imageButton = findImage(path);
                replaceImage(path, imageButton);
                continue;
            }
            if (name == /*"Slider_26x26.png"*/ "Slider_52x52.png") { // 2x
                //imageSlider = findImage(path);
                replaceImage(path, imageSlider);
                continue;
            }
            if (name == "Scaling_36_26.png") {
                //imageScaling = findImage(path);
                replaceImage(path, imageScaling);
                continue;
            }
            if (name == /*"Light_14x14.png"*/ "Light_28x28.png") { // 2x
                //imageLight = findImage(path);
                replaceImage(path, imageLight);
                continue;
            }
            if (name == "LFO_36_26.png") {
                //imageLFO = findImage(path);
                replaceImage(path, imageLFO);
                continue;
            }
            if (name == /*"OperatorEditor_287x218.png"*/ "OperatorEditor_574x436_png") { // 2x
                //imageOperator = findImage(path);
                replaceImage(path, imageOperator);
                continue;
            }
            if (name == /*"GlobalEditor_864x144.png"*/ "GlobalEditor_1728x288_png") { // 2x
                //imageGlobal = findImage(path);
                replaceImage(path, imageGlobal);
                continue;
            }
            TRACE("ERROR: unknown image id=\"%s\"; skipped", name.toRawUTF8());
        }        
    }
}

Typeface::Ptr DXLookNFeel::getTypefaceForFont(const Font &) {
    return Typeface::createSystemTypefaceFor(BinaryData::NotoSansRegular_ttf, BinaryData::NotoSansRegular_ttfSize);
}

void DXLookNFeel::drawRotarySlider( Graphics &g, int x, int y, int width, int height, float sliderPosProportional,
     float rotaryStartAngle, float rotaryEndAngle,  Slider &slider ) {
     const auto bounds = Rectangle<float>((float) x, (float) y, (float) width, (float) height).reduced(2.0f);
     const float radius = jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
     const float centreX = bounds.getCentreX();
     const float centreY = bounds.getCentreY();
     const auto rotaryAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

     g.setColour(Colour(0x33121d2b));
     g.fillEllipse(bounds.expanded(1.0f));

     Path halo;
     halo.addCentredArc(centreX, centreY, radius + 1.5f, radius + 1.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
     g.setColour(Colour(0x22ffffff));
     g.strokePath(halo, PathStrokeType(2.0f));

     if ( imageKnob.isNull() ) {
         LookAndFeel_V4::drawRotarySlider(g, x, y, width, height, sliderPosProportional, rotaryStartAngle, rotaryEndAngle, slider);
     } else {
         const double fractRotation = (slider.getValue() - slider.getMinimum())  /   (slider.getMaximum() - slider.getMinimum());
         const int nFrames = imageKnob.getHeight()/imageKnob.getWidth();
         const int frameIdx = (int) ceil(fractRotation * ((double) nFrames - 1.0));
         g.drawImage(imageKnob, (int) bounds.getX(), (int) bounds.getY(), (int) bounds.getWidth(), (int) bounds.getHeight(),
                     0, frameIdx * imageKnob.getWidth(), imageKnob.getWidth(), imageKnob.getWidth());
     }

     Path accentArc;
     accentArc.addCentredArc(centreX, centreY, radius + 1.5f, radius + 1.5f, 0.0f, rotaryStartAngle, rotaryAngle, true);
     g.setColour(fillColour);
     g.strokePath(accentArc, PathStrokeType(2.2f));

     g.setColour(Colour(0xffffd28c));
     g.fillEllipse(centreX - 2.0f + std::cos(rotaryAngle) * (radius - 9.0f),
                   centreY - 2.0f + std::sin(rotaryAngle) * (radius - 9.0f),
                   4.0f,
                   4.0f);
};

void DXLookNFeel::drawToggleButton(Graphics& g, ToggleButton& button, bool isMouseOverButton, bool isButtonDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(2.0f);
    auto track = bounds.withSizeKeepingCentre(jmin(bounds.getWidth(), 48.0f), jmin(bounds.getHeight(), 24.0f));
    auto knobSize = track.getHeight() - 4.0f;
    auto knobX = button.getToggleState() ? track.getRight() - knobSize - 2.0f : track.getX() + 2.0f;

    g.setColour(button.getToggleState() ? fillColour : roundBackground.brighter(0.2f));
    g.fillRoundedRectangle(track, track.getHeight() * 0.5f);

    g.setColour(button.getToggleState() ? Colour(0x66ffffff) : Colour(0x33000000));
    g.drawRoundedRectangle(track, track.getHeight() * 0.5f, 1.0f);

    g.setColour(button.getToggleState() ? Colour(0xffffd28c) : Colour(0xffd6dde8));
    g.fillEllipse(knobX, track.getY() + 2.0f, knobSize, knobSize);

    if (button.getWidth() > 54 && button.getButtonText().isNotEmpty()) {
        g.setColour(Colour(0xfff8f2e7));
        g.setFont(Font(13.0f, Font::bold));
        g.drawText(button.getButtonText(), 0, 0, button.getWidth(), button.getHeight(), Justification::centredRight, false);
    }
}

void DXLookNFeel::drawButtonBackground(Graphics &g, Button &button, const Colour& backgroundColour, bool isMouseOverButton, bool isButtonDown) {
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto baseColour = isButtonDown ? fillColour.darker(0.25f)
                                   : (isMouseOverButton ? fillColour.brighter(0.08f) : fillColour);

    ColourGradient buttonGradient(baseColour.brighter(0.25f), bounds.getTopLeft(),
                                  baseColour.darker(0.28f), bounds.getBottomLeft(), false);
    g.setGradientFill(buttonGradient);
    g.fillRoundedRectangle(bounds, 8.0f);

    g.setColour(Colour(0x44ffffff));
    g.drawRoundedRectangle(bounds, 8.0f, 1.0f);
}

void DXLookNFeel::drawLinearSlider (Graphics& g, int x, int y, int width, int height,
                                 float sliderPos, float minSliderPos, float maxSliderPos,
                                  const Slider::SliderStyle style, Slider& slider) {
    ignoreUnused(maxSliderPos, style, slider);

    auto bounds = Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    auto track = bounds.withHeight(4.0f).withCentre(bounds.getCentre());
    auto thumb = Rectangle<float>(sliderPos - 8.0f, bounds.getCentreY() - 8.0f, 16.0f, 16.0f);

    g.setColour(roundBackground.brighter(0.25f));
    g.fillRoundedRectangle(track, 2.0f);

    g.setColour(fillColour);
    g.fillRoundedRectangle(Rectangle<float>(track.getX(), track.getY(), sliderPos - minSliderPos, track.getHeight()), 2.0f);

    g.setColour(Colour(0xffffd28c));
    g.fillEllipse(thumb);
    g.setColour(Colour(0x55ffffff));
    g.drawEllipse(thumb, 1.0f);
}

void DXLookNFeel::positionComboBoxText(ComboBox& box, Label& label) {
    ComboBox *src = &box;

    // I'm not proud of this one, but really... it must be another way to do this....
    ComboBoxImage* img = dynamic_cast<ComboBoxImage*>(src);
    if( img != 0 ) {
        return;
    }

    LookAndFeel_V4::positionComboBoxText(box, label);
}

Colour DXLookNFeel::fillColour = Colour(0xff47d8c7);
Colour DXLookNFeel::lightBackground = Colour(0xff242b38);
Colour DXLookNFeel::background = Colour(0xff10151d);
Colour DXLookNFeel::roundBackground = Colour(0xff1a2230);

