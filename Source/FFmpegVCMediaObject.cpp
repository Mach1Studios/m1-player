#include "FFmpegVCMediaObject.h"

// TODO:
// - Video without audio needs to still play
// - Control flow for waiting for decode buffer sometimes trips because we seek at an unexpected time

FFmpegVCMediaObject::FFmpegVCMediaObject()
    : transportSource(std::make_unique<juce::AudioTransportSource>())
    , mediaReader(std::make_unique<FFmpegMediaReader>(192000, 102))  // Same buffer sizes as in FFmpegVideoComponent
    , currentAVFrame(nullptr)
    , playSpeed(1.0)
    , isPaused(true)
    , blockSize(0)
    , sampleRate(0)
{
    if (mediaReader)
        mediaReader->addVideoListener(this);
    
    transportSource->setSource(mediaReader.get(), 0, nullptr);
}

FFmpegVCMediaObject::~FFmpegVCMediaObject()
{
    if (mediaReader)
        mediaReader->removeVideoListener(this);
    
    transportSource->setSource(nullptr);
    releaseResources();
}

void FFmpegVCMediaObject::start()
{
    DBG("FFmpegVCMediaObject::start() at " + juce::String(getPositionInSeconds()));
    if (isPaused && !isPlaying())
    {
        if (mediaReader->getNumberOfAudioChannels() > 0)
        {
            // run as usual if there is audio to decode
            transportSource->start();
        }
        else
        {
            // No audio, start decoding thread if not already running
            if (!mediaReader->isThreadRunning())
            {
                mediaReader->startThread();
            }
            else
            {
                mediaReader->continueDecoding();
            }
        }
        
        isPaused = false;
        
        // Invoke callback for onPlaybackStarted, this must be thread safe
        if (onPlaybackStarted != nullptr)
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                onPlaybackStarted();
            else
                juce::MessageManager::callAsync(std::move(onPlaybackStarted));
        }
    }
}

void FFmpegVCMediaObject::stop()
{
    DBG("FFmpegVCMediaObject::stop() at " + juce::String(getPositionInSeconds()));
    
    if (isPlaying() && !isPaused && isOpen())
    {
        if (mediaReader->getNumberOfAudioChannels() > 0)
        {
            transportSource->stop();
        }
        else
        {
            // No audio, stop decoding thread and timer
            if (mediaReader->isThreadRunning())
            {
                mediaReader->stopThread(1000);
            }
        }

        isPaused = true;

        // Invoke callback for onPlaybackStopped, this must be thread safe
        if (onPlaybackStopped != nullptr)
        {
            if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                onPlaybackStopped();
            else
                juce::MessageManager::callAsync(std::move(onPlaybackStopped));
        }
    }
}

bool FFmpegVCMediaObject::isPlaying()
{
    if (mediaReader->getNumberOfAudioChannels() > 0)
    {
        // track via audiostream
        return transportSource->isPlaying();
    }
    else
    {
        return mediaReader->isThreadRunning() && !isPaused;
    }
}

void FFmpegVCMediaObject::releaseResources()
{
    if (mediaReader)
        mediaReader->closeMediaFile();
    
    transportSource->releaseResources();
}

int FFmpegVCMediaObject::getNumChannels()
{
    return mediaReader ? mediaReader->getNumberOfAudioChannels() : 0;
}

void FFmpegVCMediaObject::prepareToPlay(int sessionBlockSize, int sessionSampleRate)
{
    blockSize = sessionBlockSize;
    sampleRate = sessionSampleRate;
    readBuffer.setSize(2, sessionBlockSize);
    transportSource->prepareToPlay(sessionBlockSize, sessionSampleRate);
}

bool FFmpegVCMediaObject::hasVideo()
{
    return mediaReader && mediaReader->getVideoStreamIndex() >= 0;
}

bool FFmpegVCMediaObject::hasAudio()
{
    return mediaReader && mediaReader->getNumberOfAudioChannels() > 0;
}

bool FFmpegVCMediaObject::clipLoaded()
{
    return mediaReader && mediaReader->isMediaOpen();
}

void FFmpegVCMediaObject::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    transportSource->getNextAudioBlock(info);
}

juce::URL FFmpegVCMediaObject::getMediaFilePath()
{
    return currentMediaFilePath;
}

juce::int64 FFmpegVCMediaObject::getNextReadPositionInSamples()
{
    return mediaReader ? mediaReader->getNextReadPosition() : 0;
}

juce::int64 FFmpegVCMediaObject::getAudioSampleRate()
{
    return mediaReader ? mediaReader->getSampleRate() : 0;
}

juce::int64 FFmpegVCMediaObject::getVideoFrameRate()
{
    return mediaReader ? mediaReader->getFramesPerSecond() : 0;
}

void FFmpegVCMediaObject::setGain(float newGain)
{
    transportSource->setGain(newGain);
}

float FFmpegVCMediaObject::getGain()
{
    return transportSource->getGain();
}

double FFmpegVCMediaObject::getLengthInSeconds()
{
    return mediaReader ? mediaReader->getDuration() : 0.0;
}

double FFmpegVCMediaObject::getPositionInSeconds()
{
    if (hasAudio())
    {
        return transportSource->getCurrentPosition();
    }
    else
    {
        return mediaReader->getCurrentPositionSeconds();
    }
}

