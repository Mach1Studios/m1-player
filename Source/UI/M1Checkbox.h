#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

#if !defined(DEFAULT_FONT_SIZE)
#define DEFAULT_FONTSIZE 10
#endif

using namespace murka;

class M1Checkbox : public murka::View<M1Checkbox> {
public:
    void internalDraw(Murka & m) {
        if (didntInitialiseYet) {
            animatedData = *((bool*)dataToControl) ? 1.0 : 0.0;
            didntInitialiseYet = false;
        }
        
        float highlight_animation = A(inside() * enabled);
        
		m.pushStyle();
        m.enableFill();
        m.setColor(100 + 110 * enabled + 30 * highlight_animation, 220);
        if (drawAsCircle) {
            m.drawCircle(getSize().y / 2, getSize().y / 2, getSize().y / 2);
        } else {
            m.drawRectangle(0, 0, getSize().y, getSize().y);
        }
        m.setColor(40 + 20 * !enabled, 255);
        if (drawAsCircle) {
            m.drawCircle(getSize().y / 2, getSize().y / 2, getSize().y / 2 - 2);
        } else {
            m.drawRectangle(1, 1, getSize().y - 2, getSize().y - 2);
        }
        
        m.setColor(100 + 110 * enabled + 30 * highlight_animation, 220);
        
        animatedData = A(*((bool*)dataToControl));
        if (drawAsCircle) {
            m.drawCircle(getSize().y / 2, getSize().y / 2,
                         4 * animatedData);
        } else {
            m.drawRectangle(getSize().y / 2 - (8 * animatedData)/2, getSize().y / 2 - (8 * animatedData)/2, 8 * animatedData, 8 * animatedData);
        }

        m.setColor(100 + 110 * enabled + 30 * highlight_animation, 220);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
        m.prepare<murka::Label>({shape.size.y + 6, 2, 150, shape.size.y + 5}).text(label).draw();
        m.disableFill();
        m.popStyle();

        // Action
        if ((mouseDownPressed(0)) && (inside()) && enabled) {
            *((bool*)dataToControl) = !*((bool*)dataToControl);
            changed = true;
		}
		else {
			changed = false;
		}

		checked = *((bool*)dataToControl);
	}
    
    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool checked = true;
    bool drawAsCircle = true;
    std::string label;
    float fontSize = DEFAULT_FONT_SIZE;
    bool* dataToControl = nullptr;
    
    M1Checkbox & controlling(bool* dataPointer) {
        dataToControl = dataPointer;
        return *this;
    }
    
    M1Checkbox & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    M1Checkbox & withFontSize(float fontSize_) {
        fontSize = fontSize_;
        return *this;
    }
    
    M1Checkbox & drawnAsCircle(float drawAsCircle_) {
        drawAsCircle = drawAsCircle_;
        return *this;
    }

    MURKA_PARAMETER(M1Checkbox, // class name
                    bool, // parameter type
                    enabled, // parameter variable name
                    enable, // setter
                    true // default
    )
    
};
