#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1PlayerControlButton : public murka::View<M1PlayerControlButton> {
public:
    M1PlayerControlButton() {
    }
    
    int secondsWithoutMouseMove = 0;
    
    void internalDraw(Murka & m) {
        drawingFunc({0, 0, m.getSize().width(), m.getSize().height()});
        if ((mouseDownPressed(0)) && (inside())) {
            onClick();
        }
    }
    
    MurkaColor color;
    std::function<void(MurkaShape shape)> drawingFunc;
    
    M1PlayerControlButton& withColor(MurkaColor c) {
        color = c;
        return *this;
    }
    
    M1PlayerControlButton& withDrawingCallback(std::function<void(MurkaShape)> callback) {
        drawingFunc = callback;
        return *this;
    }
    
    std::function<void()> onClick = [](){};

    M1PlayerControlButton& withOnClickCallback(std::function<void()> clickCallback) {
        onClick = clickCallback;
        return *this;
    }
};
