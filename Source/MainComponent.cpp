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

    // setup the listener
    playerOSC.AddListener([&](juce::OSCMessage msg) {
        // got an relayed settings update of a panner plugin
        if (msg.getAddressPattern() == "/panner-settings") {
            
            // if incoming panner port does not currently exist add it
            int plugin_port;
            if (msg[0].isInt32()) {
                plugin_port = msg[0].getInt32();
            }
            
            int state;
            if (msg.size() >= 2 && msg[1].isInt32()) {
                state = msg[1].getInt32();
            }

            int input_mode;
            float azi; float ele; float div; float gain;
            float st_azi, st_spr;
            int panner_mode;
            bool auto_orbit;
            bool found = false;
            if (msg.size() >= 10) {
                if (msg[4].isInt32()) {
                    input_mode = msg[4].getInt32();
                }
                if (msg[5].isFloat32()) {
                    azi = msg[5].getFloat32();
                }
                if (msg[6].isFloat32()) {
                    ele = msg[6].getFloat32();
                }
                if (msg[7].isFloat32()) {
                    div = msg[7].getFloat32();
                }
                if (msg[8].isFloat32()) {
                    gain = msg[8].getFloat32();
                }
                if (msg[9].isInt32()) {
                    panner_mode = msg[9].getInt32();
                }
            }
            if (msg.size() >= 13) {
                if (msg[10].isInt32()) {
                    auto_orbit = msg[10].getInt32();
                }
                if (msg[11].isFloat32()) {
                    st_azi = msg[11].getFloat32();
                }
                if (msg[12].isFloat32()) {
                    st_spr = msg[12].getFloat32();
                }
            }
            
            if (msg.size() >= 1) {
                state = msg[1].getInt32();

                // update to panner
                for (auto& panner : panners) {
                    if (panner.port == plugin_port) {
                        found = true;
        
                        // update existing found panner obj
                        if (state == -1) { // remove panner if panner was deleted
                            auto iter = std::find_if(panners.begin(), panners.end(), find_plugin(plugin_port));
                            if (iter != panners.end()) {
                                // if panner found, delete it
                                panners.erase(iter);
                                DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", disconnected!");
                            }
                        }
                        else {
                            // update state of panner
                            panner.state = state;
                            
                            if (msg.size() >= 10) {
                                // update found panner object
                                // check for a display name or otherwise use the port
                                (msg[2].isString() && msg[2].getString() != "") ? panner.displayName = msg[2].getString().toStdString() : panner.displayName = std::to_string(plugin_port);
                                // check for a color
                                if (msg[3].isColour() && msg[3].getColour().alpha != 0) {
                                    panner.color.r = msg[3].getColour().red;
                                    panner.color.g = msg[3].getColour().green;
                                    panner.color.b = msg[3].getColour().blue;
                                    panner.color.a = msg[3].getColour().alpha;
                                }
                                
                                // randomize the color if one isnt assigned yet
                                if (panner.color.a == 0) {
                                    // randomize a color
                                    panner.color.r = juce::Random().nextInt(255);
                                    panner.color.g = juce::Random().nextInt(255);
                                    panner.color.b = juce::Random().nextInt(255);
                                    panner.color.a = 255;
                                }
                                
                                panner.m1Encode.setInputMode((Mach1EncodeInputModeType)input_mode);
                                panner.m1Encode.setAzimuthDegrees(azi);
                                panner.m1Encode.setElevationDegrees(ele);
                                panner.m1Encode.setDiverge(div);
                                panner.m1Encode.setOutputGain(gain, true);
                                panner.m1Encode.setPannerMode((Mach1EncodePannerModeType)panner_mode);
                                panner.azimuth = azi; // TODO: remove these?
                                panner.elevation = ele; // TODO: remove these?
                                panner.diverge = div; // TODO: remove these?
                                panner.gain = gain; // TODO: remove these?

                                if (input_mode == 1 && msg.size() >= 13) {
                                    panner.m1Encode.setAutoOrbit(auto_orbit);
                                    panner.m1Encode.setOrbitRotationDegrees(st_azi);
                                    panner.m1Encode.setStereoSpread(st_spr/100.); // normalize
                                    panner.autoOrbit = auto_orbit; // TODO: remove these?
                                    panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                                    panner.stereoSpread = st_spr/100.; // TODO: remove these?
                                }
                                
                                DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", in="+std::to_string(input_mode)+", az="+std::to_string(azi)+", el="+std::to_string(ele)+", di="+std::to_string(div)+", gain="+std::to_string(gain));
                            }
                        }
                    }
                }
            }

            if (!found) {
                if (msg.size() >= 10) {
                    // update the current settings from the incoming osc messsage
                    PannerSettings panner;
                    panner.port = plugin_port;
                    panner.state = state;
                    
                    // check for a display name or otherwise use the port
                    (msg[2].isString() && msg[2].getString() != "") ? panner.displayName = msg[2].getString().toStdString() : panner.displayName = std::to_string(plugin_port);
                    // check for a color
                    if (msg[3].isColour() && msg[3].getColour().alpha != 0) {
                        panner.color.r = msg[3].getColour().red;
                        panner.color.g = msg[3].getColour().green;
                        panner.color.b = msg[3].getColour().blue;
                        panner.color.a = msg[3].getColour().alpha;
                    }
                    
                    // randomize the color if one isnt assigned yet
                    if (panner.color.a == 0) {
                        // randomize a color
                        panner.color.r = juce::Random().nextInt(255);
                        panner.color.g = juce::Random().nextInt(255);
                        panner.color.b = juce::Random().nextInt(255);
                        panner.color.a = 255;
                    }
                    
                    panner.m1Encode.setInputMode((Mach1EncodeInputModeType)input_mode);
                    panner.m1Encode.setAzimuthDegrees(azi);
                    panner.m1Encode.setElevationDegrees(ele);
                    panner.m1Encode.setDiverge(div);
                    panner.m1Encode.setOutputGain(gain, true);
                    panner.m1Encode.setPannerMode((Mach1EncodePannerModeType)panner_mode);
                    panner.azimuth = azi; // TODO: remove these?
                    panner.elevation = ele; // TODO: remove these?
                    panner.diverge = div; // TODO: remove these?
                    panner.gain = gain; // TODO: remove these?
                     
                    if (input_mode == 1 && msg.size() >= 13) {
                        panner.m1Encode.setAutoOrbit(auto_orbit);
                        panner.m1Encode.setOrbitRotationDegrees(st_azi);
                        panner.m1Encode.setStereoSpread(st_spr/100.); // normalize
                        panner.autoOrbit = auto_orbit; // TODO: remove these?
                        panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                        panner.stereoSpread = st_spr/100.; // TODO: remove these?
                    }

                    panners.push_back(panner);
                    DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", in="+std::to_string(input_mode)+", az="+std::to_string(azi)+", el="+std::to_string(ele)+", di="+std::to_string(div)+", gain="+std::to_string(gain));
                }
            }
        } else {
            // display a captured unexpected osc message
            if (msg.size() > 0) {
                DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString());
                if (msg[0].isFloat32()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + std::to_string(msg[0].getFloat32()));
                } else if (msg[0].isInt32()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + std::to_string(msg[0].getInt32()));
                } else if (msg[0].isString()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + msg[0].getString());
                }
            }
        }
    });
    
    // playerOSC update timer loop (only used for checking the connection)
    startTimerHz(1);
    
    // Debug video TODO: REMOVE!
    const juce::String& currentFile = "/Users/zebra/Downloads/testvideo.mp4";
    openFile(currentFile);
}

