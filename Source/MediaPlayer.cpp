#include "MediaPlayer.h"

//==============================================================================
MediaPlayer::MediaPlayer()
{
    // Initialize temp audio buffer
    tempAudioBuffer.setSize(2, 4096);
    
    // Configure VLC for headless video processing
    configureVLCForHeadlessVideo();
}

MediaPlayer::~MediaPlayer()
{
    // Base class destructor handles cleanup
}

//==============================================================================
// Legacy FFmpegVCMediaObject compatibility methods
bool MediaPlayer::isOpen() const
{
    // For image files, we're always "open" if the image is valid
    if (isImageFile)
    {
        std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(imageFileMutex));
        bool result = imageFileFrame.isValid();
        //DBG("MediaPlayer::isOpen - Image file, result: " + juce::String(result ? "true" : "false"));
        return result;
    }
    
    // For video/audio files, check VLC state
    double duration = getTotalDuration();
    bool result = duration > 0.0;
    //DBG("MediaPlayer::isOpen - Video/audio file, duration: " + juce::String(duration) + ", result: " + juce::String(result ? "true" : "false"));
    return result;
}

int MediaPlayer::getNumChannels() const
{
    // Default to stereo, could be enhanced to get actual channel count from VLC
    // VLCMediaPlayer doesn't expose getAudioChannelCount directly
    return 2;
}

void MediaPlayer::prepareToPlay(int sessionBlockSize, int /*sessionSampleRate*/)
{
    // Resize temp buffer for audio processing
    tempAudioBuffer.setSize(getNumChannels(), sessionBlockSize);
    tempAudioBuffer.clear();
}

void MediaPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
{
    // Clear output first
    info.clearActiveBufferRegion();
    
    if (!isPlaying() || !hasAudio())
        return;
    
    // For now, we rely on the base VLCMediaPlayer's audio handling
    // This could be enhanced to provide more direct audio access
    
    // Apply gain if needed
    float gain = audioGain.load();
    if (gain != 1.0f && info.buffer != nullptr)
    {
        for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
        {
            juce::FloatVectorOperations::multiply(info.buffer->getWritePointer(channel, info.startSample),
                                                 gain, info.numSamples);
        }
    }
}

void MediaPlayer::releaseResources()
{
    // Base class handles resource cleanup
}

juce::URL MediaPlayer::getMediaFilePath() const
{
    return currentMediaFilePath;
}

int64_t MediaPlayer::getVideoFrameRate() const
{
    return static_cast<int64_t>(videoFrameRate.load());
}

juce::Image& MediaPlayer::getFrame()
{
    // If this is an image file, return the loaded image
    if (isImageFile)
    {
        std::lock_guard<std::mutex> lock(imageFileMutex);
        return imageFileFrame;
    }
    
    // For video files, get the actual video frame from VLC base class
    if (VLCMediaPlayer::hasVideo())
    {
        // Get the current video frame from VLC
        // This accesses the base class's currentVideoFrame which is updated by VLC callbacks
        juce::Image vlcFrame = getCurrentVideoFrame();
        
        if (vlcFrame.isValid())
        {
            // Store a copy for returning by reference
            std::lock_guard<std::mutex> lock(imageFileMutex);
            imageFileFrame = vlcFrame;
            return imageFileFrame;
        }
        else
        {
            // Fallback: create a simple placeholder if no frame is available yet
            auto videoSize = getVideoSize();
            int width = videoSize.getWidth();
            int height = videoSize.getHeight();
            
            if (width <= 0 || height <= 0)
            {
                width = 1920;
                height = 1080;
            }
            
            std::lock_guard<std::mutex> lock(imageFileMutex);
            if (!imageFileFrame.isValid() || 
                imageFileFrame.getWidth() != width || 
                imageFileFrame.getHeight() != height)
            {
                imageFileFrame = juce::Image(juce::Image::ARGB, width, height, true);
                
                juce::Graphics g(imageFileFrame);
                g.fillAll(juce::Colours::black);
                
                g.setColour(juce::Colours::white);
                g.setFont(juce::Font(16.0f));
                g.drawText("Loading video...", imageFileFrame.getBounds(), 
                          juce::Justification::centred, true);
            }
            return imageFileFrame;
        }
    }
    
    // No valid frame available
    std::lock_guard<std::mutex> lock(imageFileMutex);
    return imageFileFrame;
}

