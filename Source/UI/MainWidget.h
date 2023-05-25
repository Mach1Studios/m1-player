#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"

#include "Mach1Decode.h"

class MainWidget : public View<MainWidget> {
    bool inited = false;
	MurImage imgLogo;

public:
	MurImage* imgVideo = nullptr;

	M1OrientationOSCClient m1OrientationOSCClient;
	M1OrientationClientWindow orientationControlWindow;
	bool showOrientationControlMenu = false;
	bool showedOrientationControlBefore = false;

	std::shared_ptr<foleys::AVClip> clipVideo;
	std::shared_ptr<foleys::AVClip> clipAudio;

	juce::AudioTransportSource*  transportSourceVideo = nullptr;
	juce::AudioTransportSource* transportSourceAudio = nullptr;

	Mach1Point3D currentOrientation = { 0.0, 0.0, 0.0 };

	double currentPlayerWidgetFov = 0;

	bool drawReference = false;

	void setStatus(bool success, std::string message)
	{
		//this->status = message;
		std::cout << success << " , " << message << std::endl;
	}

	void internalDraw(Murka& m) {

        if (!inited) {
			m1OrientationOSCClient.init(6345);
			m1OrientationOSCClient.setStatusCallback(std::bind(&MainWidget::setStatus, this, std::placeholders::_1, std::placeholders::_2));

			imgLogo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);

			inited = true;
		}

