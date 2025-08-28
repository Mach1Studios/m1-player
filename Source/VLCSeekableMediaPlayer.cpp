#include "VLCSeekableMediaPlayer.h"

//==============================================================================
VLCSeekableMediaPlayer::VLCSeekableMediaPlayer()
{
    // Initialize temp audio buffer
    tempAudioBuffer.setSize(2, 4096);
}

VLCSeekableMediaPlayer::~VLCSeekableMediaPlayer()
{
    // Base class destructor handles cleanup
}

//==============================================================================
// Legacy FFmpegVCMediaObject compatibility methods
bool VLCSeekableMediaPlayer::isOpen() const
{
    return getTotalDuration() > 0.0;
}

int VLCSeekableMediaPlayer::getNumChannels() const
{
    // Default to stereo, could be enhanced to get actual channel count from VLC
    // VLCMediaPlayer doesn't expose getAudioChannelCount directly
    return 2;
}

void VLCSeekableMediaPlayer::prepareToPlay(int sessionBlockSize, int /*sessionSampleRate*/)
{
    // Resize temp buffer for audio processing
    tempAudioBuffer.setSize(getNumChannels(), sessionBlockSize);
    tempAudioBuffer.clear();
}

void VLCSeekableMediaPlayer::getNextAudioBlock(const juce::AudioSourceChannelInfo& info)
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

void VLCSeekableMediaPlayer::releaseResources()
{
    // Base class handles resource cleanup
}

juce::URL VLCSeekableMediaPlayer::getMediaFilePath() const
{
    return currentMediaFilePath;
}

int64_t VLCSeekableMediaPlayer::getVideoFrameRate() const
{
    return static_cast<int64_t>(videoFrameRate.load());
}

juce::Image& VLCSeekableMediaPlayer::getFrame()
{
    std::lock_guard<std::mutex> lock(videoFrameMutex);
    
    // For now, return a placeholder image
    // This could be enhanced to capture actual video frames from VLC
    if (!currentVideoFrame.isValid() && hasVideo())
    {
        auto videoSize = getVideoSize();
        if (videoSize.getWidth() > 0 && videoSize.getHeight() > 0)
        {
            currentVideoFrame = juce::Image(juce::Image::ARGB, 
                                          videoSize.getWidth(), 
                                          videoSize.getHeight(), 
                                          true);
            
            // Fill with a placeholder color
            juce::Graphics g(currentVideoFrame);
            g.fillAll(juce::Colours::darkgrey);
            g.setColour(juce::Colours::white);
            g.drawText("VLC Video Frame", currentVideoFrame.getBounds(), 
                      juce::Justification::centred, true);
        }
    }
    
    return currentVideoFrame;
}

void VLCSeekableMediaPlayer::setPositionNormalized(double newPositionNormalized)
{
    double duration = getTotalDuration();
    if (duration > 0.0)
    {
        setPosition(newPositionNormalized * duration);
    }
}

void VLCSeekableMediaPlayer::setPlaySpeed(double newSpeed)
{
    playbackSpeed = newSpeed;
    
    // VLCMediaPlayer doesn't expose setRate directly, so we'll store it
    // This could be enhanced if the base class exposes rate control
}

double VLCSeekableMediaPlayer::getPlaySpeed() const
{
    return playbackSpeed.load();
}

void VLCSeekableMediaPlayer::setGain(float newGain)
{
    audioGain = newGain;
}

float VLCSeekableMediaPlayer::getGain() const
{
    return audioGain.load();
}

bool VLCSeekableMediaPlayer::open(juce::URL filepath)
{
    if (filepath.isLocalFile())
    {
        const auto file = filepath.getLocalFile();
        juce::String error;
        bool result = VLCMediaPlayer::open(file, &error);
        if (result)
        {
            currentMediaFilePath = filepath;
        }
        else
        {
            DBG("Failed to open URL: " + error);
        }
        return result;
    }
    return false;
}

juce::Result VLCSeekableMediaPlayer::load(const juce::File& file)
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

void VLCSeekableMediaPlayer::setOffsetSeconds(double seconds)
{
    offsetSeconds = seconds;
    // This could be used to adjust playback timing
}

void VLCSeekableMediaPlayer::videoEnded()
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
void VLCSeekableMediaPlayer::updateVideoFrame()
{
    // This method could be enhanced to capture actual video frames from VLC
    // For now, it's a placeholder for future implementation
}

void VLCSeekableMediaPlayer::notifyPlaybackCallbacks()
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