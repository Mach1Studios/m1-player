#include "MainComponent.h"

#define MINUS_3DB_AMP (0.707945784f)
#define MINUS_6DB_AMP (0.501187234f)

//==============================================================================
MainComponent::MainComponent() : m_decode_strategy(&MainComponent::nullStrategy),
                                 m_transcode_strategy(&MainComponent::nullStrategy) 
{
    // Make sure you set the size of the component after
    // you add any child components.
    juce::OpenGLAppComponent::setSize(800, 600);

    // Initialize audio device manager with default settings
    juce::String error = audioDeviceManager.initialise(
        0,      // numInputChannels
        2,      // numOutputChannels
        nullptr,// XML settings = null
        true    // selectDefaultDeviceOnFailure
    );

    if (error.isNotEmpty())
    {
        DBG("Error initializing audio device manager: " + error);
    }

    // Register callbacks
    audioDeviceManager.addAudioCallback(this);
    audioDeviceManager.addChangeListener(this);

    // Setup OSC
    playerOSC = std::make_unique<PlayerOSC>();

    // print build time for debug
    juce::String date(__DATE__);
    juce::String time(__TIME__);
    DBG("[PLAYER] Build date: " + date + " | Build time: " + time);

    createMenuBar();

    initializeAppProperties();
    loadRecentFileList();
}

MainComponent::~MainComponent() 
{
    // Clean up orientation client
    m1OrientationClient.command_disconnect();
    m1OrientationClient.close();

    // Clean up audio device selector and its window
    if (audioDeviceSelector)
    {
        audioDeviceSelector = nullptr;
    }

    // Remove callbacks
    audioDeviceManager.removeAudioCallback(this);
    audioDeviceManager.removeChangeListener(this);
    
    // Remove menu bar
#if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
#endif
    
    // Shutdown OpenGL
    juce::OpenGLAppComponent::shutdownOpenGL();

    saveRecentFiles();
}

void MainComponent::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext& context)
{
    // Create a temporary AudioBuffer to wrap the output channels
    juce::AudioBuffer<float> tempBuffer(const_cast<float**>(outputChannelData), numOutputChannels, numSamples);
    juce::AudioSourceChannelInfo bufferToFill(&tempBuffer, 0, numSamples);
    
    // Clear the output buffer first
    for (int i = 0; i < numOutputChannels; ++i)
        juce::FloatVectorOperations::clear(outputChannelData[i], numSamples);
    
    getNextAudioBlock(bufferToFill);
}

void MainComponent::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    prepareToPlay(device->getCurrentBufferSizeSamples(),
                 device->getCurrentSampleRate());
}

void MainComponent::audioDeviceStopped()
{
    releaseResources();
}

//==============================================================================
void MainComponent::initialise() {
    murka::JuceMurkaBaseComponent::initialise();

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
        // TODO: We can hit this on macos if we are running on a newer macos than what JUCE has defined!!!!
    }
    settingsFile = settingsFile.getChildFile("settings.json");
    DBG("Opening settings file: " + settingsFile.getFullPathName().quoted());

    // Informs OrientationManager that this client is expected to send additional offset for the final orientation to be calculated and to count instances for error handling
    m1OrientationClient.setClientType("player"); // Needs to be set before the init() function
    m1OrientationClient.initFromSettings(settingsFile.getFullPathName().toStdString());
    m1OrientationClient.setStatusCallback(std::bind(&MainComponent::setStatus, this, std::placeholders::_1,
                                                    std::placeholders::_2));

    imgLogo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);
    imgHideUI.loadFromRawData(BinaryData::hide_ui_png, BinaryData::hide_ui_pngSize);
    imgUnhideUI.loadFromRawData(BinaryData::unhide_ui_png, BinaryData::unhide_ui_pngSize);

    // Set the audio device manager for the media object
    currentMedia.setAudioDeviceManager(&audioDeviceManager);

    // setup the listener
    playerOSC->AddListener([&](juce::OSCMessage msg) {
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
            float azi = 0.0;
            float ele = 0.0;
            float div = 0.0;
            float gain = 0.0;
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
                for (auto &panner: panners) {
                    if (panner.port == plugin_port) {
                        found = true;

                        // update existing found panner obj
                        if (state == -1) {
                            // remove panner if panner was deleted
                            auto iter = std::find_if(panners.begin(), panners.end(), find_plugin(plugin_port));
                            if (iter != panners.end()) {
                                // if panner found, delete it
                                panners.erase(iter);
                                DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", disconnected!");
                            }
                        } else {
                            // update state of panner
                            panner.state = state;

                            if (msg.size() >= 10) {
                                // update found panner object
                                // check for a display name or otherwise use the port
                                (msg[2].isString() && msg[2].getString() != "")
                                    ? panner.displayName = msg[2].getString().toStdString()
                                    : panner.displayName = std::to_string(plugin_port);
                                // check for a color
                                if (msg[3].isColour() && msg[3].getColour().alpha != 0) {
                                    panner.color.r = msg[3].getColour().red;
                                    panner.color.g = msg[3].getColour().green;
                                    panner.color.b = msg[3].getColour().blue;
                                    panner.color.a = msg[3].getColour().alpha;
                                }

                                panner.m1Encode.setInputMode((Mach1EncodeInputMode) input_mode);
                                panner.m1Encode.setAzimuthDegrees(azi);
                                panner.m1Encode.setElevationDegrees(ele);
                                panner.m1Encode.setDiverge(div);
                                panner.m1Encode.setOutputGain(gain, true);
                                panner.m1Encode.setPannerMode((Mach1EncodePannerMode) panner_mode);
                                panner.azimuth = azi; // TODO: remove these?
                                panner.elevation = ele; // TODO: remove these?
                                panner.diverge = div; // TODO: remove these?
                                panner.gain = gain; // TODO: remove these?

                                if (input_mode == 1 && msg.size() >= 13) {
                                    panner.m1Encode.setAutoOrbit(auto_orbit);
                                    panner.m1Encode.setOrbitRotationDegrees(st_azi);
                                    panner.m1Encode.setStereoSpread(st_spr / 100.0f); // normalize
                                    panner.autoOrbit = auto_orbit; // TODO: remove these?
                                    panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                                    panner.stereoSpread = st_spr / 100.0f; // TODO: remove these?
                                }

                                DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", in="+std::to_string(input_mode)
                                    +", az="+std::to_string(azi)+", el="+std::to_string(ele)+", di="+std::to_string(div)
                                    +", gain="+std::to_string(gain));
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
                    (msg[2].isString() && msg[2].getString() != "")
                        ? panner.displayName = msg[2].getString().toStdString()
                        : panner.displayName = std::to_string(plugin_port);
                    // check for a color
                    if (msg[3].isColour() && msg[3].getColour().alpha != 0) {
                        panner.color.r = msg[3].getColour().red;
                        panner.color.g = msg[3].getColour().green;
                        panner.color.b = msg[3].getColour().blue;
                        panner.color.a = msg[3].getColour().alpha;
                    }

                    panner.m1Encode.setInputMode((Mach1EncodeInputMode) input_mode);
                    panner.m1Encode.setAzimuthDegrees(azi);
                    panner.m1Encode.setElevationDegrees(ele);
                    panner.m1Encode.setDiverge(div);
                    panner.m1Encode.setOutputGain(gain, true);
                    panner.m1Encode.setPannerMode((Mach1EncodePannerMode) panner_mode);
                    panner.azimuth = azi; // TODO: remove these?
                    panner.elevation = ele; // TODO: remove these?
                    panner.diverge = div; // TODO: remove these?
                    panner.gain = gain; // TODO: remove these?

                    if (input_mode == 1 && msg.size() >= 13) {
                        panner.m1Encode.setAutoOrbit(auto_orbit);
                        panner.m1Encode.setOrbitRotationDegrees(st_azi);
                        panner.m1Encode.setStereoSpread(st_spr / 100.0f); // normalize
                        panner.autoOrbit = auto_orbit; // TODO: remove these?
                        panner.stereoOrbitAzimuth = st_azi; // TODO: remove these?
                        panner.stereoSpread = st_spr / 100.0f; // TODO: remove these?
                    }

                    panners.push_back(panner);
                    DBG("[OSC] Panner: port="+std::to_string(plugin_port)+", in="+std::to_string(input_mode)+", az="+std
                        ::to_string(azi)+", el="+std::to_string(ele)+", di="+std::to_string(div)+", gain="+std::
                        to_string(gain));
                }
            }
        } else {
            // display a captured unexpected osc message
            if (msg.size() > 0) {
                DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString());
                if (msg[0].isFloat32()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + std::to_string(
                        msg[0].getFloat32()));
                } else if (msg[0].isInt32()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + std::to_string(
                        msg[0].getInt32()));
                } else if (msg[0].isString()) {
                    DBG("[OSC] Recieved unexpected msg | " + msg.getAddressPattern().toString() + ", " + msg[0].
                        getString());
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
    
    currentMedia.prepareToPlay(blockSize, sampleRate);
    
    // Setup for Mach1Decode
    smoothedChannelCoeffs.resize(m1Decode.getFormatCoeffCount());
    spatialMixerCoeffs.resize(m1Decode.getFormatCoeffCount());
    for (int input_channel = 0; input_channel < m1Decode.getFormatChannelCount(); input_channel++) {
        smoothedChannelCoeffs[input_channel * 2 + 0].reset(sampleRate, (double) 0.01);
        smoothedChannelCoeffs[input_channel * 2 + 1].reset(sampleRate, (double) 0.01);
    }
    
    // restructure output buffer
    readBuffer.setSize(m1Decode.getFormatCoeffCount(), blockSize);
}