void MainComponent::prepareToPlay(int samplesPerBlockExpected, double newSampleRate)
{
	// This function will be called when the audio device is started, or when
	// its settings (i.e. sample rate, ablock size, etc) are changed.
	sampleRate = newSampleRate;
	blockSize = samplesPerBlockExpected;
    
    if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
        clip->prepareToPlay(blockSize, sampleRate);
        transportSource.prepareToPlay(blockSize, sampleRate);
    }
        
    // Setup for Mach1Decode
    smoothedChannelCoeffs.resize(m1Decode.getFormatCoeffCount());
    spatialMixerCoeffs.resize(m1Decode.getFormatCoeffCount());
    for (int input_channel = 0; input_channel < m1Decode.getFormatChannelCount(); input_channel++) {
        smoothedChannelCoeffs[input_channel * 2].reset(sampleRate, (double)0.01);
        smoothedChannelCoeffs[input_channel * 2 + 1].reset(sampleRate, (double)0.01);
    }
    
    readBuffer.setSize(m1Decode.getFormatCoeffCount(), blockSize);
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (clip){
        // the TransportSource takes care of start, stop and resample
        juce::AudioSourceChannelInfo info (&readBuffer,
                                           bufferToFill.startSample,
                                           bufferToFill.numSamples);
      
		// first read video 
		if (clip->hasVideo()) {
			readBuffer.setSize(clip->getNumChannels(), bufferToFill.numSamples);
			readBuffer.clear();

			juce::AudioSourceChannelInfo info(&readBuffer,
				bufferToFill.startSample,
				bufferToFill.numSamples);

			transportSource.getNextAudioBlock(info);
        } else {
            // then read audio source
            detectedNumInputChannels = clip->getNumChannels();

            readBuffer.setSize(detectedNumInputChannels, bufferToFill.numSamples);
            readBuffer.clear();

            juce::AudioSourceChannelInfo info(&readBuffer,
                bufferToFill.startSample,
                bufferToFill.numSamples);

            transportSource.getNextAudioBlock(info);

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
                    readBuffer.clear(channel, 0, bufferToFill.numSamples);
                
                // Mach1Decode processing loop
                auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
                m1Decode.setRotationDegrees({ ori_deg.GetYaw(), ori_deg.GetPitch(), ori_deg.GetRoll() });

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
                                    
                    // copy from readBuffer for doubled channels
                    for (auto channel = 0; channel < detectedNumInputChannels; ++channel){
                        tempBuffer.copyFrom(channel * 2 + 0, 0, readBuffer, channel, 0, bufferToFill.numSamples);
                        tempBuffer.copyFrom(channel * 2 + 1, 0, readBuffer, channel, 0, bufferToFill.numSamples);
                    }
                
                    for (int sample = 0; sample < info.numSamples; sample++) {
                        for (int channel = 0; channel < detectedNumInputChannels; channel++) {
                            outBufferL[sample] += tempBuffer.getReadPointer(channel * 2 + 0)[sample] * smoothedChannelCoeffs[channel * 2].getNextValue();
                            outBufferR[sample] += tempBuffer.getReadPointer(channel * 2 + 1)[sample] * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
                        }
                    }
                } else if (detectedNumInputChannels == 1) {
                    // mono
                    bufferToFill.buffer->copyFrom(0, 0, readBuffer, 0, 0, info.numSamples);
                    bufferToFill.buffer->copyFrom(1, 0, readBuffer, 0, 0, info.numSamples);
                }
                else if (detectedNumInputChannels == 2) {
                    // stereo
                    bufferToFill.buffer->copyFrom(0, 0, readBuffer, 0, 0, info.numSamples);
                    bufferToFill.buffer->copyFrom(1, 0, readBuffer, 1, 0, info.numSamples);
                }
                else {
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
        }
        bufferToFill.clearActiveBufferRegion();
    } else {
        detectedNumInputChannels = 0;
    }
}