void FFmpegVCMediaObject::setPosition(double newPositionInSeconds)
{
    if (!isOpen())
        return;
    
    if (hasAudio())
    {
        transportSource->setPosition(newPositionInSeconds);
    }
    else
    {
        mediaReader->setPositionSeconds(newPositionInSeconds);
    }
}

void FFmpegVCMediaObject::setPositionNormalized(double newPositionNormalized)
{
    setPosition(newPositionNormalized * getLengthInSeconds());
}

bool FFmpegVCMediaObject::open(juce::URL url)
{
    if (url.isLocalFile())
    {
        const auto file = url.getLocalFile();
        load(file);
        currentMediaFilePath = url;
        return true;
    }
    return false;
}

juce::Result FFmpegVCMediaObject::load(const juce::File& file)
{
    transportSource->stop();
    transportSource->setSource(nullptr); // Reset the source

    // Reset state variables
    currentAVFrame = nullptr;
    currentFrameAsImage = juce::Image();
    playSpeed = 1.0;
    isPaused = true;
    readBuffer.clear();

    if (mediaReader->loadMediaFile(file))
    {
        // Ensure blockSize and sampleRate are initialized
        if (blockSize > 0 && sampleRate > 0)
        {
            // Ensure prepareToPlay is called with the correct sample rate and block size
            mediaReader->prepareToPlay(blockSize, sampleRate);
        }
        else
        {
            // Set default values or handle the error
            DBG("blockSize or sampleRate not initialized properly.");
            return juce::Result::fail("Invalid blockSize or sampleRate");
        }

        transportSource->setSource(mediaReader.get(), 0, nullptr,
                                   mediaReader->getSampleRate() * playSpeed,
                                   mediaReader->getNumberOfAudioChannels());

        currentMediaFilePath = juce::URL(file);

        // Check if the new file has video
        if (mediaReader->getVideoStreamIndex() < 0)
        {
            // No video, clear current frame
            currentAVFrame = nullptr;
            currentFrameAsImage = juce::Image();
        }
        else
        {
            // resize because we have a video stream
            videoSizeChanged(mediaReader->getVideoWidth(), mediaReader->getVideoHeight(), mediaReader->getPixelFormat());
        }
        
        // Start the decoding thread if there's no audio
        if (mediaReader->getNumberOfAudioChannels() <= 0)
        {
            if (!mediaReader->isThreadRunning())
            {
                mediaReader->startThread();
            }
        }

        return juce::Result::ok();
    }
    return juce::Result::fail("Failed to load file");
}

void FFmpegVCMediaObject::closeMedia()
{
    if (mediaReader)
    {
        mediaReader->closeMediaFile();
        currentAVFrame = nullptr;
        currentFrameAsImage = juce::Image();
    }
}

juce::Image& FFmpegVCMediaObject::getFrame()
{
    if (currentAVFrame)
    {
        videoScaler.convertFrameToImage(currentFrameAsImage, currentAVFrame);
    }
    return currentFrameAsImage;
}

bool FFmpegVCMediaObject::isOpen() const
{
    return mediaReader && mediaReader->isMediaOpen();
}

void FFmpegVCMediaObject::setPlaySpeed(double newSpeed)
{
    if (newSpeed != playSpeed)
    {
        playSpeed = newSpeed;
        if (isOpen())
        {
            bool wasPlaying = isPlaying();
            if (wasPlaying)
                transportSource->stop();
            
            juce::int64 lastPos = mediaReader->getNextReadPosition();
            transportSource->setSource(mediaReader.get(), 0, nullptr,
                                       mediaReader->getSampleRate() * playSpeed,
                                       mediaReader->getNumberOfAudioChannels());
            
            if (wasPlaying)
                transportSource->start();
            
            mediaReader->setNextReadPosition(lastPos);
        }
    }
}

double FFmpegVCMediaObject::getPlaySpeed() const
{
    return playSpeed;
}

void FFmpegVCMediaObject::videoFileChanged(const juce::File& newSource)
{
    // This method can be used to perform any necessary setup when the video file changes
}

void FFmpegVCMediaObject::videoSizeChanged(const int width, const int height, const AVPixelFormat format)
{
    double aspectRatio = mediaReader->getVideoAspectRatio() * mediaReader->getPixelAspectRatio();
    int w = width;
    int h = height;
    
    if (aspectRatio > 0.0)
    {
        if (width / height > aspectRatio)
            w = height * aspectRatio;
        else
            h = width / aspectRatio;

        currentFrameAsImage = juce::Image(juce::Image::PixelFormat::ARGB, w, h, true);
        videoScaler.setupScaler(width, height, format, currentFrameAsImage.getWidth(), currentFrameAsImage.getHeight(), AV_PIX_FMT_BGR0);
    }
    else
    {
        // Clear the current frame
        currentFrameAsImage = juce::Image();
    }
}

void FFmpegVCMediaObject::displayNewFrame(const AVFrame* frame)
{
    currentAVFrame = frame;
}

void FFmpegVCMediaObject::positionSecondsChanged(const double position)
{
    // This method can be used to update any UI elements showing the current position
}

void FFmpegVCMediaObject::videoEnded()
{
    stop();
}
