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
    FFmpegVideoFramesFIFO.h

    Author:  Clemens Bergthaller
 
    Video Component that allows basic video playback using FFmpeg

    Simple class working as a FIFO for AV frames.
*/

#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>



extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}


class FFmpegVideoFramesFIFO
{
public:
    FFmpegVideoFramesFIFO(unsigned int new_size)
    : size(new_size)
    , readIndex(0)
    , writeIndex(0)
    {
        // initialize frame FIFO, set data to NULL
        videoFramesFifo.resize (size, std::make_pair (0.0, nullptr));
        //allocate memory for the frames
        for (int i=0; i<videoFramesFifo.size(); ++i) {
            videoFramesFifo [i].second = av_frame_alloc();
        }
    }
    
    ~FFmpegVideoFramesFIFO()
    {
        //free memory
        for (int i=0; i<videoFramesFifo.size(); ++i) {
            av_frame_free (&(videoFramesFifo [i].second));
        }
    }
    
    unsigned int getReadIndex()
    {
        return readIndex;
    }
    
    unsigned int getWriteIndex()
    {
        return writeIndex;
    }
    
    unsigned int getSize()
    {
        return size;
    }
    
    AVFrame* getFrameAtReadIndex()
    {
        return videoFramesFifo[readIndex].second;
    }
    
    double getSecondsAtReadIndex()
    {
        return videoFramesFifo[readIndex].first;
    }
    
    AVFrame* getFrameAtWriteIndex()
    {
        return videoFramesFifo[writeIndex].second;
    }
    
    double getSecondsAtWriteIndex()
    {
        return videoFramesFifo[writeIndex].first;
    }
    
    unsigned int advanceReadIndex()
    {
        readIndex = (++readIndex)%size;
        return readIndex;
    }
    
    unsigned int advanceWriteIndex()
    {
        writeIndex = (++writeIndex)%size;
        return writeIndex;
    }
    
    void reset()
    {
        readIndex = 0;
        writeIndex = 0;
        for (int i=0; i < videoFramesFifo.size(); ++i) {
            videoFramesFifo [i].first = 0.0;
        }
    }
    
    unsigned countNewFrames()
    {
        return (size + writeIndex - readIndex) % size;
    }
    
    void setSecondsAtWriteIndex(double new_seconds)
    {
        videoFramesFifo[writeIndex].first = new_seconds;
    }
    
    unsigned int findOffsetForSeconds(double seconds)
    {
        //Try to find the position of the current video frame in FIFO, starting at read position. The current frame might
        //not be the next frame in fifo. Frames before the current frame must be dropped.
        auto offset=0;
        while ( (videoFramesFifo [(readIndex + offset) % size].first < seconds) && offset < countNewFrames() )
            offset++;
        return offset;
    }
    
    
    
private:
    /*! vector with pairs of position (in seconds) and corresponding frames. */
    std::vector<std::pair<double, AVFrame*> > videoFramesFifo;
    
    unsigned int size = 0;
    unsigned int readIndex = 0;
    unsigned int writeIndex = 0;

};
