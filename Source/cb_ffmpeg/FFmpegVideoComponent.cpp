#include <JuceHeader.h>
#include "FFmpegVideoComponent.h"
#include "../cb_strings/StringHelper.h"

#define AUDIO_BUFFER_SIZE 192000        // 4 seconds at 48kHz
#define VIDEO_FRAME_BUFFER_SIZE  102    // a bit more as 4 seconds at 25 FPS

//==============================================================================
FFmpegVideoComponent::FFmpegVideoComponent()
    : transportSource(new juce::AudioTransportSource)
, videoReader(new FFmpegVideoReader (AUDIO_BUFFER_SIZE, VIDEO_FRAME_BUFFER_SIZE))
    , currentAVFrame(nullptr)
    , playSpeed(1.0)
{
    frameWasPainted = false;
    
    setOpaque (true);
    startTimerHz(80);
    
    if (videoReader)
        videoReader->addVideoListener(this);
    
    //temporarily set source for audio transport source, get's reset when opening file
    transportSource->setSource (videoReader.get(), 0, nullptr);

    setAudioChannels (0, 2);
    
    onPlaybackStarted = nullptr;
    onPlaybackStopped = nullptr;

//    if (juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice())
//    {
//        DBG ("Current Samplerate: " + juce::String (device->getCurrentSampleRate()));
//        DBG ("Current Buffersize: " + juce::String (device->getCurrentBufferSizeSamples()));
//        DBG ("Current Bitdepth:   " + juce::String (device->getCurrentBitDepth()));
//    }
//    else{
//        DBG("AudioDevice not loaded!");
//    }
}


FFmpegVideoComponent::~FFmpegVideoComponent()
{
    if (videoReader)
        videoReader->removeVideoListener (this);
    
    //remove video reader as source from transport source to prevent releasing resources multiple times
    transportSource->setSource(nullptr);
    
    shutdownAudio();
}

void FFmpegVideoComponent::paint (juce::Graphics& g)
{
    //convert video frame to image and paint it
    g.fillAll (juce::Colours::black);
    g.setColour(juce::Colours::white);
    if ( !videoReader )
    {
        g.drawFittedText ("FFMpegVideoComponent: Video Reader not initialized.", getLocalBounds(), juce::Justification::centred, 1);
    }
    else if ( !videoReader->isVideoOpen() )
    {
        g.drawFittedText ("FFMpegVideoComponent: No Video loaded", getLocalBounds(), juce::Justification::centred, 1);
    }
    else if( !currentAVFrame )
    {
        g.drawFittedText ("FFMpegVideoComponent: No frame loaded to display.", getLocalBounds(), juce::Justification::centred, 1);
    }
    else if ( !currentFrameAsImage.isValid() )
    {
        g.drawFittedText ("FFMpegVideoComponent: Current image is not valid.", getLocalBounds(), juce::Justification::centred, 1);
    }
    else
    {
        videoScaler.convertFrameToImage (currentFrameAsImage, currentAVFrame);
        g.drawImageAt (currentFrameAsImage, (getWidth() - currentFrameAsImage.getWidth()) * 0.5,
                                            (getHeight() - currentFrameAsImage.getHeight()) * 0.5);
    }
}

void FFmpegVideoComponent::resized()
{
    if (!videoReader)
        return;

    double aspectRatio = videoReader->getVideoAspectRatio() * videoReader->getPixelAspectRatio();
    
    if (aspectRatio > 0.0)
    {
        double w = getWidth();
        double h = getHeight();
        if (w / h > aspectRatio)
            w = h * aspectRatio;
        else
            h = w / aspectRatio;
        currentFrameAsImage = juce::Image (juce::Image::PixelFormat::ARGB, static_cast<int> (w), static_cast<int> (h), true);
        videoScaler.setupScaler (videoReader->getVideoWidth(),
                                 videoReader->getVideoHeight(),
                                 videoReader->getPixelFormat(),
                                 currentFrameAsImage.getWidth(),
                                 currentFrameAsImage.getHeight (),
                                 AV_PIX_FMT_BGR0);
    }
}

