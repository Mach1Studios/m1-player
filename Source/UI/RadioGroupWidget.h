#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

class RadioGroupWidget : public View<RadioGroupWidget> {
    
public:
	void internalDraw(Murka & m) {
		int c = labels.size();
		float w = (m.getSize().width() / c);
		
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

		for (int i = 0; i < c; i++) {
			m.enableFill();
			if (i == hoverIndex) {
				m.setColor(120);
			}
			else if (i == selectedIndex) {
				m.setColor(100);
			}
			else {
				m.setColor(50);
			}
			m.drawRectangle(0, 0, w, getSize().y);

			m.setColor(30);
			m.disableFill();
			m.drawRectangle(0, 0, w, getSize().y);

			m.setColor(255);

			float offset = (font->stringWidth(labels[i]) / 2);
			font->drawString(labels[i], w / 2 - offset, getSize().y / 2 - font->getLineHeight() / 2);

			m.translate(w, 0, 0);
		}

		m.pushMatrix();
		m.popStyle();
	};


	// Whatever the parameters and the results are, the functions have to be defined here
	float r = 80, g = 80, b = 80;

	std::vector<std::string> labels;
	int selectedIndex = 0;
	bool changed = false;

	// Internal state
	float lastTimeClicked = 0;
};
