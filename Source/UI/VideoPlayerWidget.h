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
            float curY =  radius * std::sin(theta);

            if (i > 0) {
                m.drawLine(prevX, prevY,curX, curY);
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
    
    void drawReticle(float x, float y, std::string label, bool reticleHovered, Murka& m) {
        float realx = x;
        float realy = y;
        
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();
        m.drawCircle(realx-1, realy-1, (10 + 3 * A(reticleHovered)));
        
        if (realx+14 > getSize().x){
            //draw rollover shape on left side
            float left_rollover = (realx+14)-getSize().x;
            m.drawCircle(left_rollover-14, realy+5, (10 + 3 * A(reticleHovered)));
        }
        if (realx-14 < 0){
            //draw rollover shape on right side
            float right_rollover = abs(realx - 14);
            m.drawCircle(getSize().x-right_rollover+14, realy+5, (10 + 3 * A(reticleHovered)));
        }
        
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, (10 + 2 * A(reticleHovered)));
        m.setColor(M1_ACTION_YELLOW);
        m.disableFill();
        M1Label& l = m.prepare<M1Label>(MurkaShape(realx-9, realy-7 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();
        
        if (realx + 20 > getSize().x){
            //draw rollover shape on left side
            float left_rollover = (realx+8)-getSize().x;
            m.prepare<M1Label>(MurkaShape(left_rollover-16, realy-2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();
        }
        if (realx-20 < 0){
            //draw rollover shape on right side
            float right_rollover = abs(realx-8);
            m.prepare<M1Label>(MurkaShape(getSize().x-right_rollover, realy-2 - 2 * A(reticleHovered), 50, 50)).text(label.c_str()).draw();
        }
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

            // draw panners as circles
            m.pushStyle();
            for (int i = 0; i < pannerSettings.size(); i++) {
                // convert azimuth and elevation to cartesian coordinates
                float azi = -pannerSettings[i].azimuth * M_PI / 180;
                float ele = pannerSettings[i].elevation * M_PI / 180;

                float x = sin(azi) * cos(ele);
                float y = sin(ele);
                float z = cos(azi) * cos(ele);

                // draw the panner circle
                MurkaPoint p = m.getScreenPoint(camera, { x, y, z });

                float circleRadius = 15;
                std::string pannerName = pannerSettings[i].displayName;
                juceFontStash::Rectangle rect = m.getCurrentFont()->getStringBoundingBox(pannerName, 0, 0);
                float rectHeight = m.getCurrentFont()->getLineHeight() + 4;
                float rectX = circleRadius * 2 / 3;

                m.pushMatrix();
                m.translate(p.x, p.y, 0);
                m.setLineWidth(3);
                m.setColor(0, 0, 255);
                m.drawVbo(circle, GL_TRIANGLE_STRIP, 0, circle.getVertices().size());
                m.drawRectangle(rectX, -rectHeight / 2, rect.width + 8, rectHeight);

                m.setColor(255, 255, 255);
                m.drawString(pannerName, rectX + 4, -m.getCurrentFont()->getLineHeight() / 2);
                m.popMatrix();
            }
            m.popStyle();
        }
        else {
            // draw flat
            if (imgVideo && imgVideo->isAllocated()) {
                m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y);
            }
            
//            // Draw reticles for each discovered panner and each channel
//            if (panners.size() > 0) {
//                for (int p = 0; p < panners.size(); p++) {
//                    std::vector<std::string> pointsNames = panners[p].m1Encode.getPointsNames();
//                    std::vector<Mach1Point3D> points = panners[p].m1Encode.getPoints();
//                    for (int i = 0; i < panners[p].m1Encode.getPointsCount(); i++) {
//                        float r, d;
//                        float x = points[i].z;
//                        float y = points[i].x;
//                        if (x == 0 && y == 0) {
//                            r = 0;
//                            d = 0;
//                        } else {
//                            d = sqrtf(x*x + y * y) / sqrt(2.0);
//                            float rotation_radian = atan2(x, y);//acos(x/d);
//                            float rotation_degree = juce::radiansToDegrees(rotation_radian);
//                            r = (rotation_degree/360.) + 0.5; // normalize 0->1
//                        }
//                        drawReticle(r * shape.size.x, (-points[i].y + 1.0)/2 * shape.size.y, pointsNames[i], false, m);
//                    }
//                }
//            }
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
