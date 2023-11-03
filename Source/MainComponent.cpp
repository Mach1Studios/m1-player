#include "MainComponent.h"

//==============================================================================
MainComponent::MainComponent()
{
	// Make sure you set the size of the component after
    // you add any child components.
	juce::OpenGLAppComponent::setSize(800, 600);

	// specify the number of input and output channels that we want to open
	setAudioChannels(0, 2);
}

MainComponent::~MainComponent()
{
    m1OrientationClient.command_disconnect();
    m1OrientationClient.close();
	shutdownAudio();
	juce::OpenGLAppComponent::shutdownOpenGL();
}

//==============================================================================
void MainComponent::initialise() 
{
	murka::JuceMurkaBaseComponent::initialise();

	videoEngine.getFormatManager().registerFormat(std::make_unique<foleys::FFmpegFormat>());

    // We will assume the folders are properly created during the installation step
    juce::File settingsFile;
    // Using common support files installation location
    juce::File m1SupportDirectory = juce::File::getSpecialLocation(juce::File::commonApplicationDataDirectory);

    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        // test for any mac OS
        settingsFile = m1SupportDirectory.getChildFile("Application Support").getChildFile("Mach1");
    } else if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::Windows) != 0) {
        // test for any windows OS
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    } else {
        settingsFile = m1SupportDirectory.getChildFile("Mach1");
    }
    settingsFile = settingsFile.getChildFile("settings.json");
    DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());
    
    // Informs OrientationManager that this client is expected to send additional offset for the final orientation to be calculated and to count instances for error handling
    m1OrientationClient.setClientType("player"); // Needs to be set before the init() function
    m1OrientationClient.initFromSettings(settingsFile.getFullPathName().toStdString());
    m1OrientationClient.setStatusCallback(std::bind(&MainComponent::setStatus, this, std::placeholders::_1, std::placeholders::_2));

	imgLogo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);
    
    playerOSC.AddListener([&](juce::OSCMessage msg) {
        if (msg.getAddressPattern() == "/m1-activate-client") {
            DBG("[OSC] Recieved msg | Activate: "+std::to_string(msg[0].getInt32()));
            // Capturing monitor mode
            int active = msg[0].getInt32();
            if (active == 1) {
                playerOSC.setAsActivePlayer(true);
            } else if (active == 0) {
                playerOSC.setAsActivePlayer(false);
            }
        }
    });

    // playerOSC update timer loop
    startTimer(200);
} 

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, ablock size, etc) are changed.
	sampleRate = newSampleRate;
	blockSize = samplesPerBlockExpected;

    // Setup for Mach1Decode
    smoothedChannelCoeffs.resize(m1Decode.getFormatCoeffCount());
    spatialMixerCoeffs.resize(m1Decode.getFormatCoeffCount());
    for (int input_channel = 0; input_channel < m1Decode.getFormatChannelCount(); input_channel++) {
        smoothedChannelCoeffs[input_channel * 2].reset(sampleRate, (double)0.01);
        smoothedChannelCoeffs[input_channel * 2 + 1].reset(sampleRate, (double)0.01);
    }
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
	std::shared_ptr<foleys::AVClip> clip = (clipAudio.get() != nullptr) ? clipAudio : clipVideo;

    if (clip){
        // the AudioTransportSource takes care of start, stop and resample
		 
		// first read video source
		if (clipVideo.get() != nullptr && clipVideo != clipAudio) {
			readBufferVideo.setSize(clipVideo->getNumChannels(), bufferToFill.numSamples);
			readBufferVideo.clear();

			juce::AudioSourceChannelInfo info(&readBufferVideo,
				bufferToFill.startSample,
				bufferToFill.numSamples);

			transportSourceVideo.getNextAudioBlock(info);
		}

		// then read audio source
		detectedNumInputChannels = clip->getNumChannels();

		readBufferAudio.setSize(detectedNumInputChannels, bufferToFill.numSamples);
		readBufferAudio.clear();

		juce::AudioSourceChannelInfo info(&readBufferAudio,
			bufferToFill.startSample,
			bufferToFill.numSamples);

		transportSourceAudio.getNextAudioBlock(info);

		tempBuffer.setSize(detectedNumInputChannels * 2, bufferToFill.numSamples);
		tempBuffer.clear();

        /// Detect input audio channels
        if (detectedNumInputChannels > 0) {
            /// Mono or Stereo
            // TODO: mute or block audio playback by default
            // TODO: add button for playing stereo/mono audio in videoplayer?
      
            /// Multichannel

            // if you've got more output channels than input clears extra outputs
            for (auto channel = detectedNumInputChannels; channel < 2 && channel < detectedNumInputChannels; ++channel)
                readBufferAudio.clear(channel, 0, bufferToFill.numSamples);
            
            // Mach1Decode processing loop
			/*
			M1OrientationYPR ypr = m1OrientationClient.getOrientation().getYPR();
			currentOrientation.x = m1OrientationClient.getTrackingYawEnabled() ? ypr.yaw : 0.0f;
			currentOrientation.y = m1OrientationClient.getTrackingPitchEnabled() ? ypr.pitch : 0.0f;
			currentOrientation.z = m1OrientationClient.getTrackingRollEnabled() ? ypr.roll : 0.0f;
            */
            m1Decode.setRotationDegrees({ currentOrientation.yaw, currentOrientation.pitch, currentOrientation.roll });

            m1Decode.beginBuffer();
            spatialMixerCoeffs = m1Decode.decodeCoeffs();
            m1Decode.endBuffer();
            
            // Update spatial mixer coeffs from Mach1Decode for a smoothed value
            for (int channel = 0; channel < detectedNumInputChannels; ++channel) {
                smoothedChannelCoeffs[channel * 2 + 0].setTargetValue(spatialMixerCoeffs[channel * 2    ]);
                smoothedChannelCoeffs[channel * 2 + 1].setTargetValue(spatialMixerCoeffs[channel * 2 + 1]);
            }
            
            float* outBufferL = bufferToFill.buffer->getWritePointer(0);
            float* outBufferR = bufferToFill.buffer->getWritePointer(1);
            std::vector<float> spatialCoeffsBufferL, spatialCoeffsBufferR;

            if (detectedNumInputChannels == m1Decode.getFormatChannelCount()){ // dumb safety check, TODO: do better i/o error handling
                                
                // copy from readBufferAudio for doubled channels
                for (auto channel = 0; channel < detectedNumInputChannels; ++channel){
                    tempBuffer.copyFrom(channel * 2 + 0, 0, readBufferAudio, channel, 0, bufferToFill.numSamples);
                    tempBuffer.copyFrom(channel * 2 + 1, 0, readBufferAudio, channel, 0, bufferToFill.numSamples);
                }
            
                for (int sample = 0; sample < info.numSamples; sample++) {
                    for (int channel = 0; channel < detectedNumInputChannels; channel++) {
						outBufferL[sample] += tempBuffer.getReadPointer(channel * 2 + 0)[sample] * smoothedChannelCoeffs[channel * 2].getNextValue();
						outBufferR[sample] += tempBuffer.getReadPointer(channel * 2 + 1)[sample] * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
                    }
                }
            } else if (detectedNumInputChannels == 1) {
				// mono
				bufferToFill.buffer->copyFrom(0, 0, readBufferAudio, 0, 0, info.numSamples);
				bufferToFill.buffer->copyFrom(1, 0, readBufferAudio, 0, 0, info.numSamples);
			}
			else if (detectedNumInputChannels == 2) {
				// stereo
				bufferToFill.buffer->copyFrom(0, 0, readBufferAudio, 0, 0, info.numSamples);
				bufferToFill.buffer->copyFrom(1, 0, readBufferAudio, 1, 0, info.numSamples);
			}
			else {
				// Invalid Decode I/O; clear buffers
				for (int channel = detectedNumInputChannels; channel < 2; ++channel)
					bufferToFill.buffer->clear(channel, 0, bufferToFill.numSamples);
            }
            
            // clear remaining input channels
            for (auto channel = 2; channel < detectedNumInputChannels; ++channel)
                readBufferAudio.clear(channel, 0, bufferToFill.numSamples);
            
        } else {
            bufferToFill.clearActiveBufferRegion();
        }
    } else {
        detectedNumInputChannels = 0;
    }
}

