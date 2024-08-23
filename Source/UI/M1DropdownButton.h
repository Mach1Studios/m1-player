#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1DropdownButton : public murka::View<M1DropdownButton> {
public:
    void internalDraw(Murka & m) {
        if (outlineEnabled) {
            // outline border
            m.disableFill();
            m.setColor(outlineColor);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
        }
        
        if (drawTriangle) {
            m.enableFill();
            m.setColor(outlineColor);
            MurkaPoint triangleCenter = {m.getSize().width() - 15, 15};
            std::vector<MurkaPoint3D> triangle;
            triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
            triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0}); // top middle
            triangle.push_back({triangleCenter.x , triangleCenter.y + 8, 0});
            triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
            m.drawPath(triangle);
        }
        
        m.setColor(labelColor);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (int)fontSize);
        // interior text
        m.prepare<murka::Label>({labelPaddingLeft, shape.size.y/heightDivisor, shape.size.x - labelPaddingLeft, shape.size.y}).withAlignment(textAlignment).text(label).draw();
        
        pressed = false;
        if ((isHovered()) && (mouseDownPressed(0))) {
            pressed = true; // Only sets to true the frame the "pressed" happened
        }        
    }
    
    std::string label = "";
    float labelPaddingLeft = 5;
    MurkaColor labelColor = MurkaColor(LABEL_TEXT_COLOR);
    bool pressed = false;
    float fontSize = 10;
    bool outlineEnabled = false;
    TextAlignment textAlignment = TEXT_CENTER;
    int heightDivisor = 3;
    MurkaColor bgColor, outlineColor;
    bool drawTriangle = false;
    
    operator bool() {
        return pressed;
    }
    
    M1DropdownButton & withLabel(std::string label_) {
        label = label_;
        return *this;
    }
    
    M1DropdownButton & withLabelColor(MurkaColor lblc) {
        labelColor = lblc;
        return *this;
    }
    
    M1DropdownButton & withOutline(bool outlineEnabled_ = false) {
        outlineEnabled = outlineEnabled_;
        return *this;
    }
    
    M1DropdownButton & withFontSize(float fontSize_) {
        fontSize = fontSize_;
        return *this;
    }
    
    M1DropdownButton & withBackgroundColor(MurkaColor bgc) {
        bgColor = bgc;
        return *this;
    }
    
    M1DropdownButton & withOutlineColor(MurkaColor olc) {
        outlineColor = olc;
        return *this;
    }
    
    M1DropdownButton & withTriangle(bool triangle) {
        drawTriangle = triangle;
        return *this;
    }
};