void MainComponent::releaseResources()
{
	if (clip.get() != nullptr) {
        clip->releaseResources();
	}
    transportSource.releaseResources();
}

void MainComponent::timecodeChanged(int64_t, double seconds)
{
    // Use this to update the seekbar location
}

//==============================================================================

bool MainComponent::isInterestedInFileDrag(const juce::StringArray&)
{
	return true;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
    // TODO: test new dropped files first before clearing
    transportSource.stop();
    transportSource.setSource(nullptr);
    
	for (int i = 0; i < files.size(); ++i) {
		const juce::String& currentFile = files[i];
		
		openFile(currentFile);

		if (clip.get() != nullptr) {
			clip->setNextReadPosition(0);
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
            // clear the old clip
            if (clip.get() != nullptr) {
                clip->removeTimecodeListener(this);
            }
            
			newClip->prepareToPlay(blockSize, sampleRate);
            clip = newClip;
            clip->setLooping(false); // TODO: change this for standalone mode with exposed setting
			clip->addTimecodeListener(this);
            transportSource.setSource(clip.get(), 0, nullptr);
		}
	}

    // Audio Setup
    // TODO: make clearer logic to when we should get the audioClip assigned to the transport
    if (b_standalone_mode)
	{
		std::shared_ptr<foleys::AVClip> newClip = videoEngine.createClipFromFile(juce::URL(filepath));

		if (newClip.get() == nullptr)
			return;

		if (newClip->hasAudio()) {
            // clear the old clip
            if (clip.get() != nullptr) {
                clip->removeTimecodeListener(this);
            }
            
			newClip->prepareToPlay(blockSize, sampleRate);
            clip = newClip;
            clip->addTimecodeListener(this);
            transportSource.setSource(clip.get(), 0, nullptr);
            
			detectedNumInputChannels = clip->getNumChannels();

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
    clip = nullptr;
	murka::JuceMurkaBaseComponent::shutdown();
}

void MainComponent::syncWithDAWPlayhead() {
    if (std::fabs(m1OrientationClient.getPlayerLastUpdate() - lastUpdateForPlayer) < 0.001f) {
        // Sync already established
        return;
    }
    
    if (clip.get() == nullptr) {
        // no video loaded yet so no reason to sync in this mode
        return;
    }

    lastUpdateForPlayer = m1OrientationClient.getPlayerLastUpdate();

    auto length = transportSource.getLengthInSeconds();
    auto player_ph_pos = clip->getNextReadPosition() / clip->getSampleRate();
    float daw_ph_pos = m1OrientationClient.getPlayerPositionInSeconds(); // this incorporates the offset (daw_ph - offset)
    bool end_reached = daw_ph_pos >= length;

    // Prevent playback from continuing if we reached the end of the loaded video clip's length
    if (end_reached && !(transportSource.isPlaying())) {
        DBG("[Playhead] Reached end of video length.");
        return;
    }
     
    // seek sync
    auto playback_delta = static_cast<float>(daw_ph_pos - player_ph_pos);
    //DBG("Playhead Pos: "+std::to_string(playback_delta));
    if (std::fabs(playback_delta) > 0.05 && !end_reached) {
        if (clip != nullptr) {
            clip->setNextReadPosition(static_cast<juce::int64>(daw_ph_pos * clip->getSampleRate()));
        }
    }

    // play / stop sync
    bool video_not_synced = (clip != nullptr && m1OrientationClient.getPlayerIsPlaying() != transportSource.isPlaying());
    if (video_not_synced) {
        if (m1OrientationClient.getPlayerIsPlaying()) {
            transportSource.start();
        }
        else {
            transportSource.stop();
        }
    }
}

void MainComponent::draw_orientation_client(murka::Murka &m, M1OrientationClient &m1OrientationClient) {
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
        slots.push_back({ icon,
            name,
            name == m1OrientationClient.getCurrentDevice().getDeviceName(),
            i,
            [&](int idx)
            {
                m1OrientationClient.command_startTrackingUsingDevice(devices[idx]);
            }
        });
    }
    
    // trigger a server side refresh for listed devices while menu is open
    m1OrientationClient.command_refresh();
    //bool showOrientationSettingsPanelInsideWindow = (m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
    orientationControlWindow = &(m.prepare<M1OrientationClientWindow>({ 400 , 378, 290, 400}));
    orientationControlWindow->withDeviceSlots(slots);
    orientationControlWindow->withOrientationClient(m1OrientationClient);
    orientationControlWindow->draw();
}

void MainComponent::draw() {
    if ((m.mouseDelta().x != 0) || (m.mouseDelta().y != 0)) {
        secondsWithoutMouseMove = 0;
    }
    
    // update standalone mode flag
    // TODO: introduce a button to swap modes
    if (playerOSC.getNumberOfMonitors() > 0) {
        b_standalone_mode = false;
    } else {
        b_standalone_mode = true;
    }
    
    if (b_standalone_mode) {
        // TODO: Allow playhead control
        // TODO: Audio/Video Sync
    } else {
        // check for monitor discovery to get DAW playhead pos
        // sync with DAW
        syncWithDAWPlayhead();
        // TODO: Disable interaction on play/stop controls in UI
    }

	// update video frame
	if (clip.get() != nullptr && clip->hasVideo()) {
		foleys::VideoFrame& frame = clip->getFrame(clip->getCurrentTimeInSeconds());
        DBG("[Video] Time: "+std::to_string(clip->getCurrentTimeInSeconds())+", Block:"+std::to_string(clip->getNextReadPosition()));
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
	m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);

	auto& videoPlayerWidget = m.prepare<VideoPlayerWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });

    auto vid_rot = Mach1::Float3{ videoPlayerWidget.rotationCurrent.y, videoPlayerWidget.rotationCurrent.x, videoPlayerWidget.rotationCurrent.z }.EulerRadians();
    currentOrientation.SetRotation(vid_rot);
    
    if (playerOSC.IsConnected() && playerOSC.IsActivePlayer()) {
        // send the current orientation of player instead 
        // send mouse offset of player orientation and send to helper
        if (videoPlayerWidget.isUpdatedRotation) {
            auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
            playerOSC.sendPlayerYPR(ori_deg.GetYaw(), ori_deg.GetPitch(), ori_deg.GetRoll());
        }
        prev_mouse_offset = videoPlayerWidget.rotationOffsetMouse;
    }
    
    if (m1OrientationClient.isConnectedToServer()) {
        // add server orientation to player via a calculated offset
        Mach1::Orientation oc_orientation = m1OrientationClient.getOrientation();

        Mach1::Quaternion ori_quat = oc_orientation.GetGlobalRotationAsQuaternion();
        Mach1::Float3 ori_vec_deg = oc_orientation.GetGlobalRotationAsEulerDegrees();

        Mach1::Quaternion last_quat = previousClientOrientation.GetGlobalRotationAsQuaternion();

        if (!ori_quat.IsApproximatelyEqual(last_quat)) {
            Mach1::Float3 last_vec_deg = previousClientOrientation.GetGlobalRotationAsEulerDegrees();

            // update the player orientation
            DBG("OM-Client:        Y=" + std::to_string(ori_vec_deg.GetYaw()) + ", P=" + std::to_string(ori_vec_deg.GetPitch()) + ", R=" + std::to_string(ori_vec_deg.GetRoll()));
            videoPlayerWidget.rotationOffset.x += m1OrientationClient.getTrackingYawEnabled() ? ori_vec_deg.GetYaw() - last_vec_deg.GetYaw() : 0.0f;
            videoPlayerWidget.rotationOffset.y += m1OrientationClient.getTrackingPitchEnabled() ? ori_vec_deg.GetPitch() - last_vec_deg.GetPitch() : 0.0f;
            videoPlayerWidget.rotationOffset.z += m1OrientationClient.getTrackingRollEnabled() ? ori_vec_deg.GetRoll() - last_vec_deg.GetRoll() : 0.0f;
            DBG("OM-Client Offset: Y=" + std::to_string(ori_vec_deg.GetYaw() - last_vec_deg.GetYaw()) + ", P=" + std::to_string(ori_vec_deg.GetPitch() - last_vec_deg.GetPitch()) + ", R=" + std::to_string(ori_vec_deg.GetRoll() - last_vec_deg.GetRoll()));
            
            // store last input value
            previousClientOrientation = oc_orientation;
        }
    }
    
	if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {

		if (clip->hasVideo()) {
			videoPlayerWidget.imgVideo = &imgVideo;
		}

		float length = transportSource.getLengthInSeconds();
		float playheadPosition = transportSource.getCurrentPosition() / length;
		videoPlayerWidget.playheadPosition = playheadPosition;
	}
	
	// draw overlay if video empty
	if (clip.get() == nullptr) {
		videoPlayerWidget.drawOverlay = true;
	}

	// draw panners
	videoPlayerWidget.pannerSettings = panners;
	videoPlayerWidget.draw();
	
	// draw reference
    if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {

		if (drawReference) {
			m.drawImage(imgVideo, 0, 0, imgVideo.getWidth() * 0.3, imgVideo.getHeight() * 0.3);
		}

        if (!(secondsWithoutMouseMove > 5)) { // skip drawing if mouse has not interacted in a while
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
            cropStereoscopicCheckbox.dataToControl = &(videoPlayerWidget.crop_Stereoscopic_TopBottom);
            cropStereoscopicCheckbox.label = "CROP STEREOSCOPIC";
            if (!bHideUI) {
                cropStereoscopicCheckbox.draw();
            }
        }
	}
	else { // Either no clip at all (nullptr) or no audio or video in a clip
	}
    
    MurkaShape playerControlShape = {m.getWindowWidth() / 2 - 150,
        m.getWindowHeight() / 2 - 50,
        300,
        100};

    auto& playerControls = m.prepare<M1PlayerControls>(playerControlShape);
    if (b_standalone_mode) { // Standalone mode
        playerControls.withPlayerData("00:00", "22:22",
                        true, // showPositionReticle
                        0.5, // currentPosition
                        transportSource.isPlaying(), // playing
                        []() {}, // playButtonPress
                        [&](double newPositionNormalised) {
            // refreshing player position
            transportSource.setPosition(newPositionNormalised);
        });
        playerControls.withVolumeData(0.5,
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = newVolume;
            transportSource.setGain(mediaVolume);
        });
    } else { // Slave mode
        playerControls.withPlayerData("", "",
                        false, // showPositionReticle
                        0.5, // currentPosition
                        transportSource.isPlaying(), // playing
                        [&]() { // playButtonPress
                            if (transportSource.isPlaying()) {
                                transportSource.stop();
                            }
                            else {
                                transportSource.start();
                            }
                        },
                        [&](double newPositionNormalised) {
            // refreshing player position
            transportSource.setPosition(newPositionNormalised);
        });
        playerControls.withVolumeData(0.5,
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = newVolume;
            transportSource.setGain(mediaVolume);
        });
    }
    playerControls.withStandaloneMode(b_standalone_mode);
    playerControls.bypassingBecauseofInactivity = (secondsWithoutMouseMove > 5);
    m.setColor(20, 20, 20, 200 * (1 - (secondsWithoutMouseMove > 5))); // if there has not been mouse activity hide the UI element
    m.drawRectangle(playerControlShape);
    
    if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
        playerControls.draw();
    } else {
        std::string message = "Drop an audio or video file here";
        juceFontStash::Rectangle boundingBox = m.getCurrentFont()->getStringBoundingBox(message, 0, 0);
        m.setColor(ENABLED_PARAM);
        m.prepare<murka::Label>({ m.getWindowWidth() * 0.5 - boundingBox.width * 0.5, m.getWindowHeight() * 0.5 - boundingBox.height, 350, 30 }).text(message).draw();
    }

	// arrow orientation reset keys
	if (m.isKeyPressed(MurkaKey::MURKA_KEY_UP)) { // up arrow
        videoPlayerWidget.rotationOffsetMouse = { 0, 0, 0 };
        videoPlayerWidget.rotation = { 0, 0, 0 };
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_DOWN)) { // down arrow
        videoPlayerWidget.rotationOffsetMouse = { 0, 0, 0 };
        videoPlayerWidget.rotation = { 180., 0, 0 };
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_RIGHT)) { // right arrow
        videoPlayerWidget.rotationOffsetMouse = { 0, 0, 0 };
        videoPlayerWidget.rotation = { 90., 0, 0 };
	}

	if (m.isKeyPressed(MurkaKey::MURKA_KEY_LEFT)) { // left arrow
        videoPlayerWidget.rotationOffsetMouse = { 0, 0, 0 };
        videoPlayerWidget.rotation = { 270., 0, 0 };
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

    if (m.isKeyPressed('z')) {
        videoPlayerWidget.drawFlat = !videoPlayerWidget.drawFlat;
    }
    
	if (m.isKeyPressed('o')) {
		videoPlayerWidget.drawOverlay = !videoPlayerWidget.drawOverlay;
	}

	if (m.isKeyPressed('d')) {
		videoPlayerWidget.crop_Stereoscopic_TopBottom = !videoPlayerWidget.crop_Stereoscopic_TopBottom;
	}

	// toggles hiding the UI for better video review
	if (m.isKeyPressed('h')) {
		bHideUI = !bHideUI;
	}

	if (m.isKeyPressed('q')) {
        bShowHelpUI = !bShowHelpUI;
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
    
    if (b_standalone_mode) { // block interaction unless in standalone mode
        if (m.isKeyPressed(MURKA_KEY_SPACE)) { // Space bar is 32 on OSX
            if (transportSource.isPlaying()) {
                transportSource.stop();
            }
            else {
                transportSource.start();
            }
        }
        if (m.isKeyPressed(MurkaKey::MURKA_KEY_RETURN)) {
            if (transportSource.isPlaying()) {
                transportSource.stop();
            }
            else {
                transportSource.setPosition(0);
            }
        }
    }

	// draw m1 logo
	m.drawImage(imgLogo, m.getWindowWidth() - imgLogo.getWidth()*0.3 - 10, m.getWindowHeight() - imgLogo.getHeight()*0.3 - 10, imgLogo.getWidth() * 0.3, imgLogo.getHeight() * 0.3);

    
    // Settings pane is open
    if (showSettingsMenu) {

        /// LEFT SIDE
        
        /// RIGHT SIDE
                    
        // orientation client window
        draw_orientation_client(m, m1OrientationClient);
    }
        
    if (bShowHelpUI) {
        m.getCurrentFont()->drawString("Fov : " + std::to_string(currentPlayerWidgetFov), 10, 10);
        m.getCurrentFont()->drawString("Frame: " + std::to_string(transportSource.getCurrentPosition()), 10, 30);
        m.getCurrentFont()->drawString("Standalone mode: " + std::to_string(b_standalone_mode), 10, 50);

        m.getCurrentFont()->drawString("Hotkeys:", 10, 130);
        m.getCurrentFont()->drawString("[w] - FOV+", 10, 150);
        m.getCurrentFont()->drawString("[s] - FOV-", 10, 170);
        m.getCurrentFont()->drawString("[z] - Equirectangular / 2D", 10, 190);
        m.getCurrentFont()->drawString("[g] - Overlay 2D Reference", 10, 210);
        m.getCurrentFont()->drawString("[o] - Overlay Reference", 10, 230);
        m.getCurrentFont()->drawString("[d] - Crop stereoscopic", 10, 250);
        m.getCurrentFont()->drawString("[h] - Hide UI", 10, 270);
        m.getCurrentFont()->drawString("[Arrow Keys] - Orientation Resets", 10, 290);

        auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
        m.getCurrentFont()->drawString("OverlayCoords:", 10, 350);
        m.getCurrentFont()->drawString("Y: " + std::to_string(ori_deg.GetYaw()), 10, 370);
        m.getCurrentFont()->drawString("P: " + std::to_string(ori_deg.GetPitch()), 10, 390);
        m.getCurrentFont()->drawString("R: " + std::to_string(ori_deg.GetRoll()), 10, 410);
    }
    
    // update the previous orientation for calculating offset
    videoPlayerWidget.rotationPrevious = videoPlayerWidget.rotationCurrent;
    
    // update the mousewheel scroll for testing
    lastScrollValue = m.mouseScroll();
}

//==============================================================================
void MainComponent::timerCallback() {
    // Added if we need to move the OSC stuff from the processorblock
    playerOSC.update(); // test for connection
    secondsWithoutMouseMove += 1;
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