void MainComponent::fallbackDecodeStrategy(const AudioSourceChannelInfo &bufferToFill,
                                           const AudioSourceChannelInfo &info) {
    // Invalid Decode I/O; clear buffers
    for (auto channel = detectedNumInputChannels; channel < 2; ++channel) {
        if (channel < bufferToFill.buffer->getNumChannels())
        {
            bufferToFill.buffer->clear(channel, 0, bufferToFill.numSamples);
        }
    }
}

void MainComponent::stereoDecodeStrategy(const AudioSourceChannelInfo &bufferToFill,
                                         const AudioSourceChannelInfo &info) {
    bufferToFill.buffer->copyFrom(0, 0, readBuffer, 0, 0, info.numSamples);
    if (bufferToFill.buffer->getNumChannels() > 1)
    {
        bufferToFill.buffer->copyFrom(1, 0, readBuffer, 1, 0, info.numSamples);
    }
}

void MainComponent::monoDecodeStrategy(const AudioSourceChannelInfo &bufferToFill, const AudioSourceChannelInfo &info) {
    bufferToFill.buffer->copyFrom(0, 0, readBuffer, 0, 0, info.numSamples);
    if (bufferToFill.buffer->getNumChannels() > 1)
    {
        bufferToFill.buffer->copyFrom(1, 0, readBuffer, 0, 0, info.numSamples);
    }
    bufferToFill.buffer->applyGain(MINUS_3DB_AMP); // apply -3dB pan-law gain to all channels
}

void MainComponent::readBufferDecodeStrategy(const AudioSourceChannelInfo &bufferToFill,
                                             const AudioSourceChannelInfo &info) {
    auto sample_count = bufferToFill.numSamples;
    auto channel_count = detectedNumInputChannels;
    float *outBufferR = nullptr;
    float *outBufferL = bufferToFill.buffer->getWritePointer(0);
    if (bufferToFill.buffer->getNumChannels() > 1)
    {
        outBufferR = bufferToFill.buffer->getWritePointer(1);
    }
    auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
    m1Decode.setRotationDegrees({ori_deg.GetYaw(), ori_deg.GetPitch(), ori_deg.GetRoll()});
    spatialMixerCoeffs = m1Decode.decodeCoeffs();

    // Update spatial mixer coeffs from Mach1Decode for a smoothed value
    for (int channel = 0; channel < channel_count; ++channel) {
        smoothedChannelCoeffs[channel * 2 + 0].setTargetValue(spatialMixerCoeffs[channel * 2 + 0]);
        smoothedChannelCoeffs[channel * 2 + 1].setTargetValue(spatialMixerCoeffs[channel * 2 + 1]);
    }

    // copy from readBuffer for doubled channels
    for (auto channel = 0; channel < channel_count; ++channel) {
        tempBuffer.copyFrom(channel * 2 + 0, 0, readBuffer, channel, 0, sample_count);
        tempBuffer.copyFrom(channel * 2 + 1, 0, readBuffer, channel, 0, sample_count);
    }

    // apply decode coeffs to output buffer
    for (int sample = 0; sample < info.numSamples; sample++) {
        for (int channel = 0; channel < channel_count; channel++) {
            auto left_sample = tempBuffer.getReadPointer(channel * 2 + 0)[sample];
            auto right_sample = tempBuffer.getReadPointer(channel * 2 + 1)[sample];
            outBufferL[sample] += left_sample * smoothedChannelCoeffs[channel * 2 + 0].getNextValue();
            if (bufferToFill.buffer->getNumChannels() > 1)
            {
                outBufferR[sample] += right_sample * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
            }
        }
    }
}

void MainComponent::intermediaryBufferDecodeStrategy(const AudioSourceChannelInfo &bufferToFill,
                                                     const AudioSourceChannelInfo &info) {
    auto sample_count = bufferToFill.numSamples;
    auto channel_count = detectedNumInputChannels;
    float *outBufferR = nullptr;
    float *outBufferL = bufferToFill.buffer->getWritePointer(0);
    if (bufferToFill.buffer->getNumChannels() > 1)
    {
        outBufferR = bufferToFill.buffer->getWritePointer(1);
    }
    auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
    m1Decode.setRotationDegrees({ori_deg.GetYaw(), ori_deg.GetPitch(), ori_deg.GetRoll()});
    spatialMixerCoeffs = m1Decode.decodeCoeffs();

    // Update spatial mixer coeffs from Mach1Decode for a smoothed value
    for (int channel = 0; channel < channel_count; ++channel) {
        smoothedChannelCoeffs[channel * 2 + 0].setTargetValue(spatialMixerCoeffs[channel * 2 + 0]);
        smoothedChannelCoeffs[channel * 2 + 1].setTargetValue(spatialMixerCoeffs[channel * 2 + 1]);
    }

    // copy from intermediaryBuffer for doubled channels
    for (auto channel = 0; channel < channel_count; ++channel) {
        tempBuffer.copyFrom(channel * 2 + 0, 0, intermediaryBuffer, channel, 0, sample_count);
        tempBuffer.copyFrom(channel * 2 + 1, 0, intermediaryBuffer, channel, 0, sample_count);
    }

    // apply decode coeffs to output buffer
    for (int sample = 0; sample < info.numSamples; sample++) {
        for (int channel = 0; channel < channel_count; channel++) {
            auto left_sample = tempBuffer.getReadPointer(channel * 2 + 0)[sample];
            auto right_sample = tempBuffer.getReadPointer(channel * 2 + 1)[sample];
            outBufferL[sample] += left_sample * smoothedChannelCoeffs[channel * 2 + 0].getNextValue();
            if (bufferToFill.buffer->getNumChannels() > 1)
            {
                outBufferR[sample] += right_sample * smoothedChannelCoeffs[channel * 2 + 1].getNextValue();
            }
        }
    }
}

void MainComponent::intermediaryBufferTranscodeStrategy(const AudioSourceChannelInfo &bufferToFill,
                                                        const AudioSourceChannelInfo &info) {
    auto out = m1Transcode.getOutputNumChannels();
    auto sampleCount = bufferToFill.numSamples;

    if (out == 0) {
        return;
    }

    // restructure output buffer
    if (intermediaryBuffer.getNumSamples() != out || intermediaryBuffer.getNumSamples() != sampleCount) {
        intermediaryBuffer.setSize(out, bufferToFill.numSamples);
        intermediaryBuffer.clear();
    }

    std::vector<float*> readPtrs;
    readPtrs.resize(readBuffer.getNumChannels());
    for(int i = 0; i < readBuffer.getNumChannels(); i++) {
        readPtrs[i] = readBuffer.getWritePointer(i); // won't be written to. m1Transcode just expects non-const float**.
    }

    std::vector<float*> intermediaryPtrs;
    intermediaryPtrs.resize(intermediaryBuffer.getNumChannels());
    for(int i = 0; i < intermediaryBuffer.getNumChannels(); i++) {
        intermediaryPtrs[i] = intermediaryBuffer.getWritePointer(i);
    }

    try {
        m1Transcode.processConversion(readPtrs.data(), intermediaryPtrs.data(), sampleCount);
    } catch (const std::exception& e) {
        // Log error but don't crash
        DBG("Transcode error: " << e.what());
        intermediaryBuffer.clear();
        
        // Display error to user
        showErrorPopup = true;
        errorMessage = "TRANSCODE ERROR";
        errorMessageInfo = "No valid transcode conversion path found.";
        errorStartTime = std::chrono::steady_clock::now();
    }
}

void MainComponent::nullStrategy(const AudioSourceChannelInfo &bufferToFill, const AudioSourceChannelInfo &info)
{
    // Display error to user
    showErrorPopup = true;
    errorMessage = "OUTPUT ERROR";
    errorMessageInfo = "No valid audio strategy available.";
    errorStartTime = std::chrono::steady_clock::now();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo &bufferToFill) {
    const juce::ScopedLock audioLock(audioCallbackLock);

    // If no clip has been loaded, exit this routine.
    if (!currentMedia.clipLoaded()) {
        setDetectedInputChannelCount(0);
        return;
    }
    
    // The TransportSource takes care of start, stop and resample.
    juce::AudioSourceChannelInfo info(&readBuffer, bufferToFill.startSample, bufferToFill.numSamples);

    // If standalone mode is active, or the loaded clip has no audio, exit this routine.
    if (!b_standalone_mode || !currentMedia.hasAudio()) {
        return;
    }

    // then read audio source
    setDetectedInputChannelCount(currentMedia.getNumChannels());

    readBuffer.setSize(detectedNumInputChannels, bufferToFill.numSamples);
    readBuffer.clear();

    // the AudioTransportSource takes care of start, stop and resample
    if (currentMedia.hasAudio())
    {
        // TODO: fix for mono audio files
        currentMedia.getNextAudioBlock(info);

        tempBuffer.setSize(detectedNumInputChannels * 2, bufferToFill.numSamples);
        tempBuffer.clear();

        if (detectedNumInputChannels <= 0) {
            bufferToFill.clearActiveBufferRegion();
            return;
        }

        // if you've got more output channels than input clears extra outputs
        for (auto channel = detectedNumInputChannels; channel < 2 && channel < detectedNumInputChannels; ++channel) {
            readBuffer.clear(channel, 0, bufferToFill.numSamples);
        }

        // config transcode
        if (pendingFormatChange ||
            m1Transcode.getFormatName(m1Transcode.getInputFormat()) != selectedInputFormat ||
            m1Transcode.getFormatName(m1Transcode.getOutputFormat()) != selectedOutputFormat) {
            reconfigureAudioTranscode();
            reconfigureAudioDecode();
        }

        // Processing loop
        (this->*m_transcode_strategy)(bufferToFill, info);
        (this->*m_decode_strategy)(bufferToFill, info);

        // clear remaining input channels
        for (auto channel = 2; channel < detectedNumInputChannels; ++channel) {
            readBuffer.clear(channel, 0, bufferToFill.numSamples);
        }
    }
    else
    {
        // no audio, clear the buffer
        bufferToFill.clearActiveBufferRegion();
    }
}

