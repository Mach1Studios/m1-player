#pragma once

#include <JuceHeader.h>

#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegMediaReader.h"
#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoScaler.h"
#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoListener.h"

class FFmpegVCMediaObject : public FFmpegVideoListener, public juce::Timer
{
public:
    FFmpegVCMediaObject();
    ~FFmpegVCMediaObject();
    
    std::function<void()> onPlaybackStarted;
    std::function<void()> onPlaybackStopped;

    void start();
    void stop();
    bool isPlaying();
    void releaseResources();
    int getNumChannels();
    void prepareToPlay(int sessionBlockSize, int sessionSampleRate);
    bool hasVideo();
    bool hasAudio();
    bool clipLoaded();
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info);
    juce::URL getMediaFilePath();
    juce::int64 getNextReadPositionInSamples();
    juce::int64 getAudioSampleRate();
    juce::int64 getVideoFrameRate();
    juce::Image& getFrame();
    double getLengthInSeconds();
    double getPositionInSeconds();
    void setPosition(double newPositionInSeconds);
    void setPositionNormalized(double newPositionNormalized);
    void setPlaySpeed(double newSpeed);
    double getPlaySpeed() const;
    void setGain(float newGain);
    float getGain();
    
    bool open(juce::URL filepath);
    juce::Result load(const juce::File& file);
    void closeMedia();
    bool isOpen() const;
    
    // FFmpegVideoListener implementation
    void videoFileChanged(const juce::File& newSource) override;
    void videoSizeChanged(const int width, const int height, const AVPixelFormat format) override;
    void displayNewFrame(const AVFrame* frame) override;
    void positionSecondsChanged(const double position) override;
    
    int getSamplerateLegacy();
    
    void setOffsetSeconds(double seconds);

private:
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    std::unique_ptr<FFmpegMediaReader> mediaReader;
    juce::Image currentFrameAsImage;
    const AVFrame* currentAVFrame;
    FFmpegVideoScaler videoScaler;
    double playSpeed;
    bool isPaused;
    int blockSize;
    int sampleRate;
    juce::AudioBuffer<float> readBuffer;    
    juce::URL currentMediaFilePath;
    double offsetSeconds = 0;

    /*! Callback for a timer. This is used to paint the current  frame. */
    void timerCallback () override;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFmpegVCMediaObject)
};
