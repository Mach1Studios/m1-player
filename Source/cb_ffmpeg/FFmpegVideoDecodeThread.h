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
    FFmpegVideoReader.h

    Author:  Daniel Walz, Clemens Bergthaller
 
    This is a thread providing access to file based video streams using FFmpeg. This class handles the following tasks:
    - open files
    - set position to read from
    - decode data
    - buffer sound/video data
    - provide access to meta data like duration, sample rate, FPS, images size, etc.
 
    @see FFmpegVideoComponent
    @see FFmpegVideoListener
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "AudioBufferFIFO.h"
#include "FFmpegVideoListener.h"
#include "FFmpegVideoFramesFIFO.h"

extern "C"
{
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}

class FFmpegVideoDecodeThread : public juce::Thread
{
public:
    FFmpegVideoDecodeThread ();
    FFmpegVideoDecodeThread (AudioBufferFIFO<float>& fifo, const int videoFifoSize);
    virtual ~FFmpegVideoDecodeThread ();
    
    void run() override;

    /*! opens a video file  */
    int loadVideoFile (const juce::File& inputFile);
    
    /*! closes the current video file */
    void closeVideoFile ();
    
    /*! returns the current video file*/
    juce::File getVideoFile () const;
    
    /*! returns true if a video is currently open */
    bool isVideoOpen() const ;
    
    /*! registers a listener  */
    void addVideoListener (FFmpegVideoListener* listener);
    
    /*! deregisters a listener */
    void removeVideoListener (FFmpegVideoListener* listener);
    
    /** Return the framerate. If framerate in the decoder context is not set, this will return the timebase of the video stream. */
    double getFramesPerSecond () const;
    
    /** Sets or resets the reading position in the current video file. */
    void setPositionSeconds (const double newPositionSeconds, bool seek = false);
    
    /** returns the position in seconds */
    double getCurrentPositionSeconds () const;
    
    /** returns the position in PTS */
    int64_t getCurrentPTS () const;
    
    /*! returns the duration of the current video in seconds */
    double getDuration () const;

    /** get the width of the current video. */
    int getVideoWidth () const;

    /** get the height of the current video. */
    int getVideoHeight () const;
    
    /** get the pixel format of the video stream. */
    enum AVPixelFormat getPixelFormat () const;
    
    /*! Returns the sample format */
    enum AVSampleFormat getSampleFormat () const;

    /*! returns the aspect ratio video frame */
    double getVideoAspectRatio () const;
    
    /** This will return the aspect ratio of each pixel */
    double getPixelAspectRatio () const;

    /*! returns the sample rate */
    double getSampleRate () const;

    AVRational getAudioTimeBase() const;
    
    AVRational getVideoTimeBase() const;
    
    /*! Returns the number of audio channels. */
    int getNumberOfAudioChannels () const;
    

    
private:
    /*!
     open a codec and look for a stream of the given media type.
     @param decoderContext (AVCodecContext**) codec context to search for streams
     @param mediaType (AVMediaType) the media type to look for
     @param refCounted (bool) ...
     @returns index of the stream found, -1 if stream not found.
     */
    int openCodecContext (AVCodecContext** decoderContext,
                          enum AVMediaType mediaType,
                          bool refCounted);
    
    /*! returns pointer to the format context. */
    AVFormatContext* getFormatContext() const;
    
    /*! returns pointer to the video codec context. */
    AVCodecContext* getVideoContext () const;
    
    /*! returns pointer to the audio codec context. */
    AVCodecContext* getAudioContext () const;
    
    /*! reads and decodes av packets*/
    int readAndDecodePacket();
    
    /** Decodes an audio packet added and adds samples to the audio FIFO. Called by readAndDecodePacket(). This method returns an error code if something
        goes wrong. */
    int decodeAudioPacket (AVPacket* packet);
    
    /** Decodes a video packet and adds frames to video FIFO. Called by readAndDecodePacket(). This method returns an error code if something goes
        wrong. */
    int decodeVideoPacket (AVPacket* packet);
    
    /*! This method interrupts the decoding thread, and waits until the current decoding cycle is finished*/
    void pauseDecoding();
    
    /*! This method lets the decoding continue, if it was paused before */
    void continueDecoding();
    
    /* Contexts */
    AVFormatContext* formatContext;
    AVCodecContext* videoContext;
    AVCodecContext* audioContext;
    SwrContext* audioConverterContext;
    
    /*! index of the video stream */
    int videoStreamIndex;
    /*! index of the audio stream */
    int audioStreamIndex;
    
    /*! states if this thread should be paused after the current decoding cycle*/
    std::atomic<bool> decodingShouldPause;
    
    /*! states if this thread is paused */
    std::atomic<bool> decodingIsPaused;
    
    /*! counts the frames that were skipped while finding the wanted frame after seeking */
    int skippedVideoFramesCounter = 0;
    
    /*! counts the frames that were skipped while finding the wanted frame after seeking */
    int skippedAudioFramesCounter = 0;
    
    bool _firstDataHasArrived = false;
    
    bool _isBufferFilledEnough = false;
    
    /*! counts succesive audio  frames */
    int countAudioFrames;
    
    /*! counts succesive video  frames */
    int countVideoFrames;
    
    /*! counts groups of succesive audio  frames */
    int countAudioFrameGroups;
    
    /*! counts groups of succesive video frames */
    int countVideoFrameGroups;
    
    /*! pointer to the current audio frame */
    AVFrame* audioFrame;
    
    /*! audio buffer used for conversion */
    juce::AudioBuffer<float> audioConvertBuffer;
    
    /*! video frames fifo to buffer the decoded frames*/
    FFmpegVideoFramesFIFO videoFramesFifo;

protected:
    juce::File  videoFile;

    /** Gives access to the audio sources fifo to fill it */
    AudioBufferFIFO<float>& audioFifo;
    
    /*! Event that is used to wait until the decoding thread has paused after being told to pause. */
    juce::WaitableEvent waitForDecodingToPause;
    
    /*! Event that is used to let the the decoding thread wait until told to continue. */
    juce::WaitableEvent waitUntilContinue;
    
    /*! Event used to wait until the first data arrives when the thread just started. */
    juce::WaitableEvent waitForFirstData;
    
    /*! Event used to wait until the reading buffers are filled enough to continue with display */
    juce::WaitableEvent waitUntilBuffersAreFullEnough;
    
    /*! current PTS, this is more or less the position in the stream*/
    std::atomic<double> currentPositionSeconds;
    
    /*! Listener list for all listeners added. */
    juce::ListenerList<FFmpegVideoListener> videoListeners;
    
    /*! states if a video files is currently open. */
    bool _isVideoOpen = false;
    
    /*! states if end of file was reached.*/
    std::atomic<bool> endOfFileReached;// = false;

    /*! the value 1/SampleRate as member for faster calculation, when dividing by sample rate*/
    double sampleRateInverted = 0.0;
};
