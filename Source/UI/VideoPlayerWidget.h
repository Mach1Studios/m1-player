#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"
#include "m1_orientation_client/UI/M1Label.h"
#include "../MeshGenerator.h"
#include "../TypesForDataExchange.h"

class VideoPlayerSurface : public View<VideoPlayerSurface> {
private:
    bool inited = false;
    MurVbo sphere, circle;
    MurImage imgOverlay;
    MurShader videoShader;
    bool draggingNow = false;
    
    // Rectangle Selection for Object Detection
    bool isSelectingRectangle = false;
    bool hasValidSelection = false;
    MurkaPoint selectionStartPoint;
    MurkaPoint selectionEndPoint;
    MurkaShape selectionRectangle;
    
    // Object Detection Reticle
    bool showObjectReticle = false;
    MurkaPoint objectReticlePosition;
    
    std::function<void(juce::Rectangle<int>)> onRectangleSelected;

    std::string fragmentShader = R"(
        varying vec2 vUv;
        varying vec4 vCol;
        uniform sampler2D mainTexture;
        uniform vec4 color;
        uniform bool vflip;
        uniform bool useTexture;
        uniform bool cropStereoscopicTopBottom;
        uniform bool cropStereoscopicLeftRight;

        void main()
        {
            vec2 uv = vUv;
            
            if(cropStereoscopicTopBottom) uv.y = uv.y * 0.5;
            if(cropStereoscopicLeftRight) uv.x = uv.x * 0.5;

            if (vflip) uv.y = 1 - uv.y;
            gl_FragColor = color * vCol * (useTexture ? texture(mainTexture, uv) : vec4(1.0, 1.0, 1.0, 1.0));
        }
    )";

    void drawCircle(Murka& m, float radius, int segments = 32) {
        const float tau = 2.0f * M_PI;
        float theta = 0.0f;
        float thetaStep = tau / static_cast<float>(segments);

        float prevX = radius;
        float prevY = 0;

        for (int i = 0; i <= segments; ++i) {
            float curX = radius * std::cos(theta);
            float curY = radius * std::sin(theta);

            if (i > 0) {
                m.drawLine(prevX, prevY, curX, curY);
            }

            prevX = curX;
            prevY = curY;
            theta += thetaStep;
        }
    }
    
    void drawObjectReticle(Murka& m, MurkaPoint pos) {
        float reticleSize = 20.0f;
        
        m.pushStyle();
        m.setLineWidth(2);
        m.setColor(0, 255, 0, 200); // Green color for object detection reticle
        
        // Draw crosshair
        m.drawLine(pos.x - reticleSize, pos.y, pos.x + reticleSize, pos.y);
        m.drawLine(pos.x, pos.y - reticleSize, pos.x, pos.y + reticleSize);
        
        // Draw circle around target
        float circleRadius = reticleSize * 0.7f;
        m.drawCircle(pos.x, pos.y, circleRadius);
        
        m.popStyle();
    }
    
    void drawSelectionRectangle(Murka& m) {
        if (!isSelectingRectangle && !hasValidSelection) return;
        
        m.pushStyle();
        m.setLineWidth(2);
        m.setColor(255, 255, 0, 180); // Yellow color for selection rectangle
        
        // Draw selection rectangle outline
        m.drawRectangle(selectionRectangle.position.x, selectionRectangle.position.y,
                       selectionRectangle.size.x, selectionRectangle.size.y);
        
        // Draw semi-transparent fill
        m.setColor(255, 255, 0, 40);
        m.drawRectangle(selectionRectangle.position.x, selectionRectangle.position.y,
                       selectionRectangle.size.x, selectionRectangle.size.y);
        
        m.popStyle();
    }

