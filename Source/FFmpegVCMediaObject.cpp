#include "FFmpegVCMediaObject.h"

// TODO:
// - Video without audio needs to still play
// - Control flow for waiting for decode buffer sometimes trips because we seek at an unexpected time

FFmpegVCMediaObject::FFmpegVCMediaObject()
    : transportSource(std::make_unique<juce::AudioTransportSource>())
    , videoReader(std::make_unique<FFmpegMediaReader>(192000, 102))  // Same buffer sizes as in FFmpegVideoComponent
    , currentAVFrame(nullptr)
    , playSpeed(1.0)
    , isPaused(true)
    , blockSize(0)
    , sampleRate(0)
{
    if (videoReader)
        videoReader->addVideoListener(this);
    
    transportSource->setSource(videoReader.get(), 0, nullptr);
}

FFmpegVCMediaObject::~FFmpegVCMediaObject()
{
    if (videoReader)
        videoReader->removeVideoListener(this);
    
    transportSource->setSource(nullptr);
    releaseResources();
}

void FFmpegVCMediaObject::start()
{
    DBG("FFmpegVCMediaObject::start() at " + juce::String(getPlayPosition()));
    if (isPaused && !isPlaying())
    {
        transportSource->start();
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
    DBG("FFmpegVCMediaObject::stop() at " + juce::String(getPlayPosition()));
    if (isPlaying() && !isPaused && isVideoOpen())
    {
        transportSource->stop();
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
    return transportSource->isPlaying();
}

void FFmpegVCMediaObject::releaseResources()
{
    if (videoReader)
        videoReader->closeMediaFile();
    
    transportSource->releaseResources();
}

int FFmpegVCMediaObject::getNumChannels()
{
    return videoReader ? videoReader->getNumberOfAudioChannels() : 0;
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
    return videoReader && videoReader->getVideoStreamIndex() >= 0;
}

bool FFmpegVCMediaObject::hasAudio()
{
    return videoReader && videoReader->getNumberOfAudioChannels() > 0;
}

bool FFmpegVCMediaObject::clipLoaded()
{
    return videoReader && videoReader->isMediaOpen();
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
    return videoReader ? videoReader->getNextReadPosition() : 0;
}

juce::int64 FFmpegVCMediaObject::getAudioSampleRate()
{
    return videoReader ? videoReader->getSampleRate() : 0;
}

juce::int64 FFmpegVCMediaObject::getVideoFrameRate()
{
    return videoReader ? videoReader->getFramesPerSecond() : 0;
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
    return videoReader ? videoReader->getDuration() : 0.0;
}

double FFmpegVCMediaObject::getCurrentTimelinePositionInSeconds()
{
    return transportSource->getCurrentPosition();
}

void FFmpegVCMediaObject::setTimelinePosition(juce::int64 timecodeInSamples)
{
    if (videoReader)
        videoReader->setNextReadPosition(timecodeInSamples);
}

void FFmpegVCMediaObject::setCurrentTimelinePositionInSeconds(double newPositionInSeconds)
{
    transportSource->setPosition(newPositionInSeconds);
}

void FFmpegVCMediaObject::setPositionNormalized(double newPositionNormalized)
{
    transportSource->setPosition(newPositionNormalized * getLengthInSeconds());
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

    if (videoReader->loadMediaFile(file))
    {
        // Ensure prepareToPlay is called with the correct sample rate and block size
        videoReader->prepareToPlay(blockSize, sampleRate);
        
        transportSource->setSource(videoReader.get(), 0, nullptr,
                                   videoReader->getSampleRate() * playSpeed,
                                   videoReader->getNumberOfAudioChannels());
        currentMediaFilePath = juce::URL(file);
        return juce::Result::ok();
    }
    return juce::Result::fail("Failed to load file");
}
void FFmpegVCMediaObject::closeVideo()
{
    if (videoReader)
    {
        videoReader->closeMediaFile();
        currentAVFrame = nullptr;
        currentFrameAsImage = juce::Image();
    }
}

juce::Image& FFmpegVCMediaObject::getFrame(double currentTimeInSeconds)
{
    if (currentAVFrame)
    {
        videoScaler.convertFrameToImage(currentFrameAsImage, currentAVFrame);
    }
    return currentFrameAsImage;
}

bool FFmpegVCMediaObject::isVideoOpen() const
{
    return videoReader && videoReader->isMediaOpen();
}

double FFmpegVCMediaObject::getVideoDuration() const
{
    return videoReader ? videoReader->getDuration() : 0.0;
}

void FFmpegVCMediaObject::setPlaySpeed(double newSpeed)
{
    if (newSpeed != playSpeed)
    {
        playSpeed = newSpeed;
        if (isVideoOpen())
        {
            bool wasPlaying = isPlaying();
            if (wasPlaying)
                transportSource->stop();
            
            juce::int64 lastPos = videoReader->getNextReadPosition();
            transportSource->setSource(videoReader.get(), 0, nullptr,
                                       videoReader->getSampleRate() * playSpeed,
                                       videoReader->getNumberOfAudioChannels());
            videoReader->setNextReadPosition(lastPos);
            
            if (wasPlaying)
                transportSource->start();
        }
    }
}

double FFmpegVCMediaObject::getPlaySpeed() const
{
    return playSpeed;
}

void FFmpegVCMediaObject::setPlayPosition(double newPositionSeconds)
{
    if (!isVideoOpen())
        return;

    bool wasPlaying = isPlaying();
    if (wasPlaying)
        transportSource->stop();
    
    videoReader->setNextReadPosition(newPositionSeconds * videoReader->getSampleRate());

    if (wasPlaying)
        transportSource->start();
}

double FFmpegVCMediaObject::getPlayPosition() const
{
    return transportSource->getCurrentPosition();
}

void FFmpegVCMediaObject::videoFileChanged(const juce::File& newSource)
{
    // This method can be used to perform any necessary setup when the video file changes
}

void FFmpegVCMediaObject::videoSizeChanged(const int width, const int height, const AVPixelFormat format)
{
    if (width > 0 && height > 0) 
    {
        videoScaler.setupScaler(width, height, format, width, height, AV_PIX_FMT_BGR0);
        currentFrameAsImage = juce::Image(juce::Image::PixelFormat::ARGB, width, height, true);
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