double MediaPlayer::getLengthInSeconds() const
{
    // For image files, return a fixed duration (e.g., 10 seconds for UI purposes)
    if (isImageFile)
    {
        return 10.0;
    }
    
    // For video/audio files, use VLC duration
    return getTotalDuration();
}

double MediaPlayer::getPositionInSeconds() const
{
    // For image files, return 0 (static image)
    if (isImageFile)
    {
        return 0.0;
    }
    
    // For video/audio files, use VLC position
    return getCurrentTime();
}

void MediaPlayer::setPosition(double newPositionInSeconds)
{
    DBG("MediaPlayer::setPosition - Seeking to time: " + juce::String(newPositionInSeconds) + "s");
    
    if (isImageFile)
    {
        DBG("MediaPlayer::setPosition - Ignoring seek for image file");
        return;
    }
    
    double duration = getTotalDuration();
    if (duration > 0.0 && newPositionInSeconds >= 0.0 && newPositionInSeconds <= duration)
    {
        DBG("MediaPlayer::setPosition - Calling VLC seekToTime(" + juce::String(newPositionInSeconds) + ")");
        seekToTime(newPositionInSeconds);
    }
    else
    {
        DBG("MediaPlayer::setPosition - Invalid seek request. Duration: " + juce::String(duration) + 
            "s, Requested: " + juce::String(newPositionInSeconds) + "s");
    }
}

void MediaPlayer::setPositionNormalized(double newPositionNormalized)
{
    DBG("MediaPlayer::setPositionNormalized - Seeking to normalized position: " + juce::String(newPositionNormalized));
    
    double duration = getTotalDuration();
    if (duration > 0.0)
    {
        double targetTimeInSeconds = newPositionNormalized * duration;
        DBG("MediaPlayer::setPositionNormalized - Duration: " + juce::String(duration) + 
            "s, Target time: " + juce::String(targetTimeInSeconds) + "s");
        setPosition(targetTimeInSeconds);
    }
    else
    {
        DBG("MediaPlayer::setPositionNormalized - Cannot seek, invalid duration: " + juce::String(duration));
    }
}

void MediaPlayer::setPlaySpeed(double newSpeed)
{
    playbackSpeed = newSpeed;
    
    // VLCMediaPlayer doesn't expose setRate directly, so we'll store it
    // This could be enhanced if the base class exposes rate control
}

double MediaPlayer::getPlaySpeed() const
{
    return playbackSpeed.load();
}

void MediaPlayer::setGain(float newGain)
{
    audioGain = newGain;
}

float MediaPlayer::getGain() const
{
    return audioGain.load();
}

bool MediaPlayer::open(juce::URL filepath)
{
    if (filepath.isLocalFile())
    {
        const auto file = filepath.getLocalFile();
        currentMediaFilePath = filepath;
        
        DBG("MediaPlayer::open - Opening file: " + file.getFullPathName());
        
        // Check if this is an image file
        juce::String extension = file.getFileExtension().toLowerCase();
        if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" || 
            extension == ".gif" || extension == ".tif" || extension == ".tiff" ||
            extension == ".webp" || extension == ".svg")
        {
            // Handle as image file - load directly into JUCE Image
            DBG("MediaPlayer::open - Detected image file");
            return loadImageFile(file);
        }
        else
        {
            // Handle as video/audio file via VLC
            DBG("MediaPlayer::open - Opening as video/audio file via VLC");
            juce::String error;
            bool result = VLCMediaPlayer::open(file, &error);
            if (result)
            {
                DBG("MediaPlayer::open - VLC open successful");
                
                // Wait a bit for media parsing to complete
                // VLC needs time to analyze the file and determine track information
                for (int attempts = 0; attempts < 50; ++attempts)  // Wait up to 500ms
                {
                    juce::Thread::sleep(10);
                    
                    // Check if we have valid duration (indicates parsing is complete)
                    double duration = getTotalDuration();
                    bool hasVideo = VLCMediaPlayer::hasVideo();
                    bool hasAudio = VLCMediaPlayer::hasAudio();
                    
                    DBG("MediaPlayer::open - Attempt " + juce::String(attempts + 1) + 
                        " - Duration: " + juce::String(duration) + 
                        ", hasVideo: " + juce::String(hasVideo ? "true" : "false") + 
                        ", hasAudio: " + juce::String(hasAudio ? "true" : "false"));
                    
                    if (duration > 0.0 || hasVideo || hasAudio)
                    {
                        DBG("MediaPlayer::open - Media parsing completed successfully");
                        break;
                    }
                }
                
                DBG("MediaPlayer::open - Final state - hasVideo: " + juce::String(VLCMediaPlayer::hasVideo() ? "true" : "false"));
                DBG("MediaPlayer::open - Final state - hasAudio: " + juce::String(VLCMediaPlayer::hasAudio() ? "true" : "false"));
                DBG("MediaPlayer::open - Final state - getTotalDuration: " + juce::String(getTotalDuration()));
            }
            else
            {
                DBG("MediaPlayer::open - Failed to open URL: " + error);
            }
            return result;
        }
    }
    return false;
}