public:
    MurCamera camera;
    MurkaPoint3D rotation = { 0, 0, 0 };
    MurkaPoint3D rotationOffsetMouse = { 0, 0, 0 };
    MurkaPoint3D rotationOffset = { 0, 0, 0 };
    MurkaPoint3D rotationCurrent = { 0, 0, 0 };
    bool isUpdatedRotation = false;

    std::vector<PannerSettings> pannerSettings;

    bool drawFlat = false;
    bool wasDrawnFlat = false;
    bool drawOverlay = false;
    bool crop_Stereoscopic_TopBottom = false;
    bool crop_Stereoscopic_LeftRight = false;

    MurImage* imgVideo = nullptr;
    
    void drawReticle(Murka& m, MurkaPoint p, std::string name, PannerSettings::Color color) {
        float circleRadius = 15;
        juceFontStash::Rectangle rect = m.getCurrentFont()->getStringBoundingBox(name, 0, 0);
        float rectHeight = m.getCurrentFont()->getLineHeight() + 4;
        float rectX = 14;

        m.pushStyle();
        m.pushMatrix();

        m.translate(p.x, p.y, 0);
        m.setLineWidth(3);
        m.setColor(color.r, color.g, color.b);
        m.drawVbo(circle, GL_TRIANGLE_STRIP, 0, circle.getVertices().size());
        m.drawRectangle(rectX, -rectHeight / 2, rect.width + 12, rectHeight);

        m.setColor(0, 0, 0);
        m.drawString(name, rectX + 6, -m.getCurrentFont()->getLineHeight() / 2);

        m.popMatrix();
        m.popStyle();
    }

    MurkaPoint project3DToFlat2D(const MurkaPoint3D& point3D) {
        float azimuth = atan2(-point3D.x, point3D.z); // Calculate azimuth angle (swapped x and z)
        float elevation = asin(point3D.y / sqrt(point3D.x * point3D.x + point3D.y * point3D.y + point3D.z * point3D.z)); // Calculate elevation angle (using vector length)

        // Convert spherical coordinates to 2D coordinates
        float x = (azimuth + M_PI) / (2 * M_PI); // Normalize azimuth to [0, 1]
        float y = 0.5 - elevation / M_PI; // Normalize elevation to [0, 1]
        //DBG(">>" + std::to_string(azimuth) + " , " + std::to_string(elevation) + " , " + std::to_string(point3D.z) + " , " + std::to_string(point3D.x));

        MurkaPoint projectedPoint;
        projectedPoint.x = getSize().x * x;
        projectedPoint.y = getSize().y * y; // Invert y coordinate

        return projectedPoint;
    }

    void internalDraw(Murka& m) {
        if (!inited) {
            imgOverlay.loadFromRawData(BinaryData::overlay_png, BinaryData::overlay_pngSize);

            sphere = MeshGenerator().generateSphereMesh(1, 1, 100);
            sphere.setOpenGLContext(m.getOpenGLContext());
            sphere.setup();
            m.updateVbo(sphere);

            videoShader.setOpenGLContext(m.getOpenGLContext());
            videoShader.load(m.vertexShaderBase, fragmentShader);

            circle = MeshGenerator().generateCircleMesh(15, 25);
            circle.setOpenGLContext(m.getOpenGLContext());
            circle.setup();
            m.updateVbo(circle);

            inited = true;
        }

        MurkaPoint3D rot = rotation + rotationOffsetMouse + rotationOffset;
        isUpdatedRotation = (rotationCurrent != rot);
        rotationCurrent = rot;
        
        // Handle rectangle selection for object detection (Shift + Option + Mouse)
        bool shiftHeld = m.isKeyHeld(murka::MurkaKey::MURKA_KEY_SHIFT);
        bool optionHeld = m.isKeyHeld(murka::MurkaKey::MURKA_KEY_ALT); // Alt is Option on Mac
        bool selectionModifiersHeld = shiftHeld && optionHeld;
        
        // Debug modifier key states
        static bool lastShiftState = false;
        static bool lastOptionState = false;
        static bool lastModifiersState = false;
        
        if (shiftHeld != lastShiftState || optionHeld != lastOptionState || selectionModifiersHeld != lastModifiersState) {
            DBG("Modifier keys - Shift: " + juce::String(std::to_string(shiftHeld)) + ", Option: " + juce::String(std::to_string(optionHeld)) + ", Combined: " + juce::String(std::to_string(selectionModifiersHeld)));
            lastShiftState = shiftHeld;
            lastOptionState = optionHeld;
            lastModifiersState = selectionModifiersHeld;
        }
        
        if (selectionModifiersHeld && inside()) {
            // Rectangle selection mode
            if (mouseDownPressed(0) && !isSelectingRectangle) {
                // Start rectangle selection
                DBG("Starting rectangle selection at: " + juce::String(mousePosition().x) + ", " + juce::String(mousePosition().y));
                isSelectingRectangle = true;
                hasValidSelection = false;
                selectionStartPoint = mousePosition();
                selectionEndPoint = selectionStartPoint;
                selectionRectangle = MurkaShape(selectionStartPoint.x, selectionStartPoint.y, 0, 0);
            }
            
            if (isSelectingRectangle) {
                if (mouseDown(0)) {
                    // Update rectangle during drag
                    selectionEndPoint = mousePosition();
                    
                    float x = std::min(selectionStartPoint.x, selectionEndPoint.x);
                    float y = std::min(selectionStartPoint.y, selectionEndPoint.y);
                    float w = std::abs(selectionEndPoint.x - selectionStartPoint.x);
                    float h = std::abs(selectionEndPoint.y - selectionStartPoint.y);
                    
                    selectionRectangle = MurkaShape(x, y, w, h);
                } else {
                    // Finish rectangle selection
                    DBG("Finishing rectangle selection - size: " + juce::String(selectionRectangle.size.x) + "x" + juce::String(selectionRectangle.size.y));
                    isSelectingRectangle = false;
                    
                    // Only process if rectangle is large enough
                    if (selectionRectangle.size.x > 10 && selectionRectangle.size.y > 10) {
                        hasValidSelection = true;
                        
                        // Call callback to handle the selection
                        if (onRectangleSelected) {
                            juce::Rectangle<int> rect(
                                static_cast<int>(selectionRectangle.position.x),
                                static_cast<int>(selectionRectangle.position.y),
                                static_cast<int>(selectionRectangle.size.x),
                                static_cast<int>(selectionRectangle.size.y)
                            );
                            DBG("Calling rectangle selection callback with rect: " + juce::String(rect.getX()) + ", " + juce::String(rect.getY()) + ", " + juce::String(rect.getWidth()) + ", " + juce::String(rect.getHeight()));
                            onRectangleSelected(rect);
                        } else {
                            DBG("No rectangle selection callback set!");
                        }
                    } else {
                        hasValidSelection = false;
                        DBG("Rectangle selection too small, ignoring");
                    }
                }
            }
        } else {
            // Normal video interaction mode
            if (drawFlat == false) {
                wasDrawnFlat = false; // reset gate
                camera.setPosition(MurkaPoint3D(0, 0, 0));
                camera.lookAt(MurkaPoint3D(0, 0, -10));

                if (inside() && mouseDownPressed(0) && !draggingNow && !isSelectingRectangle) {
                    draggingNow = true;
                }

                if (draggingNow && !mouseDown(0)) {
                    draggingNow = false;
                }

                if (draggingNow) {
                    rotationOffsetMouse.x += 0.25 * mouseDelta().x;
                    rotationOffsetMouse.y -= 0.25 * mouseDelta().y;
                }
            }
        }
        
        if (drawFlat == false) {
            // r.y, r.x, r.z
            camera.setRotation(MurkaPoint3D{ rotationCurrent.y, -rotationCurrent.x , rotationCurrent.z }); // YPR -> +P-Y+R 3d camera

            m.beginCamera(camera);
            m.setColor(255);

            if (imgVideo && imgVideo->isAllocated()) {
                m.bindShader(&videoShader);

                videoShader.setUniform1i("cropStereoscopicTopBottom", crop_Stereoscopic_TopBottom);
                videoShader.setUniform1i("cropStereoscopicLeftRight", crop_Stereoscopic_LeftRight);

                m.bind(*imgVideo);
                m.drawVbo(sphere, GL_TRIANGLE_STRIP, 0, sphere.getIndexes().size());
                m.unbind(*imgVideo);
                m.unbindShader();
            }

            if (drawOverlay) {
                m.bind(imgOverlay);
                m.drawVbo(sphere, GL_TRIANGLE_STRIP, 0, sphere.getIndexes().size());
                m.unbind(imgOverlay);
            }

            m.endCamera(camera);

            // draw panners
            for (int i = 0; i < pannerSettings.size(); i++) {
                if (pannerSettings[i].diverge != 0) {
                    pannerSettings[i].m1Encode.generatePointResults();

                    std::vector<std::string> pointsNames = pannerSettings[i].m1Encode.getPointsNames();
                    std::vector<Mach1Point3D> points = pannerSettings[i].m1Encode.getPoints();

                    for (int j = 0; j < pannerSettings[i].m1Encode.getPointsCount(); j++) {
                        MurkaPoint p = m.getScreenPoint(camera, { -points[j].z, points[j].y, points[j].x });
                        if (p.x >= 0 && p.y >= 0) {
                            if (pointsNames[j] == "LFE" || pointsNames[j] == "W" || pointsNames[j] == "1" || pointsNames[j] == "-1" || pointsNames[j] == "2" || pointsNames[j] == "-2" || pointsNames[j] == "Y" || pointsNames[j] == "-Y" || pointsNames[j] == "Z" || pointsNames[j] == "-Z") {
                                // skip drawing certain non-positional points
                            } else if (pointsNames[j] == "3" || pointsNames[j] == "X") {
                                // convert point name to something more helpful to the user
                                drawReticle(m, p, pannerSettings[i].displayName + ": " + "FRONT", pannerSettings[i].color);
                            } else if (pointsNames[j] == "-3" || pointsNames[j] == "-X") {
                                // convert point name to something more helpful to the user
                                drawReticle(m, p, pannerSettings[i].displayName + ": " + "BACK", pannerSettings[i].color);
                            } else {
                                drawReticle(m, p, pannerSettings[i].displayName + ": " + pointsNames[j], pannerSettings[i].color);
                            }
                        }
                    }
                }
            }
        }
        else
        {
            /// Draw flat
            // Reset rotation components for recentering
            rotation = {0.0f, 0.0f, 0.0f};
            rotationOffsetMouse = {0.0f, 0.0f, 0.0f};
            rotationOffset = {0.0f, 0.0f, 0.0f};
            rotationCurrent = {0.0f, 0.0f, 0.0f};
            
            if (imgVideo && imgVideo->isAllocated()) {
                if (crop_Stereoscopic_TopBottom) {
                    m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y * 2);
                } else if (crop_Stereoscopic_LeftRight) {
                    m.drawImage(*imgVideo, 0, 0, getSize().x * 2, getSize().y);
                } else {
                    m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y);
                }
            }
            
            if (drawOverlay) {
                m.drawImage(imgOverlay, 0, 0, getSize().x, getSize().y);
            }
            
            // draw panners
            for (int i = 0; i < pannerSettings.size(); i++) {
                if (pannerSettings[i].diverge != 0) {
                    pannerSettings[i].m1Encode.generatePointResults();

                    std::vector<std::string> pointsNames = pannerSettings[i].m1Encode.getPointsNames();
                    std::vector<Mach1Point3D> points = pannerSettings[i].m1Encode.getPoints();

                    for (int j = 0; j < pannerSettings[i].m1Encode.getPointsCount(); j++) {
                        MurkaPoint p = project3DToFlat2D({ -points[j].z, points[j].y, points[j].x });
                        drawReticle(m, p, pannerSettings[i].displayName + ": " + pointsNames[j], pannerSettings[i].color);
                    }
                }
            }
            
            // Draw object detection reticle in flat mode
            if (showObjectReticle) {
                drawObjectReticle(m, objectReticlePosition);
            }
            
            wasDrawnFlat = true;
        }
        
        // Draw rectangle selection overlay (works in both 3D and flat modes)
        drawSelectionRectangle(m);
    }

    // Object Detection Methods
    void setObjectReticlePosition(float x, float y) {
        objectReticlePosition.x = x * getSize().x;
        objectReticlePosition.y = y * getSize().y;
        showObjectReticle = true;
    }
    
    void hideObjectReticle() {
        showObjectReticle = false;
    }
    
    void clearSelection() {
        hasValidSelection = false;
        isSelectingRectangle = false;
        selectionRectangle = MurkaShape(0, 0, 0, 0);
    }
    
    void setRectangleSelectionCallback(std::function<void(juce::Rectangle<int>)> callback) {
        onRectangleSelected = callback;
    }
};

