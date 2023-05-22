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
	shutdownAudio();
}

//==============================================================================
void MainComponent::initialise() 
{
	murka::JuceMurkaBaseComponent::initialise();

	imgLogo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);

	videoEngine.getFormatManager().registerFormat(std::make_unique<foleys::FFmpegFormat>());

	m1OrientationOSCClient.init(6345);
	m1OrientationOSCClient.setStatusCallback(std::bind(&MainComponent::setStatus, this, std::placeholders::_1, std::placeholders::_2));
} 

void MainComponent::setStatus(bool success, std::string message)
{
	//this->status = message;
	std::cout << success << " , " << message << std::endl;
}


void MainComponent::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, ablock size, etc) are changed.
	sampleRate = newSampleRate;
	blockSize = samplesPerBlockExpected;

	if (clip.get() != nullptr)
		clip->prepareToPlay(blockSize, sampleRate);

	transportSource.prepareToPlay(blockSize, sampleRate);

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
    if (clip){
        detectedNumInputChannels = clip->getNumChannels();
        
        readBuffer.setSize(detectedNumInputChannels, bufferToFill.numSamples);
        readBuffer.clear();
        
        juce::AudioBuffer<float> tempBuffer;

        tempBuffer.setSize(detectedNumInputChannels * 2, bufferToFill.numSamples);
        tempBuffer.clear();
        
        juce::AudioSourceChannelInfo info(&readBuffer,
            bufferToFill.startSample,
            bufferToFill.numSamples);
        
        // the AudioTransportSource takes care of start, stop and resample
        transportSource.getNextAudioBlock(info);

        /// Detect input audio channels
        if (detectedNumInputChannels > 0) {
            /// Mono or Stereo
            // TODO: mute or block audio playback by default
            // TODO: add button for playing stereo/mono audio in videoplayer?
            
            // clear channels
//            for (auto channel = 0; channel < detectedNumInputChannels; ++channel)
//                info.buffer->clear(channel, 0, bufferToFill.numSamples);
            
            /// Multichannel

            // if you've got more output channels than input clears extra outputs
            for (auto channel = detectedNumInputChannels; channel < 2; ++channel)
                readBuffer.clear(channel, 0, bufferToFill.numSamples);
            
            // Mach1Decode processing loop
    //        (parameters.getParameter(paramYawEnable)->getValue()) ? currentOrientation.x = parameters.getParameter(paramYaw)->getValue() : currentOrientation.x = 0.0f;
    //        (parameters.getParameter(paramPitchEnable)->getValue()) ? currentOrientation.y = parameters.getParameter(paramPitch)->getValue() : currentOrientation.y = 0.0f;
    //        (parameters.getParameter(paramRollEnable)->getValue()) ? currentOrientation.z = parameters.getParameter(paramRoll)->getValue() : currentOrientation.z = 0.0f;
            m1Decode.setRotation(currentOrientation);
            m1Decode.beginBuffer();
            spatialMixerCoeffs = m1Decode.decodeCoeffs();
            m1Decode.endBuffer();
            
            // Update spatial mixer coeffs from Mach1Decode for a smoothed value
            for (int channel = 0; channel < detectedNumInputChannels; ++channel) {
                smoothedChannelCoeffs[channel * 2    ].setTargetValue(spatialMixerCoeffs[channel * 2    ]);
                smoothedChannelCoeffs[channel * 2 + 1].setTargetValue(spatialMixerCoeffs[channel * 2 + 1]);
            }
            
            float* outBufferL = bufferToFill.buffer->getWritePointer(0);
            float* outBufferR = bufferToFill.buffer->getWritePointer(1);
            std::vector<float> spatialCoeffsBufferL, spatialCoeffsBufferR;

            if (detectedNumInputChannels == m1Decode.getFormatChannelCount()){ // dumb safety check, TODO: do better i/o error handling
                                
                // copy from readBuffer for doubled channels
                for (auto channel = 0; channel < detectedNumInputChannels; ++channel){
                    tempBuffer.copyFrom(channel * 2    , 0, readBuffer, channel, 0, bufferToFill.numSamples);
                    tempBuffer.copyFrom(channel * 2 + 1, 0, readBuffer, channel, 0, bufferToFill.numSamples);
                }
                
//                for (int sample = 0; sample < info.numSamples; sample++) {
//                    for (int channel = 0; channel < info.buffer->getNumChannels(); channel++) {
//                        info.buffer->getWritePointer(channel)[sample] = 0;
//                    }
//                }
                
                for (int sample = 0; sample < info.numSamples; sample++) {
                    for (int channel = 0; channel < detectedNumInputChannels; channel++) {
                        outBufferL[sample] += tempBuffer.getReadPointer(channel * 2    )[sample] * smoothedChannelCoeffs[channel * 2    ].getNextValue();
                        outBufferR[sample] += tempBuffer.getReadPointer(channel * 2 + 1)[sample] * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
                    }
                }
            } else {
                // Invalid Decode I/O; clear buffers
                for (int channel = detectedNumInputChannels; channel < 2; ++channel)
                    bufferToFill.buffer->clear(channel, 0, bufferToFill.numSamples);
            }
            
            // clear remaining input channels
            for (auto channel = 2; channel < detectedNumInputChannels; ++channel)
                readBuffer.clear(channel, 0, bufferToFill.numSamples);
            
        } else {
            bufferToFill.clearActiveBufferRegion();
        }
    } else {
        detectedNumInputChannels = 0;
    }
}