void MainComponent::releaseResources()
{
	if (clipVideo.get() != nullptr) {
		clipVideo->releaseResources();
		transportSourceVideo.releaseResources();
	}
	if (clipAudio.get() != nullptr) {
		clipAudio->releaseResources();
		transportSourceAudio.releaseResources();
	}
}

void MainComponent::timecodeChanged(int64_t, double seconds)
{
}

//==============================================================================

bool MainComponent::isInterestedInFileDrag(const juce::StringArray&)
{
	return true;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
	for (int i = 0; i < files.size(); ++i) {
		const juce::String& currentFile = files[i];
		
		openFile(currentFile);

		if (clipVideo.get() != nullptr) {
			clipVideo->setNextReadPosition(0);
			transportSourceVideo.stop();
		}

		if (clipAudio.get() != nullptr) {
			clipAudio->setNextReadPosition(0);
			transportSourceAudio.stop();
		}
	}
	juce::Process::makeForegroundProcess();
}

void MainComponent::openFile(juce::File filepath)
{
	// Video Setup
	{
		std::shared_ptr<foleys::AVClip> newClip = videoEngine.createClipFromFile(juce::URL(filepath));

		if (newClip.get() == nullptr)
			return;

		/// Video Setup
		if (newClip->hasVideo()) {
			newClip->prepareToPlay(blockSize, sampleRate);

			if (clipVideo.get() != nullptr) {
				clipVideo->removeTimecodeListener(this);
			}

			newClip->addTimecodeListener(this);
		
			transportSourceVideo.setSource(newClip.get(), 0, nullptr);

			clipVideo = newClip;
		}
	}

    // Audio Setup
	{
		std::shared_ptr<foleys::AVClip> newClip = videoEngine.createClipFromFile(juce::URL(filepath));

		if (newClip.get() == nullptr)
			return;

		if (newClip->hasAudio())
		{
			newClip->prepareToPlay(blockSize, sampleRate);

			detectedNumInputChannels = newClip.get()->getNumChannels();

			// Setup for Mach1Decode API
			m1Decode.setPlatformType(Mach1PlatformDefault);
			m1Decode.setFilterSpeed(0.99);

			if (detectedNumInputChannels == 4) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoHorizon_4);
			}
			else if (detectedNumInputChannels == 8) {
				bool useIsotropic = true; // TODO: implement this switch
				if (useIsotropic) {
					m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_8);
				}
				else {
				}
			}
			else if (detectedNumInputChannels == 12) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_12);
			}
			else if (detectedNumInputChannels == 14) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_14);
			}
			else if (detectedNumInputChannels == 32) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_32);
			}
			else if (detectedNumInputChannels == 36) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_36);
			}
			else if (detectedNumInputChannels == 48) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_48);
			}
			else if (detectedNumInputChannels == 60) {
				m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_60);
			}
			 
			transportSourceAudio.setSource(newClip.get(), 0, nullptr);

			clipAudio = newClip;
		}
	}
}

