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
#include "../Config.h"

#if !defined(DEFAULT_FONT_SIZE)
#define DEFAULT_FONTSIZE 10
#endif

using namespace murka;

class M1PlayerControls : public murka::View<M1PlayerControls> {
public:
    M1PlayerControls() {
        playIcon.loadFromRawData(BinaryData::play_png, BinaryData::play_pngSize);
        stopIcon.loadFromRawData(BinaryData::stop_png, BinaryData::stop_pngSize);
        deviceOrientationIcon.loadFromRawData(BinaryData::device_orientation_png, BinaryData::device_orientation_pngSize);
    }

    void internalDraw(Murka & m) {
        if (bypassingBecauseofInactivity) return;
        
        if (isPlaying) {
            // Stop button
            m.setColor(ENABLED_PARAM);
            m.prepare<M1PlayerControlButton>({getSize().x / 2 - 5, getSize().y / 2,
                getSize().y / 4,
                getSize().y / 4})
                .withDrawingCallback([&](MurkaShape shape) {
                    m.setColor(ENABLED_PARAM);
                    m.drawImage(stopIcon, shape.x(), shape.y(), shape.width()/2, shape.height()/2);
                })
                .withOnClickCallback([&](){
                    playPausePressedCallback();
                })
            .draw();
            
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-4);
            m.setColor(ENABLED_PARAM);
            juceFontStash::Rectangle stop_label_box = m.getCurrentFont()->getStringBoundingBox("STOP", 0, 0); // used to find size of text
            m.prepare<murka::Label>({(getSize().x / 2) - (stop_label_box.width / 2),
                40 + 35,
                stop_label_box.width,
                stop_label_box.height}).text("STOP").draw();

        } else {
            // Play button
            m.setColor(ENABLED_PARAM);
            m.prepare<M1PlayerControlButton>({getSize().x / 2 - 5, getSize().y / 2,
                getSize().y / 4,
                getSize().y / 4})
                .withDrawingCallback([&](MurkaShape shape) {
                    m.setColor(ENABLED_PARAM);
                    m.drawImage(playIcon, shape.x(), shape.y(), shape.width()/2, shape.height()/2);
                })
                .withOnClickCallback([&](){
                    playPausePressedCallback();
                })
            .draw();
            
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-4);
            m.setColor(ENABLED_PARAM);
            juceFontStash::Rectangle play_label_box = m.getCurrentFont()->getStringBoundingBox("PLAY", 0, 0); // used to find size of text
            m.prepare<murka::Label>({(getSize().x / 2) - (play_label_box.width / 2),
                40 + 35,
                play_label_box.width,
                play_label_box.height})
            .withAlignment(TEXT_CENTER)
            .text("PLAY")
            .draw();

        }
        
        // Timeline progressbar stand in label
        if (!standaloneMode) {
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-2);
            float width = m.getCurrentFont()->getStringBoundingBox("SYNC TO DAW MODE", 0, 0).width;
            m.prepare<murka::Label>({getSize().x / 2 - width / 2, 30, width, 30}).text("SYNC TO DAW MODE").draw();
        }

        // Connect button
        // TODO: fix placement (too far right)
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
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.311);
                m.drawCircle(shape.width() / 2, shape.height() / 2, shape.height() * 0.311);
                m.popStyle();
            })
            .withOnClickCallback([&](){
                connectButtonCallback();
            })
        .draw();
        
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-5);
        m.setColor(ENABLED_PARAM);
        m.prepare<murka::Label>({getSize().x * 0.85 - getSize().y / 4 - 10,
            40 + 30,
            getSize().y / 4 + 100,
            getSize().y / 4}).text("CONNECTED").draw();
        m.prepare<murka::Label>({getSize().x * 0.85 - getSize().y / 4,
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
            
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-4);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ 43, 40 + 35, 70, 30 }).text("VOLUME").draw();
            
            // current time readout
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-5);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ 10, 25 - 4, 30, 10 }).text(currentTime).draw();
            
            // timeline line
            m.setLineWidth(1);
            m.setColor(ENABLED_PARAM);
            m.drawLine(40, 25, getSize().x - 40, 25);
            
            // Position slider
            m.setColor(M1_ACTION_YELLOW);
            float positionSliderWIdth = getSize().x - 40 - 40;
            float cursorPositionInPixels = currentPositionNormalized * (positionSliderWIdth);
            float sliderHeight = 20;
            m.drawLine(40 + cursorPositionInPixels, 25 - sliderHeight / 2,
                       40 + cursorPositionInPixels, 25 + sliderHeight / 2);
            MurkaShape positionSlider = MurkaShape(40, 10, positionSliderWIdth, 30);
            
            double hoveredTimelinePosition = -1.0;
            if (positionSlider.inside(mousePosition())) {

                float normalizedPositionInsideSlider = (mousePosition().x - positionSlider.position.x) / (positionSlider.size.x);
//
                hoveredTimelinePosition = normalizedPositionInsideSlider;
            }
            
            // Drawing a little hovered line
            if (hoveredTimelinePosition > 0.0) {
                double hoveredPositionInPixels = positionSliderWIdth * hoveredTimelinePosition;
                m.setColor(120);
                m.drawLine(40 + hoveredPositionInPixels, 25 - sliderHeight / 2,
                           40 + hoveredPositionInPixels, 25 + sliderHeight / 2);

            }
            
            // total time readout
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-5);
            m.setColor(ENABLED_PARAM);
            m.prepare<murka::Label>({ getSize().x - 36, 25 - 4, 30, 10 }).text(totalTime).draw();
            
            if (mouseDownPressed(0)) {
                if (hoveredTimelinePosition > 0) {
                    onPositionChangeCallback(hoveredTimelinePosition);
                }
            }
        }
    }
    
    MurImage playIcon, stopIcon, deviceOrientationIcon;
    float internalVolume = 0.0;
    float internalPosition = 0.0;
    float animatedData = 0;
    bool didntInitialiseYet = true;
    bool changed = false;
    bool checked = false; // TODO: implement a way to uncheck/check button fill
    std::string label;
    float fontSize = DEFAULT_FONT_SIZE;
    bool* dataToControl = nullptr;
    bool showCircleWithText = true;
    bool useButtonMode = false;
    bool bypassingBecauseofInactivity = false;
    
    std::function<void(double newPositionNormalised)> onPositionChangeCallback;
    double currentPositionNormalized = 0.0;
    std::string currentTime = "00:00";
    std::string totalTime = "00:00";

    bool isPlaying = false;
    std::function<void()> playPausePressedCallback;
    std::function<void()> connectButtonCallback;
    
    M1PlayerControls & withPlayerData(std::string current_timecode,
                                      std::string total_timecode,
                                      bool showPositionReticle = true,
                                      double currentPosition = 0.0,
                                      bool playing = false,
                                      std::function<void()> playButtonPress = []() {},
                                      std::function<void()> connectButtonPress = []() {},
                                      std::function<void(double)> onPositionChange = [](double newPositionNormalised ){}) {
        currentPositionNormalized = currentPosition;
        if (currentPositionNormalized < 0.0) currentPositionNormalized = 0.0;
        if (currentPositionNormalized > 1.0) currentPositionNormalized = 1.0;
        
        onPositionChangeCallback = onPositionChange;
        isPlaying = playing;
        playPausePressedCallback = playButtonPress;
        connectButtonCallback = connectButtonPress;
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
        internalVolume = volume;
        onVolumeChangeCallback = onVolumeChange;
        return *this;
    }
    
    M1PlayerControls & withPlayPauseCallback(std::function<void()> playPausePressed) {
        playPausePressedCallback = playPausePressed;
    }
};

class VideoPlayerPlayhead : public View<VideoPlayerPlayhead> {
public:
    void internalDraw(Murka& m) {
        float w = getSize().x - 20;
        float h = getSize().y / 2;

        m.drawLine(10, h, 10 + w, h);
        m.drawCircle(10 + playheadPosition * w, h, 10);

        if (inside() && mouseDownPressed(0)) {
            playheadPosition = (mousePosition().x - 10) / w;
        }
    }
    float playheadPosition = 0.0;
};
