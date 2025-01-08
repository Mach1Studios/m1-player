#pragma once

#include "MurkaAssets.h"
#include "MurkaBasicWidgets.h"
#include "MurkaContext.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaTypes.h"
#include "MurkaView.h"

using namespace murka;

class M1DropdownButton : public murka::View<M1DropdownButton>
{
public:
    void internalDraw(Murka& m)
    {
        if (outlineEnabled)
        {
            // outline border
            m.disableFill();
            m.setColor(outlineColor);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
        }

        if (drawTriangle)
        {
            m.enableFill();
            m.setColor(outlineColor);
            MurkaPoint triangleCenter = { m.getSize().width() - 15, 15 };
            std::vector<MurkaPoint3D> triangle;
            triangle.push_back({ triangleCenter.x - 5, triangleCenter.y, 0 });
            triangle.push_back({ triangleCenter.x + 5, triangleCenter.y, 0 }); // top middle
            triangle.push_back({ triangleCenter.x, triangleCenter.y + 8, 0 });
            triangle.push_back({ triangleCenter.x - 5, triangleCenter.y, 0 });
            m.drawPath(triangle);
        }

        m.setColor(labelColor);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (int)fontSize);
        // interior text
        juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(label, 0, 0); // used to find size of text
        m.prepare<murka::Label>({ labelPadding_x, labelPadding_y + shape.size.y / 2 - label_box.height / 2, shape.size.x - labelPadding_x, shape.size.y }).withAlignment(textAlignment).text(label).draw();

        pressed = false;
        if ((isHovered()) && (mouseDownPressed(0)))
        {
            pressed = true; // Only sets to true the frame the "pressed" happened
        }
    }

    std::string label = "";
    float labelPadding_x = 0;
    MurkaColor labelColor = MurkaColor(LABEL_TEXT_COLOR);
    bool pressed = false;
    float fontSize = 10;
    bool outlineEnabled = false;
    TextAlignment textAlignment = TEXT_CENTER;
    int labelPadding_y = 0;
    MurkaColor bgColor, outlineColor;
    bool drawTriangle = false;

    operator bool()
    {
        return pressed;
    }

    M1DropdownButton& withLabel(std::string label_)
    {
        label = label_;
        return *this;
    }

    M1DropdownButton& withLabelColor(MurkaColor lblc)
    {
        labelColor = lblc;
        return *this;
    }

    M1DropdownButton& withOutline(bool outlineEnabled_ = false)
    {
        outlineEnabled = outlineEnabled_;
        return *this;
    }

    M1DropdownButton& withFontSize(float fontSize_)
    {
        fontSize = fontSize_;
        return *this;
    }

    M1DropdownButton& withBackgroundColor(MurkaColor bgc)
    {
        bgColor = bgc;
        return *this;
    }

    M1DropdownButton& withOutlineColor(MurkaColor olc)
    {
        outlineColor = olc;
        return *this;
    }

    M1DropdownButton& withTriangle(bool triangle)
    {
        drawTriangle = triangle;
        return *this;
    }
};
