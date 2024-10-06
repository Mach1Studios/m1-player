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
 
    This is a reader to decode and buffer video files using FFmpeg. It inherits juce::PositionableAudioSource
    to provide basic transport functions.
    Furthermore it inherits FFmpegVideoDecodeThread which is where all the FFmpeg stuff happens, i.e.
    open files, decoding sound/images, buffering sample/images, etc.
 
    @see FFmpegVideoComponent
    @see FFmpegVideoListener
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>

#include "AudioBufferFIFO.h"
#include "FFmpegVideoListener.h"
#include "FFmpegVideoDecodeThread.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}

class FFmpegVideoReader
    : public juce::PositionableAudioSource
    , public FFmpegVideoDecodeThread
{
public:
    FFmpegVideoReader (const int audioFifoSize=192000, const int videoFifoSize=102);
    ~FFmpegVideoReader();
    
    /*! load a file */
    int loadVideoFile (const juce::File& inputFile);
    
    /*! returns current presentation timecode according to audio stream */
    double getPositionSeconds() const;
    
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;

    void releaseResources () override;
    
    /*! Loads next audio data block into buffer, also advances the threads position */
    void getNextAudioBlock (const juce::AudioSourceChannelInfo &bufferToFill) override;
    
    /** Wait until the decoder thread has finished enough data. This is needed for non-realtime processing. */
    bool waitForNextAudioBlockReady (const juce::AudioSourceChannelInfo &bufferToFill, const int msecs) const;
    
    /** Seeks in the stream */
    void setNextReadPosition (juce::int64 newPosition) override;

    /*! Returns the sample count of the next sample to be returned by getNextAudioBlock */
    juce::int64 getNextReadPosition () const override;
    
    /*! Returns the total length in samples. May not be accurate, because it uses ffmpegs
        duration property multiplied with the samplerate */
    juce::int64 getTotalLength () const override;
    
    /*! Must be implemented for a positionable audio source, but we don't need it, always returns false.*/
    bool isLooping() const override;
    
    
private:
    int audioFifoSize;
    int videoFrameFIFOSize;
    
    AudioBufferFIFO<float> audioFifo;
    
    juce::int64 nextReadPos;

    juce::WeakReference<FFmpegVideoReader>::Master masterReference;
    friend class juce::WeakReference<FFmpegVideoReader>;
};
