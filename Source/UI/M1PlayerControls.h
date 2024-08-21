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
        deviceOrientationIcon.loadFromRawData(BinaryData::device_orientation_png, BinaryData::device_orientation_pngSize);
    }
    
    int secondsWithoutMouseMove = 0;
    
    MurImage playIcon, stopIcon, deviceOrientationIcon;
    
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
        
        // Play button
        m.setColor(ENABLED_PARAM);
        m.prepare<M1PlayerControlButton>({getSize().x / 2 - 5, getSize().y / 2,
            getSize().y / 4,
            getSize().y / 4})
            .withDrawingCallback([&](MurkaShape shape) {
                m.setColor(ENABLED_PARAM);
                m.drawImage(playIcon, shape.x(), shape.y(), shape.width()/2, shape.height()/2);
            })
        .draw();
        
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 6);
        m.setColor(ENABLED_PARAM);
        m.prepare<murka::Label>({getSize().x / 2 - getSize().y / 4 + 10,
            40 + 35,
            getSize().y / 4 + 20,
            getSize().y / 4}).text("PLAY").draw();

        // Timeline progressbar
        if (!standaloneMode) {
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 6);
            float width = m.getCurrentFont()->getStringBoundingBox("SYNC TO DAW MODE", 0, 0).width;
            m.prepare<murka::Label>({getSize().x / 2 - width / 2, 30, width, 30}).text("SYNC TO DAW MODE").draw();
        }

        // Connect button
        m.prepare<M1PlayerControlButton>({getSize().x * 0.85 - 15,
            getSize().y * 0.4 + 12,
            10, 10})
            .withDrawingCallback([&](MurkaShape shape) {
                m.pushStyle();
                m.disableFill();
                m.setLineWidth(3);
                m.setColor(GRID_LINES_4_RGB);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.44);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.42);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.40);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.38);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.36);
                m.popStyle();
            })
        .draw();
        
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 6);
        m.setColor(ENABLED_PARAM);
        m.prepare<murka::Label>({getSize().x * 0.85 - getSize().y / 4 - 15,
            40 + 30,
            getSize().y / 4 + 100,
            getSize().y / 4}).text("CONNECTED").draw();
        m.prepare<murka::Label>({getSize().x * 0.85 - getSize().y / 4 - 5,
            40 + 40,
            getSize().y / 4 + 100,
            getSize().y / 4}).text("DEVICE").draw();

        if (standaloneMode) {
            // Volume slider
            // TODO: Add an image for the label
            m.setLineWidth(1);
            auto& volumeSlider = m.prepare<M1Slider>({ 45, 40, 40, 30 })
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
            
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 6);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ 40, 40 + 35, 70, 30 }).text("VOLUME").draw();
            
            // current time readout
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 5);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ 10, 25 - 4, 30, 10 }).text(currentTime).draw();
            // timeline line
            m.setLineWidth(1);
            m.setColor(ENABLED_PARAM);
            m.drawLine(40, 25, getSize().x - 40, 25);
            // Position slider
            m.setColor(M1_ACTION_YELLOW);
            float positionSliderWIdth = getSize().x - 30 - 30;
            float cursorPositionInPixels = currentPositionNormalized * positionSliderWIdth;
            float sliderHeight = 20;
            m.drawLine(40 + cursorPositionInPixels, 25 - sliderHeight / 2,
                       40 + cursorPositionInPixels, 25 + sliderHeight / 2);
            MurkaShape positionSlider = MurkaShape(40, 20, getSize().x - 80, 60);
            // total time readout
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 5);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ getSize().x - 38, 25 - 4, 30, 10 }).text(totalTime).draw();
            
            if (mouseDownPressed(0)) {
                if (positionSlider.inside(mousePosition())) {
                    float normalizedPositionInsideSlider = ((positionSlider.position - mousePosition()) / positionSlider.size).x;
                    onPositionChangeCallback(normalizedPositionInsideSlider);
                    std::cout << "player position change request" << normalizedPositionInsideSlider << std::endl;
                }
            }
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
    std::string currentTime = "00:00";
    std::string totalTime = "00:00";

    bool isPlaying = false;
    std::function<void()> playButtonCallback;

    M1PlayerControls & withPlayerData(std::string current_timecode,
                                      std::string total_timecode,
                                      bool showPositionReticle = true,
                                      double currentPosition = 0.0,
                                      bool playing = false,
                                      std::function<void()> playButtonPress = []() {},
                                      std::function<void(double)> onPositionChange = [](double newPositionNormalised ){}) {
        currentPositionNormalized = currentPosition;
        onPositionChangeCallback = onPositionChange;
        isPlaying = playing;
        playButtonCallback = playButtonPress;
        currentTime = current_timecode;
        totalTime = total_timecode;
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
