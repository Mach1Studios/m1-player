#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "../SphereMeshGenerator.h"


class VideoPlayerSurface : public View<VideoPlayerSurface> {
    bool inited = false;
    MurVbo sphere;
	MurkaPoint3D rotationOffset = { 0, 0, 0 };

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
				float t = mouseDelta().x;
				rotationOffset.x += 0.25 * mouseDelta().x;
				rotationOffset.y -= 0.25 * mouseDelta().y;
			}

			rotationCurrent = rotation + rotationOffset;
			
			// - r.y, r.x, r.z
			camera.setRotation(MurkaPoint3D{ -rotationCurrent.y, -90 - rotationCurrent.x , -rotationCurrent.z }); // YPR -> -P-Y-R 3d camera

            m.beginCamera(camera);
            m.setColor(255);
            if (imgVideo && imgVideo->isAllocated()) {
                m.bind(*imgVideo);
	            m.drawVbo(sphere, GL_TRIANGLE_STRIP, 0, sphere.getIndexes().size());
                m.unbind(*imgVideo);
            }
            m.endCamera(camera);

        }
        else {
			if (imgVideo && imgVideo->isAllocated()) {
				m.drawImage(*imgVideo, 0, 0, getSize().x, getSize().y);
			}
        }
    };

    MurCamera camera;
	MurkaPoint3D rotation = { 0, 0, 0 };
	MurkaPoint3D rotationCurrent = { 0, 0, 0 };
	
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

        if (inside() && mouseDownPressed(0)) {
            playheadPosition = (mousePosition().x - 10) / w;
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
		videoPlayerSurface.rotation = rotation;
		videoPlayerSurface.camera.setFov(fov);
        videoPlayerSurface.draw();

        auto& videoPlayerPlayhead = m.prepare<VideoPlayerPlayhead>({ 0, getSize().y - playheadHeight, getSize().x, playheadHeight });
        videoPlayerPlayhead.playheadPosition = playheadPosition;
        videoPlayerPlayhead.draw();

        playheadPosition = videoPlayerPlayhead.playheadPosition;
		rotationCurrent = videoPlayerSurface.rotationCurrent;
    };

    float drawFlat = false;
    int fov = 70;

    MurImage* imgVideo = nullptr;
	MurkaPoint3D rotation;
	MurkaPoint3D rotationCurrent;
	float playheadPosition = 0.0;
};