void MainComponent::releaseResources()
{
	transportSource.releaseResources();
	if (clip.get() != nullptr)
		clip->releaseResources();
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
	openFile(files[0]);

	if (clip.get() != nullptr) {
		clip->setNextReadPosition(0);
		transportSource.start();
	}
	juce::Process::makeForegroundProcess();
}

void MainComponent::openFile(juce::File name)
{
	if (clip.get() != nullptr)
		clip->removeTimecodeListener(this);

	auto newClip = videoEngine.createClipFromFile(juce::URL(name));

	if (newClip.get() == nullptr)
		return;

    newClip->prepareToPlay(blockSize, sampleRate);
    clip = newClip;
    
    /// Video Setup
    if (newClip->hasVideo()) {
//        newClip->get
//        newClip->getFrameDurationInSeconds()
    }
    
    /// Audio Setup
    if (newClip->hasAudio()) {
        detectedNumInputChannels = clip.get()->getNumChannels();
        //tempBuffer.setSize(detectedNumInputChannels, blockSize);
        
        // Setup for Mach1Decode API
        m1Decode.setPlatformType(Mach1PlatformDefault);
        m1Decode.setFilterSpeed(0.99);
        
        if (detectedNumInputChannels == 4){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoHorizon_4);
        } else if (detectedNumInputChannels == 8){
            bool useIsotropic = true; // TODO: implement this switch
            if (useIsotropic) {
                m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_8);
            } else {
            }
        } else if (detectedNumInputChannels == 12){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_12);
        } else if (detectedNumInputChannels == 14){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_14);
        } else if (detectedNumInputChannels == 32){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_32);
        } else if (detectedNumInputChannels == 36){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_36);
        } else if (detectedNumInputChannels == 48){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_48);
        } else if (detectedNumInputChannels == 60){
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_60);
        }
    }
	
    clip->addTimecodeListener(this);
	transportSource.setSource(clip.get(), 0, nullptr);
}

void MainComponent::shutdown()
{ 
	clip = nullptr;

	murka::JuceMurkaBaseComponent::shutdown();
}

