#pragma once

#include "juce_ffmpeg/Source/cb_ffmpeg/FFmpegVideoReader.h"

class M1PlayerMediaObject {
private:
    juce::AudioTransportSource transportSource;
    int blockSize = 32;
    int sampleRate = 22050;

public:
    
    M1PlayerMediaObject() {}
    ~M1PlayerMediaObject() {
        releaseResources();
    }
    
    virtual void start() {
        transportSource.start();
    }
    
    virtual void stop() {
        transportSource.stop();
    }
    
    virtual bool isPlaying() {
        return transportSource.isPlaying();
    }
    
    virtual void releaseResources() {
        if (clipLoaded()) {
        }
        transportSource.setSource(nullptr);
    }
    
    virtual int getNumChannels() {}
    
    virtual void prepareToPlay(int sessionBlockSize, int sessionSampleRate) {
        blockSize = sessionBlockSize;
        sampleRate = sessionSampleRate;
    }
    
    virtual bool hasVideo() {}
    virtual bool hasAudio() {}
    virtual bool clipLoaded() {}
    
    virtual void getNextAudioBlock (const AudioSourceChannelInfo& info) {
        return transportSource.getNextAudioBlock(info);
    }
    
    virtual juce::URL getMediaFilePath() {}
    virtual int64 getNextReadPositionInSamples() {}
    virtual int64 getAudioSampleRate() {}
    virtual juce::Image& getFrame(double currentTimeInSeconds) {}
    
    virtual void setGain(float newGain) {
        transportSource.setGain(newGain);
    }
    
    virtual float getGain() {
        return transportSource.getGain();
    }
    
    virtual double getLengthInSeconds() {
        return transportSource.getLengthInSeconds();
    }
    
    virtual double getCurrentTimelinePositionInSeconds() {
        return transportSource.getCurrentPosition();
    }
    
    virtual void setTimelinePosition(int64 timecodeInSamples) {}
    
    virtual void setCurrentTimelinePositionInSeconds(double newPositionInSeconds) {
        transportSource.setPosition(newPositionInSeconds);
    }
    
    virtual void setPositionNormalized(double newPositionNormalized) {
        transportSource.setPosition(newPositionNormalized * getLengthInSeconds());
    }
    
    virtual bool open(juce::URL filepath) {}
};
