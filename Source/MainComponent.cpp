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
            int plugin_port = 0;
            if (msg[0].isInt32()) {
                plugin_port = msg[0].getInt32();
            }
            
            int state = 0;
            if (msg.size() >= 2 && msg[1].isInt32()) {
                state = msg[1].getInt32();
            }

            int input_mode = 0;
            float azi = 0.0; float ele = 0.0; float div = 0.0; float gain = 0.0;
            float st_azi = 0.0, st_spr = 0.0;
            int panner_mode = 0;
            bool auto_orbit = true;
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
                                    panner.m1Encode.setStereoSpread(st_spr/100.0f); // normalize
                                    panner.autoOrbit = auto_orbit; // TODO: remove these?
                                    panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                                    panner.stereoSpread = st_spr/100.0f; // TODO: remove these?
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
                        panner.m1Encode.setStereoSpread(st_spr/100.0f); // normalize
                        panner.autoOrbit = auto_orbit; // TODO: remove these?
                        panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                        panner.stereoSpread = st_spr/100.0f; // TODO: remove these?
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
    
    // Update timer loop used for:
    // - checking the connection of playerOSC
    // - checking for mouse inactivity
    startTimerHz(1); // once a second
    
    // Telling Murka we're not in a plugin
    // this is to ensure Murka does not expect to pass
    // keystrokes to a hosting layer
    m.isPlugin = false;
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
        }
        
        if (b_standalone_mode && clip->hasAudio()) {
            // then read audio source
            detectedNumInputChannels = clip->getNumChannels();

            readBuffer.setSize(detectedNumInputChannels, bufferToFill.numSamples);
            readBuffer.clear();

            // the AudioTransportSource takes care of start, stop and resample
            transportSource.getNextAudioBlock(info);

            tempBuffer.setSize(detectedNumInputChannels * 2, bufferToFill.numSamples);
            tempBuffer.clear();

            if (detectedNumInputChannels > 0) {
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
                
                // setup output buffers
                float* outBufferL = bufferToFill.buffer->getWritePointer(0);
                float* outBufferR = bufferToFill.buffer->getWritePointer(1);

                if (detectedNumInputChannels == m1Transcode.getInputNumChannels()) { // dumb safety check, TODO: do better i/o error handling
                    
                    for (auto channel = 0; channel < detectedNumInputChannels; ++channel) {
                        // TODO: handle transcode -> decode
                    }
                }
                else if (detectedNumInputChannels == m1Decode.getFormatChannelCount()){ // dumb safety check, TODO: do better i/o error handling
                                    
                    // copy from readBuffer for doubled channels
                    for (auto channel = 0; channel < detectedNumInputChannels; ++channel) {
                        tempBuffer.copyFrom(channel * 2 + 0, 0, readBuffer, channel, 0, bufferToFill.numSamples);
                        tempBuffer.copyFrom(channel * 2 + 1, 0, readBuffer, channel, 0, bufferToFill.numSamples);
                    }
                
                    // apply decode coeffs to output buffer
                    for (int sample = 0; sample < info.numSamples; sample++) {
                        for (int channel = 0; channel < detectedNumInputChannels; channel++) {
                            outBufferL[sample] += tempBuffer.getReadPointer(channel * 2 + 0)[sample] * smoothedChannelCoeffs[channel * 2].getNextValue();
                            outBufferR[sample] += tempBuffer.getReadPointer(channel * 2 + 1)[sample] * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
                        }
                    }
                /// Mono or Stereo
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
                /// Multichannel
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

void MainComponent::showFileChooser()
{
    // TODO: test new dropped files first before clearing
    file_chooser = std::make_unique<juce::FileChooser>("Open", File::getCurrentWorkingDirectory(), "*.mp4'*.m4v;*.mov;*.mkv;*.webm;*.avi;*.wmv;*.ogv;*.aif;*.aiff;*.wav;*.mp3;*.vorbis;*.opus;*.ogg;*.flac;*.pcm;*.alac;*.aac;*.tif;*.tiff;*.png;*.jpg;*.jpeg;*.gif;*.webp;*.svg", true);
    file_chooser->launchAsync(FileBrowserComponent::openMode | FileBrowserComponent::canSelectFiles, [&, this] (const FileChooser& chooser) {
        auto fileUrl = chooser.getURLResult();

        if (fileUrl.isLocalFile()) {
            openFile(fileUrl.getLocalFile());
            
            if (clip.get() != nullptr) {
                clip->setNextReadPosition(0);
            }
        }
        
        juce::Process::makeForegroundProcess();
    });
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray&)
{
	return true;
}

void MainComponent::filesDropped(const juce::StringArray& files, int, int)
{
	for (int i = 0; i < files.size(); ++i) {
		const juce::String& currentFile = files[i];
		openFile(currentFile);
	}
	juce::Process::makeForegroundProcess();
}

void MainComponent::openFile(juce::File filepath)
{
    // TODO: test new dropped files first before clearing
    transportSource.stop();
    transportSource.setSource(nullptr);
    
	// Video Setup
    std::shared_ptr<foleys::AVClip> newClip = videoEngine.createClipFromFile(juce::URL(filepath));

    if (newClip.get() == nullptr)
        return;

    /// Video Setup
    if (newClip.get() != nullptr) {
        
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

    // Audio Setup

    if (newClip->hasAudio()) {

        // at this point clip should be assigned but checking newClip anyone for easier reading
        detectedNumInputChannels = newClip->getNumChannels();

        // Setup for Mach1Decode API
        m1Decode.setPlatformType(Mach1PlatformDefault);
        m1Decode.setFilterSpeed(0.99f);

        // TODO: Detect any Mach1Spatial comment metadata
        // TODO: Mach1Transcode for common otherformats, setup temp UI for input->output format selection

        // Mach1 Spatial Formats
        if (detectedNumInputChannels == 4) {
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoHorizon_4);
        }
        else if (detectedNumInputChannels == 8) {
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_8);
        }
        else if (detectedNumInputChannels == 12) {
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_12);
        }
        else if (detectedNumInputChannels == 14) {
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_14);
        }
        // Test Transcode Inputs
        // TODO: Create UI for selecting input format
        else if (detectedNumInputChannels == 6) {
            // Assume 5.1
            m1Transcode.setInputFormat(m1Transcode.getFormatFromString("5.1_C"));
            m1Transcode.setOutputFormat(m1Transcode.getFormatFromString("M1Spatial-14"));
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_14);
        }
        else if (detectedNumInputChannels == 9) {
            // Assume 2OA ACN
            m1Transcode.setInputFormat(m1Transcode.getFormatFromString("ACNSN3DO2A"));
            m1Transcode.setOutputFormat(m1Transcode.getFormatFromString("M1Spatial-14"));
            m1Decode.setDecodeAlgoType(Mach1DecodeAlgoSpatial_14);
        }
    }
    
    // restart timeline
    if (b_standalone_mode) {
        if (clip.get() != nullptr) {
            clip->setNextReadPosition(0);
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
    
    float rightSide_LeftBound_x = m.getSize().width()/2 + 40;
    float settings_topBound_y = m.getSize().height()*0.23f + 18;

    // trigger a server side refresh for listed devices while menu is open
    m1OrientationClient.command_refresh();
    //bool showOrientationSettingsPanelInsideWindow = (m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
    orientationControlWindow = &(m.prepare<M1OrientationClientWindow>({ rightSide_LeftBound_x, settings_topBound_y, 300, 400}));
    orientationControlWindow->withDeviceSlots(slots);
    orientationControlWindow->withOrientationClient(m1OrientationClient);
    orientationControlWindow->draw();
}

void MainComponent::draw() {
    if ((m.mouseDelta().x != 0) || (m.mouseDelta().y != 0)) {
        secondsWithoutMouseMove = 0;
    }
    
    // update standalone mode flag
    if (playerOSC.getNumberOfMonitors() > 0) {
        if (b_wants_to_switch_to_standalone) {
            b_standalone_mode = true;
        } else {
            b_standalone_mode = false;
        }
    } else {
        b_standalone_mode = true;
    }
    
    if (b_standalone_mode) {
    } else {
        // check for monitor discovery to get DAW playhead pos
        // sync with DAW
        syncWithDAWPlayhead();
    }

	// update video frame
	if (clip.get() != nullptr && clip->hasVideo()) {
        auto clipLengthInSeconds = transportSource.getLengthInSeconds();
		foleys::VideoFrame& frame = clip->getFrame(clip->getCurrentTimeInSeconds());
        //DBG("[Video] Time: " + std::to_string(clip->getCurrentTimeInSeconds()) + ", Block:" + std::to_string(clip->getNextReadPosition()) + ", normalized: " + std::to_string( clip->getCurrentTimeInSeconds() /  clipLengthInSeconds ));
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
		videoPlayerWidget.playheadPosition = (float)playheadPosition;
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
			m.drawImage(imgVideo, 0, 0, imgVideo.getWidth() * 0.3f, imgVideo.getHeight() * 0.3f);
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
        auto clipLengthInSeconds = transportSource.getLengthInSeconds();
        double currentPosition = 0.0;
        if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
            currentPosition = clip->getCurrentTimeInSeconds() / clipLengthInSeconds;
        }
        playerControls.withPlayerData((clip.get() != nullptr && clip->hasVideo()) ? formatTime(clip->getCurrentTimeInSeconds()) : "00:00", formatTime(clipLengthInSeconds),
                        true, // showPositionReticle
                        currentPosition, // currentPosition
                        transportSource.isPlaying(), // playing
                        [&]() {
                            // playButtonPress
                            if (transportSource.isPlaying()) {
                                transportSource.stop();
                            }
                            else {
                                transportSource.start();
                            }
                        },
                        [&]() {
                            // connectButtonPress
                            showSettingsMenu = true;
                        },
                        [&](double newPositionNormalised) {
                            // refreshing player position
                            //clip->setNextReadPosition(static_cast<juce::int64>(newPositionNormalised * clip->getLengthInSeconds() * clip->getSampleRate()));
                            transportSource.setPosition(newPositionNormalised * transportSource.getLengthInSeconds());
                        });
        playerControls.withVolumeData(transportSource.getGain(),
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = (float)newVolume;
            transportSource.setGain(mediaVolume);
        });
    } else { // Slave mode
        playerControls.withPlayerData("", "",
                        false, // showPositionReticle
                        0, // currentPosition
                        transportSource.isPlaying(), // playing
                        [&]() { 
                            // playButtonPress
                            // blocked since control should only be from DAW side
                        },
                        [&]() { 
                            // connectButtonPress
                            showSettingsMenu = true;
                        },
                        [&](double newPositionNormalised) {
            // refreshing player position
            transportSource.setPosition(newPositionNormalised);
        });
        playerControls.withVolumeData(0.5,
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = (float)newVolume;
            transportSource.setGain(mediaVolume);
        });
    }
    playerControls.withStandaloneMode(b_standalone_mode);
    playerControls.bypassingBecauseofInactivity = (secondsWithoutMouseMove > 5);
    m.setColor(20, 20, 20, 200 * (1 - (secondsWithoutMouseMove > 5))); // if there has not been mouse activity hide the UI element
    if (!showSettingsMenu) { // do not draw playcontrols if the settings menu is open
        m.drawRectangle(playerControlShape);
        
        if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
            playerControls.draw();
        } else {
            if (!(secondsWithoutMouseMove > 5)) { // skip drawing if mouse has not interacted in a while
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
                std::string message = "Drop an audio or video file here";
                juceFontStash::Rectangle boundingBox = m.getCurrentFont()->getStringBoundingBox(message, 0, 0);
                m.setColor(ENABLED_PARAM);
                m.prepare<murka::Label>({ m.getWindowWidth() * 0.5 - boundingBox.width * 0.5, m.getWindowHeight() * 0.5 - boundingBox.height, 350, 30 }).text(message).draw();
            }
        }
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
    
    // close settings menu if open
    if (m.isKeyPressed(MurkaKey::MURKA_KEY_ESC)) {
        if (showSettingsMenu) {
            showSettingsMenu = false;
        }
    }
    
    if (showSettingsMenu) {
        // TODO: if we click outside of the settings menu while it is open we should close the menu
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
                if (clip.get() != nullptr) {
                    clip->setNextReadPosition(0);
                }
            }
        }
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

    std::function<void()> deleteTheSettingsButton = [&]() {
        // Temporary solution to delete the TextField:
        // Searching for an id to delete the text field widget.
        // To be redone after the UI library refactoring.
        
        imIdentifier idToDelete;
        for (auto childTuple: m.imChildren) {
            auto childIdTuple = childTuple.first;
            if (std::get<1>(childIdTuple) == typeid(M1DropdownButton).name()) {
                idToDelete = childIdTuple;
            }
        }
        m.imChildren.erase(idToDelete);
    };
    
    /// BOTTOM BAR
    m.setColor(20, 220);
    if (showSettingsMenu) {
        // bottom bar becomes the settings pane
        // TODO: Animate this drawer opening and closing
        m.drawRectangle(0, m.getSize().height()*0.15f, m.getSize().width(), m.getSize().height() * 0.85f);
    } else {
        if (!(secondsWithoutMouseMove > 5)) { // skip drawing if mouse has not interacted in a while
            m.drawRectangle(0, m.getSize().height() - 50, m.getSize().width(), 50); // bottom bar
        }
    }
    
    /// SETTINGS BUTTON
    if (showSettingsMenu || !(secondsWithoutMouseMove > 5)) { // skip drawing if mouse has not interacted in a while
        m.setColor(ENABLED_PARAM);
        float settings_button_y = m.getSize().height() - 10;
        if (showSettingsMenu) {
            auto& showSettingsWhileOpenedButton = m.prepare<M1DropdownButton>({ m.getSize().width()/2 - 30, settings_button_y - 30,
                120, 30 })
            .withLabel("SETTINGS")
            .withFontSize(DEFAULT_FONT_SIZE)
            .withOutline(false);
            showSettingsWhileOpenedButton.textAlignment = TEXT_LEFT;
            showSettingsWhileOpenedButton.heightDivisor = 3;
            showSettingsWhileOpenedButton.labelPaddingLeft = 0;
            showSettingsWhileOpenedButton.draw();
            
            if (showSettingsWhileOpenedButton.pressed) {
                showSettingsMenu = false;
                deleteTheSettingsButton();
            }
            
            // draw settings arrow indicator pointing down
            m.enableFill();
            m.setColor(LABEL_TEXT_COLOR);
            MurkaPoint triangleCenter = {m.getSize().width()/2 + 40, settings_button_y - 10 - 6};
            std::vector<MurkaPoint3D> triangle;
            triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0});
            triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0}); // bottom middle
            triangle.push_back({triangleCenter.x , triangleCenter.y + 5, 0});
            triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0});
            m.drawPath(triangle);
        } else {
            auto& showSettingsWhileClosedButton = m.prepare<M1DropdownButton>({ m.getSize().width()/2 - 30, settings_button_y - 30,
                120, 30 })
            .withLabel("SETTINGS")
            .withFontSize(DEFAULT_FONT_SIZE)
            .withOutline(false);
            showSettingsWhileClosedButton.textAlignment = TEXT_LEFT;
            showSettingsWhileClosedButton.heightDivisor = 3;
            showSettingsWhileClosedButton.labelPaddingLeft = 0;
            showSettingsWhileClosedButton.draw();
            
            if (showSettingsWhileClosedButton.pressed) {
                showSettingsMenu = true;
                deleteTheSettingsButton();
            }
            
            // draw settings arrow indicator pointing up
            m.enableFill();
            m.setColor(LABEL_TEXT_COLOR);
            MurkaPoint triangleCenter = {m.getSize().width()/2 + 40, settings_button_y - 11};
            std::vector<MurkaPoint3D> triangle;
            triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
            triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0}); // top middle
            triangle.push_back({triangleCenter.x , triangleCenter.y - 5, 0});
            triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
            m.drawPath(triangle);
        }
    }
    
    // Settings pane is open
    if (showSettingsMenu) {
        // Settings rendering
        float leftSide_LeftBound_x = 40;
        float rightSide_LeftBound_x = m.getSize().width()/2 + 40;
        float settings_topBound_y = m.getSize().height()*0.23f + 18;
        
        /// LEFT SIDE
        
        m.setColor(ENABLED_PARAM);
        juceFontStash::Rectangle pm_label_box = m.getCurrentFont()->getStringBoundingBox("PLAYER MODE", 0, 0);
        m.prepare<murka::Label>({
            leftSide_LeftBound_x,
            settings_topBound_y,
            pm_label_box.width + 20, pm_label_box.height })
            .text("PLAYER MODE")
            .withAlignment(TEXT_LEFT)
            .draw();
        
        auto& playModeRadioGroup = m.prepare<RadioGroupWidget>({ leftSide_LeftBound_x + 4, settings_topBound_y + 20, m.getSize().width()/2 - 88, 30 });
        playModeRadioGroup.labels = { "SYNC TO DAW", "STANDALONE" };
        playModeRadioGroup.selectedIndex = b_standalone_mode; // swapped since b_standalone_mode = 0 = standalone
        playModeRadioGroup.drawAsCircles = true;
        playModeRadioGroup.draw();
        if (playModeRadioGroup.changed) {
            if (playModeRadioGroup.selectedIndex == 0) {
                if (playerOSC.getNumberOfMonitors() > 0) {
                    b_standalone_mode = false;
                    b_wants_to_switch_to_standalone = false;
                } else {
                    // stay in standalone
                }
            }
            else if (playModeRadioGroup.selectedIndex == 1) {
                // override and allow swap to standalone mode if a media file is loaded
                b_wants_to_switch_to_standalone = true;
            }
            else {
                if (clip.get() != nullptr) {
                    // clip loaded so default to standalone mode
                    b_standalone_mode = true;
                } else {
                    // no clip loaded, look for sync
                    b_standalone_mode = false;
                }
            }
        }
        
        juceFontStash::Rectangle mf_label_box = m.getCurrentFont()->getStringBoundingBox("MEDIA FILE", 0, 0);
        m.prepare<murka::Label>({
            leftSide_LeftBound_x,
            settings_topBound_y + 80,
            mf_label_box.width + 20, mf_label_box.height })
            .text("MEDIA FILE")
            .withAlignment(TEXT_LEFT)
            .draw();
        
        // Media loader
        float load_button_width = 50;
        std::string file_path = "LOAD MEDIA FILE HERE...";
        if (clip.get() != nullptr) {
            file_path = clip->getMediaFile().getSubPath().toStdString();
        }

        juceFontStash::Rectangle load_button_box = m.getCurrentFont()->getStringBoundingBox("LOAD", 0, 0);
        juceFontStash::Rectangle file_path_button_box = m.getCurrentFont()->getStringBoundingBox(file_path, 0, 0);

        auto& media_file_path_label = m.prepare<M1Label>({leftSide_LeftBound_x, settings_topBound_y + 100,
            m.getSize().width()/2 - 45 - load_button_box.width - 60, 20})
            .withText(file_path)
            .withTextAlignment(TEXT_LEFT)
            .withVerticalTextOffset(3)
            .withForegroundColor(MurkaColor(ENABLED_PARAM))
            .withBackgroundFill(MurkaColor(BACKGROUND_COMPONENT), MurkaColor(BACKGROUND_COMPONENT))
            .withOnClickCallback([&](){
                // TODO: allow editing path and reloading new video if exists
            });
        media_file_path_label.labelPadding_x = 10;
        media_file_path_label.draw();

        // load button
        m.prepare<M1Label>({
            m.getSize().width()/2 - load_button_box.width - 60, settings_topBound_y + 100,
            load_button_width, 20})
            .withText("LOAD")
            .withTextAlignment(TEXT_CENTER)
            .withVerticalTextOffset(3)
            .withStrokeBorder(MurkaColor(ENABLED_PARAM))
            .withBackgroundFill(MurkaColor(BACKGROUND_COMPONENT), MurkaColor(BACKGROUND_GREY))
            .withOnClickFlash()
            .withOnClickCallback([&](){
                transportSource.stop();
                transportSource.setSource(nullptr);
                showFileChooser();
            })
            .draw();
        
        // view mode
        juceFontStash::Rectangle vm_label_box = m.getCurrentFont()->getStringBoundingBox("VIEW MODE", 0, 0);
        m.prepare<murka::Label>({
            leftSide_LeftBound_x,
            settings_topBound_y + 160,
            vm_label_box.width + 20, vm_label_box.height })
            .text("VIEW MODE")
            .withAlignment(TEXT_LEFT)
            .draw();
        
        auto& viewModeRadioGroup = m.prepare<RadioGroupWidget>({ leftSide_LeftBound_x + 4, settings_topBound_y + 180, m.getSize().width()/2 - 88, 30 });
        viewModeRadioGroup.labels = { "3D", "2D" };
        viewModeRadioGroup.selectedIndex = videoPlayerWidget.drawFlat ? 1 : 0;
        viewModeRadioGroup.drawAsCircles = true;
        viewModeRadioGroup.draw();
        if (viewModeRadioGroup.changed) {
            if (viewModeRadioGroup.selectedIndex == 0) {
                videoPlayerWidget.drawFlat = false;
                drawReference = false;
            }
            else if (viewModeRadioGroup.selectedIndex == 1) {
                videoPlayerWidget.drawFlat = true;
                drawReference = false;
            }
            else {
                videoPlayerWidget.drawFlat = false;
                drawReference = true;
            }
        }
        
        auto& overlayCheckbox = m.prepare<M1Checkbox>({ 
            leftSide_LeftBound_x,
            settings_topBound_y + 240,
            200, 18 })
            .withLabel("OVERLAY (O)");
        overlayCheckbox.dataToControl = &videoPlayerWidget.drawOverlay;
        overlayCheckbox.checked = videoPlayerWidget.drawOverlay;
        overlayCheckbox.enabled = true;
        overlayCheckbox.drawAsCircle = false;
        overlayCheckbox.draw();
        if (overlayCheckbox.changed) {
            videoPlayerWidget.drawOverlay = !videoPlayerWidget.drawOverlay;
        }
        
        auto& cropStereoscopicCheckbox = m.prepare<M1Checkbox>({
            leftSide_LeftBound_x,
            settings_topBound_y + 270,
            200, 18 })
            .withLabel("CROP STEREOSCOPIC (D)");
        cropStereoscopicCheckbox.dataToControl = &videoPlayerWidget.crop_Stereoscopic_TopBottom;
        cropStereoscopicCheckbox.checked = videoPlayerWidget.crop_Stereoscopic_TopBottom;
        cropStereoscopicCheckbox.enabled = (clip.get() != nullptr && clip->hasVideo()); // only if video file exists
        cropStereoscopicCheckbox.drawAsCircle = false;
        cropStereoscopicCheckbox.draw();
        if (cropStereoscopicCheckbox.changed) {
            videoPlayerWidget.crop_Stereoscopic_TopBottom = !videoPlayerWidget.crop_Stereoscopic_TopBottom;
        }
        
        auto& twoDRefCheckbox = m.prepare<M1Checkbox>({
            leftSide_LeftBound_x,
            settings_topBound_y + 300,
            200, 18 })
            .withLabel("2D REFERENCE (G)");
        twoDRefCheckbox.dataToControl = &drawReference;
        twoDRefCheckbox.checked = drawReference;
        twoDRefCheckbox.enabled = (clip.get() != nullptr && clip->hasVideo()); // only if video file exists
        twoDRefCheckbox.drawAsCircle = false;
        twoDRefCheckbox.draw();
        if (twoDRefCheckbox.changed) {
            drawReference = !drawReference;
        }
        
        /// RIGHT SIDE
                    
        // orientation client window
        draw_orientation_client(m, m1OrientationClient);
    }
    
    /// Player label
    if (!(secondsWithoutMouseMove > 5) || showSettingsMenu) { // skip drawing if mouse has not interacted in a while
        m.setColor(ENABLED_PARAM);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        auto& playerLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - 30, 80, 20));
        playerLabel.label = "PLAYER";
        playerLabel.alignment = TEXT_CENTER;
        playerLabel.enabled = false;
        playerLabel.highlighted = false;
        playerLabel.draw();
        
        m.setColor(ENABLED_PARAM);
        m.drawImage(imgLogo, 25, m.getSize().height() - 30, 161 / 4, 39 / 4);
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
