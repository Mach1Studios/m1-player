#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "../SphereMeshGenerator.h"


class VideoPlayerSurface : public View<VideoPlayerSurface> {
    bool inited = false;
    MurVbo sphere;
	MurkaPoint3D rotationOffset = { 0, 0, 0 };
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

public:
    void internalDraw(Murka& m) {

        if (!inited) {
			imgOverlay.loadFromRawData(BinaryData::overlay_png, BinaryData::overlay_pngSize);

            sphere = SphereMeshGenerator().generateSphereMesh(1, 1, 100);
            sphere.setOpenGLContext(m.getOpenGLContext());
            sphere.setup();
            m.updateVbo(sphere);

			videoShader.setOpenGLContext(m.getOpenGLContext());
			videoShader.load(m.vertexShaderBase, fragmentShader);
			

			inited = true;
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
	bool drawOverlay = false;
	float cropStereoscopic = false;

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
		videoPlayerSurface.drawOverlay = drawOverlay;
		videoPlayerSurface.cropStereoscopic = cropStereoscopic;
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
	float drawOverlay = false;
	float cropStereoscopic = false;
	
	int fov = 70;

    MurImage* imgVideo = nullptr;
	MurkaPoint3D rotation;
	MurkaPoint3D rotationCurrent;
	float playheadPosition = 0.0;
};
