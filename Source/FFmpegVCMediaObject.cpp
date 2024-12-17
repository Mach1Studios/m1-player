#include "FFmpegVCMediaObject.h"

// TODO:
// - Video without audio needs to still play
// - Control flow for waiting for decode buffer sometimes trips because we seek at an unexpected time

FFmpegVCMediaObject::FFmpegVCMediaObject()
    : transportSource(std::make_unique<juce::AudioTransportSource>())
    , mediaReader(std::make_unique<FFmpegMediaReader>(48000 * 10, 30 * 10))  // Same buffer sizes as in FFmpegVideoComponent
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

void FFmpegVCMediaObject::timerCallback()
{
    if (!isOpen())
        return;

    // Get the next video frame
    const AVFrame* frame = mediaReader->getNextVideoFrameWithOffset(offsetSeconds);

    if (frame)
    {
        DBG("New frame received in timer callback");
        displayNewFrame(frame);
    }
    else if (mediaReader->isEndOfFile())
    {
        DBG("End of file reached in timer callback");
        stop();
    }
}

void FFmpegVCMediaObject::start()
{
    if (isPaused && !isPlaying())
    {
        // Start video timer first
        if (hasVideo())
        {
//            DBG("Starting video timer at " + juce::String(getVideoFrameRate()) + " Hz");
            startTimerHz(static_cast<int>(getVideoFrameRate()));
        }

        // Then handle audio/decoding
        if (hasAudio())
        {
            DBG("Starting audio transport");
            transportSource->start();
        }
        
        if (!mediaReader->isThreadRunning())
        {
            DBG("Starting media reader thread");
            mediaReader->startThread();
        }
        
        mediaReader->continueDecoding();
        isPaused = false;
    }
}

