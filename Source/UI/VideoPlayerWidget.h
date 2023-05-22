#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "../SphereMeshGenerator.h"


class VideoPlayerSurface : public View<VideoPlayerSurface> {
    bool inited = false;
    MurVbo sphere;

public:
    void internalDraw(Murka& m) {

        if (!inited) {
            inited = true;

            sphere = SphereMeshGenerator().generateSphereMesh(1, 1, 100);
            sphere.setOpenGLContext(m.getOpenGLContext());
            sphere.setup();
            m.updateVbo(sphere);
        }

        if (drawFlat == false) {
            
            camera.setPosition(MurkaPoint3D(0, 0, 0));
            camera.lookAt(MurkaPoint3D(0, 0, 10));

            if (inside() && mouseDragged(0)) {
                MurkaPoint3D r = camera.getRotation();
                r.x += 0.25 * mouseDelta().y;
                r.y += 0.25 * -mouseDelta().x;
                camera.setRotation(r);
            }

            m.beginCamera(camera);
            m.setColor(255);
            if (imgVideo) {
                m.bind(*imgVideo);
            }
            m.drawVbo(sphere, GL_TRIANGLE_STRIP, 0, sphere.getIndexes().size());
            if (imgVideo) {
                m.unbind(*imgVideo);
            }
            m.endCamera(camera);

        }
        else {
            m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y);
        }
    };

    MurCamera camera;

    bool drawFlat = false;
    MurImage* imgVideo = nullptr;
};

class VideoPlayerPlayhead : public View<VideoPlayerPlayhead> {

public:
    void internalDraw(Murka& m) {

        float w = getSize().x - 20;
        float h = getSize().y / 2;

        m.drawLine(10, h, 10 + w, h);
        m.drawCircle(10 + playheadPosition * w, h, 10);

        if (inside() && m.mouseDown(0)) {
            playheadPosition = (mousePosition().x - 10) / w;
        }
        else {
        }

    };

    float playheadPosition = 0.0;
};

class VideoPlayerWidget : public View<VideoPlayerWidget> {

public:
    void internalDraw(Murka& m) {

        float playheadHeight = 50;

        auto& videoPlayerSurface = m.prepare<VideoPlayerSurface>({ 0, 0, getSize().x, getSize().y - playheadHeight });
        videoPlayerSurface.imgVideo = imgVideo;
        videoPlayerSurface.drawFlat = drawFlat;
        videoPlayerSurface.camera.setFov(fov);
        videoPlayerSurface.draw();

        auto& videoPlayerPlayhead = m.prepare<VideoPlayerPlayhead>({ 0, getSize().y - playheadHeight, getSize().x, playheadHeight });
        videoPlayerPlayhead.playheadPosition = playheadPosition;
        videoPlayerPlayhead.draw();

        playheadPosition = videoPlayerPlayhead.playheadPosition;
        rotation = videoPlayerSurface.camera.getRotation();
    };

    float drawFlat = false;
    int fov = 70;

    MurImage* imgVideo = nullptr;
    MurkaPoint3D rotation;
    float playheadPosition = 0.0;
};