FFmpegVideoReader* FFmpegVideoComponent::getVideoReader() const
{
    return videoReader.get();
}

//**************************************************************
//juce::AudioTransportSource ***********************************
//**************************************************************

void FFmpegVideoComponent::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
//    DBG("FFMpegVideoComponent: prepareToPlay, SR: " + juce::String(sampleRate) );
    readBuffer.setSize (2, samplesPerBlockExpected);
    //video reader does not need to be prepared, since it's source of the
    //transport source and gets prepared by it.
    if (transportSource)
        transportSource->prepareToPlay (samplesPerBlockExpected, sampleRate);

}

void FFmpegVideoComponent::releaseResources()
{
//    DBG("FFMpegVideoComponent::releaseResources()");
}

void FFmpegVideoComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto numInputChannels = videoReader->getNumberOfAudioChannels();

    //create this to assign read buffer
    juce::AudioSourceChannelInfo info (&readBuffer, bufferToFill.startSample, bufferToFill.numSamples);
    
    // get next audio block from source into read buffer
    transportSource->getNextAudioBlock (info);

    //copy read buffer to audio buffer of this AudioAppComponent
    if (numInputChannels > 0)
    {
        for (int i=0; i < bufferToFill.buffer->getNumChannels(); ++i) {

            bufferToFill.buffer->copyFrom (i, bufferToFill.startSample,
                                           readBuffer.getReadPointer (i % numInputChannels),
                                           bufferToFill.numSamples);
            if (bufferToFill.buffer->getNumChannels() == 2 &&
                readBuffer.getNumChannels() > 2) {
                // add center to left and right
                bufferToFill.buffer->addFrom (i, bufferToFill.startSample,
                                              readBuffer.getReadPointer (2),
                                              bufferToFill.numSamples, 0.7);
            }
        }
    }
    else
    {
        bufferToFill.clearActiveBufferRegion();
    }
}



//**************************************************************
//juce::VideoComponent mimic ***********************************
//**************************************************************


juce::Result FFmpegVideoComponent::load(const juce::File &file)
{
    transportSource->stop();
    if ( videoReader->loadVideoFile (file) )
    {
        return juce::Result::ok();
    }
    else
    {
        return juce::Result::fail("FFMpegVideoComponent: video could not be loaded");
    }
}

void FFmpegVideoComponent::closeVideo()
{
    if ( videoReader )
    {
        videoReader->closeVideoFile();
        currentAVFrame = nullptr;
        currentFrameAsImage = juce::Image();
    }
}

bool FFmpegVideoComponent::isVideoOpen() const
{
    return videoReader->isVideoOpen();
}

double FFmpegVideoComponent::getVideoDuration() const
{
    if (videoReader)
        return videoReader->getDuration();
    else
        return -1.0;
}

void FFmpegVideoComponent::setPlaySpeed(double newSpeed)
{
    //to change playback speed transport needs to be stopped and reset with new speed
    if ( newSpeed != playSpeed)
    {
        playSpeed = newSpeed;
        if (isVideoOpen())
        {
            if ( !isPaused )
                transportSource->stop();
            
            juce::int64 lastPos = videoReader->getNextReadPosition();
            transportSource->setSource (videoReader.get(), 0, nullptr,
                                        videoReader->getSampleRate() * playSpeed,
                                        videoReader->getNumberOfAudioChannels());
            videoReader->setNextReadPosition (lastPos);
            
            if( !isPaused )
                transportSource->start();
        }
    }
}

double FFmpegVideoComponent::getPlaySpeed() const
{
    return playSpeed;
}

void FFmpegVideoComponent::play()
{
    DBG("FFMpegVideoComponent::play() at " + StringHelper::convertSecondsToTimeString(getPlayPosition()));
    if ( isPaused && !isPlaying())
    {
        transportSource->start();
        isPaused = false;
        //invoke callback for onPlaybackStarted, this must be thread safe
        if ( onPlaybackStarted != nullptr )
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                onPlaybackStarted();
            else
                juce::MessageManager::callAsync(std::move(onPlaybackStarted));
        }
    }
}