void FFmpegVCMediaObject::stop()
{
    DBG("FFmpegVCMediaObject::stop() at " + juce::String(getPositionInSeconds()));
    
    if (isPlaying() && !isPaused && isOpen())
    {
        transportSource->stop();
        
        // Always stop the video timer if it's running
        if (isTimerRunning())
        {
            stopTimer();
        }
        
        // Stop decoding thread if no audio
        if (!hasAudio())
        {
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
    if (hasAudio())
    {
        return transportSource->isPlaying();
    }
    else
    {
        return isTimerRunning() && !isPaused;
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
    return mediaReader ? (juce::int64)mediaReader->getSampleRate() : mediaReader->DEFAULT_SAMPLE_RATE;
}

juce::int64 FFmpegVCMediaObject::getVideoFrameRate()
{
    return mediaReader ? (juce::int64)mediaReader->getFramesPerSecond() : mediaReader->DEFAULT_FRAMERATE;
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
    return mediaReader->getCurrentPositionSeconds();
}

int FFmpegVCMediaObject::getSamplerateLegacy() {
    return mediaReader->getSampleRate();
}

void FFmpegVCMediaObject::setOffsetSeconds(double seconds) {
    offsetSeconds = seconds;
}

void FFmpegVCMediaObject::setPosition(double newPositionInSeconds)
{
    if (!isOpen())
        return;

    bool wasPlaying = isPlaying();
//    if (wasPlaying)
//        transportSource->stop();
    
    //set position directly in media reader since the the transport source does not compensate for playback speed
    offsetSeconds = 0;
    mediaReader->setNextReadPosition (newPositionInSeconds * mediaReader->getSampleRate());

//    if (wasPlaying)
//        transportSource->start();
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
    DBG("Loading new file: " + file.getFullPathName());
    
    // Stop playback and clear existing resources
    stop();
    transportSource->setSource(nullptr);
    mediaReader->closeMediaFile();

    // Reset state variables
    currentAVFrame = nullptr;
    currentFrameAsImage = juce::Image();
    playSpeed = 1.0;
    isPaused = true;
    offsetSeconds = 0;
    readBuffer.clear();
    stopTimer(); // Ensure timer is stopped

    // Reset video scaler
//    videoScaler.releaseScaler();

    if (mediaReader->loadMediaFile(file))
    {
        int _videoExists = hasVideo();
        int _audioExists = hasAudio();
        DBG("File loaded successfully. Has video: " + juce::String(_videoExists) +
            ", Has audio: " + juce::String(_audioExists));

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

        currentMediaFilePath = juce::URL(file);

        // Check if the new file has video
        if (hasVideo())
        {
            DBG("Setting up video pipeline. Width: " + juce::String(mediaReader->getVideoWidth()) + 
                ", Height: " + juce::String(mediaReader->getVideoHeight()));
            
            // Only call videoSizeChanged once
            videoSizeChanged(mediaReader->getVideoWidth(), 
                           mediaReader->getVideoHeight(), 
                           mediaReader->getPixelFormat());
            
            // Don't call videoSizeChanged again, just verify the setup
            if (!currentFrameAsImage.isValid() /* || !videoScaler.isValid() */)
            {
                DBG("Error: Video pipeline initialization failed");
                return juce::Result::fail("Video pipeline initialization failed");
            }
        }

        // Set up audio pipeline
        if (hasAudio())
        {
            transportSource->setSource(mediaReader.get(), 0, nullptr,
                                    mediaReader->getSampleRate() * playSpeed,
                                    mediaReader->getNumberOfAudioChannels());
        }
        else
        {
            transportSource->setSource(nullptr);

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
    if (currentAVFrame && currentFrameAsImage.isValid())
    {
        videoScaler.convertFrameToImage(currentFrameAsImage, currentAVFrame);
    }
    else if (currentAVFrame)
    {
        DBG("Warning: Have frame but image is invalid in getFrame()");
    }
    return currentFrameAsImage;
}

bool FFmpegVCMediaObject::isOpen() const
{
    return mediaReader && mediaReader->isMediaOpen();
}

void FFmpegVCMediaObject::setPlaySpeed(double newSpeed)
{
    DBG ("SET PLAY SPEED CALLED " + std::to_string(newSpeed));
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
    DBG("Video size changed - Width: " + juce::String(width) + 
        ", Height: " + juce::String(height) + 
        ", Format: " + juce::String(format));
        
    // Only proceed if we have valid dimensions
    if (width <= 0 || height <= 0)
    {
        DBG("Invalid dimensions, clearing frame image");
        currentFrameAsImage = juce::Image();
        return;
    }

    double aspectRatio = mediaReader->getVideoAspectRatio() * mediaReader->getPixelAspectRatio();
    if (aspectRatio <= 0.0) aspectRatio = static_cast<double>(width) / height;
    
    int w = width;
    int h = height;
    
    if (width / height > aspectRatio)
        w = height * aspectRatio;
    else
        h = width / aspectRatio;

    DBG("Creating new frame image with dimensions: " + juce::String(w) + "x" + juce::String(h));
    currentFrameAsImage = juce::Image(juce::Image::PixelFormat::ARGB, w, h, true);
    
    // Ensure we're using the correct pixel format
    AVPixelFormat srcFormat = (format == AV_PIX_FMT_NONE) ? AV_PIX_FMT_YUV420P : format;
    
    DBG("Setting up scaler - Source format: " + juce::String(av_get_pix_fmt_name(srcFormat)) + 
        " -> BGR0");
    
    videoScaler.setupScaler(width, height, srcFormat,
                           w, h, AV_PIX_FMT_BGR0);
}

void FFmpegVCMediaObject::displayNewFrame(const AVFrame* frame)
{
    DBG("New frame received: " + juce::String(frame != nullptr ? "valid" : "null"));
    if (frame && !currentFrameAsImage.isValid())
    {
        DBG("Warning: Received frame but currentFrameAsImage is invalid!");
        // Attempt to reinitialize the image
        videoSizeChanged(mediaReader->getVideoWidth(), 
                        mediaReader->getVideoHeight(), 
                        mediaReader->getPixelFormat());
    }
    currentAVFrame = frame;
}

void FFmpegVCMediaObject::positionSecondsChanged(const double position)
{
    // This method can be used to update any UI elements showing the current position
}
