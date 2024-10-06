/*
The MIT License (MIT)

Copyright (c) 2022-2023, Clemens Bergthaller

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/

/**
    FFmpegVideoComponent.h
 
    Author:  Clemens Bergthaller
 
    Video Component that allows basic video playback using FFmpeg

    This component is an alternative video component to the juce video component (which uses operating
    system functions for video playback and therefore has problems with some codecs, in my case h.264).
    This component tries to mimic the behaviour of the juce counterpart to some extend, but of course there
    are limits, i.e. it can read file streams only.
 
    It uses an FFmpegVideoReader and it's callbacks to play sound and display video images.
    
    This component was developed to display videos in h.264 format specifically, so it was ONLY tested with
    files using this format. Keep this in mind when using this! It still might work on some other videos. And I
    might change this in the future.
 
    To develop this video component and it it's substitues, I used my limited knowledge about FFmpeg and
    juce to mess with the ideas of Daniel Walz who developed:
    https://github.com/filmstro/filmstro_ffmpeg.
 
    Daniel Walz also developed a far more advanced video engine to also encode video data:
    https://github.com/ffAudio/foleys_video_engine
 
    @see FFmpegVideoReader
    @see FFmpegVideoListener
*/

#pragma once

#include <JuceHeader.h>

#include "FFmpegVideoReader.h"
#include "FFmpegVideoListener.h"
#include "FFmpegVideoScaler.h"

//==============================================================================

class FFmpegVideoComponent  :
    public juce::Timer,
    public juce::AudioAppComponent,
    public FFmpegVideoListener
{
public:
    FFmpegVideoComponent();
    ~FFmpegVideoComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    
    FFmpegVideoReader* getVideoReader() const;
    
    
    //***** juce::AudioAppComponent overrides *****
    // these member functions are overrides for AudioAppComponent
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

    
    //***** juce::VideoComponent mimic *****
    // these member functions mimic the behaviour of a juce::VideoComponent
    
    juce::Result load(const juce::File &file);
    bool isVideoOpen() const;
    void closeVideo();
    
    double getVideoDuration() const;
    void play();
    void setPlayPosition(double newPositionSeconds);
    double getPlayPosition() const;
    void setPlaySpeed(double newSpeed);
    double getPlaySpeed() const;
    
    
    //***** juce::VideoComponent and juce::AudioTransportSource *****
    // these member functions override and mimic the member functions
    // of a juce::VideoComponent and juce::AudioTransportSource

    void stop();
    bool isPlaying() const;
    
    
    //***** Callbacks/Listeners *****
    // ...
    
    /*! Callback for a timer. This is used to paint the current  frame. */
    void timerCallback () override;
    
    // FFmpegVideoListener Callbacks *******************************
    
    /*! callback from video reader to react when a new file has been loaded. */
    void videoFileChanged (const juce::File& video) override;
    /*! callback from video reader to react when the resolution of the video changes. */
    void videoSizeChanged (const int width, const int height, const AVPixelFormat) override;
    /** callback from video reader to display a new frame */
    void displayNewFrame (const AVFrame*) override;
    /** callback from video reader that current position has changed  */
    void positionSecondsChanged (const double pts) override;
    /** callback from video reader when playback has ended  */
    void videoEnded() override;

    std::function< void()> 	onPlaybackStarted;
    std::function< void()> 	onPlaybackStopped; 
    std::function< void(const juce::String&)> 	onErrorOccurred;

    
private:
    /*! This transport source provides basic transport capabilities using the video reader as source  */
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    
    /*! This is the video reader that decodes the video stream using FFmpeg. */
    std::unique_ptr<FFmpegVideoReader> videoReader;
    
    /*! the current frame read from stream*/
    const AVFrame* currentAVFrame;

    /*! the current frame as juce Image*/
    juce::Image currentFrameAsImage;
    
    /*! the audio buffer for this AudioAppComponent */
    juce::AudioSampleBuffer readBuffer;

    /*! scaler object to to scaling and conversion of images */
    FFmpegVideoScaler videoScaler;

    /*! member stating if the current frame was painted */
    bool frameWasPainted;
    
    double playSpeed = 1.0;
    
    bool isPaused = false;
        
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFmpegVideoComponent)
};
