#pragma once

#include <JuceHeader.h>
#include <juce_libvlc/juce_libvlc.h>

/**
 * VLC-based implementation that extends VLCMediaPlayer.
 * 
 * This implementation provides two different logic paths:
 * 1. JUCE Image display: For image files, display directly via JUCE Image
 * 2. libVLC video decoding: For video files, use libVLC for decoding but always use audio sample time for seeking position (even if no audio)
 */
class MediaPlayer : public VLCMediaPlayer
{
public:
    MediaPlayer();
    ~MediaPlayer() override;
    
    // Override to configure VLC for headless video processing
    void configureVLCForHeadlessVideo();
    
    // Handle image files directly
    bool loadImageFile(const juce::File& imageFile);
    
    // Force video frame refresh
    void refreshVideoFrame();

    //==============================================================================
    // Legacy FFmpegVCMediaObject compatibility methods
    void start() { play(); }
    void pause() { VLCMediaPlayer::pause(); }
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
    double getLengthInSeconds() const;
    double getPositionInSeconds() const;
    void setPosition(double newPositionInSeconds);
    bool isPlaying() const { return VLCMediaPlayer::isPlaying(); }
    bool hasVideo() const { return isImageFile || VLCMediaPlayer::hasVideo(); }
    bool hasAudio() const { return !isImageFile && VLCMediaPlayer::hasAudio(); }
    void setPositionNormalized(double newPositionNormalized);
    void setPlaySpeed(double newSpeed);
    double getPlaySpeed() const;
    void setGain(float newGain);
    float getGain() const;
    
    bool open(juce::URL filepath);
    juce::Result load(const juce::File& file);
    void closeMedia() { close(); }
    void close() override;
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
    
    // Note: Video frame and mutex are inherited from VLCMediaPlayer base class.
    // Do NOT declare them here as it would shadow the base class members
    // and break video frame updates from VLC callbacks.
    
    // Local frame storage for image files only (not videos)
    juce::Image imageFileFrame;
    std::mutex imageFileMutex;
    
    // Audio processing
    juce::AudioBuffer<float> tempAudioBuffer;
    
    // Audio device manager reference
    juce::AudioDeviceManager* audioDeviceManager = nullptr;
    
    // Image file handling
    bool isImageFile = false;
    
    // Video frame refresh counter
    mutable std::atomic<int> frameRefreshCounter { 0 };
    
    //==============================================================================
    // Internal methods
    void updateVideoFrame();
    void notifyPlaybackCallbacks();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MediaPlayer)
};