class VideoPlayerWidget : public View<VideoPlayerWidget> {
public:
    void internalDraw(Murka& m) {
        auto& videoPlayerSurface = m.prepare<VideoPlayerSurface>({ 0, 0, getSize().x, getSize().y });
        videoPlayerSurface.imgVideo = imgVideo;
        videoPlayerSurface.drawFlat = drawFlat;
        videoPlayerSurface.wasDrawnFlat = wasDrawnFlat;
        videoPlayerSurface.drawOverlay = drawOverlay;
        videoPlayerSurface.crop_Stereoscopic_TopBottom = crop_Stereoscopic_TopBottom;
        videoPlayerSurface.crop_Stereoscopic_LeftRight = crop_Stereoscopic_LeftRight;
        videoPlayerSurface.rotation = rotation;
        videoPlayerSurface.rotationOffset = rotationOffset;
        videoPlayerSurface.camera.setFov(fov);
        videoPlayerSurface.pannerSettings = pannerSettings;
        
        // Forward object detection settings to surface
        if (rectangleSelectionCallback) {
            videoPlayerSurface.setRectangleSelectionCallback(rectangleSelectionCallback);
        }
        if (objectReticleVisible) {
            videoPlayerSurface.setObjectReticlePosition(objectReticleX, objectReticleY);
        } else {
            videoPlayerSurface.hideObjectReticle();
        }
        
        // Handle selection clearing request
        if (shouldClearSelection) {
            videoPlayerSurface.clearSelection();
            shouldClearSelection = false;
        }
        
        videoPlayerSurface.draw();

        rotationCurrent = videoPlayerSurface.rotationCurrent;
        rotationOffsetMouse = videoPlayerSurface.rotationOffsetMouse;

        isUpdatedRotation = videoPlayerSurface.isUpdatedRotation;
    }
    