void MainComponent::setStatus(bool success, std::string message)
{
	//this->status = message;
	std::cout << success << " , " << message << std::endl;
}

void MainComponent::shutdown()
{ 
	clipVideo = nullptr;
	clipAudio = nullptr;

	murka::JuceMurkaBaseComponent::shutdown();
}

void MainComponent::draw() {
	// sync playhead from monitor
	if (m1OrientationClient.getPlayerLastUpdate() != lastUpdateForPlayer) {
		lastUpdateForPlayer = m1OrientationClient.getPlayerLastUpdate();

		float length = (std::max)(transportSourceAudio.getLengthInSeconds(), transportSourceVideo.getLengthInSeconds());
		float pos = (std::max)(transportSourceAudio.getCurrentPosition(), transportSourceVideo.getCurrentPosition());
		DBG(std::to_string(m1OrientationClient.getPlayerPositionInSeconds() - pos));
		if (fabs(m1OrientationClient.getPlayerPositionInSeconds() - pos) > 0.1 && m1OrientationClient.getPlayerPositionInSeconds() < length) {
			transportSourceVideo.setPosition(m1OrientationClient.getPlayerPositionInSeconds() + 0.05);
			transportSourceAudio.setPosition(m1OrientationClient.getPlayerPositionInSeconds() + 0.05);
		}

		if ((clipVideo.get() != nullptr && m1OrientationClient.getPlayerIsPlaying() != transportSourceVideo.isPlaying()) ||
			(clipAudio.get() != nullptr && m1OrientationClient.getPlayerIsPlaying() != transportSourceAudio.isPlaying())) {
			if (m1OrientationClient.getPlayerIsPlaying()) {
				transportSourceVideo.start();
				transportSourceAudio.start();
			}
			else {
				transportSourceVideo.stop();
				transportSourceAudio.stop();
			}
		}
	}

	// update video frame
	if (clipVideo.get() != nullptr) {
		foleys::VideoFrame& frame = clipVideo->getFrame(clipVideo->getCurrentTimeInSeconds());
		if (frame.image.getWidth() > 0 && frame.image.getHeight() > 0) {
			if (imgVideo.getWidth() != frame.image.getWidth() || imgVideo.getHeight() != frame.image.getHeight()) {
				imgVideo.allocate(frame.image.getWidth(), frame.image.getHeight());
			}
			juce::Image::BitmapData srcData(frame.image, juce::Image::BitmapData::readOnly);
			imgVideo.loadData(srcData.data, GL_BGRA);
		}
	}

	m.clear(20);
	m.setColor(255);
	m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);

	auto& videoPlayerWidget = m.prepare<VideoPlayerWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });

	if (playerOSC.IsConnected() && playerOSC.IsActivePlayer()) {
        // sending un-normalized full range values in degrees
        
        // calculate normalized signed offset and send to server
        M1OrientationYPR offset;
        offset = currentOrientation - previousOrientation;
        
        playerOSC.sendPlayerYPR(offset.yaw, offset.pitch, offset.roll);
        
        // add server orientation to player
		M1OrientationYPR ypr = m1OrientationClient.getOrientation().getYPRasDegrees();
		videoPlayerWidget.rotation.x = m1OrientationClient.getTrackingYawEnabled() ? ypr.yaw : 0.0f;
		videoPlayerWidget.rotation.y = m1OrientationClient.getTrackingPitchEnabled() ? ypr.pitch : 0.0f;
		videoPlayerWidget.rotation.z = m1OrientationClient.getTrackingRollEnabled() ? ypr.roll : 0.0f;
	}

	currentOrientation.yaw = videoPlayerWidget.rotationCurrent.x;
	currentOrientation.pitch = videoPlayerWidget.rotationCurrent.y;
	currentOrientation.roll = videoPlayerWidget.rotationCurrent.z;
	currentPlayerWidgetFov = videoPlayerWidget.fov;

	if (clipVideo.get() != nullptr || clipAudio.get() != nullptr) {

		if (clipVideo.get() != nullptr) {
			videoPlayerWidget.imgVideo = &imgVideo;
		}

		float length = 0;
		if (clipVideo.get() != nullptr && clipAudio.get() != nullptr) length = ((std::min)(transportSourceAudio.getLengthInSeconds(), transportSourceVideo.getLengthInSeconds()));
		else if (clipVideo.get() != nullptr) length = transportSourceVideo.getLengthInSeconds();
		else if (clipAudio.get() != nullptr) length = transportSourceAudio.getLengthInSeconds();

		float playheadPosition = transportSourceAudio.getCurrentPosition() / length;

		videoPlayerWidget.playheadPosition = playheadPosition;

		if (videoPlayerWidget.playheadPosition != playheadPosition) {
			float pos = videoPlayerWidget.playheadPosition * length;
			transportSourceVideo.setPosition(pos);
			transportSourceAudio.setPosition(pos);
		}
	}
	
	// draw overlay if video empty
	if (clipVideo.get() == nullptr) {
		videoPlayerWidget.drawOverlay = true;
	}
	videoPlayerWidget.draw();
	
	// draw reference
	if (clipVideo.get() != nullptr || clipAudio.get() != nullptr) {
		// play button
		{
			bool isPlaying = (transportSourceVideo.isPlaying() || transportSourceAudio.isPlaying());
			auto& playButton = m.prepare<murka::Button>({ 10, m.getWindowHeight() - 100, 60, 30 }).text(!isPlaying ? "play" : "pause").draw();
			if (playButton.pressed) {
				if (isPlaying) {
					transportSourceVideo.stop();
					transportSourceAudio.stop();
				}
				else {
					transportSourceVideo.start();
					transportSourceAudio.start();
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
				transportSourceVideo.stop();
				transportSourceAudio.stop();
			}
		}

		if (drawReference) {
			m.drawImage(imgVideo, 0, 0, imgVideo.getWidth() * 0.3, imgVideo.getHeight() * 0.3);
		}

		auto& modeRadioGroup = m.prepare<RadioGroupWidget>({ 20, 20, 90, 30 });
		modeRadioGroup.labels = { "3D", "2D" };
		if (!bHideUI) {
			modeRadioGroup.selectedIndex = videoPlayerWidget.drawFlat ? 1 : 0;
			modeRadioGroup.draw();
			if (modeRadioGroup.changed) {
				if (modeRadioGroup.selectedIndex == 0) {
					videoPlayerWidget.drawFlat = false;
					drawReference = false;
				}
				else if (modeRadioGroup.selectedIndex == 1) {
					videoPlayerWidget.drawFlat = true;
					drawReference = false;
				}
				else {
					videoPlayerWidget.drawFlat = false;
					drawReference = true;
				}
			}
		}

		auto& drawOverlayCheckbox = m.prepare<murka::Checkbox>({ 20, 50, 130, 30 });
		drawOverlayCheckbox.dataToControl = &(videoPlayerWidget.drawOverlay);
		drawOverlayCheckbox.label = "OVERLAY";
		if (!bHideUI) {
			drawOverlayCheckbox.draw();
		}

		auto& cropStereoscopicCheckbox = m.prepare<murka::Checkbox>({ 20, 80, 130, 30 });
		cropStereoscopicCheckbox.dataToControl = &(videoPlayerWidget.cropStereoscopic);
		cropStereoscopicCheckbox.label = "CROP STEREOSCOPIC";
		if (!bHideUI) {
			cropStereoscopicCheckbox.draw();
		}
	}
	else {
		std::string message = "Drop an audio or video file here";
		float width = m.getCurrentFont()->getStringBoundingBox(message, 0, 0).width;
		m.prepare<murka::Label>({ m.getWindowWidth() * 0.5 - width * 0.5, m.getWindowHeight() * 0.5, 350, 30 }).text(message).draw();
	}


	if (m.isKeyPressed('z')) {
		videoPlayerWidget.drawFlat = !videoPlayerWidget.drawFlat;
	}

	// TODO: fix these reset keys, they are supposed to set the overal camera to front/back/left/right not the offset
	if (m.isKeyPressed(MurkaKey::MURKA_KEY_UP)) { // up arrow
		videoPlayerWidget.rotationOffset.x = 0;
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_DOWN)) { // down arrow
		videoPlayerWidget.rotationOffset.x = 180;
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_RIGHT)) { // right arrow
		videoPlayerWidget.rotationOffset.x = 90;
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_LEFT)) { // left arrow
		videoPlayerWidget.rotationOffset.x = 270;
	}

	if (m.isKeyPressed('w') || m.mouseScroll().y > lastScrollValue.y) {
		videoPlayerWidget.fov -= 10; 
	}

	if (m.isKeyPressed('s') || m.mouseScroll().y < lastScrollValue.y) {
		videoPlayerWidget.fov += 10;
	}

	if (m.isKeyPressed('g')) {
		drawReference = !drawReference;
	}

	if (m.isKeyPressed('o')) {
		videoPlayerWidget.drawOverlay = !videoPlayerWidget.drawOverlay;
	}

	if (m.isKeyPressed('d')) {
		videoPlayerWidget.cropStereoscopic = !videoPlayerWidget.cropStereoscopic;
	}

	// toggles hiding the UI for better video review
	if (m.isKeyPressed('h')) {
		bHideUI = !bHideUI;
	}


	if (m.isKeyHeld('q')) {
		m.getCurrentFont()->drawString("Fov : " + std::to_string(currentPlayerWidgetFov), 10, 10);
		m.getCurrentFont()->drawString("Playing: " + std::string(clipVideo.get() != nullptr ? "yes" : "no"), 10, 90);
		m.getCurrentFont()->drawString("Frame: " + std::to_string((std::max)(transportSourceAudio.getCurrentPosition(), transportSourceVideo.getCurrentPosition())), 10, 110);

		m.getCurrentFont()->drawString("Hotkeys:", 10, 130);
		m.getCurrentFont()->drawString("[w] - FOV+", 10, 150);
		m.getCurrentFont()->drawString("[s] - FOV-", 10, 170);
		m.getCurrentFont()->drawString("[z] - Equirectangular / 2D", 10, 190);
		m.getCurrentFont()->drawString("[g] - Overlay 2D Reference", 10, 210);
		m.getCurrentFont()->drawString("[o] - Overlay Reference", 10, 230);
		m.getCurrentFont()->drawString("[d] - Crop stereoscopic", 10, 250);
        m.getCurrentFont()->drawString("[h] - Hide UI", 10, 270);
		m.getCurrentFont()->drawString("[Arrow Keys] - Orientation Resets", 10, 290);

		m.getCurrentFont()->drawString("OverlayCoords:", 10, 350);
		m.getCurrentFont()->drawString("Y: " + std::to_string(currentOrientation.yaw), 10, 370);
		m.getCurrentFont()->drawString("P: " + std::to_string(currentOrientation.pitch), 10, 390);
		m.getCurrentFont()->drawString("R: " + std::to_string(currentOrientation.roll), 10, 410);
	}
    
    // Quick mute for Yaw orientation input from device
    if (m.isKeyPressed('j')) {
        m1OrientationClient.command_setTrackingYawEnabled(!m1OrientationClient.getTrackingYawEnabled());
    }

    // Quick mute for Pitch orientation input from device
    if (m.isKeyPressed('k')) {
        m1OrientationClient.command_setTrackingPitchEnabled(!m1OrientationClient.getTrackingPitchEnabled());
    }
    
    // Quick mute for Roll orientation input from device
    if (m.isKeyPressed('l')) {
        m1OrientationClient.command_setTrackingRollEnabled(!m1OrientationClient.getTrackingRollEnabled());
    }
    
	if (m.eventState.isKeyPressed(' ')) {
		if (transportSourceVideo.isPlaying() || transportSourceAudio.isPlaying()) {
			transportSourceVideo.stop();
			transportSourceAudio.stop();
		}
		else {
			transportSourceVideo.start();
			transportSourceAudio.start();
		}
	}

	// draw m1 logo
	m.drawImage(imgLogo, m.getWindowWidth() - imgLogo.getWidth()*0.3 - 10, m.getWindowHeight() - imgLogo.getHeight()*0.3 - 10, imgLogo.getWidth() * 0.3, imgLogo.getHeight() * 0.3);

	std::vector<M1OrientationClientWindowDeviceSlot> slots;

	std::vector<M1OrientationDeviceInfo> devices = m1OrientationClient.getDevices();
	for (int i = 0; i < devices.size(); i++) {
		std::string icon = "";
        if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeSerial && devices[i].getDeviceName().find("Bluetooth-Incoming-Port") != std::string::npos) {
            icon = "bt";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeSerial && devices[i].getDeviceName().find("Mach1-") != std::string::npos) {
            icon = "bt";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeBLE) {
            icon = "bt";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeSerial) {
            icon = "usb";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeCamera) {
            icon = "camera";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeEmulator) {
            icon = "none";
        } else {
            icon = "wifi";
        }

		std::string name = devices[i].getDeviceName();
		slots.push_back({ icon, name, name == m1OrientationClient.getCurrentDevice().getDeviceName(), i, [&](int idx)
			{
				m1OrientationClient.command_startTrackingUsingDevice(devices[idx]);
			}
        });
	}

	auto& orientationControlButton = m.prepare<M1OrientationWindowToggleButton>({ m.getSize().width() - 40 - 5, 5, 40, 40 }).onClick([&](M1OrientationWindowToggleButton& b) {
		showOrientationControlMenu = !showOrientationControlMenu;
		})
		.withInteractiveOrientationGimmick(m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone, m1OrientationClient.getOrientation().getYPRasDegrees().yaw)
			.draw();

    // TODO: move this to be to the left of the orientation client window button
    if (std::holds_alternative<bool>(m1OrientationClient.getCurrentDevice().batteryPercentage)) {
        // it's false, which means the battery percentage is unknown
    } else {
        // it has a battery percentage value
        int battery_value = std::get<int>(m1OrientationClient.getCurrentDevice().batteryPercentage);
        m.getCurrentFont()->drawString("Battery: " + std::to_string(battery_value), m.getWindowWidth() - 100, m.getWindowHeight() - 100);
    }
    
    if (orientationControlButton.hovered && (m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone)) {
        std::string deviceReportString = "CONNECTED DEVICE: " + m1OrientationClient.getCurrentDevice().getDeviceName();
        auto font = m.getCurrentFont();
        auto bbox = font->getStringBoundingBox(deviceReportString, 0, 0);
        //m.setColor(40, 40, 40, 200);
        // TODO: fix this bounding box (doesnt draw the same place despite matching settings with Label.draw
        //m.drawRectangle(     m.getSize().width() - 40 - 10 /* padding */ - bbox.width - 5, 5, bbox.width + 10, 40);
        m.setColor(230, 230, 230);
        m.prepare<M1Label>({ m.getSize().width() - 40 - 10 /* padding */ - bbox.width - 5, 5 + 10, bbox.width + 10, 40 }).text(deviceReportString).withTextAlignment(TEXT_CENTER).draw();
    }
    
    if (showOrientationControlMenu) {
        bool showOrientationSettingsPanelInsideWindow = (m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
        orientationControlWindow = &(m.prepare<M1OrientationClientWindow>({ m.getSize().width() - 218 - 5 , 5, 218, 300 + 100 * showOrientationSettingsPanelInsideWindow })
            .withDeviceList(slots)
            .withSettingsPanelEnabled(showOrientationSettingsPanelInsideWindow)
            .withOscSettingsEnabled((m1OrientationClient.getCurrentDevice().getDeviceType() == M1OrientationManagerDeviceTypeOSC))
            .onClickOutside([&]() {
            if (!orientationControlButton.hovered) { // Only switch showing the orientation control if we didn't click on the button
                showOrientationControlMenu = !showOrientationControlMenu;
                if (showOrientationControlMenu && !showedOrientationControlBefore) {
                    orientationControlWindow->startRefreshing();
                }
            }
                })
            .onDisconnectClicked([&]() {
                m1OrientationClient.command_disconnect();
             })
            .onRecenterClicked([&]() {
                m1OrientationClient.command_recenter();
            })
            .onOscSettingsChanged([&](int requested_osc_port, std::string requested_osc_msg_address) {
                m1OrientationClient.command_setAdditionalDeviceSettings("osc_add="+requested_osc_msg_address);
                m1OrientationClient.command_setAdditionalDeviceSettings("osc_p="+std::to_string(requested_osc_port));
            })
            .onYPRSwitchesClicked([&](int whichone) {
                if (whichone == 0) m1OrientationClient.command_setTrackingYawEnabled(!m1OrientationClient.getTrackingYawEnabled());
                if (whichone == 1) m1OrientationClient.command_setTrackingPitchEnabled(!m1OrientationClient.getTrackingPitchEnabled());
                if (whichone == 2) m1OrientationClient.command_setTrackingRollEnabled(!m1OrientationClient.getTrackingRollEnabled());
            })
            .withYPRTrackingSettings(
                                     m1OrientationClient.getTrackingYawEnabled(),
                                     m1OrientationClient.getTrackingPitchEnabled(),
                                     m1OrientationClient.getTrackingRollEnabled(),
                                     std::pair<int, int>(0, 180),
                                     std::pair<int, int>(0, 180),
                                     std::pair<int, int>(0, 180)
            )
            .withYPR(
                     m1OrientationClient.getOrientation().getYPRasDegrees().yaw,
                     m1OrientationClient.getOrientation().getYPRasDegrees().pitch,
                     m1OrientationClient.getOrientation().getYPRasDegrees().roll
            ));
            orientationControlWindow->draw();
    }
    
    // update the previous orientation for calculating offset
    previousOrientation = currentOrientation;
    
    // update the mousewheel scroll for testing
    lastScrollValue = m.mouseScroll();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // You can add your component specific drawing code here!
    // This will draw over the top of the openGL background.
}

void MainComponent::resized()
{
    // This is called when the MainComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