void MainComponent::releaseResources() {
    currentMedia.releaseResources();
}

void MainComponent::timecodeChanged(int64_t, double seconds) {
    // Use this to update the seekbar location
}

//==============================================================================

void MainComponent::showFileChooser()
{
    // Ensure we're on the message thread before showing file chooser
    if (!juce::MessageManager::getInstance()->isThisTheMessageThread())
    {
        juce::MessageManager::callAsync([this]() { showFileChooser(); });
        return;
    }

    // Stop playback and release resources before showing dialog
    currentMedia.stop();
    currentMedia.releaseResources();

    // Create file chooser with appropriate flags
    auto flags = juce::FileBrowserComponent::openMode | 
                 juce::FileBrowserComponent::canSelectFiles;
    
    // Format file extensions properly with semicolons
    juce::String filePattern = "*.mp4;*.m4v;*.mov;*.mkv;*.webm;*.avi;*.wmv;*.ogv;*.aif;*.aiff;*.wav;*.mp3;*.vorbis;*.opus;*.ogg;*.flac;*.pcm;*.alac;*.aac;*.tif;*.tiff;*.png;*.jpg;*.jpeg;*.gif;*.webp;*.svg";
    
    try {
        file_chooser = std::make_unique<juce::FileChooser>(
            "Select a media file...",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            filePattern,
            true  // Use native dialog
        );

        file_chooser->launchAsync(flags, [this](const juce::FileChooser& fc) {
            auto results = fc.getResults();
            if (results.size() > 0)
            {
                auto file = results.getReference(0);
                if (file.existsAsFile())
                {
                    // Instead of calling currentMedia.load directly, use openFile
                    openFile(file);
                }
            }
        });
    }
    catch (const std::exception& e) {
        DBG("FileChooser error: " << e.what());
    }
}

bool MainComponent::isInterestedInFileDrag(const juce::StringArray &) {
    return true;
}

void MainComponent::filesDropped(const juce::StringArray &files, int, int) {
    for (int i = 0; i < files.size(); ++i) {
        const juce::String &currentFile = files[i];
        openFile(juce::File(currentFile));  // Convert String to File object
    }
    juce::Process::makeForegroundProcess();
}

void MainComponent::openFile(juce::File filepath) {
    // TODO: test new dropped files first before clearing
    const juce::ScopedLock audioLock(audioCallbackLock);
    const juce::ScopedLock renderLock(renderCallbackLock);

    currentMedia.stop();
    currentMedia.closeMedia();

  	// Video Setup
    currentMedia.open(juce::URL(filepath));
    addToRecentFiles(filepath);

    // Audio Setup
    if (currentMedia.hasAudio()) {
        setDetectedInputChannelCount(currentMedia.getNumChannels());
    }
    
    // restart timeline
    if (b_standalone_mode) {
        if (currentMedia.clipLoaded()) {
            currentMedia.setPosition(0);
        }
    }
     
    // TODO: Resize window to match video aspect ratio
}

void MainComponent::setStatus(bool success, std::string message) {
    //this->status = message;
    std::cout << success << " , " << message << std::endl;
}

void MainComponent::shutdown()
{ 
	murka::JuceMurkaBaseComponent::shutdown();
}

void MainComponent::syncWithDAWPlayhead() 
{
    // Early exits with minimal checks
    if (!currentMedia.clipLoaded() || !currentMedia.hasVideo())
        return;
        
    double currentUpdate = playerOSC->getPlayerLastUpdate();
    if (std::fabs(currentUpdate - lastUpdateForPlayer) < 0.01f)
    {
        return;
    }
//    DBG("[SYNC] Last sync update: " + juce::String(std::fabs(currentUpdate - lastUpdateForPlayer)));

    // Get current external state
    const double externalTimeInSeconds = playerOSC->getPlayerPositionInSeconds(); // offset applied on monitor side
    const double mediaLength = currentMedia.getLengthInSeconds();
    const double playerPositionInSeconds = currentMedia.getPositionInSeconds();
    const bool shouldBePlaying = playerOSC->getPlayerIsPlaying();
    const bool isCurrentlyPlaying = currentMedia.isPlaying();
    
    // Ensure we don't go beyond the media length
    if (externalTimeInSeconds >= mediaLength) {
        if (currentMedia.isPlaying()) {
            currentMedia.stop();
        }
        lastUpdateForPlayer = currentUpdate;
        return;
    }

//    DBG("[SYNC] Sync State - Thread: " + juce::String((int)juce::MessageManager::getInstance()->isThisTheMessageThread()) +
//        ", Should Play: " + juce::String((int)playerOSC->getPlayerIsPlaying()) +
//        ", Is Playing: " + juce::String((int)currentMedia.isPlaying()) +
//        ", Position: " + juce::String(playerPosition) +
//        ", External: " + juce::String(externalTimecode));

    // Sync while playing
    const double timeDifference = externalTimeInSeconds - playerPositionInSeconds;
    
    // Define thresholds for sync actions
    const double seekThreshold = 2.0;    // Seek if difference is > 2 seconds
    const double syncThreshold = 0.01;    // Minor sync if difference is > 10ms
    
//    DBG("[SYNC] Sync - External: " + juce::String(externalTimecode) +
//        "s, Player: " + juce::String(playerPosition) +
//        "s, Diff: " + juce::String(timeDifference) + "s");

    if (std::fabs(timeDifference) > seekThreshold || externalTimeInSeconds < playerPositionInSeconds)
    {
        // Major difference - perform seek
        DBG("[SYNC] Seeking to correct large sync difference");
        currentMedia.setPosition(externalTimeInSeconds);
    } else {
        currentMedia.setOffsetSeconds(timeDifference);
    }
    
    // Handle play state changes first
    if (isCurrentlyPlaying != shouldBePlaying) {
        if (shouldBePlaying) {
            currentMedia.start();
//            // Ensure playback commands happen on the message thread
//                DBG("[SYNC] Starting playback (already on message thread)");
//                currentMedia.setPosition(externalTimecode);
//                currentMedia.start();
        }
        else
        {
            currentMedia.stop();
            // Ensure playback commands happen on the message thread
//                DBG("[SYNC] Stopping playback (already on message thread)");
//                currentMedia.stop();
        }
    }
    lastUpdateForPlayer = currentUpdate;
}

void MainComponent::draw_orientation_client(murka::Murka &m, M1OrientationClient &m1OrientationClient) {
    std::vector<M1OrientationClientWindowDeviceSlot> slots;

    std::vector<M1OrientationDeviceInfo> devices = m1OrientationClient.getDevices();
    for (int i = 0; i < devices.size(); i++) {
        std::string icon = "";
        if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeSerial && devices[i].
            getDeviceName().find("Bluetooth-Incoming-Port") != std::string::npos) {
            icon = "bt";
        } else if (devices[i].getDeviceType() == M1OrientationDeviceType::M1OrientationManagerDeviceTypeSerial &&
                   devices[i].getDeviceName().find("Mach1-") != std::string::npos) {
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
        slots.push_back({
            icon,
            name,
            name == m1OrientationClient.getCurrentDevice().getDeviceName(),
            i,
            [&](int idx) {
                m1OrientationClient.command_startTrackingUsingDevice(devices[idx]);
            }
        });
    }

    float rightSide_LeftBound_x = m.getSize().width() / 2 + 40;
    float settings_topBound_y = m.getSize().height() * 0.23f + 18;

    // trigger a server side refresh for listed devices while menu is open
    m1OrientationClient.command_refresh();
    //bool showOrientationSettingsPanelInsideWindow = (m1OrientationClient.getCurrentDevice().getDeviceType() != M1OrientationManagerDeviceTypeNone);
    
    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-2);
    orientationControlWindow = &(m.prepare<M1OrientationClientWindow>({ rightSide_LeftBound_x, settings_topBound_y, 300, 400}));
    orientationControlWindow->withDeviceSlots(slots);
    orientationControlWindow->withOrientationClient(m1OrientationClient);
    orientationControlWindow->draw();
}

