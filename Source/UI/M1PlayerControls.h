#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"

using namespace murka;

class M1PlayerControls : public murka::View<M1PlayerControls>, public juce::Timer {
public:
    M1PlayerControls() {
        startTimerHz(1);
    };
    
    int secondsWithoutMouseMove = 0;
    
    // Timer callback for resizing
    void timerCallback() override {
        secondsWithoutMouseMove += 1;
    }
    
    void internalDraw(Murka & m) {
        
        if ((m.mouseDelta().x != 0) || (m.mouseDelta().y != 0)) {
            secondsWithoutMouseMove = 0;
        }
        
        if (secondsWithoutMouseMove > 5) return;

        m.setColor(20, 20, 20, 150);
        m.drawRectangle(0, 0, m.getSize().x(), m.getSize().y());
    }
    
    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool checked = false; // TODO: implement a way to uncheck/check button fill
    std::string label;
    double fontSize = 10;
    bool* dataToControl = nullptr;
    bool showCircleWithText = true;
    bool useButtonMode = false;
    
    std::function<void(double newPositionNormalised)> onPositionChangeCallback;
    double currentPositionNormalized = 0.0;
    
    M1PlayerControls & withPlayerData(std::string startTime,
                                      std::string endTime,
                                      bool showPositionReticle = true,
                                      double currentPosition = 0.0,
                                      std::function<void(double)> onPositionChange = [](double newPositionNormalised ){}) {
        currentPositionNormalized = currentPosition;
        onPositionChangeCallback = onPositionChange;
        return *this;
    }
    
    std::function<void(double newVolume)> onVolumeChangeCallback;
    double currentVolume = 0.0;
    
    M1PlayerControls & withVolumeData(double volume,
                                      std::function<void(double newVolume)> onVolumeChange) {
        onVolumeChangeCallback = onVolumeChange;
        return *this;
    }


    
};
