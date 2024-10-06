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
    AudioBufferFIFO.h
 
    Author:  Daniel Walz, Clemens Bergthaller
 
    This class converts and scales av frames to JUCE images.
*/

#pragma once

#include <JuceHeader.h> 
#include <atomic>

template<typename FloatType>
class AudioBufferFIFO  : public juce::AbstractFifo
{
public:
    /*! Creates a FIFO with a buffer of given number of channels and given number of samples */
    AudioBufferFIFO (int channels, int buffersize) :
        AbstractFifo (buffersize)
    {
        buffer.setSize (channels, buffersize);
    }

    /*! Resize the buffer with new number of channels and new number of samples */
    void setSize (const int channels, const int newBufferSize)
    {
        buffer.setSize (channels, newBufferSize);
        setTotalSize (newBufferSize);
        reset ();
    }

    /*! Push samples into the FIFO from raw float arrays */
//    void addToFifo (const FloatType** samples, int numSamples)
//    {
//        jassert (getFreeSpace() >= numSamples);
//        int start1, size1, start2, size2;
//        prepareToWrite (numSamples, start1, size1, start2, size2);
//        if (size1 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                buffer.copyFrom (channel, start1, samples[channel], size1);
//        if (size2 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                buffer.copyFrom (channel, start2, samples[channel] + size1, size2);
//
//        finishedWrite (size1 + size2);
//    }

    /*! Puts samples from an AudioBuffer into this fifo */
    void addToFifo (const juce::AudioBuffer<FloatType>& samples, int numSamples = -1, int sourceOffset = 0)
    {
        const int addSamples = (numSamples < 0 ? samples.getNumSamples() : numSamples) - sourceOffset;
        jassert (getFreeSpace() >= addSamples);
        
        int start1, size1, start2, size2;
        prepareToWrite (addSamples, start1, size1, start2, size2);
        if (size1 > 0)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.copyFrom (channel, start1, samples.getReadPointer (channel, sourceOffset), size1);
        if (size2 > 0)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                buffer.copyFrom (channel, start2, samples.getReadPointer (channel, sourceOffset + size1), size2);
        finishedWrite (size1 + size2);
    }
    
    /* Reads samples from this fifo into raw float arrays */
//    void readFromFifo (FloatType** samples, int numSamples)
//    {
//        jassert (getNumReady() >= numSamples);
//        int start1, size1, start2, size2;
//        prepareToRead (numSamples, start1, size1, start2, size2);
//        if (size1 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                juce::FloatVectorOperations::copy (samples [channel],
//                                                   buffer.getReadPointer (channel, start1),
//                                                   size1);
//        if (size2 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                juce::FloatVectorOperations::copy (samples [channel] + size1,
//                                                   buffer.getReadPointer (channel, start2),
//                                                   size2);
//        finishedRead (size1 + size2);
//    }

    /* read samples from the fifo into AudioBuffers */
//    void readFromFifo (juce::AudioBuffer<FloatType>& samples, int numSamples=-1)
//    {
//        const int readSamples = numSamples > 0 ? numSamples : samples.getNumSamples();
//        jassert (getNumReady() >= readSamples);
//
//        int start1, size1, start2, size2;
//        prepareToRead (readSamples, start1, size1, start2, size2);
//        if (size1 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                samples.copyFrom (channel, 0, buffer.getReadPointer (channel, start1), size1);
//        if (size2 > 0)
//            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
//                samples.copyFrom (channel, size1, buffer.getReadPointer (channel, start2), size2);
//        finishedRead (size1 + size2);
//    }

    /*! Reads sample data from this fifo into AudioSourceChannelInfo buffers, so it can be used in an getNextAudioBlock
     * of audio sources. */
    void readFromFifo (const juce::AudioSourceChannelInfo& info, int numSamples=-1)
    {
        const int readSamples = numSamples > 0 ? numSamples : info.numSamples;
        jassert (getNumReady() >= readSamples);

        int start1, size1, start2, size2;
        prepareToRead (readSamples, start1, size1, start2, size2);
        if (size1 > 0)
            for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
                    info.buffer->copyFrom (channel, info.startSample, buffer.getReadPointer (channel, start1), size1);
        if (size2 > 0)
            for (int channel = 0; channel < info.buffer->getNumChannels(); ++channel)
                info.buffer->copyFrom (channel, info.startSample + size1, buffer.getReadPointer (channel, start2), size2);
        finishedRead (size1 + size2);
    }

    /*! Returns the number of channels of the underlying buffer */
    int getNumChannels () const {
        return buffer.getNumChannels();
    }

    /*! Clears all samples and sets the FIFO state to empty */
    void clear () {
        buffer.clear ();
        reset();
    }

private:
    /*! The actual audio buffer */
    juce::AudioBuffer<FloatType>    buffer;
    
    
};