void MainComponent::draw()
{
    const juce::ScopedLock renderLock(renderCallbackLock);

    // countdown for hiding UI when mouse is not active in window
    if ((m.mouseDelta().x != 0) || (m.mouseDelta().y != 0)) {
        // Only reset the timer if controls are not manually hidden
        if (!bHideUI) {
            secondsWithoutMouseMove = 0;
        }
    }

    // update standalone mode flag
    if (playerOSC->getNumberOfMonitors() > 0) {
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
        syncWithDAWPlayhead();
    }

	// update video frame
	if (currentMedia.clipLoaded() && currentMedia.hasVideo()) 
    {
        auto clipLengthInSeconds = currentMedia.getLengthInSeconds();
        juce::Image& frame = currentMedia.getFrame();
        //DBG("[Video] Time: " + std::to_string(clip->getCurrentTimeInSeconds()) + ", Block:" + std::to_string(clip->getNextReadPosition()) + ", normalized: " + std::to_string( clip->getCurrentTimeInSeconds() /  clipLengthInSeconds ));
		if (frame.isValid() && frame.getWidth() > 0 && frame.getHeight() > 0)
        {
			if (imgVideo.getWidth() != frame.getWidth() || imgVideo.getHeight() != frame.getHeight()) 
            {
				imgVideo.allocate(frame.getWidth(), frame.getHeight());
			}
			juce::Image::BitmapData srcData(frame, juce::Image::BitmapData::readOnly);
			imgVideo.loadData(srcData.data, GL_BGRA);
		}
	} else {
        // No video, clear imgVideo
        if (imgVideo.isAllocated())
        {
            imgVideo.clear();
        }
    }

	m.clear(20);
	m.setColor(255);
	m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);

	auto& videoPlayerWidget = m.prepare<VideoPlayerWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });

    auto vid_rot = Mach1::Float3{ videoPlayerWidget.rotationCurrent.x, videoPlayerWidget.rotationCurrent.y, videoPlayerWidget.rotationCurrent.z }.EulerRadians();
    currentOrientation.SetRotation(vid_rot);

    if (playerOSC->IsConnected() && playerOSC->IsActivePlayer()) 
    {
        // send the current orientation of player instead
        // send mouse offset of player orientation and send to helper
        if (!videoPlayerWidget.wasDrawnFlat && videoPlayerWidget.drawFlat)
        {
            // send once
            playerOSC->sendPlayerYPR(0, 0, 0);
        }
        else if (videoPlayerWidget.isUpdatedRotation)
        {
            auto ori_deg = currentOrientation.GetGlobalRotationAsEulerDegrees();
            playerOSC->sendPlayerYPR(ori_deg.GetYaw(), ori_deg.GetPitch(), ori_deg.GetRoll());
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
            DBG("OM-Client:        Y=" + std::to_string(ori_vec_deg.GetYaw()) + ", P=" + std::to_string(ori_vec_deg.
                GetPitch()) + ", R=" + std::to_string(ori_vec_deg.GetRoll()));
            videoPlayerWidget.rotationOffset.x += m1OrientationClient.getTrackingYawEnabled()
                                                      ? ori_vec_deg.GetYaw() - last_vec_deg.GetYaw()
                                                      : 0.0f;
            videoPlayerWidget.rotationOffset.y += m1OrientationClient.getTrackingPitchEnabled()
                                                      ? ori_vec_deg.GetPitch() - last_vec_deg.GetPitch()
                                                      : 0.0f;
            videoPlayerWidget.rotationOffset.z += m1OrientationClient.getTrackingRollEnabled()
                                                      ? ori_vec_deg.GetRoll() - last_vec_deg.GetRoll()
                                                      : 0.0f;
            DBG("OM-Client Offset: Y=" + std::to_string(ori_vec_deg.GetYaw() - last_vec_deg.GetYaw()) + ", P=" + std::
                to_string(ori_vec_deg.GetPitch() - last_vec_deg.GetPitch()) + ", R=" + std::to_string(ori_vec_deg.
                    GetRoll() - last_vec_deg.GetRoll()));

            // store last input value
            previousClientOrientation = oc_orientation;
        }
    }
    
	if (currentMedia.clipLoaded() && (currentMedia.hasVideo() || currentMedia.hasAudio())) {

		if (currentMedia.hasVideo()) {
			videoPlayerWidget.imgVideo = &imgVideo;
		}

		float length = currentMedia.getLengthInSeconds();
		float playheadPosition = currentMedia.getPositionInSeconds() / length;
		videoPlayerWidget.playheadPosition = (float)playheadPosition;
	}
	
	// draw overlay if video empty
	if (!currentMedia.clipLoaded()) {
		videoPlayerWidget.drawOverlay = true;
	}

	// draw panners
    // TODO: add some protection here?
	videoPlayerWidget.pannerSettings = panners;
	videoPlayerWidget.draw();
	
	// draw reference
    if (currentMedia.clipLoaded() && (currentMedia.hasVideo() || currentMedia.hasAudio())) {
        if (drawReference) {
            m.drawImage(imgVideo, 0, 0, imgVideo.getWidth() * 0.3f, imgVideo.getHeight() * 0.3f);
        }
    } else {
        // Either no clip at all (nullptr) or no audio or video in a clip
    }

    MurkaShape playerControlShape = {
        m.getWindowWidth() / 2 - 150,
        m.getWindowHeight() / 2 - 50,
        300,
        100
    };

    auto& playerControls = m.prepare<M1PlayerControls>(playerControlShape);
    if (b_standalone_mode) { // Standalone mode
        double currentPosition = 0.0;
        if (currentMedia.clipLoaded() && (currentMedia.hasVideo() || currentMedia.hasAudio())) {
            currentPosition = currentMedia.getPositionInSeconds() / currentMedia.getLengthInSeconds();
        }
        playerControls.withPlayerData((currentMedia.clipLoaded()) ? formatTime(currentMedia.getPositionInSeconds()) : "00:00", formatTime(currentMedia.getLengthInSeconds()),
                        true, // showPositionReticle
                        currentPosition, // currentPosition
                        currentMedia.isPlaying(), // playing
                        m1OrientationClient.isConnectedToDevice(), // connected to device
                        (m1OrientationClient.isConnectedToDevice()) ? m1OrientationClient.getOrientation().GetGlobalRotationAsEulerDegrees().GetYaw() : 0.0f,
                        [&]() {
                            // playButtonPress
                            if (currentMedia.isPlaying()) {
                                if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                                    juce::MessageManager::callAsync([this]() {
                                        DBG("Starting playback on message thread");
                                        currentMedia.stop();
                                    });
                                } else {
                                    DBG("Starting playback (already on message thread)");
                                    currentMedia.stop();
                                }
                            }
                            else {
                                if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                                    juce::MessageManager::callAsync([this]() {
                                        DBG("Starting playback on message thread");
                                        currentMedia.start();
                                    });
                                } else {
                                    DBG("Starting playback (already on message thread)");
                                    currentMedia.start();
                                }
                            }
                        },
                        [&]() {
                            // connectButtonPress
                            showSettingsMenu = true;
                        },
                        [&]() {
                            // closeButtonPress - force UI to hide by setting secondsWithoutMouseMove beyond timeout
                            secondsWithoutMouseMove = UI_HIDE_TIMEOUT_SECONDS + 1;
                            bHideUI = true;
                        }),
                        [&](double newPositionNormalised) {
                            // refreshing player position
                            currentMedia.setPosition(newPositionNormalised * currentMedia.getLengthInSeconds());
                        };
        playerControls.withVolumeData(currentMedia.getGain(),
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = (float)newVolume;
            currentMedia.setGain(mediaVolume);
        });
    } else { // Slave mode
        playerControls.withPlayerData("", "",
                        false, // showPositionReticle
                        0, // currentPosition
                        currentMedia.isPlaying(), // playing
                        m1OrientationClient.isConnectedToDevice(), // connected to device
                        (m1OrientationClient.isConnectedToDevice()) ? m1OrientationClient.getOrientation().GetGlobalRotationAsEulerDegrees().GetYaw() : 0.0f,
                        [&]() {
                            // playButtonPress
                            // blocked since control should only be from DAW side
                        },
                        [&]() { 
                            // connectButtonPress
                            showSettingsMenu = true;
                        },
                        [&]() {
                            // closeButtonPress - force UI to hide by setting secondsWithoutMouseMove beyond timeout
                            secondsWithoutMouseMove = UI_HIDE_TIMEOUT_SECONDS + 1;
                            bHideUI = true;
                        }),
                        [&](double newPositionNormalised) {
                            // refreshing player position
                            currentMedia.setPositionNormalized(newPositionNormalised);
                        };
        playerControls.withVolumeData(0.5,
                        [&](double newVolume){
            // refreshing the volume
            mediaVolume = (float)newVolume;
            currentMedia.setGain(mediaVolume);
        });
    }
    playerControls.withStandaloneMode(b_standalone_mode);
    playerControls.bypassingBecauseofInactivity = (secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS);
    m.setColor(20, 20, 20, 200 * (1 - (secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS)));
    // if there has not been mouse activity hide the UI element
    if (!bHideUI && !showSettingsMenu) {
        // do not draw playcontrols if the settings menu is open
        m.drawRectangle(playerControlShape);
        
        if (currentMedia.clipLoaded() && (currentMedia.hasVideo() || currentMedia.hasAudio())) {
            playerControls.draw();
        } else {
            if (!(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS)) {
                // skip drawing if mouse has not interacted in a while
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
                std::string message = "Drop an audio or video file here";
                juceFontStash::Rectangle boundingBox = m.getCurrentFont()->getStringBoundingBox(message, 0, 0);
                m.setColor(ENABLED_PARAM);
                m.prepare<murka::Label>({
                    m.getWindowWidth() * 0.5 - boundingBox.width * 0.5, m.getWindowHeight() * 0.5 - boundingBox.height,
                    350, 30
                }).text(message).draw();
            }
        }
    }
    
    // Draw "Show Controls" button
    if (bHideUI) {
        m.setColor(20, 20, 20, 150);
        MurkaShape showControlsShape = {
            m.getWindowWidth() - 40,
            10,
            30,
            30
        };
        m.drawRectangle(showControlsShape);
        
        m.prepare<M1PlayerControlButton>(showControlsShape)
            .withDrawingCallback([&](MurkaShape shape) {
                m.setColor(ENABLED_PARAM);
                m.drawImage(imgUnhideUI, 
                    shape.x() + (shape.width() - imgUnhideUI.getWidth()/20) / 2,
                    shape.y() + (shape.height() - imgUnhideUI.getHeight()/20) / 2,
                    imgUnhideUI.getWidth()/20,
                    imgUnhideUI.getHeight()/20
                );
            })
            .withOnClickCallback([&]() {
                bHideUI = false;
                secondsWithoutMouseMove = 0;
            })
            .draw();
    }
    // Draw "Close Controls" button
    else if (!bHideUI && !(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS)) {
        m.setColor(20, 20, 20, 150);
        MurkaShape closeControlsShape = {
            m.getWindowWidth() - 40,
            10,
            30,
            30
        };
        m.drawRectangle(closeControlsShape);
        
        m.prepare<M1PlayerControlButton>(closeControlsShape)
            .withDrawingCallback([&](MurkaShape shape) {
                m.setColor(ENABLED_PARAM);
                m.drawImage(imgHideUI,
                    shape.x() + (shape.width() - imgHideUI.getWidth()/20) / 2,
                    shape.y() + (shape.height() - imgHideUI.getHeight()/20) / 2,
                    imgHideUI.getWidth()/20,
                    imgHideUI.getHeight()/20
                );
            })
            .withOnClickCallback([&]() {
                bHideUI = true;
                secondsWithoutMouseMove = UI_HIDE_TIMEOUT_SECONDS + 1;
            })
            .draw();
    }

    // arrow orientation reset keys
    if (m.isKeyPressed(MurkaKey::MURKA_KEY_UP)) {
        // up arrow
        videoPlayerWidget.rotationOffsetMouse = {0, 0, 0};
        videoPlayerWidget.rotation = {0, 0, 0};
    }

    if (m.isKeyPressed(MurkaKey::MURKA_KEY_DOWN)) {
        // down arrow
        videoPlayerWidget.rotationOffsetMouse = {0, 0, 0};
        videoPlayerWidget.rotation = {180., 0, 0};
    }

    if (m.isKeyPressed(MurkaKey::MURKA_KEY_RIGHT)) {
        // right arrow
        videoPlayerWidget.rotationOffsetMouse = {0, 0, 0};
        videoPlayerWidget.rotation = {90., 0, 0};
    }

    if (m.isKeyPressed(MurkaKey::MURKA_KEY_LEFT)) {
        // left arrow
        videoPlayerWidget.rotationOffsetMouse = {0, 0, 0};
        videoPlayerWidget.rotation = {270., 0, 0};
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
        // Cycle through stereoscopic modes: OFF -> TB -> LR
        if (!videoPlayerWidget.crop_Stereoscopic_TopBottom && !videoPlayerWidget.crop_Stereoscopic_LeftRight) {
            // OFF -> TB
            videoPlayerWidget.crop_Stereoscopic_TopBottom = true;
            videoPlayerWidget.crop_Stereoscopic_LeftRight = false;
        } else if (videoPlayerWidget.crop_Stereoscopic_TopBottom) {
            // TB -> LR
            videoPlayerWidget.crop_Stereoscopic_TopBottom = false;
            videoPlayerWidget.crop_Stereoscopic_LeftRight = true;
        } else {
            // LR -> OFF
            videoPlayerWidget.crop_Stereoscopic_TopBottom = false;
            videoPlayerWidget.crop_Stereoscopic_LeftRight = false;
        }
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
            if (currentMedia.isPlaying()) {
                if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                    juce::MessageManager::callAsync([this]() {
                        DBG("Stopping playback on message thread");
                        currentMedia.stop();
                    });
                } else {
                    DBG("Stopping playback (already on message thread)");
                    currentMedia.stop();
                }
            }
            else {
                if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                    juce::MessageManager::callAsync([this]() {
                        DBG("Starting playback on message thread");
                        currentMedia.start();
                    });
                } else {
                    DBG("Starting playback (already on message thread)");
                    currentMedia.start();
                }
            }
        }
        if (m.isKeyPressed(MurkaKey::MURKA_KEY_RETURN)) {
            if (currentMedia.isPlaying()) {
                if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                    juce::MessageManager::callAsync([this]() {
                        DBG("Stopping playback on message thread");
                        currentMedia.stop();
                    });
                } else {
                    DBG("Stopping playback (already on message thread)");
                    currentMedia.stop();
                }
            }
            else {
                if (currentMedia.clipLoaded()) {
                    if (!juce::MessageManager::getInstance()->isThisTheMessageThread()) {
                        juce::MessageManager::callAsync([this]() {
                            currentMedia.setPosition(0);
                        });
                    } else {
                        currentMedia.setPosition(0);
                    }
                }
            }
        }
    }

    if (!bHideUI && bShowHelpUI) {
        m.getCurrentFont()->drawString("FOV : " + std::to_string(videoPlayerWidget.fov), 10, 10);
        m.getCurrentFont()->drawString("Frame: " + std::to_string(currentMedia.getPositionInSeconds()), 10, 30);
        m.getCurrentFont()->drawString("Standalone mode: " + std::to_string(b_standalone_mode), 10, 50);
        m.getCurrentFont()->drawString("Hotkeys:", 10, 130);
        m.getCurrentFont()->drawString("[w] - FOV+", 10, 150);
        m.getCurrentFont()->drawString("[s] - FOV-", 10, 170);
        m.getCurrentFont()->drawString("[z] - Equirectangular / 2D", 10, 190);
        m.getCurrentFont()->drawString("[g] - Overlay 2D Reference", 10, 210);
        m.getCurrentFont()->drawString("[o] - Overlay Reference", 10, 230);
        m.getCurrentFont()->drawString("[d] - Cycle stereoscopic modes (Off/TB/LR)", 10, 250);
        m.getCurrentFont()->drawString("[h] - Hide UI", 10, 290);
        m.getCurrentFont()->drawString("[Arrow Keys] - Orientation Resets", 10, 310);

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

    if (!bHideUI)
    {
        /// BOTTOM BAR
        m.setColor(20, 220);
        if (showSettingsMenu) {
            // bottom bar becomes the settings pane
            // TODO: Animate this drawer opening and closing
            m.drawRectangle(0, m.getSize().height() * 0.15f, m.getSize().width(), m.getSize().height() * 0.85f);
        } else {
            if (!(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS)) {
                // skip drawing if mouse has not interacted in a while
                m.drawRectangle(0, m.getSize().height() - 50, m.getSize().width(), 50); // bottom bar
            }
        }
        
        /// INPUT FORMAT SELECTOR
        
        // Only show format selector if we have multichannel audio
        if (!(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS) && currentMedia.clipLoaded() && currentMedia.getNumChannels() > 2) {
            // Format selector dropdown
            m.setColor(ENABLED_PARAM);
            int dropdownItemHeight = 20;
            float format_selector_x = 65.25 /* logo */ + 25; // Position to the left of settings
            float format_selector_width = m.getSize().width() / 2 - 65.25 - 25 * 3; // ensure we don't overlap with settings button
            
            auto& formatSelectorButton = m.prepare<M1DropdownButton>({
                format_selector_x, m.getSize().height() - 5 - 30,
                format_selector_width, 20
            })
            .withLabel(selectedInputFormat.empty() ? "SELECT FORMAT" : selectedInputFormat)
            .withFontSize(DEFAULT_FONT_SIZE)
            .withOutline(true)
            .withTriangle(false);
            
            formatSelectorButton.textAlignment = TEXT_LEFT;
            formatSelectorButton.draw();
            
            auto& formatSelectorMenu = m.prepare<M1DropdownMenu>({
                format_selector_x, m.getSize().height() - 5 - 30 - currentFormatOptions.size() * dropdownItemHeight,
                format_selector_width, currentFormatOptions.size() * dropdownItemHeight })
            .withOptions(currentFormatOptions);
            
            // Handle dropdown menu
            if (formatSelectorButton.pressed) {
                // Update format options based on current channel count
                currentFormatOptions = getMatchingFormatNames(currentMedia.getNumChannels());
                formatSelectorMenu.open();
            }
            
            formatSelectorMenu.optionHeight = dropdownItemHeight;
            formatSelectorMenu.textAlignment = TEXT_LEFT;
            formatSelectorMenu.highlightLabelColor = BACKGROUND_GREY;
            formatSelectorMenu.fontSize = DEFAULT_FONT_SIZE;
            if (formatSelectorMenu.opened) {
                formatSelectorMenu.draw();
            }
            
            if (formatSelectorMenu.changed)
            {
                const juce::ScopedLock audioLock(audioCallbackLock);
                
                setTranscodeInputFormat(currentFormatOptions[formatSelectorMenu.selectedOption]);
            }
        }
        
        /// SETTINGS BUTTON
        if (showSettingsMenu || !(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS)) {
            // skip drawing if mouse has not interacted in a while
            m.setColor(ENABLED_PARAM);
            float settings_button_y = m.getSize().height() - 15;
            if (showSettingsMenu) {
                auto &showSettingsWhileOpenedButton = m.prepare<M1DropdownButton>({
                    m.getSize().width() / 2 - 30, settings_button_y - 30,
                    120, 30
                })
                .withLabel("SETTINGS")
                .withFontSize(DEFAULT_FONT_SIZE)
                .withOutline(false);
                showSettingsWhileOpenedButton.textAlignment = TEXT_LEFT;
                showSettingsWhileOpenedButton.labelPadding_y = 3;
                showSettingsWhileOpenedButton.labelPadding_x = 0;
                showSettingsWhileOpenedButton.draw();
                
                if (showSettingsWhileOpenedButton.pressed) {
                    showSettingsMenu = false;
                    deleteTheSettingsButton();
                }
                
                // draw settings arrow indicator pointing down
                m.enableFill();
                m.setColor(LABEL_TEXT_COLOR);
                MurkaPoint triangleCenter = {m.getSize().width() / 2 + 40, settings_button_y - 10 - 6};
                std::vector<MurkaPoint3D> triangle;
                triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0});
                triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0}); // bottom middle
                triangle.push_back({triangleCenter.x, triangleCenter.y + 5, 0});
                triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0});
                m.drawPath(triangle);
            } else {
                auto &showSettingsWhileClosedButton = m.prepare<M1DropdownButton>({
                    m.getSize().width() / 2 - 30, settings_button_y - 30,
                    120, 30
                })
                .withLabel("SETTINGS")
                .withFontSize(DEFAULT_FONT_SIZE)
                .withOutline(false);
                showSettingsWhileClosedButton.textAlignment = TEXT_LEFT;
                showSettingsWhileClosedButton.labelPadding_y = 3;
                showSettingsWhileClosedButton.labelPadding_x = 0;
                showSettingsWhileClosedButton.draw();
                
                if (showSettingsWhileClosedButton.pressed) {
                    showSettingsMenu = true;
                    deleteTheSettingsButton();
                }
                
                // draw settings arrow indicator pointing up
                m.enableFill();
                m.setColor(LABEL_TEXT_COLOR);
                MurkaPoint triangleCenter = {m.getSize().width() / 2 + 40, settings_button_y - 11};
                std::vector<MurkaPoint3D> triangle;
                triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
                triangle.push_back({triangleCenter.x + 5, triangleCenter.y, 0}); // top middle
                triangle.push_back({triangleCenter.x, triangleCenter.y - 5, 0});
                triangle.push_back({triangleCenter.x - 5, triangleCenter.y, 0});
                m.drawPath(triangle);
            }
        }
        
        // Settings pane is open
        if (showSettingsMenu) {
            // Settings rendering
            float leftSide_LeftBound_x = 40;
            float rightSide_LeftBound_x = m.getSize().width() / 2 + 40;
            float settings_topBound_y = m.getSize().height() * 0.23f + 18;
            
            /// LEFT SIDE
            
            m.setColor(ENABLED_PARAM);
            juceFontStash::Rectangle pm_label_box = m.getCurrentFont()->getStringBoundingBox("PLAYER MODE", 0, 0);
            m.prepare<murka::Label>({
                leftSide_LeftBound_x,
                settings_topBound_y,
                pm_label_box.width + 20, pm_label_box.height
            })
            .text("PLAYER MODE")
            .withAlignment(TEXT_LEFT)
            .draw();
            
            auto &playModeRadioGroup = m.prepare<RadioGroupWidget>({
                leftSide_LeftBound_x + 4, settings_topBound_y + 20, m.getSize().width() / 2 - 88, 30
            });
            playModeRadioGroup.labels = {"SYNC TO DAW", "STANDALONE"};
            playModeRadioGroup.selectedIndex = b_standalone_mode; // swapped since b_standalone_mode = 0 = standalone
            playModeRadioGroup.drawAsCircles = true;
            playModeRadioGroup.draw();
            if (playModeRadioGroup.changed) {
                if (playModeRadioGroup.selectedIndex == 0) {
                    if (playerOSC->getNumberOfMonitors() > 0) {
                        b_standalone_mode = false;
                        b_wants_to_switch_to_standalone = false;
                    } else {
                        // stay in standalone
                        showErrorPopup = true;
                        errorMessage = "UNABLE TO SYNC TO DAW";
                        errorMessageInfo = "Could not find any instances of M1-Monitor";
                        errorStartTime = std::chrono::steady_clock::now();
                    }
                } else if (playModeRadioGroup.selectedIndex == 1) {
                    // override and allow swap to standalone mode if a media file is loaded
                    b_wants_to_switch_to_standalone = true;
                } else {
                    if (currentMedia.clipLoaded()) {
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
                mf_label_box.width + 20, mf_label_box.height
            })
            .text("MEDIA FILE")
            .withAlignment(TEXT_LEFT)
            .draw();
            
            // Media loader
            float load_button_width = 50;
            std::string file_path = "LOAD MEDIA FILE HERE...";
            if (currentMedia.clipLoaded()) {
                file_path = currentMedia.getMediaFilePath().getSubPath().toStdString();
            }
            
            juceFontStash::Rectangle load_button_box = m.getCurrentFont()->getStringBoundingBox("LOAD", 0, 0);
            juceFontStash::Rectangle file_path_button_box = m.getCurrentFont()->getStringBoundingBox(file_path, 0, 0);
            
            // shrink font size for media path
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-3);
            auto& media_file_path_label = m.prepare<M1Label>({leftSide_LeftBound_x, settings_topBound_y + 100,
                m.getSize().width()/2 - 45 - load_button_box.width - 60, 20})
            .withText(file_path)
            .withTextAlignment(TEXT_LEFT)
            .withVerticalTextOffset(file_path_button_box.height/2)
            .withForegroundColor(MurkaColor(ENABLED_PARAM))
            .withBackgroundFill(MurkaColor(BACKGROUND_COMPONENT), MurkaColor(BACKGROUND_COMPONENT))
            .withOnClickCallback([&]() {
                // TODO: allow editing the path and reloading via return if new video exists
            });
            media_file_path_label.labelPadding_x = 10;
            media_file_path_label.draw();
            
            // reset font size
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
            
            // load button
            m.prepare<M1Label>({
                m.getSize().width() / 2 - load_button_box.width - 60, settings_topBound_y + 100,
                load_button_width, 20
            })
            .withText("LOAD")
            .withTextAlignment(TEXT_CENTER)
            .withVerticalTextOffset(3)
            .withStrokeBorder(MurkaColor(ENABLED_PARAM))
            .withBackgroundFill(MurkaColor(BACKGROUND_COMPONENT), MurkaColor(BACKGROUND_GREY))
            .withOnClickFlash()
            .withOnClickCallback([&]() {
                showFileChooser();
            })
            .draw();
            
            // view mode
            juceFontStash::Rectangle vm_label_box = m.getCurrentFont()->getStringBoundingBox("VIEW MODE", 0, 0);
            m.prepare<murka::Label>({
                leftSide_LeftBound_x,
                settings_topBound_y + 160,
                vm_label_box.width + 20, vm_label_box.height
            })
            .text("VIEW MODE")
            .withAlignment(TEXT_LEFT)
            .draw();
            
            auto &viewModeRadioGroup = m.prepare<RadioGroupWidget>({
                leftSide_LeftBound_x + 4, settings_topBound_y + 180, m.getSize().width() / 2 - 88, 30
            });
            viewModeRadioGroup.labels = {"3D", "2D"};
            viewModeRadioGroup.selectedIndex = videoPlayerWidget.drawFlat ? 1 : 0;
            viewModeRadioGroup.drawAsCircles = true;
            viewModeRadioGroup.draw();
            if (viewModeRadioGroup.changed) {
                if (viewModeRadioGroup.selectedIndex == 0) {
                    videoPlayerWidget.drawFlat = false;
                    drawReference = false;
                } else if (viewModeRadioGroup.selectedIndex == 1) {
                    videoPlayerWidget.drawFlat = true;
                    drawReference = false;
                } else {
                    videoPlayerWidget.drawFlat = false;
                    drawReference = true;
                }
            }
            
            auto &overlayCheckbox = m.prepare<M1Checkbox>({
                leftSide_LeftBound_x,
                settings_topBound_y + 240,
                200, 18
            })
            .withLabel("OVERLAY (O)");
            overlayCheckbox.dataToControl = &videoPlayerWidget.drawOverlay;
            overlayCheckbox.checked = videoPlayerWidget.drawOverlay;
            overlayCheckbox.enabled = true;
            overlayCheckbox.drawAsCircle = false;
            overlayCheckbox.draw();
            if (overlayCheckbox.changed) {
                videoPlayerWidget.drawOverlay = !videoPlayerWidget.drawOverlay;
            }
            
            // STEREOSCOPIC DROPDOWN
            juceFontStash::Rectangle st_label_box = m.getCurrentFont()->getStringBoundingBox("CROP STEREOSCOPIC (D)", 0, 0);
            float stereo_y_position = settings_topBound_y + 285;
            
            m.prepare<murka::Label>({
                leftSide_LeftBound_x,
                stereo_y_position,
                150, st_label_box.height
            })
            .text("CROP STEREOSCOPIC (D)")
            .withAlignment(TEXT_LEFT)
            .draw();

            std::vector<std::string> stereoscopicOptions = {"NONE", "TOP-BOTTOM", "LEFT-RIGHT"};
            
            int selectedStereoscopicOption = 0; // Default to NONE
            if (videoPlayerWidget.crop_Stereoscopic_TopBottom) {
                selectedStereoscopicOption = 1; // TOP-BOTTOM
            } else if (videoPlayerWidget.crop_Stereoscopic_LeftRight) {
                selectedStereoscopicOption = 2; // LEFT-RIGHT
            }
            
            // Only show stereoscopic dropdown if video is loaded
            if (currentMedia.clipLoaded() && currentMedia.hasVideo()) {
                auto& stereoscopicButton = m.prepare<M1DropdownButton>({
                    leftSide_LeftBound_x + 170, stereo_y_position - 4,
                    100, 20
                })
                .withLabel(stereoscopicOptions[selectedStereoscopicOption])
                .withFontSize(DEFAULT_FONT_SIZE)
                .withOutline(true)
                .withTriangle(false);
                
                stereoscopicButton.textAlignment = TEXT_LEFT;
                stereoscopicButton.draw();
                
                // Create dropdown menu
                auto& stereoscopicMenu = m.prepare<M1DropdownMenu>({
                    leftSide_LeftBound_x + 170, stereo_y_position - 4 - stereoscopicOptions.size() * 20,
                    100, stereoscopicOptions.size() * 20
                })
                .withOptions(stereoscopicOptions);
                
                if (stereoscopicButton.pressed) {
                    stereoscopicMenu.open();
                }
                
                stereoscopicMenu.optionHeight = 20;
                stereoscopicMenu.textAlignment = TEXT_LEFT;
                stereoscopicMenu.selectedOption = selectedStereoscopicOption;
                
                if (stereoscopicMenu.opened) {
                    stereoscopicMenu.draw();
                }
                
                if (stereoscopicMenu.changed) {
                    if (stereoscopicMenu.selectedOption == 0) { // NONE
                        videoPlayerWidget.crop_Stereoscopic_TopBottom = false;
                        videoPlayerWidget.crop_Stereoscopic_LeftRight = false;
                    } else if (stereoscopicMenu.selectedOption == 1) { // TOP-BOTTOM
                        videoPlayerWidget.crop_Stereoscopic_TopBottom = true;
                        videoPlayerWidget.crop_Stereoscopic_LeftRight = false;
                    } else if (stereoscopicMenu.selectedOption == 2) { // LEFT-RIGHT
                        videoPlayerWidget.crop_Stereoscopic_TopBottom = false;
                        videoPlayerWidget.crop_Stereoscopic_LeftRight = true;
                    }
                }
            } else {
                // If no video, just draw a disabled label
                m.setColor(DISABLED_PARAM);
                m.prepare<murka::Label>({
                    leftSide_LeftBound_x + 170, stereo_y_position - 0,
                    100, 20
                })
                .text("NONE")
                .withAlignment(TEXT_LEFT)
                .draw();
                m.setColor(ENABLED_PARAM);
            }
            
            auto &twoDRefCheckbox = m.prepare<M1Checkbox>({
                leftSide_LeftBound_x,
                settings_topBound_y + 330,
                200, 18
            })
            .withLabel("2D REFERENCE (G)");
            twoDRefCheckbox.dataToControl = &drawReference;
            twoDRefCheckbox.checked = drawReference;
            twoDRefCheckbox.enabled = (currentMedia.clipLoaded() && currentMedia.hasVideo()); // only if video file exists
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
        if (!(secondsWithoutMouseMove > UI_HIDE_TIMEOUT_SECONDS) || showSettingsMenu) {
            // skip drawing if mouse has not interacted in a while
            m.setColor(ENABLED_PARAM);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
            auto &playerLabel = m.prepare<
            M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - 30, 80, 20));
            playerLabel.label = "PLAYER";
            playerLabel.alignment = TEXT_CENTER;
            playerLabel.enabled = false;
            playerLabel.highlighted = false;
            playerLabel.draw();
            
            m.setColor(ENABLED_PARAM);
            m.drawImage(imgLogo, 25, m.getSize().height() - 30, 161 / 4, 39 / 4);
        }
    }
    
    // Display error popup
    if (showErrorPopup) {
        // reset font size
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);

        errorOpacity = 1.0f;
        auto errorBoundingBox = m.getCurrentFont()->getStringBoundingBox(errorMessage, 0, 0);
        auto errorInfoBoundingBox = m.getCurrentFont()->getStringBoundingBox(errorMessageInfo, 0, 0);
        
        auto currentTime = std::chrono::steady_clock::now();
        float elapsedSeconds = std::chrono::duration<float>(currentTime - errorStartTime).count();

        if (elapsedSeconds < fadeDuration) {
            errorOpacity = 1.0f - (elapsedSeconds / fadeDuration);
            
            int xPosition = (m.getSize().width() - errorBoundingBox.width) / 2;
            int yPosition = 20;

            m.setColor(25, 25, 25, static_cast<int>(errorOpacity * 255));
            m.drawRectangle(xPosition, yPosition, errorBoundingBox.width, errorBoundingBox.height);

            int textWidth = m.getCurrentFont()->stringWidth(errorMessage);

            m.setColor(255, 0, 0, static_cast<int>(errorOpacity * 255));
            m.getCurrentFont()->drawString(
                errorMessage,
                xPosition + (errorBoundingBox.width - textWidth) / 2,
                yPosition + (errorBoundingBox.height / 2) - 5
            );
            if (errorMessageInfo != "") {
                xPosition = (m.getSize().width() - errorInfoBoundingBox.width) / 2;
                yPosition += 20;
                
                m.setColor(25, 25, 25, static_cast<int>(errorOpacity * 255));
                m.drawRectangle(xPosition, yPosition, errorInfoBoundingBox.width, errorInfoBoundingBox.height);

                int textWidth = m.getCurrentFont()->stringWidth(errorMessageInfo);
                
                m.setColor(255, 0, 0, static_cast<int>(errorOpacity * 255));
                m.getCurrentFont()->drawString(
                    errorMessageInfo,
                    xPosition + (errorInfoBoundingBox.width - textWidth) / 2,
                    yPosition + (errorInfoBoundingBox.height / 2) - 5
                );
            }
        } else {
            errorOpacity = 0.0f;
            showErrorPopup = false;
            // reset error messages
            errorMessage = "";
            errorMessageInfo = "";
        }
    }

    // update the previous orientation for calculating offset
    videoPlayerWidget.rotationPrevious = videoPlayerWidget.rotationCurrent;

    // update the mousewheel scroll for testing
    lastScrollValue = m.mouseScroll();
}

