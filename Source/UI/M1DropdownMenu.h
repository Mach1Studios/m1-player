#pragma once

#include "MurkaAssets.h"
#include "MurkaBasicWidgets.h"
#include "MurkaContext.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaTypes.h"
#include "MurkaView.h"

using namespace murka;

class M1DropdownMenu : public murka::View<M1DropdownMenu>
{
    enum closingModeTypes { modeUnknown,
        modeMouseUp,
        modeMouseDown };
    closingModeTypes closingMode = closingModeTypes::modeUnknown;

    int mouseKeptDownFrames = 0;

public:
    void internalDraw(Murka& m)
    {
        if (closingMode == modeUnknown)
        {
            if (mouseDown(0))
                mouseKeptDownFrames++;
            if (mouseKeptDownFrames > 20)
                closingMode = modeMouseUp;
            if (!mouseDown(0))
                closingMode = modeMouseDown;
        }

        if (opened)
        {
            // outline border
            m.setColor(outlineColor);
            m.drawRectangle(0, 0, shape.size.x, shape.size.y);
            m.setColor(backgroundColor);
            m.drawRectangle(1, 1, shape.size.x - 2, shape.size.y - 2);

            bool hoveredAnything = false;

            // Scrolbar calculations

            bool drawScrollbar = (options.size() * optionHeight) > shape.size.y; // draw scrollbar if there's too many options
            if (drawScrollbar)
            {
                maxScrollbarOffset = (options.size() * optionHeight) - shape.size.y;
            }

            int scrollbarWidth = 15;
            float scrollbarHeightRatio = shape.size.y / (options.size() * optionHeight);
            float scrollbarHeight = scrollbarHeightRatio * shape.size.y;
            float scrollbarPositionY = (scrollbarOffsetInPixels / maxScrollbarOffset) * (shape.size.y - scrollbarHeight);

            MurkaShape scrollbarShape = MurkaShape(shape.size.x - scrollbarWidth, scrollbarPositionY, scrollbarWidth, scrollbarHeight);
            bool preciseHoveredScrollbar = scrollbarShape.inside(mousePosition());

            if (!drawScrollbar)
            {
                scrollbarOffsetInPixels = 0;
            }

            bool coarseHoveredScrollbar = (drawScrollbar ? mousePosition().x > shape.size.x - scrollbarWidth : false);

            // Drawing the options

            for (int i = 0; i < options.size(); i++)
            {
                bool hoveredAnOption = (mousePosition().y > i * optionHeight - scrollbarOffsetInPixels) && (mousePosition().y < (i + 1) * optionHeight - scrollbarOffsetInPixels) && inside();

                hoveredAnOption *= !coarseHoveredScrollbar; // If we hovered scrollbar section, we're not going to press any element
                hoveredAnything += hoveredAnOption; // Setting to true if any of those are true

                if (hoveredAnOption)
                {
                    m.setColor(outlineColor);
                    m.drawRectangle(1, i * optionHeight - scrollbarOffsetInPixels, shape.size.x - 2, optionHeight);
                    m.setColor(highlightLabelColor);
                    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
                    juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(options[i], 0, 0); // used to find size of text
                    m.prepare<murka::Label>({ labelPadding_x, (optionHeight * i) + optionHeight / 2 - label_box.height / 2 - scrollbarOffsetInPixels, shape.size.x - labelPadding_x, optionHeight }).text(options[i]).withAlignment(textAlignment).draw();

                    if (closingMode == modeMouseDown)
                    {
                        if (mouseDownPressed(0))
                        {
                            if (selectedOption != i)
                            {
                                changed = true;
                                close(true);
                            }
                            else
                                close(false);
                            selectedOption = i;
                        }
                    }
                    if (closingMode == modeMouseUp)
                    {
                        if (mouseReleased(0))
                        {
                            opened = false; // Closing the menu
                            if (selectedOption != i)
                            {
                                changed = true;
                            }
                            selectedOption = i;
                        }
                    }
                }
                else
                {
                    m.setColor(labelColor); // default color
                    if (i == selectedOption)
                    {
                        m.setColor(selectedLabelColor);
                    }
                    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, fontSize);
                    juceFontStash::Rectangle label_box = m.getCurrentFont()->getStringBoundingBox(options[i], 0, 0); // used to find size of text
                    m.prepare<murka::Label>({ labelPadding_x, (optionHeight * i) + optionHeight / 2 - label_box.height / 2 - scrollbarOffsetInPixels, shape.size.x - labelPadding_x, optionHeight }).text(options[i]).withAlignment(textAlignment).draw();
                }

                // Closing if pressed/released outside of the menu
                if (!inside() && !hoveredAnything && !holdingScrollbar && opened)
                {
                    if ((closingMode == modeMouseDown) && (mouseDownPressed(0)))
                    {
                        close(false);
                    }
                    if ((closingMode == modeMouseUp) && (mouseReleased(0)))
                    {
                        close(false);
                    }
                }
            }

            if (drawScrollbar)
            {
                if (preciseHoveredScrollbar)
                    m.setColor(ENABLED_PARAM);
                else
                    m.setColor(120, 120, 120);
                m.drawRectangle(scrollbarShape);
            }

            if (preciseHoveredScrollbar && mouseDownPressed(0))
            {
                holdingScrollbar = true;
            }

            // TODO: Add mouse scrolling to control the scroll bar too
            if (holdingScrollbar)
            {
                scrollbarOffsetInPixels -= mouseDelta().y * m.getScreenScale();

                if (scrollbarOffsetInPixels > maxScrollbarOffset)
                    scrollbarOffsetInPixels = maxScrollbarOffset;
                if (scrollbarOffsetInPixels < 0)
                    scrollbarOffsetInPixels = 0;
            }

            if (holdingScrollbar && !mouseDown(0))
            {
                holdingScrollbar = false;
            }
        }
        else
        {
            changed = false;
        }
    }

    bool holdingScrollbar = false;

    void close(bool hasChanged = false)
    {
        opened = false;
        mouseKeptDownFrames = 0;
        closingMode = closingModeTypes::modeUnknown;
        changed = hasChanged;
    }

    void open()
    {
        opened = true;
        mouseKeptDownFrames = 0;
        closingMode = closingModeTypes::modeUnknown;
    }

    float scrollbarOffsetInPixels = 0;
    float maxScrollbarOffset = 0;
    bool grabbedScrollbar = false;

    bool changed = false;
    bool opened = false;
    int selectedOption = 0;
    std::vector<std::string> options;
    std::string* dataToControl = nullptr;

    int optionHeight = 30;
    int fontSize = 10;
    bool enabled = true;
    std::string label;
    float labelPadding_x = 0;
    MurkaColor highlightLabelColor = MurkaColor(LABEL_TEXT_COLOR);
    MurkaColor labelColor = MurkaColor(LABEL_TEXT_COLOR);
    MurkaColor selectedLabelColor = MurkaColor(LABEL_TEXT_COLOR);
    MurkaColor backgroundColor = MurkaColor(BACKGROUND_GREY);
    MurkaColor outlineColor = MurkaColor(ENABLED_PARAM);
    MurkaShape triggerButtonShape;
    TextAlignment textAlignment = TEXT_CENTER;

    M1DropdownMenu& controlling(std::string* dataPointer)
    {
        dataToControl = dataPointer;
        return *this;
    }

    M1DropdownMenu& withLabel(std::string label_)
    {
        label = label_;
        return *this;
    }

    M1DropdownMenu& withLabelColor(MurkaColor lblc)
    {
        labelColor = lblc;
        return *this;
    }

    M1DropdownMenu& withSelectedLabelColor(MurkaColor slblc)
    {
        selectedLabelColor = slblc;
        return *this;
    }

    M1DropdownMenu& withHighlightLabelColor(MurkaColor hlblc)
    {
        highlightLabelColor = hlblc;
        return *this;
    }

    M1DropdownMenu& withBackgroundColor(MurkaColor bgc)
    {
        backgroundColor = bgc;
        return *this;
    }

    M1DropdownMenu& withOutlineColor(MurkaColor olc)
    {
        outlineColor = olc;
        return *this;
    }

    M1DropdownMenu& withFontSize(float fontSize_)
    {
        fontSize = fontSize_;
        return *this;
    }

    M1DropdownMenu& withOptions(std::vector<std::string> options_)
    {
        options = options_;
        return *this;
    }

    M1DropdownMenu& withTriggerButtonPlacedAt(MurkaShape shape)
    {
        triggerButtonShape = shape;
        return *this;
    }
};
