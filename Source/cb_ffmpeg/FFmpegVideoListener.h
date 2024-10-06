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

    This is an interface allowing classes to listen to video readers.

    @see FFmpegVideoComponent
    @see FFmpegVideoReader
 */

#pragma once

#include <JuceHeader.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}

class FFmpegVideoListener
{

public:
    FFmpegVideoListener() {}
    virtual ~FFmpegVideoListener() {}

    /** This is called when the video source file has changed */
    virtual void videoFileChanged (const juce::File& newSource) {}

    /** This is called whenever the video size changed. */
    virtual void videoSizeChanged (const int width, const int height, const AVPixelFormat) {}

    /** This is called when a frame is to be displayed. */
    virtual void displayNewFrame (const AVFrame*) {}

    /** This will notify about the current position. */
    virtual void positionSecondsChanged (const double) {}
    
    /*! this gets called when the video has ended */
    virtual void videoEnded(){}
};