std::string MainComponent::getTranscodeInputFormat() const {
    return selectedInputFormat;
}

std::string MainComponent::getTranscodeOutputFormat() const {
    return selectedOutputFormat;
}

void MainComponent::setTranscodeInputFormat(const std::string &name) {
    if (!name.empty() && m1Transcode.getFormatFromString(name) != -1) {
        // Queue the format change instead of applying immediately
        m1Transcode.setInputFormat(m1Transcode.getFormatFromString(name));
        selectedInputFormat = name;
        pendingFormatChange = true;
    }
}

void MainComponent::setTranscodeOutputFormat(const std::string &name) {
    if (!name.empty() && m1Transcode.getFormatFromString(name) != -1) {
        // Queue the format change instead of applying immediately
        m1Transcode.setOutputFormat(m1Transcode.getFormatFromString(name));
        selectedOutputFormat = name;
        pendingFormatChange = true;
    }
}

//==============================================================================
void MainComponent::timerCallback() {
    // Added if we need to move the OSC stuff from the processorblock
    playerOSC->update(); // test for connection
    secondsWithoutMouseMove += 1;

    // Update last known position if media is loaded
    if (currentMedia.clipLoaded()) {
        lastKnownMediaPlayState = currentMedia.isPlaying();
        lastKnownMediaPosition = currentMedia.getPositionInSeconds();
    }
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    // This will draw over the top of the openGL background.
}

