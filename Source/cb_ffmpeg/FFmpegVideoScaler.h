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
    FFmpegVideoScaler.h
 
    Author:  Daniel Walz, Clemens Bergthaller
 
    This class converts and scales av frames to JUCE images.
 */

#pragma once

extern "C" {
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libavcodec/avcodec.h>
}


class FFmpegVideoScaler
{
public:
    /*! Creates a scaler object. It does nothing before you call setupScaler */
    FFmpegVideoScaler () : scalerContext (nullptr) {}

    ~FFmpegVideoScaler ()
    {
        if (scalerContext)
            sws_freeContext (scalerContext);
    }

    /*! Setup a scaler to scale video frames and to convert pixel formats */
    void setupScaler (const int in_width,  const int in_height,  const AVPixelFormat in_format,
                      const int out_width, const int out_height, const AVPixelFormat out_format)
    {
        if (scalerContext) {
            sws_freeContext (scalerContext);
            scalerContext = nullptr;
        }

        const AVPixFmtDescriptor* in_descriptor = av_pix_fmt_desc_get (in_format);
        if (!in_descriptor) {
            DBG ("No description for input pixel format");
            return;
        }
        const int in_bitsPerPixel = av_get_padded_bits_per_pixel (in_descriptor);
        for (int i=0; i < 4; ++i)
            inLinesizes [i] = i < in_descriptor->nb_components ? in_width * in_bitsPerPixel >> 3 : 0;

        const AVPixFmtDescriptor* out_descriptor = av_pix_fmt_desc_get (out_format);
        if (!out_descriptor) {
            DBG ("No description for output pixel format");
            return;
        }
        const int out_bitsPerPixel = av_get_padded_bits_per_pixel (out_descriptor);
        for (int i=0; i < 4; ++i)
            outLinesizes [i] = i < out_descriptor->nb_components ? out_width * out_bitsPerPixel >> 3 : 0;

        // create scaling context
        scalerContext = sws_getContext (in_width,  in_height, in_format,
                                        out_width, out_height, out_format,
                                        SWS_BILINEAR, NULL, NULL, NULL);
        if (!scalerContext) {
            DBG ("Impossible to create scale context for the conversion");
        }
    }


    /*! Converts an AVFrame read from ffmpeg to a JUCE Image. */
    void convertFrameToImage (juce::Image& image, const AVFrame* frame)
    {
        if (scalerContext) {
            juce::Image::BitmapData data (image, 0, 0,
                                          image.getWidth(),
                                          image.getHeight(),
                                          juce::Image::BitmapData::writeOnly);

            uint8_t* destination[4] = {data.data, nullptr, nullptr, nullptr};

            sws_scale (scalerContext,
                       frame->data,
                       frame->linesize,
                       0,
                       frame->height,
                       destination,
                       outLinesizes);
        }
    }

private:
    SwsContext* scalerContext;

    int         inLinesizes[4];
    int         outLinesizes[4];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FFmpegVideoScaler)
};

