// M1PlayerMediaObject.cpp
#pragma once

#include "cb_ffmpeg/FFmpegVideoReader.h"

class M1PlayerMediaObject {
private:
    foleys::VideoEngine videoEngine;
    std::shared_ptr<foleys::AVClip> clip;
    juce::AudioTransportSource transportSource;
    int blockSize = 32;
    int sampleRate = 22050;

public:
    
    M1PlayerMediaObject() {
        videoEngine.getFormatManager().registerFormat(std::make_unique<foleys::FFmpegFormat>());
    }
    
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
            clip->releaseResources();
        }
        transportSource.setSource(nullptr);
    }
    
    virtual int getNumChannels() {
        return clip->getNumChannels();
    }
    
    virtual void prepareToPlay(int sessionBlockSize, int sessionSampleRate) {
        blockSize = sessionBlockSize;
        sampleRate = sessionSampleRate;
        if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
            clip->prepareToPlay(blockSize, sampleRate);
            transportSource.prepareToPlay(blockSize, sampleRate);
        }
    }
    
    virtual bool hasVideo() {
        return clip->hasVideo();
    }
    
    virtual bool hasAudio() {
        return clip->hasAudio();
    }
    
    virtual bool clipLoaded() {
        return clip.get() != nullptr;
    }
    
    virtual void getNextAudioBlock (const AudioSourceChannelInfo& info) {
        return transportSource.getNextAudioBlock(info);
    }
    
    virtual juce::URL getMediaFilePath() {
        return clip->getMediaFile();
    }
    
    virtual int64 getNextReadPositionInSamples() {
        return clip->getNextReadPosition();
    }
    
    virtual int64 getAudioSampleRate() {
        return clip->getSampleRate();
    }
    
    virtual juce::Image& getFrame(double currentTimeInSeconds) {
//        FOLEYS_LOG ("transportSource.getCurrentPosition() =  " << transportSource.getCurrentPosition());
        return clip->getFrame(currentTimeInSeconds).image;
    }
    
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
    
    virtual void setTimelinePosition(int64 timecodeInSamples) {
        clip->setNextReadPosition(timecodeInSamples);
    }
    
    virtual void setCurrentTimelinePositionInSeconds(double newPositionInSeconds) {
        transportSource.setPosition(newPositionInSeconds);
    }
    
    virtual void setPositionNormalized(double newPositionNormalized) {
        transportSource.setPosition(newPositionNormalized * getLengthInSeconds());
    }
    
    virtual bool open(juce::URL filepath) {
        transportSource.stop();
        transportSource.setSource(nullptr);

        std::shared_ptr<foleys::AVClip> newClip = videoEngine.createClipFromFile(juce::URL(filepath));

        if (newClip.get() == nullptr)
            return false;
    
        /// Video Setup
        if (newClip.get() != nullptr) {
    
            // clear the old clip
//            if (clip.get() != nullptr) {
//                clip->removeTimecodeListener(this);
//            }
    
            newClip->prepareToPlay(blockSize, sampleRate);
            clip = newClip;
            clip->setLooping(false); // TODO: change this for standalone mode with exposed setting
//            clip->addTimecodeListener(this);
            transportSource.setSource(clip.get(), 0, nullptr);
        }
    }
};