void MainComponent::resized() 
{
    // This is called when the MainComponent is resized.
}

void MainComponent::reconfigureAudioDecode() {
    // Setup for Mach1Decode API
    m1Decode.setPlatformType(Mach1PlatformDefault);
    m1Decode.setFilterSpeed(0.99f);

    switch (detectedNumInputChannels) {
        case 0:
            m_decode_strategy = &MainComponent::nullStrategy;
            break;
        case 1:
            m_decode_strategy = &MainComponent::monoDecodeStrategy;
            break;
        case 2:
            m_decode_strategy = &MainComponent::stereoDecodeStrategy;
            break;
        default:
            // For any multichannel input (>2), use intermediary buffer strategy
            if (detectedNumInputChannels == 4 && selectedInputFormat == "M1Spatial-4") {
                m1Decode.setDecodeMode(M1DecodeSpatial_4);
                m_decode_strategy = &MainComponent::readBufferDecodeStrategy; // decode directly to buffer
            } else if (detectedNumInputChannels == 8 && selectedInputFormat == "M1Spatial-8") {
                m1Decode.setDecodeMode(M1DecodeSpatial_8);
                m_decode_strategy = &MainComponent::readBufferDecodeStrategy; // decode directly to buffer
            } else {
                m1Decode.setDecodeMode(M1DecodeSpatial_14);
                if (selectedInputFormat == "M1Spatial-14") {
                    m_decode_strategy = &MainComponent::readBufferDecodeStrategy; // decode directly to buffer
                } else {
                    m_decode_strategy = &MainComponent::intermediaryBufferDecodeStrategy; // decode to intermediary buffer for transcoding
                }
            }
            break;
    }
}