void FFmpegVideoComponent::setPlayPosition(double newPositionSeconds)
{
    DBG("FFMpegVideoComponent::setPlayPosition(" + juce::String(newPositionSeconds) + ")");
    
    if (!isVideoOpen())
        return;

    if ( !isPaused )
        transportSource->stop();
    
    //set position directly in video reader since the the transport source does not compensate for playback speed
    videoReader->setNextReadPosition (newPositionSeconds * videoReader->getSampleRate());

    if (!isPaused)
        transportSource->start();
}

double FFmpegVideoComponent::getPlayPosition() const
{
    return transportSource->getCurrentPosition();
}

//**************************************************************
//juce::VideoComponent and  juce::AudioTransportSource *********
//**************************************************************

void FFmpegVideoComponent::stop()
{
    DBG("FFMpegVideoComponent::stop() at " + StringHelper::convertSecondsToTimeString(getPlayPosition()));
    if ( isPlaying() && !isPaused && isVideoOpen())
    {
        transportSource->stop();
        isPaused = true;
        //invoke callback for onPlaybackStopped, this must be thread safe
        if ( onPlaybackStopped != nullptr )
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                onPlaybackStopped();
            else
                juce::MessageManager::callAsync(std::move(onPlaybackStopped));
        }
    }
}

bool FFmpegVideoComponent::isPlaying() const
{
    if ( isVideoOpen() )
        return transportSource->isPlaying();
    else
        return false;
}

//**************************************************************
//***************** Callbacks **********************************
//**************************************************************


void FFmpegVideoComponent::timerCallback ()
{
    if (!frameWasPainted) {
//        DBG("Drawing Frame...");
        repaint ();
        frameWasPainted = true;
    }
}

void FFmpegVideoComponent::videoSizeChanged (const int width, const int height, const AVPixelFormat format)
{
    //This should only happen if another video with a different resolution is loaded
    resized();
    frameWasPainted = false;
}


void FFmpegVideoComponent::videoFileChanged (const juce::File& video)
{
    isPaused = true;
    DBG("FFMpegVideoComponent: Video file changed...");

    if (juce::AudioIODevice* device = deviceManager.getCurrentAudioDevice())
    {
        readBuffer.setSize (videoReader->getNumberOfAudioChannels(),
                            device->getCurrentBufferSizeSamples());
        
        //set video reader as source for transport source
        //this will also reposition the readers position, but for some reason sound will be ahead a bit
        transportSource->setSource(videoReader.get(), 0, nullptr,
                        videoReader->getSampleRate() * playSpeed,
                        videoReader->getNumberOfAudioChannels());
        
        //TODO: this should not be necessary, because transportSource->setSource(...) already calls
        //setPlayPosition(0.0), but for some reason, the first attempt to set the position results in
        //audio playback being ahead of image data.
        setPlayPosition(0.0);
    }
    else
    {
        readBuffer.setSize (videoReader->getNumberOfAudioChannels(), 0/*readBuffer.getNumSamples()*/);
        DBG("FFMpegVideoComponent: There is something is wrong with the audio device!");
    }
    
    resized ();
}

void FFmpegVideoComponent::displayNewFrame (const AVFrame* frame)
{
    double frameSeconds = static_cast<double>(frame->pts) / videoReader->getVideoTimeBase().den;
        
    //only update current frame if the last frame has been drawn
    if (frameWasPainted)
    {
        currentAVFrame = frame;
        frameWasPainted = false;
    }
    else
    {
        DBG ("FFMpegVideoComponent: Frame not updated yet: " + juce::String (frameSeconds) + " sec");
    }
}

void FFmpegVideoComponent::positionSecondsChanged (const double pts)
{

}

void FFmpegVideoComponent::videoEnded()
{
    DBG("FFMpegVideoComponent: video has ended...");
    //invoke callback for onPlaybackStopped, this must be thread safe
    if (onPlaybackStopped != nullptr )
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread())
            onPlaybackStopped();
        else
            juce::MessageManager::callAsync(std::move(onPlaybackStopped));
    }
}

