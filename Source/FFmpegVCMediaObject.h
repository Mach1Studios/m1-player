#pragma once

#include <JuceHeader.h>

#include "M1PlayerMediaObject.h"
#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoReader.h"
#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoScaler.h"
#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoListener.h"

class FFmpegVCMediaObject : public FFmpegVideoListener
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
    juce::Image& getFrame(double currentTimeInSeconds);
    void setGain(float newGain);
    float getGain();
    double getLengthInSeconds();
    double getCurrentTimelinePositionInSeconds();
    void setTimelinePosition(juce::int64 timecodeInSamples);
    void setCurrentTimelinePositionInSeconds(double newPositionInSeconds);
    void setPositionNormalized(double newPositionNormalized);
    bool open(juce::URL filepath);

    juce::Result load(const juce::File& file);
    void closeVideo();
    bool isVideoOpen() const;
    double getVideoDuration() const;
    void setPlaySpeed(double newSpeed);
    double getPlaySpeed() const;
    void setPlayPosition(double newPositionSeconds);
    double getPlayPosition() const;

    // FFmpegVideoListener implementation
    void videoFileChanged(const juce::File& newSource) override;
    void videoSizeChanged(const int width, const int height, const AVPixelFormat format) override;
    void displayNewFrame(const AVFrame* frame) override;
    void positionSecondsChanged(const double position) override;
    void videoEnded() override;

private:
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    std::unique_ptr<FFmpegVideoReader> videoReader;
    juce::Image currentFrameAsImage;
    const AVFrame* currentAVFrame;
    FFmpegVideoScaler videoScaler;
    double playSpeed;
    bool isPaused;
    int blockSize;
    int sampleRate;
    juce::AudioBuffer<float> readBuffer;    
    juce::URL currentMediaFilePath;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFmpegVCMediaObject)
};