// TODO: Detect any Mach1Spatial comment metadata
void MainComponent::reconfigureAudioTranscode() {
    // Default to null strategy
    m_transcode_strategy = &MainComponent::nullStrategy;

    if (detectedNumInputChannels <= 2) {
        return;
    }

    // Use selected format if available, otherwise use default behavior
    if (!selectedInputFormat.empty()) {
        setTranscodeInputFormat(selectedInputFormat);

        /// INPUT PREFERRED OUTPUT OVERRIDE ASSIGNMENTS
        if (selectedInputFormat == "3.0_LCR" || // NOTE: switch to M1Spatial-14 for center channel
            selectedInputFormat == "4.0_LCRS" || // NOTE: switch to M1Spatial-14 for center channel
            selectedInputFormat == "M1Horizon-4_2")
        {
            setTranscodeOutputFormat("M1Spatial-4");
        }
        else if (selectedInputFormat == "4.0_AFormat" ||
                 selectedInputFormat == "Ambeo" ||
                 selectedInputFormat == "TetraMic" ||
                 selectedInputFormat == "SPS-200" ||
                 selectedInputFormat == "ORTF3D" ||
                 selectedInputFormat == "CoreSound-OctoMic" ||
                 selectedInputFormat == "CoreSound-OctoMic_SIM")
        {
            setTranscodeOutputFormat("M1Spatial-8");
        }
        else
        {
            setTranscodeOutputFormat("M1Spatial-14");
        }
        // TODO: Add more format overrides for higher order ambisonic to 38ch when ready
        
        if (m1Transcode.processConversionPath())
        {
            m_transcode_strategy = &MainComponent::intermediaryBufferTranscodeStrategy;
        }
        else
        {
            m_transcode_strategy = &MainComponent::nullStrategy;

        }
        pendingFormatChange = false;
    }
}