		m.clear(20);
		m.setColor(255);
		m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);

		if (clipVideo.get() != nullptr || clipAudio.get() != nullptr) {
			auto& videoPlayerWidget = m.prepare<VideoPlayerWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });

			if (m1OrientationOSCClient.isConnectedToServer()) {
				M1OrientationYPR ypr = m1OrientationOSCClient.getOrientation().getYPR();
				videoPlayerWidget.rotation.x = m1OrientationOSCClient.getTrackingYawEnabled() ? ypr.yaw : 0.0f;
				videoPlayerWidget.rotation.y = m1OrientationOSCClient.getTrackingPitchEnabled() ? ypr.pitch : 0.0f;
				videoPlayerWidget.rotation.z = m1OrientationOSCClient.getTrackingRollEnabled() ? ypr.roll : 0.0f;
			}

			currentOrientation.x = videoPlayerWidget.rotationCurrent.x;
			currentOrientation.y = videoPlayerWidget.rotationCurrent.y;
			currentOrientation.z = videoPlayerWidget.rotationCurrent.z;
			currentPlayerWidgetFov = videoPlayerWidget.fov;

			videoPlayerWidget.imgVideo = imgVideo;

			float length = 0;
			if (clipVideo.get() != nullptr && clipAudio.get() != nullptr) length = ((std::min)(transportSourceAudio->getLengthInSeconds(), transportSourceVideo->getLengthInSeconds()));
			else if (clipVideo.get() != nullptr) length = transportSourceVideo->getLengthInSeconds();
			else if (clipAudio.get() != nullptr) length = transportSourceAudio->getLengthInSeconds();

			float playheadPosition = transportSourceAudio->getCurrentPosition() / length;

			videoPlayerWidget.playheadPosition = playheadPosition;
			videoPlayerWidget.draw();

			if (videoPlayerWidget.playheadPosition != playheadPosition) {
				float pos = videoPlayerWidget.playheadPosition * length;
				transportSourceVideo->setPosition(pos);
				transportSourceAudio->setPosition(pos);
			}

			if (isKeyPressed('z')) {
				videoPlayerWidget.drawFlat = !videoPlayerWidget.drawFlat;
			}

			if (isKeyPressed('w')) {
				videoPlayerWidget.fov += 10;
			}

			if (isKeyPressed('s')) {
				videoPlayerWidget.fov -= 10;
			}

			if (isKeyPressed('g')) {
				drawReference = !drawReference;
			}

			if (isKeyPressed('o')) {
				videoPlayerWidget.drawOverlay = !videoPlayerWidget.drawOverlay;
			}

			if (isKeyPressed('d')) {
				videoPlayerWidget.cropStereoscopic = !videoPlayerWidget.cropStereoscopic;
			}

			if (drawReference) {
				m.drawImage(*imgVideo, 0, 0, imgVideo->getWidth() * 0.3, imgVideo->getHeight() * 0.3);
			}

			// play button
			{
				bool isPlaying = (transportSourceVideo->isPlaying() || transportSourceAudio->isPlaying());
				auto& playButton = m.prepare<murka::Button>({ 10, m.getWindowHeight() - 100, 60, 30 }).text(!isPlaying ? "play" : "pause").draw();
				if (playButton.pressed) {
					if (isPlaying) {
						transportSourceVideo->stop();
						transportSourceAudio->stop();
					}
					else {
						transportSourceVideo->start();
						transportSourceAudio->start();
					}
				}
			}

			// stop button
			{
				auto& stopButton = m.prepare<murka::Button>({ 80, m.getWindowHeight() - 100, 60, 30 }).text("stop").draw();
				if (stopButton.pressed) {
					if (clipVideo.get() != nullptr) {
						clipVideo->setNextReadPosition(0);
					}

					if (clipAudio.get() != nullptr) {
						clipAudio->setNextReadPosition(0);
					}

					transportSourceVideo->stop();
					transportSourceAudio->stop();
				}
			}

		}
		else {
			std::string message = "Drop a audio and video files here [Press Q for Hotkeys & Info]";
			float width = m.getCurrentFont()->getStringBoundingBox(message, 0, 0).width;
			m.prepare<murka::Label>({ m.getWindowWidth() * 0.5 - width * 0.5, m.getWindowHeight() * 0.5, 350, 30 }).text(message).draw();
		}

		if (isKeyHeld('q')) {
			m.getCurrentFont()->drawString("Fov : " + std::to_string(currentPlayerWidgetFov), 10, 10);
			m.getCurrentFont()->drawString("Playing: " + std::string(clipVideo.get() != nullptr ? "yes" : "no"), 10, 90);
			m.getCurrentFont()->drawString("Frame: " + std::to_string((std::max)(transportSourceAudio->getCurrentPosition(), transportSourceVideo->getCurrentPosition())), 10, 110);

			m.getCurrentFont()->drawString("Hotkeys:", 10, 130);
			m.getCurrentFont()->drawString("[w] - FOV+", 10, 150);
			m.getCurrentFont()->drawString("[s] - FOV-", 10, 170);
			m.getCurrentFont()->drawString("[z] - Equirectangular / 2D", 10, 190);
			m.getCurrentFont()->drawString("[g] - Overlay 2D Reference", 10, 210);
			m.getCurrentFont()->drawString("[o] - Overlay Reference", 10, 230);
			m.getCurrentFont()->drawString("[d] - Crop stereoscopic", 10, 250);
			m.getCurrentFont()->drawString("[Arrow Keys] - Orientation Resets", 10, 270);

			m.getCurrentFont()->drawString("OverlayCoords:", 10, 330);
			m.getCurrentFont()->drawString("Y: " + std::to_string(currentOrientation.x), 10, 350);
			m.getCurrentFont()->drawString("P: " + std::to_string(currentOrientation.y), 10, 370);
			m.getCurrentFont()->drawString("R: " + std::to_string(currentOrientation.z), 10, 390);
		}

		if (m.eventState.isKeyPressed(' ')) {
			if (transportSourceVideo->isPlaying() || transportSourceAudio->isPlaying()) {
				transportSourceVideo->stop();
				transportSourceAudio->stop();
			}
			else {
				transportSourceVideo->start();
				transportSourceAudio->start();
			}
		}


		// draw m1 logo
		m.drawImage(imgLogo, m.getWindowWidth() - imgLogo.getWidth()*0.3 - 10, m.getWindowHeight() - imgLogo.getHeight()*0.3 - 10, imgLogo.getWidth() * 0.3, imgLogo.getHeight() * 0.3);

		std::vector<M1OrientationClientWindowDeviceSlot> slots;

		std::vector<M1OrientationDeviceInfo> devices = m1OrientationOSCClient.getDevices();
		for (int i = 0; i < devices.size(); i++) {
			std::string icon = "";
			if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeBLE) icon = "bt";
			else icon = "wifi";

			std::string name = devices[i].getDeviceName();
			slots.push_back({ icon, name, name == m1OrientationOSCClient.getCurrentDevice().getDeviceName(), i, [&](int idx)
				{
					m1OrientationOSCClient.command_startTrackingUsingDevice(devices[idx]);
				}
				});
		}


		//TODO: set size with getWidth()
		auto& orientationControlButton = m.prepare<M1OrientationWindowToggleButton>({ m.getSize().width() - 40 - 5, 5, 40, 40 }).onClick([&](M1OrientationWindowToggleButton& b) {
			showOrientationControlMenu = !showOrientationControlMenu;
			})
			.withInteractiveOrientationGimmick(m1OrientationOSCClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone, m.getElapsedTime() * 100)
				.draw();

		auto ytt = m1OrientationOSCClient.getCurrentDevice().getDeviceType();

		if (orientationControlButton.hovered && (m1OrientationOSCClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone)) {
			std::string deviceReportString = "Tracking device:" + m1OrientationOSCClient.getCurrentDevice().getDeviceName();
			auto font = m.getCurrentFont();
			auto bbox = font->getStringBoundingBox(deviceReportString, 0, 0);
			m.setColor(40, 40, 40, 200);
			m.drawRectangle(678 + 40 - bbox.width - 5, 45, bbox.width + 10, 30);
			m.setColor(230, 230, 230);
			m.prepare<M1Label>({ 678 + 40 - bbox.width - 5, 48, bbox.width + 10, 30 }).text(deviceReportString).draw();
		}

		if (showOrientationControlMenu) {
			bool showOrientationSettingsPanelInsideWindow = (m1OrientationOSCClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
			orientationControlWindow = m.prepare<M1OrientationClientWindow>({ 500, 45, 218, 300 + 100 * showOrientationSettingsPanelInsideWindow })
				.withDeviceList(slots)
				.withSettingsPanelEnabled(showOrientationSettingsPanelInsideWindow)
				.onClickOutside([&]() {
					if (!orientationControlButton.hovered) { // Only switch showing the orientation control if we didn't click on the button
						showOrientationControlMenu = !showOrientationControlMenu;
						if (showOrientationControlMenu && !showedOrientationControlBefore) {
							orientationControlWindow.startRefreshing();
						}
					}
				})
				.onDisconnectClicked([&]() {
						m1OrientationOSCClient.command_disconnect();
					})
				.onRefreshClicked([&]() {
						m1OrientationOSCClient.command_refreshDevices();
				})
				.onYPRSwitchesClicked([&](int whichone) {
						if (whichone == 0) m1OrientationOSCClient.command_setTrackingYawEnabled(m1OrientationOSCClient.getTrackingYawEnabled());
						if (whichone == 1) m1OrientationOSCClient.command_setTrackingPitchEnabled(m1OrientationOSCClient.getTrackingPitchEnabled());
						if (whichone == 2) m1OrientationOSCClient.command_setTrackingRollEnabled(m1OrientationOSCClient.getTrackingRollEnabled());
				})
				.withYPRTrackingSettings(
					m1OrientationOSCClient.getTrackingYawEnabled(),
					m1OrientationOSCClient.getTrackingPitchEnabled(),
					m1OrientationOSCClient.getTrackingPitchEnabled(),
					std::pair<int, int>(0, 180),
					std::pair<int, int>(0, 180),
					std::pair<int, int>(0, 180))
				.withYPR(
					m1OrientationOSCClient.getOrientation().getYPR().yaw,
					m1OrientationOSCClient.getOrientation().getYPR().pitch,
					m1OrientationOSCClient.getOrientation().getYPR().roll
				);

			orientationControlWindow.draw();
		}

    };


};