void MainComponent::render()
{
	// update video frame
	if (clip.get() != nullptr) {
		foleys::VideoFrame& frame = clip->getFrame(clip->getCurrentTimeInSeconds());
		if (frame.image.getWidth() > 0 && frame.image.getHeight() > 0) {
			if (imgVideo.getWidth() != frame.image.getWidth() || imgVideo.getHeight() != frame.image.getHeight()) {
				imgVideo.allocate(frame.image.getWidth(), frame.image.getHeight());
			}
			juce::Image::BitmapData srcData(frame.image, juce::Image::BitmapData::readOnly);
			imgVideo.loadData(srcData.data, GL_BGRA);
		}
	}

	m.startFrame();
	m.setScreenScale((float)openGLContext.getRenderingScale());

	m.clear(20);
	m.setColor(255);
    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, 10);

	m.begin();

	if (clip.get() != nullptr) {
        auto& videoPlayerWidget = m.prepare<VideoPlayerWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });

        currentOrientation.x = videoPlayerWidget.rotation.x;
        currentOrientation.y = videoPlayerWidget.rotation.y;
        currentOrientation.z = videoPlayerWidget.rotation.z;
        currentPlayerWidgetFov = videoPlayerWidget.fov;

		videoPlayerWidget.imgVideo = &imgVideo;

		float playheadPosition = transportSource.getCurrentPosition() / transportSource.getLengthInSeconds();

		videoPlayerWidget.playheadPosition = playheadPosition;
		videoPlayerWidget.draw();

		if (videoPlayerWidget.playheadPosition != playheadPosition) {
			transportSource.setPosition(videoPlayerWidget.playheadPosition * transportSource.getLengthInSeconds());
		}
        
        if (m.isKeyPressed('z')) {
            videoPlayerWidget.drawFlat = !videoPlayerWidget.drawFlat;
        }

        if (m.isKeyPressed('w')) {
            videoPlayerWidget.fov += 10;
        }

        if (m.isKeyPressed('s')) {
            videoPlayerWidget.fov -= 10;
        }
	}

	if (clip.get() == nullptr) {
		std::string message = "Drop a video here [Press Q for Hotkeys & Info]";
		float width = m.getCurrentFont()->getStringBoundingBox(message, 0, 0).width;
		m.prepare<murka::Label>({ m.getWindowWidth() * 0.5 - width * 0.5, m.getWindowHeight() * 0.5, 350, 30 }).text(message).draw();
	}

	if (m.isKeyHeld('q')) {
		m.getCurrentFont()->drawString("Fov : " + std::to_string(currentPlayerWidgetFov), 10, 10);
		m.getCurrentFont()->drawString("Playing: " + std::string(clip.get() != nullptr ? "yes" : "no"), 10, 90);
		m.getCurrentFont()->drawString("Frame: " + std::to_string(transportSource.getCurrentPosition()), 10, 110);

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


	if (m.isKeyPressed(' ')) {
		if (transportSource.isPlaying()) {
			transportSource.stop();
		} else {
			transportSource.start();
		}
	}

    // draw m1 logo
	//m.drawImage(imgVideo, 0, 0, imgVideo.getWidth(), imgVideo.getHeight());
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
    auto& orientationControlButton = m.prepare<M1OrientationWindowToggleButton>({m.getSize().width() - 40 - 5, 5, 40, 40}).onClick([&](M1OrientationWindowToggleButton& b){
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
        m.prepare<M1Label>({678 + 40 - bbox.width - 5, 48, bbox.width + 10, 30}).text(deviceReportString).draw();
    }

    if (showOrientationControlMenu) {
        bool showOrientationSettingsPanelInsideWindow = (m1OrientationOSCClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
        orientationControlWindow = m.prepare<M1OrientationClientWindow>({500, 45, 218, 300 + 100 * showOrientationSettingsPanelInsideWindow}).withDeviceList(slots)
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
			.onRefreshClicked([&](){
				m1OrientationOSCClient.command_refreshDevices();
            })
            .onYPRSwitchesClicked([&](int whichone){
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
    
	m.end();
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
