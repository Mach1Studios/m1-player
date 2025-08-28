#pragma once

#include <JuceHeader.h>
#include <juce_libvlc/juce_libvlc.h>

/**
 * VLC-based implementation that extends VLCMediaPlayer to replace FFmpegVCMediaObject.
 * 
 * This implementation provides three different logic paths:
 * 1. Audio-driven seeking: When video has audio, seeking is sample-accurate via audio stream
 * 2. Direct JUCE Image display: Video frames are rendered directly to JUCE Images
 * 3. libVLC video decoding: For video-only files, libVLC handles decoding and seeking
 */
class VLCSeekableMediaPlayer : public VLCMediaPlayer
{
public:
    VLCSeekableMediaPlayer();
    ~VLCSeekableMediaPlayer() override;

    //==============================================================================
    // Legacy FFmpegVCMediaObject compatibility methods
    void start() { play(); }
    void stop() { VLCMediaPlayer::stop(); }
    bool isOpen() const;
    bool clipLoaded() const { return isOpen(); }
    int getNumChannels() const;
    void prepareToPlay(int sessionBlockSize, int sessionSampleRate);
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    void releaseResources();
    
    juce::URL getMediaFilePath() const;
    int64_t getNextReadPositionInSamples() const { return getCurrentSample(); }
    int64_t getAudioSampleRate() const { return getSampleRate(); }
    int64_t getVideoFrameRate() const;
    juce::Image& getFrame();
    double getLengthInSeconds() const { return getTotalDuration(); }
    double getPositionInSeconds() const { return getCurrentTime(); }
    void setPosition(double newPositionInSeconds) { seekToTime(newPositionInSeconds); }
    bool isPlaying() const { return VLCMediaPlayer::isPlaying(); }
    bool hasVideo() const { return VLCMediaPlayer::hasVideo(); }
    bool hasAudio() const { return VLCMediaPlayer::hasAudio(); }
    void setPositionNormalized(double newPositionNormalized);
    void setPlaySpeed(double newSpeed);
    double getPlaySpeed() const;
    void setGain(float newGain);
    float getGain() const;
    
    bool open(juce::URL filepath);
    juce::Result load(const juce::File& file);
    void closeMedia() { close(); }
    int getSamplerateLegacy() const { return getSampleRate(); }
    void setOffsetSeconds(double seconds);
    void setAudioDeviceManager(juce::AudioDeviceManager* manager) { /* Store reference for future use */ audioDeviceManager = manager; }

    // Callback functions for compatibility
    std::function<void()> onPlaybackStarted;
    std::function<void()> onPlaybackStopped;
    void videoEnded();

private:
    //==============================================================================
    // Additional state for compatibility
    juce::URL currentMediaFilePath;
    std::atomic<double> playbackSpeed { 1.0 };
    std::atomic<float> audioGain { 1.0f };
    std::atomic<double> offsetSeconds { 0.0 };
    std::atomic<double> videoFrameRate { 30.0 };
    
    // Current video frame as JUCE Image for direct access
    juce::Image currentVideoFrame;
    std::mutex videoFrameMutex;
    
    // Audio processing
    juce::AudioBuffer<float> tempAudioBuffer;
    
    // Audio device manager reference
    juce::AudioDeviceManager* audioDeviceManager = nullptr;
    
    //==============================================================================
    // Internal methods
    void updateVideoFrame();
    void notifyPlaybackCallbacks();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(VLCSeekableMediaPlayer)
};