void MainComponent::setDetectedInputChannelCount(int numberOfInputChannels) {
    if (detectedNumInputChannels == numberOfInputChannels) {
        return;
    }

    detectedNumInputChannels = numberOfInputChannels;

    // Set default format when channels change
    if (numberOfInputChannels > 2) {
        selectedInputFormat = getDefaultFormatForChannelCount(numberOfInputChannels);
    } else {
        selectedInputFormat = "";
    }

    reconfigureAudioTranscode();
    reconfigureAudioDecode(); // can use intermediaryBuffer and should be called last
}

void MainComponent::createMenuBar()
{
    #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
    #endif
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "View" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (topLevelMenuIndex == 0) // File menu
    {
        menu.addItem(OpenFileMenuID, "Open...", true);
        
        // Add Recent Files submenu
        juce::PopupMenu recentFilesMenu;
        for (int i = 0; i < recentFiles.size(); ++i)
        {
            // Make sure the file still exists
            if (recentFiles[i].existsAsFile())
            {
                recentFilesMenu.addItem(RecentFileMenuID + i, 
                                      recentFiles[i].getFullPathName(),
                                      true);
            }
        }
        
        // Only disable if there are no recent files
        bool hasRecentFiles = recentFiles.size() > 0;
        menu.addSubMenu("Open Recent", recentFilesMenu, hasRecentFiles);
        menu.addSeparator();
        menu.addItem(SettingsMenuID, "Audio Device Settings", true);
    }
    // TODO: implement this
//    else if (topLevelMenuIndex == 1) // View menu
//    {
//        menu.addItem(View2DMenuID, "2D View", true, isViewFlat());
//        menu.addItem(View3DMenuID, "3D View", true, !isViewFlat());
//        menu.addSeparator();
//        menu.addItem(FullScreenMenuID, "Enter Full Screen", true, false);
//        menu.addItem(ToggleOverlayMenuID, "Toggle Overlay", true, isOverlayEnabled());
//    }

    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case OpenFileMenuID:
            showFileChooser();
            break;

            // TODO: implement the below:
//        case View2DMenuID:
//            setViewMode(true);
//            break;
//            
//        case View3DMenuID:
//            setViewMode(false);
//            break;
//            
//        case FullScreenMenuID:
//            if (auto* window = TopLevelWindow())
//                window->setFullScreen(!window->isFullScreen());
//            break;
//
//        case ToggleOverlayMenuID:
//            setOverlay(!isOverlayEnabled());
//            break;
            
        case SettingsMenuID:
            if (!audioDeviceSelector)
            {
                static M1AudioSettingsLookAndFeel audioSettingsLookAndFeel;
                
                // Create a custom AudioDeviceSelectorComponent with minimal options
                audioDeviceSelector.reset(new juce::AudioDeviceSelectorComponent(
                    audioDeviceManager,
                    0,                     // Minimum input channels (hide input section)
                    0,                     // Maximum input channels (hide input section)
                    0,                     // Minimum output channels
                    2,                     // Maximum output channels
                    false,                 // Show MIDI input options
                    false,                 // Show MIDI output selector
                    true,                  // Show channels as stereo pairs
                    false                  // Hide advanced options
                ));
                
                audioDeviceSelector->setLookAndFeel(&audioSettingsLookAndFeel);
                
                juce::DialogWindow::LaunchOptions options;
                options.content.setOwned(audioDeviceSelector.release());
                options.content->setSize(500, 300);
                options.dialogTitle = "Audio Settings";
                options.dialogBackgroundColour = juce::Colour(BACKGROUND_GREY);
                options.escapeKeyTriggersCloseButton = true;
                options.useNativeTitleBar = true;
                options.resizable = false;
                
                auto* dialog = options.create();
                dialog->setLookAndFeel(&audioSettingsLookAndFeel);
                
                dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                    [this, dialog](int) {
                        audioDeviceSelector = nullptr;
                        delete dialog;
                    }
                ));
            }
            break;
            
        default:
            // Handle recent files
            if (menuItemID >= RecentFileMenuID && menuItemID < RecentFileMenuID + MAX_RECENT_FILES)
            {
                int fileIndex = menuItemID - RecentFileMenuID;
                if (fileIndex < recentFiles.size())
                {
                    if (recentFiles[fileIndex].existsAsFile())
                    {
                        // Instead of calling currentMedia.load directly, use openFile
                        openFile(recentFiles[fileIndex]);
                    }
                    else
                    {
                        // Remove non-existent file and update menu
                        recentFiles.erase(recentFiles.begin() + fileIndex);
                        menuItemsChanged();
                    }
                }
            }
            break;
    }
}

void MainComponent::addToRecentFiles(const juce::File& file)
{
    // Don't add if file doesn't exist
    if (!file.existsAsFile())
        return;

    // Remove if already exists (case-insensitive comparison)
    recentFiles.erase(
        std::remove_if(recentFiles.begin(), recentFiles.end(),
            [&file](const juce::File& f) { 
                return f.getFullPathName().compareIgnoreCase(file.getFullPathName()) == 0; 
            }),
        recentFiles.end()
    );
    
    // Add to front
    recentFiles.insert(recentFiles.begin(), file);
    
    // Keep only MAX_RECENT_FILES
    if (recentFiles.size() > MAX_RECENT_FILES)
        recentFiles.resize(MAX_RECENT_FILES);

    // Save the updated list
    saveRecentFiles();

    // Force menu bar update
    menuItemsChanged();
}

void MainComponent::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &audioDeviceManager)
    {
        audioDeviceManagerChanged();
    }
}

void MainComponent::audioDeviceManagerChanged()
{
    // Store current media state before doing anything
    juce::URL currentUrl = currentMedia.getMediaFilePath();

    // Use the last known position and play state after device change
    bool wasPlaying = lastKnownMediaPlayState && b_standalone_mode;
    double currentPosition = lastKnownMediaPosition;

    // Temporarily stop playback
    if (wasPlaying)
        currentMedia.stop();

    auto* device = audioDeviceManager.getCurrentAudioDevice();
    if (!device)
        return; // No device available

    // Update device settings
    currentMedia.prepareToPlay(
        device->getCurrentBufferSizeSamples(),
        device->getCurrentSampleRate()
    );

    // Always attempt to reload the media if we had one before
    if (currentUrl.isLocalFile())
    {
        currentMedia.open(currentUrl);
        
        // Restore position and playback state
        if (currentMedia.clipLoaded())
        {
            currentMedia.setPosition(currentPosition);
            if (wasPlaying)
                currentMedia.start();
        }
    }
}

void MainComponent::initializeAppProperties()
{
    // Set up properties file options
    juce::PropertiesFile::Options options;
    options.applicationName = "M1-Player";
    options.filenameSuffix = ".settings";
    options.osxLibrarySubFolder = "Application Support/Mach1";
    options.folderName = "M1-Player";
    options.storageFormat = juce::PropertiesFile::storeAsXML;

    // TODO: Combine all our settings into one xml file
    // for now since this app is sandboxed it will save to a container on macos
    // Create the directory if it doesn't exist
    juce::File applicationSupportDirectory;
    
    #if JUCE_MAC
        applicationSupportDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                        .getChildFile("Application Support")
                                        .getChildFile("Mach1")
                                        .getChildFile("M1-Player");
    #else
        applicationSupportDirectory = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                        .getChildFile("Mach1")
                                        .getChildFile("M1-Player");
    #endif

    applicationSupportDirectory.createDirectory(); // Create directories if they don't exist
    
    // Create properties file in the correct location
    juce::File propertiesFile = applicationSupportDirectory.getChildFile("M1-Player.settings");
    appProperties = std::make_unique<juce::PropertiesFile>(propertiesFile, options);
    
    // Debug output to verify file location
    DBG("Settings file location: " + propertiesFile.getFullPathName());
}

void MainComponent::saveRecentFiles()
{
    if (appProperties == nullptr)
    {
        DBG("Error: Properties file not initialized");
        return;
    }

    // First remove the old array
    appProperties->removeValue("recentFiles");

    // Save each file path as a numbered property
    for (int i = 0; i < recentFiles.size(); ++i)
    {
        if (recentFiles[i].existsAsFile())
        {
            String propertyName = "recentFile_" + String(i);
            appProperties->setValue(propertyName, recentFiles[i].getFullPathName());
        }
    }
    
    // Save the number of files
    appProperties->setValue("recentFilesCount", juce::var((int)recentFiles.size()));

    // Force save
    bool saved = appProperties->save();
    DBG("Saving recent files: " + (saved ? String("success") : String("failed")));
}

void MainComponent::loadRecentFileList()
{
    if (appProperties == nullptr)
    {
        DBG("Error: Properties file not initialized");
        return;
    }

    recentFiles.clear();
    
    int count = appProperties->getIntValue("recentFilesCount", 0);
    
    for (int i = 0; i < count; ++i)
    {
        juce::String propertyName = "recentFile_" + juce::String(i);
        juce::String filePath = appProperties->getValue(propertyName);
        
        if (filePath.isNotEmpty())
        {
            // Create file with absolute path and verify it exists
            juce::File file(filePath);
            if (file.existsAsFile())
            {
                recentFiles.push_back(file);
                DBG("Loaded recent file: " + file.getFullPathName());
            }
            else
            {
                DBG("Failed to load recent file (doesn't exist): " + filePath);
            }
        }
    }
    
    DBG("Loaded " + juce::String(recentFiles.size()) + " recent files");
}
