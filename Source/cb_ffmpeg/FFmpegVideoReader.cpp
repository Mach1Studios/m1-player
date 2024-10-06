#include "FFmpegVideoReader.h"

FFmpegVideoReader::FFmpegVideoReader (const int audioFifoSize, const int videoFifoSize)
:   FFmpegVideoDecodeThread(audioFifo, videoFifoSize),
    audioFifoSize (audioFifoSize),
    audioFifo (2, audioFifoSize),
    nextReadPos (0)
{

}

FFmpegVideoReader::~FFmpegVideoReader()
{
    closeVideoFile();
    masterReference.clear();
}


int FFmpegVideoReader::loadVideoFile (const juce::File& inputFile)
{
    //if file does not exist, close current file
    if ( !inputFile.existsAsFile() ) {
        closeVideoFile();
        return false;
    }
    
    //open file, update file handle
    if (FFmpegVideoDecodeThread::loadVideoFile (inputFile))
    {
        //notify listeners about the new video file and it's size
        videoListeners.call (&FFmpegVideoListener::videoFileChanged, inputFile);
        videoListeners.call (&FFmpegVideoListener::videoSizeChanged, getVideoWidth(),
                                getVideoHeight(),
                                getPixelFormat()/*videoContext->pix_fmt*/);
        return true;
    }
    
    return false;
}


double FFmpegVideoReader::getPositionSeconds() const
{
    if (getSampleRate() > 0)
        return static_cast<double> (nextReadPos) / getSampleRate();
    return -1.0;
}

void FFmpegVideoReader::prepareToPlay (int samplesPerBlockExpected, double newSampleRate)
{
    //newSampleRate is ignored and not propagated to the video reader, since the is the source of the transport source.
    //The readers samplerate should not be changed, so it's data can be correctly resampled. It must always be the
    //correct samplerate of the video file. It is set when loading a file

//    DBG("FFmpegVideoReader::prepareToPlay, SR: " + juce::String(getSampleRate()));
    
    const int numChannels = getNumberOfAudioChannels();
    audioFifo.setSize (numChannels, audioFifoSize);
    
    audioFifo.reset();
    
    nextReadPos = 0;
}

void FFmpegVideoReader::releaseResources ()
{
//    DBG("FFmpegVideoReader::releaseResources()");
    audioFifo.clear();
}

void FFmpegVideoReader::getNextAudioBlock (const juce::AudioSourceChannelInfo &bufferToFill)
{
    // this triggers reading of new video frame
    setPositionSeconds (static_cast<double>(nextReadPos) / static_cast<double>(getSampleRate()), false);
    
    if (audioFifo.getNumReady() >= bufferToFill.numSamples)
    {
        audioFifo.readFromFifo (bufferToFill);
    }
    else
    {
        int numSamples = audioFifo.getNumReady();
        if (numSamples > 0) {
            audioFifo.readFromFifo (bufferToFill, numSamples);
            bufferToFill.buffer->clear (numSamples, bufferToFill.numSamples - numSamples);
        }
        else {
            bufferToFill.clearActiveBufferRegion();
        }
    }

    nextReadPos += bufferToFill.numSamples;
    
    //if the decoding thread has reached the end of file and the next read position is larger then total length
    if(endOfFileReached && nextReadPos >= getTotalLength())
    {
        DBG("End at position: " + juce::String(static_cast<double>(nextReadPos) / static_cast<double>(getSampleRate())));
        videoListeners.call (&FFmpegVideoListener::videoEnded);
    }
}

bool FFmpegVideoReader::waitForNextAudioBlockReady (const juce::AudioSourceChannelInfo &bufferToFill, const int msecs) const
{
    const juce::int64 timeout (juce::Time::getCurrentTime().toMilliseconds() + msecs);
    while (audioFifo.getNumReady () < bufferToFill.numSamples && juce::Time::getCurrentTime().toMilliseconds() < timeout)
    {
        juce::Thread::sleep (5);
    }
    return false;
}

void FFmpegVideoReader::setNextReadPosition (juce::int64 newPosition)
{
//    DBG("FFmpegVideoReader::setNextReadPosition(" + juce::String(newPosition) + ")");
    if (getSampleRate() <= 0)
        return;
    nextReadPos = newPosition;
    
    //tell decode thread to seek to position
    setPositionSeconds ( static_cast<double>(nextReadPos) / static_cast<double>(getSampleRate()), true);
}

juce::int64 FFmpegVideoReader::getNextReadPosition () const
{
    return nextReadPos;
}

juce::int64 FFmpegVideoReader::getTotalLength () const
{
    return static_cast<juce::int64> (getDuration() * getSampleRate());
}

bool FFmpegVideoReader::isLooping() const
{
    return false;
}
