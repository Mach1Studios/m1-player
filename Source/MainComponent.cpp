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

	juce::OpenGLAppComponent::shutdownOpenGL();
}

//==============================================================================
void MainComponent::initialise() 
{
	murka::JuceMurkaBaseComponent::initialise();

	videoEngine.getFormatManager().registerFormat(std::make_unique<foleys::FFmpegFormat>());

	filesDropped({ "C:/Users/User/Desktop/1.mp4" }, 0, 0);
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
			M1OrientationYPR ypr = m1OrientationOSCClient.getOrientation().getYPR();
			currentOrientation.x = m1OrientationOSCClient.getTrackingYawEnabled() ? ypr.yaw : 0.0f;
			currentOrientation.y = m1OrientationOSCClient.getTrackingPitchEnabled() ? ypr.pitch : 0.0f;
			currentOrientation.z = m1OrientationOSCClient.getTrackingRollEnabled() ? ypr.roll : 0.0f;
            */
			m1Decode.setRotation(currentOrientation);

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

		newClip->prepareToPlay(blockSize, sampleRate);

		/// Video Setup
		if (newClip->hasVideo()) {
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

		newClip->prepareToPlay(blockSize, sampleRate);

		if (newClip->hasAudio())
		{
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

void MainComponent::shutdown()
{ 
	clipVideo = nullptr;
	clipAudio = nullptr;

	murka::JuceMurkaBaseComponent::shutdown();
}

void MainComponent::render()
{
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

	m.startFrame();
	m.setScreenScale((float)openGLContext.getRenderingScale());

	m.begin();
	auto& mainWidget = m.prepare<MainWidget>({ 0, 0, m.getWindowWidth(), m.getWindowHeight() });
	mainWidget.imgVideo = &imgVideo; 
	mainWidget.clipVideo = clipVideo;
	mainWidget.clipAudio = clipAudio;
	mainWidget.transportSourceVideo = &transportSourceVideo;
	mainWidget.transportSourceAudio = &transportSourceAudio;
	mainWidget.draw();
	m.end();

	currentOrientation = mainWidget.currentOrientation;
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
