
#pragma once

#include "cb_ffmpeg/FFmpegVideoReader.h"
#include "M1PlayerMediaObject.h"

#include <JuceHeader.h>
#include "M1PlayerMediaObject.h"
#include "cb_ffmpeg/FFmpegVideoReader.h"
#include "cb_ffmpeg/FFmpegVideoScaler.h"
#include "cb_ffmpeg/FFmpegVideoListener.h"

class FFmpegVCMediaObject : public M1PlayerMediaObject, public FFmpegVideoListener
{
public:
    FFmpegVCMediaObject();
    ~FFmpegVCMediaObject() override;
    
    std::function<void()> onPlaybackStarted;
    std::function<void()> onPlaybackStopped;

    // M1PlayerMediaObject overrides
    void start() override;
    void stop() override;
    bool isPlaying() override;
    void releaseResources() override;
    int getNumChannels() override;
    void prepareToPlay(int sessionBlockSize, int sessionSampleRate) override;
    bool hasVideo() override;
    bool hasAudio() override;
    bool clipLoaded() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& info) override;
    juce::URL getMediaFilePath() override;
    juce::int64 getNextReadPositionInSamples() override;
    juce::int64 getAudioSampleRate() override;
    juce::Image& getFrame(double currentTimeInSeconds) override;
    void setGain(float newGain) override;
    float getGain() override;
    double getLengthInSeconds() override;
    double getCurrentTimelinePositionInSeconds() override;
    void setTimelinePosition(juce::int64 timecodeInSamples) override;
    void setCurrentTimelinePositionInSeconds(double newPositionInSeconds) override;
    void setPositionNormalized(double newPositionNormalized) override;
    bool open(juce::URL filepath) override;

    // New methods from FFmpegVideoComponent
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

//    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FFmpegM1PlayerMediaObject)
};
