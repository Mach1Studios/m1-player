#pragma once

#include "MurkaTypes.h"
#include "MurkaContext.h"
#include "MurkaView.h"
#include "MurkaInputEventsRegister.h"
#include "MurkaAssets.h"
#include "MurkaLinearLayoutGenerator.h"
#include "MurkaBasicWidgets.h"
#include "M1PlayerControlButton.h"
#include "M1Slider.h"


using namespace murka;

class M1PlayerControls : public murka::View<M1PlayerControls>, public juce::Timer {
public:
    M1PlayerControls() {
        startTimerHz(1);
        playIcon.loadFromRawData(BinaryData::play_png, BinaryData::play_pngSize);
        stopIcon.loadFromRawData(BinaryData::stop_png, BinaryData::stop_pngSize);
    };
    
    int secondsWithoutMouseMove = 0;
    
    MurImage playIcon, stopIcon;
    
    // Timer callback for resizing
    void timerCallback() override {
        secondsWithoutMouseMove += 1;
    }
    
    bool bypassingBecauseofInactivity = false;
    
    void internalDraw(Murka & m) {
        
        if ((m.mouseDelta().x != 0) || (m.mouseDelta().y != 0)) {
            secondsWithoutMouseMove = 0;
        }
        
        bypassingBecauseofInactivity = (secondsWithoutMouseMove > 5);
        if (bypassingBecauseofInactivity) return;

//        m.setColor(20, 20, 20, 150);
//        m.drawRectangle(0, 0, m.getSize().x(), m.getSize().y());
        
        // Play button
        m.setColor(220, 220, 220);
        m.prepare<M1PlayerControlButton>({getSize().x / 2 - getSize().y / 4, 
            getSize().y * 0.4,
            getSize().y / 4,
            getSize().y / 4})
            .withDrawingCallback([&](MurkaShape shape) {
                m.drawImage(playIcon, shape.x(), shape.y(), shape.width(), shape.height());

//                m.drawLine(shape.x() + shape.width(), shape.y(), shape.x() + shape.width(), shape.y() + shape.height());
//                m.drawLine(shape.x(), shape.y(), shape.width(), shape.height());
            })
        .draw();

        // Connect button
        m.prepare<M1PlayerControlButton>({getSize().x * 0.85 - getSize().y / 4,
            getSize().y * 0.4,
            getSize().y / 4,
            getSize().y / 4})
            .withDrawingCallback([&](MurkaShape shape) {
                
                m.pushStyle();
                m.disableFill();
                m.setLineWidth(3);
                m.setColor(130, 130, 130);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.44);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.42);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.40);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.38);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.36);

                m.popStyle();
            })
        .draw();

        if (standaloneMode) {
            // Volume slider
            // TODO: Add an image for the label
            auto& volumeSlider = m.prepare<M1Slider>({ 30, 40, 70, 30 }).withLabel("")
                .hasMovingLabel(false)
                .drawHorizontal(true);
            volumeSlider.rangeFrom = 0.0;
            volumeSlider.rangeTo = 1.0;
            volumeSlider.defaultValue = 1.0;
            volumeSlider.withCurrentValue(internalVolume);
            volumeSlider.valueUpdated = [&](double newVolume) {
                onVolumeChangeCallback(newVolume);
            };
            volumeSlider.draw();
            
            
            // Position slider
            // TODO: Add an image for the label
            auto& positionSlider = m.prepare<M1Slider>({ 30, 20, m.getSize().x() - 60, 30 }).withLabel("")
                .hasMovingLabel(false)
                .drawHorizontal(true);
            positionSlider.rangeFrom = 0.0;
            positionSlider.rangeTo = 1.0;
            positionSlider.defaultValue = 1.0;
            positionSlider.withCurrentValue(currentPositionNormalized);
            positionSlider.valueUpdated = [&](double newPosition) {
                onPositionChangeCallback(newPosition);
            };
            positionSlider.draw();

        }
    }
    
    float internalVolume = 0.0;
    float internalPosition = 0.0;
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
    bool standaloneMode = false;
    
    M1PlayerControls & withStandaloneMode(bool isStandalone) {
        standaloneMode = isStandalone;
        return *this;
    }
    
    M1PlayerControls & withVolumeData(double volume,
                                      std::function<void(double newVolume)> onVolumeChange) {
        onVolumeChangeCallback = onVolumeChange;
        return *this;
    }
    
    std::function<void()> playPausePressedCallback;

    M1PlayerControls & withPlayPauseCallback(std::function<void()> playPausePressed) {
        playPausePressedCallback = playPausePressed;
    }
    
    
    
};