juce::Result MediaPlayer::load(const juce::File& file)
{
    juce::String error;
    if (VLCMediaPlayer::open(file, &error))
    {
        currentMediaFilePath = juce::URL(file);
        
        // Notify playback started callback if set
        if (onPlaybackStarted != nullptr)
        {
            juce::MessageManager::callAsync(onPlaybackStarted);
        }
        
        return juce::Result::ok();
    }
    return juce::Result::fail(error);
}

void MediaPlayer::close()
{
    // Reset image file state
    isImageFile = false;
    
    // Clear the local image frame
    {
        std::lock_guard<std::mutex> lock(imageFileMutex);
        imageFileFrame = juce::Image();
    }
    
    // Call base class close (this also clears the VLC video frame)
    VLCMediaPlayer::close();
}

void MediaPlayer::setOffsetSeconds(double seconds)
{
    offsetSeconds = seconds;
    // This could be used to adjust playback timing
}

void MediaPlayer::videoEnded()
{
    setPosition(0);
    stop();
    
    // Notify playback stopped callback if set
    if (onPlaybackStopped != nullptr)
    {
        juce::MessageManager::callAsync(onPlaybackStopped);
    }
}

//==============================================================================
// Internal methods
void MediaPlayer::refreshVideoFrame()
{
    // Force recreation of video frame for dynamic updates
    if (!isImageFile && VLCMediaPlayer::hasVideo())
    {
        std::lock_guard<std::mutex> lock(imageFileMutex);
        imageFileFrame = juce::Image(); // Clear cached frame to force refresh from VLC
    }
}

void MediaPlayer::updateVideoFrame()
{
    // This method could be enhanced to capture actual video frames from VLC
    // For now, it's a placeholder for future implementation
}

bool MediaPlayer::loadImageFile(const juce::File& imageFile)
{
    std::lock_guard<std::mutex> lock(imageFileMutex);
    
    // Load image directly using JUCE
    imageFileFrame = juce::ImageFileFormat::loadFrom(imageFile);
    
    if (imageFileFrame.isValid())
    {
        isImageFile = true;
        DBG("Successfully loaded image file: " + imageFile.getFullPathName());
        return true;
    }
    else
    {
        DBG("Failed to load image file: " + imageFile.getFullPathName());
        return false;
    }
}

void MediaPlayer::configureVLCForHeadlessVideo()
{
    // Configure VLC to not create its own video output window
    // This prevents the "No drawable-nsobject found!" errors
    // We'll capture frames manually for JUCE Image display
    
    // Note: This would ideally be done through VLC options, but since we're extending
    // VLCMediaPlayer, we need to work with what's available
    // The actual implementation would depend on the VLCMediaPlayer interface
}

void MediaPlayer::notifyPlaybackCallbacks()
{
    // Handle playback state change notifications
    if (isPlaying() && onPlaybackStarted != nullptr)
    {
        juce::MessageManager::callAsync(onPlaybackStarted);
    }
    else if (!isPlaying() && onPlaybackStopped != nullptr)
    {
        juce::MessageManager::callAsync(onPlaybackStopped);
    }
}
