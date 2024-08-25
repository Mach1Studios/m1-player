#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#if !defined(DEFAULT_FONT_SIZE)
#define DEFAULT_FONTSIZE 10
#endif

class RadioGroupWidget : public View<RadioGroupWidget> {
    
public:
	void internalDraw(Murka & m) {
		int c = labels.size();
		float w = (m.getSize().width() / c); // each element's width
		
		int hoverIndex = -1;

		changed = false;

		if (isHovered()) {
			hoverIndex = mousePosition().x / w;
			if (mouseDownPressed(0)) {
				selectedIndex = hoverIndex;
				changed = true;
				lastTimeClicked = m.getElapsedTime();
			}
		}

		m.pushStyle();
		m.pushMatrix();
		
		auto font = m.getCurrentFont();

		float pushed = 0.2 - (m.getElapsedTime() - lastTimeClicked);
		if (pushed < 0) pushed = 0;
		pushed /= 0.2;

        if (drawAsCircles) {
            for (int i = 0; i < c; i++) {
                float animation = A(inside()/* * enabled */);
                m.enableFill();
                
                // outer line
                if (i == hoverIndex) {
                    m.setColor(120);
                }
                else if (i == selectedIndex) {
                    m.setColor(100);
                }
                else { m.setColor(50); }
                m.drawCircle(getSize().y / 4 + (i * w), getSize().y /2, getSize().y / 4);
                
                // inner fill
                m.setColor(BACKGROUND_GREY);
                m.drawCircle(getSize().y / 4 + (i * w), getSize().y /2, getSize().y / 4 - 2);
                
                // inner selected icon
                if (i == hoverIndex) {
                    m.setColor(120);
                }
                else if (i == selectedIndex) {
                    m.setColor(100);
                }
                else { m.setColor(50); }
                m.drawCircle(getSize().y / 4 + (i * w), getSize().y / 2, 3 * animation);
                
                m.setColor(ENABLED_PARAM);
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
                m.prepare<murka::Label>({getSize().y*0.75f + (i * w), getSize().y / 4, w - getSize().y / 2, getSize().y}).text(labels[i]).draw();
                m.disableFill();
            }

        } else {
            // drawing squares with labels inside
            for (int i = 0; i < c; i++) {
                if (i == hoverIndex) {
                    m.setColor(120);
                }
                else if (i == selectedIndex) {
                    m.setColor(100);
                }
                else {
                    m.setColor(50);
                }
                m.enableFill();
                m.drawRectangle(0, 0, w, getSize().y);
                
                m.setColor(BACKGROUND_GREY);
                m.disableFill();
                m.drawRectangle(0, 0, w, getSize().y);
                
                m.setColor(ENABLED_PARAM);
                float offset = (font->stringWidth(labels[i]) / 2);
                font->drawString(labels[i], w / 2 - offset, getSize().y / 2 - font->getLineHeight() / 2);
                
                m.translate(w, 0, 0);
            }
        }
		m.popMatrix();
		m.popStyle();
	};

	// Whatever the parameters and the results are, the functions have to be defined here
	float r = 80, g = 80, b = 80;

	std::vector<std::string> labels;
	int selectedIndex = 0;
	bool changed = false;
    float fontSize = DEFAULT_FONT_SIZE;
    bool drawAsCircles = false;

	// Internal state
	float lastTimeClicked = 0;
    
    RadioGroupWidget & withFontSize(float fontSize_) {
        fontSize = fontSize_;
        return *this;
    }
    
    RadioGroupWidget & drawnAsCircles(bool drawAsCircles_) {
        drawAsCircles = drawAsCircles_;
        return *this;
    }
};
