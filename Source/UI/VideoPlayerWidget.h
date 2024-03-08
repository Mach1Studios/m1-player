#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "juce_murka/Murka/MurkaBasicWidgets.h"
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

    std::string fragmentShader = R"(
        varying vec2 vUv;
        varying vec4 vCol;
        uniform sampler2D mainTexture;
        uniform vec4 color;
        uniform bool vflip;
        uniform bool useTexture;
        uniform bool cropStereoscopic;

        void main()
        {
            vec2 uv = vUv;
            
            if(cropStereoscopic) uv.y =  uv.y * 0.5;

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

    std::vector<PannerSettings> pannerSettings;

    bool drawFlat = false;
    bool drawOverlay = false;
    bool cropStereoscopic = false;

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

        if (drawFlat == false) {
            camera.setPosition(MurkaPoint3D(0, 0, 0));
            camera.lookAt(MurkaPoint3D(0, 0, -10));

            if (inside() && mouseDragged(0)) {
                float t = mouseDelta().x;
                rotationOffsetMouse.x += 0.25 * mouseDelta().x;
                rotationOffsetMouse.y -= 0.25 * mouseDelta().y;
            }

            rotationCurrent = rotation + rotationOffsetMouse + rotationOffset;

            // r.y, r.x, r.z
            camera.setRotation(MurkaPoint3D{ rotationCurrent.y, -rotationCurrent.x , rotationCurrent.z }); // YPR -> +P-Y+R 3d camera

            m.beginCamera(camera);
            m.setColor(255);

            if (imgVideo && imgVideo->isAllocated()) {
                m.bindShader(&videoShader);

                videoShader.setUniform1i("cropStereoscopic", cropStereoscopic);

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
                pannerSettings[i].m1Encode.generatePointResults();

                std::vector<std::string> pointsNames = pannerSettings[i].m1Encode.getPointsNames();
                std::vector<Mach1Point3D> points = pannerSettings[i].m1Encode.getPoints();

                for (int j = 0; j < pannerSettings[i].m1Encode.getPointsCount(); j++) {
                    MurkaPoint p = m.getScreenPoint(camera, { -points[j].z, points[j].y, points[j].x });
                    drawReticle(m, p, pannerSettings[i].displayName + ": " + pointsNames[j], pannerSettings[i].color);
                }
            }
        }
        else {
            // draw flat
            if (imgVideo && imgVideo->isAllocated()) {
                m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y);
            }

            // draw panners
            for (int i = 0; i < pannerSettings.size(); i++) {
                pannerSettings[i].m1Encode.generatePointResults();

                std::vector<std::string> pointsNames = pannerSettings[i].m1Encode.getPointsNames();
                std::vector<Mach1Point3D> points = pannerSettings[i].m1Encode.getPoints();

                for (int j = 0; j < pannerSettings[i].m1Encode.getPointsCount(); j++) {
                    MurkaPoint p = project3DToFlat2D({ -points[j].z, points[j].y, points[j].x });
                    drawReticle(m, p, pannerSettings[i].displayName + ": " + pointsNames[j], pannerSettings[i].color);
                }

            }
        }
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

class VideoPlayerWidget : public View<VideoPlayerWidget> {
public:
    void internalDraw(Murka& m) {
        float playheadHeight = 50;

        auto& videoPlayerSurface = m.prepare<VideoPlayerSurface>({ 0, 0, getSize().x, getSize().y - playheadHeight });
        videoPlayerSurface.imgVideo = imgVideo;
        videoPlayerSurface.drawFlat = drawFlat;
        videoPlayerSurface.drawOverlay = drawOverlay;
        videoPlayerSurface.cropStereoscopic = cropStereoscopic;
        videoPlayerSurface.rotation = rotation;
        videoPlayerSurface.rotationOffset = rotationOffset;
        videoPlayerSurface.camera.setFov(fov);
        videoPlayerSurface.pannerSettings = pannerSettings;
        videoPlayerSurface.draw();

        auto& videoPlayerPlayhead = m.prepare<VideoPlayerPlayhead>({ 0, getSize().y - playheadHeight, getSize().x, playheadHeight });
        videoPlayerPlayhead.playheadPosition = playheadPosition;
        videoPlayerPlayhead.draw();

        playheadPosition = videoPlayerPlayhead.playheadPosition;
        rotationCurrent = videoPlayerSurface.rotationCurrent;
        rotationOffsetMouse = videoPlayerSurface.rotationOffsetMouse;
    }

    bool drawFlat = false;
    bool drawOverlay = false;
    bool cropStereoscopic = false;

    int fov = 70;

    // Storage of tracked panner instances
    std::vector<PannerSettings> pannerSettings;

    MurImage* imgVideo = nullptr;
    MurkaPoint3D rotation = { 0, 0, 0 };
    MurkaPoint3D rotationOffset = { 0, 0, 0 };
    MurkaPoint3D rotationOffsetMouse = { 0, 0, 0 };
    MurkaPoint3D rotationCurrent = { 0, 0, 0 };
    MurkaPoint3D rotationPrevious = { 0, 0, 0 };
    float playheadPosition = 0.0;
};