    // Object Detection Methods
    void setObjectReticlePosition(float x, float y) {
        objectReticleX = x;
        objectReticleY = y;
        objectReticleVisible = true;
    }
    
    void hideObjectReticle() {
        objectReticleVisible = false;
    }
    
    void setRectangleSelectionCallback(std::function<void(juce::Rectangle<int>)> callback) {
        rectangleSelectionCallback = callback;
    }
    
    void clearSelection() {
        // This will be handled by the VideoPlayerSurface during the next draw call
        // We set a flag to clear the selection
        shouldClearSelection = true;
    }

    bool drawFlat = false;
    bool wasDrawnFlat = false; // gate for first draw call of `drawFlat`
    bool drawOverlay = false;
    bool crop_Stereoscopic_TopBottom = false;
    bool crop_Stereoscopic_LeftRight = false;

    int fov = 100;

    // Storage of tracked panner instances
    std::vector<PannerSettings> pannerSettings;

    MurImage* imgVideo = nullptr;
    MurkaPoint3D rotation = { 0, 0, 0 };
    MurkaPoint3D rotationOffset = { 0, 0, 0 };
    MurkaPoint3D rotationOffsetMouse = { 0, 0, 0 };
    MurkaPoint3D rotationCurrent = { 0, 0, 0 };
    MurkaPoint3D rotationPrevious = { 0, 0, 0 };

    bool isUpdatedRotation = false;
    float playheadPosition = 0.0;

private:
    // Object detection state
    std::function<void(juce::Rectangle<int>)> rectangleSelectionCallback;
    bool objectReticleVisible = false;
    float objectReticleX = 0.0f;
    float objectReticleY = 0.0f;
    bool shouldClearSelection = false;
};
