#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"
#include "m1_orientation_client/UI/M1Label.h"
#include "../MeshGenerator.h"
#include "../TypesForDataExchange.h"
#include "MarqueeSelection.h"

class VideoPlayerSurface : public View<VideoPlayerSurface> {
private:
    bool inited = false;
    MurVbo sphere, circle;
    MurImage imgOverlay;
    MurShader videoShader;
    bool draggingNow = false;

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
        
        if (drawFlat == false) {
            wasDrawnFlat = false; // reset gate
            camera.setPosition(MurkaPoint3D(0, 0, 0));
            camera.lookAt(MurkaPoint3D(0, 0, -10));

            if (inside() && mouseDownPressed(0) && !draggingNow) {
                draggingNow = true;
            }

            if (draggingNow && !mouseDown(0)) {
                draggingNow = false;
            }

            if (draggingNow) {
                rotationOffsetMouse.x += 0.25 * mouseDelta().x;
                rotationOffsetMouse.y -= 0.25 * mouseDelta().y;
            }
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
            wasDrawnFlat = true;
        }
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
        videoPlayerSurface.draw();

        rotationCurrent = videoPlayerSurface.rotationCurrent;
        rotationOffsetMouse = videoPlayerSurface.rotationOffsetMouse;

        isUpdatedRotation = videoPlayerSurface.isUpdatedRotation;
        
        // Draw object tracking selection overlay (only in 2D/flat mode for now)
        if (objectSelectionEnabled || objectTrackingActive)
        {
            auto& marquee = m.prepare<MarqueeSelection>({ 0, 0, getSize().x, getSize().y });
            marquee.selectionEnabled = objectSelectionEnabled && drawFlat;
            marquee.selectionColor = juce::Colour(0xFF00D4FF);
            marquee.trackingColor = juce::Colour(0xFF00FF88);
            
            // Update tracking visualization
            if (objectTrackingActive && trackingResult.objectFound)
            {
                // Clear the selection rectangle when tracking is active and object found
                marquee.hasSelection = false;
                marquee.selectionRect = juce::Rectangle<float>();
                
                // Convert normalized tracking position to view coordinates
                float viewW = getSize().x;
                float viewH = getSize().y;
                
                juce::Rectangle<float> trackRect(
                    trackingResult.bounds.getX() / static_cast<float>(videoFrameWidth) * viewW,
                    trackingResult.bounds.getY() / static_cast<float>(videoFrameHeight) * viewH,
                    trackingResult.bounds.getWidth() / static_cast<float>(videoFrameWidth) * viewW,
                    trackingResult.bounds.getHeight() / static_cast<float>(videoFrameHeight) * viewH
                );
                
                marquee.setTrackingResult(trackRect, trackingResult.confidence, false);
            }
            else if (objectTrackingActive && !trackingResult.objectFound && trackingResult.processingTimeMs > 0)
            {
                // Clear the selection when tracking (even if searching/lost)
                marquee.hasSelection = false;
                marquee.selectionRect = juce::Rectangle<float>();
                
                // Show "lost" state if we were tracking but lost the object
                marquee.setTrackingResult(lastKnownTrackingRect, 0.0f, true);
            }
            else if (objectTrackingActive)
            {
                // Still searching - hide selection but don't show tracking rect yet
                marquee.hasSelection = false;
                marquee.selectionRect = juce::Rectangle<float>();
            }
            else
            {
                marquee.clearTrackingResult();
            }
            
            // Handle selection callbacks
            marquee.onSelectionComplete = [this](juce::Rectangle<float> normalizedSelection) {
                if (onObjectSelected)
                {
                    // Convert from view coordinates to video frame coordinates
                    juce::Rectangle<int> frameSelection(
                        static_cast<int>(normalizedSelection.getX() * videoFrameWidth),
                        static_cast<int>(normalizedSelection.getY() * videoFrameHeight),
                        static_cast<int>(normalizedSelection.getWidth() * videoFrameWidth),
                        static_cast<int>(normalizedSelection.getHeight() * videoFrameHeight)
                    );
                    onObjectSelected(frameSelection);
                }
            };
            
            marquee.onSelectionCancelled = [this]() {
                if (onSelectionCancelled)
                    onSelectionCancelled();
            };
            
            marquee.draw();
            
            // Store if user made a new selection
            if (marquee.hasSelection)
            {
                currentSelection = marquee.getNormalizedSelection();
            }
        }
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
    
    //==============================================================================
    // Object Tracking Support
    
    /** Enable/disable object selection mode (marquee drawing) */
    bool objectSelectionEnabled = false;
    
    /** Whether object tracking is currently active */
    bool objectTrackingActive = false;
    
    /** Current tracking result for visualization */
    struct TrackingResultView {
        bool objectFound = false;
        juce::Point<float> centerPosition;
        juce::Rectangle<float> bounds;
        float confidence = 0.0f;
        double processingTimeMs = 0.0;
    } trackingResult;
    
    /** Video frame dimensions (needed for coordinate conversion) */
    int videoFrameWidth = 0;
    int videoFrameHeight = 0;
    
    /** Current normalized selection rectangle */
    juce::Rectangle<float> currentSelection;
    
    /** Callback when user completes a selection (provides rect in video frame coordinates) */
    std::function<void(juce::Rectangle<int>)> onObjectSelected;
    
    /** Callback when selection is cancelled */
    std::function<void()> onSelectionCancelled;
    
    /** Sets the tracking result for visualization */
    void setTrackingResult(bool found, juce::Point<float> center, 
                          juce::Rectangle<float> bounds, float confidence, double timeMs)
    {
        trackingResult.objectFound = found;
        trackingResult.centerPosition = center;
        trackingResult.bounds = bounds;
        trackingResult.confidence = confidence;
        trackingResult.processingTimeMs = timeMs;
        
        if (found)
        {
            lastKnownTrackingRect = juce::Rectangle<float>(
                bounds.getX() / static_cast<float>(videoFrameWidth) * getSize().x,
                bounds.getY() / static_cast<float>(videoFrameHeight) * getSize().y,
                bounds.getWidth() / static_cast<float>(videoFrameWidth) * getSize().x,
                bounds.getHeight() / static_cast<float>(videoFrameHeight) * getSize().y
            );
        }
    }
    
    /** Clears the tracking result */
    void clearTrackingResult()
    {
        trackingResult = TrackingResultView();
    }

private:
    juce::Rectangle<float> lastKnownTrackingRect;
};
