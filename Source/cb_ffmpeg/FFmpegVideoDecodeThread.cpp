#include "FFmpegVideoDecodeThread.h"

#include "FFmpegHelpers.h"


//you can find AV_TIME_BASE (1000000) in avutil.h, this is the inverse
#define CB_AV_TIME_BASE_INVERSE    0.000001


FFmpegVideoDecodeThread::FFmpegVideoDecodeThread (AudioBufferFIFO<float>& fifo, const int videoFifoSize)
: juce::Thread("VideoDecodeThread"),
formatContext (nullptr),
videoContext (nullptr),
audioContext (nullptr),
audioConverterContext (nullptr),
videoStreamIndex (-1),
audioStreamIndex (-1),
videoFramesFifo(videoFifoSize),
audioFifo (fifo),
currentPositionSeconds (0.0)
{
    audioFrame = av_frame_alloc();
    _isVideoOpen = false;
    endOfFileReached = false;
    
    waitForDecodingToPause.reset();
    waitUntilContinue.reset();
    waitForFirstData.reset();
    waitUntilBuffersAreFullEnough.reset();
}

FFmpegVideoDecodeThread::~FFmpegVideoDecodeThread ()
{
    av_frame_free (&audioFrame);
    //force to stop thread, if necessary
    stopThread (1000);
}

int FFmpegVideoDecodeThread::openCodecContext (AVCodecContext** codecContext,
                      enum AVMediaType mediaType,
                      bool refCounted)
{
    //find best stream for media type
    int id = av_find_best_stream (formatContext, mediaType, -1, -1, NULL, 0);

    //if stream index is valid
    if (juce::isPositiveAndBelow(id, static_cast<int> (formatContext->nb_streams)))
    {
        const AVCodec *codec = NULL;
        AVDictionary *opts = NULL;
        AVStream* stream = formatContext->streams [id];
        // find decoder for the stream
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
        if (!codec)
        {
            DBG ("Unable to find \"" + juce::String (av_get_media_type_string(mediaType)) + "\" codec");
            return -1;
        }
        // Allocate a codec context for the decoder
        *codecContext = avcodec_alloc_context3 (codec);
        if (!*codecContext)
        {
            DBG ("Unable to allocate memory for the \"" + juce::String (av_get_media_type_string(mediaType)) +
                 "\" codec context");
            return -1;
        }
        // Copy codec parameters from input stream to output codec context
        if (avcodec_parameters_to_context (*codecContext, stream->codecpar) < 0)
        {
            DBG ("Failed to copy " + juce::String (av_get_media_type_string(mediaType)) +
                 " codec parameters to decoder context");
            return -1;
        }
        // Init the decoders, with or without reference counting
        av_dict_set (&opts, "refcounted_frames", refCounted ? "1" : "0", 0);
        auto errorNo = avcodec_open2 (*codecContext, codec, &opts);
        if (errorNo < 0)
        {
            DBG ("Failed to open " + juce::String (av_get_media_type_string(mediaType)) + " codec");
            avcodec_free_context (codecContext);
            return -1;
        }
        return id;
    }
    else
    {
        DBG ("Could not find " + juce::String (av_get_media_type_string(mediaType)) + " stream in input file");
        return -1;
    }
}

int FFmpegVideoDecodeThread::loadVideoFile(const juce::File &inputFile)
{
    //if there is already a context open, close it
    if (formatContext) {
        closeVideoFile ();
        videoFile = juce::File();
    }

    // open input file, and allocate format context
    int errorCode = avformat_open_input (&formatContext, inputFile.getFullPathName().toRawUTF8(), NULL, NULL);
    if (errorCode < 0) {
        DBG ("Opening file failed");
        _isVideoOpen = false;
        return -1;
    }

    // try to get stream infos
    if (avformat_find_stream_info (formatContext, NULL) < 0)
    {
        _isVideoOpen = false;
        return false;
    }
    
    // find and open audio stream / audio context
    audioStreamIndex = -1;
    audioStreamIndex = openCodecContext (&audioContext, AVMEDIA_TYPE_AUDIO, true);
    // if stream was found and stream index is within range
    if (juce::isPositiveAndBelow (audioStreamIndex, (int)/*static_cast<int>*/ (formatContext->nb_streams)))
    {
        sampleRateInverted = 1.0 / static_cast<double>(audioContext->sample_rate);
        //set up audio converter
        AVChannelLayout ch_layout = formatContext->streams [audioStreamIndex]->codecpar->ch_layout;
        audioConverterContext = nullptr;
        errorCode = swr_alloc_set_opts2( &audioConverterContext,
                                          &ch_layout,                   // out_ch_layout
                                          AV_SAMPLE_FMT_FLTP,           // out_sample_fmt
                                          audioContext->sample_rate,    // out_sample_rate
                                          &ch_layout,                   // in_ch_layout
                                          audioContext->sample_fmt,     // in_sample_fmt
                                          audioContext->sample_rate,    // in_sample_rate
                                          0,                            // log_offset
                                          nullptr);                     // log_ctx
        if(errorCode < 0)
            DBG("Error when allocating audio converter: " + FFmpegHelpers::avErrorToString(errorCode)  );
        
        errorCode = swr_init(audioConverterContext);
        if(errorCode < 0)
            DBG("Error when setting up audio converter: " + FFmpegHelpers::avErrorToString(errorCode)  );
    }
    else
    {
        DBG("No audio stream found.");
    }
    
    //find and open video stream / video context
    videoStreamIndex = -1;
    videoStreamIndex = openCodecContext (&videoContext, AVMEDIA_TYPE_VIDEO, true);
    // if stream was found and stream index is within range
    if (videoStreamIndex < 0)
    {
        DBG("No video stream found.");
    }
    
    DBG ("\n====================================================");
    DBG ("Loaded file :      " + inputFile.getFullPathName());
    DBG ("Channels:          " + juce::String (getNumberOfAudioChannels()));
    DBG ("Duration (sec):    " + juce::String (getDuration()));
    DBG ("Framerate (1/sec): " + juce::String (getFramesPerSecond()));
    DBG ("SampleRate:        " + juce::String (getSampleRate()));
    DBG ("SampleFormat:      " + juce::String (av_get_sample_fmt_name (getSampleFormat())));
    DBG ("Width:             " + juce::String (getVideoWidth()));
    DBG ("Height:            " + juce::String (getVideoHeight()));
    DBG ("Pixel format:      " + juce::String (av_get_pix_fmt_name (getPixelFormat())));
    DBG ("Pixel aspect ratio:" + juce::String (getPixelAspectRatio()));
    DBG ("====================================================");

    // reset all FIFOs
    audioFifo.reset();
    videoFramesFifo.reset();

    videoFile = inputFile;
    _isVideoOpen = true;
    
    waitForDecodingToPause.reset();
    waitUntilContinue.reset();
    waitForFirstData.reset();
    waitUntilBuffersAreFullEnough.reset();
    
    return true;
}

void FFmpegVideoDecodeThread::closeVideoFile()
{
    waitUntilBuffersAreFullEnough.signal();
    signalThreadShouldExit();
    waitForThreadToExit(1000);

    if (videoStreamIndex >= 0) {
        avcodec_free_context (&videoContext);
        videoStreamIndex = -1;
    }
    if (audioStreamIndex >= 0) {
        avcodec_free_context (&audioContext);
        audioStreamIndex = -1;
    }
    if(audioConverterContext)
    {
        swr_free(&audioConverterContext);
    }
    avformat_close_input (&formatContext);
    _isVideoOpen = false;
    
    videoFramesFifo.reset();
    audioFifo.reset();
}

juce::File FFmpegVideoDecodeThread::getVideoFile () const
{
    return videoFile;
}

void FFmpegVideoDecodeThread::run()
{
//    DBG("run...");
    unsigned long newFramesCount = 0;   //frames in buffer ready to read
    bool bufferingStopped = false;      //keeps track if buffers are full, for debug use only

    _firstDataHasArrived = false;
    _isBufferFilledEnough = false;
    
    countVideoFrames = 0;
    countVideoFrameGroups = 0;
    countAudioFrames = 0;
    countAudioFrameGroups = 0;
    
    while (!threadShouldExit())
    {
        if (decodingShouldPause)
        {
            //pause decoding and signal that it has stopped
//            DBG("\tDecoding paused...");
            decodingIsPaused = true;
            waitForDecodingToPause.signal();
            //wait until continue signal
//            DBG("Wait until thread is continued...");
            waitUntilContinue.reset();
            waitUntilContinue.wait(-1);
//            DBG("\tDecoding continued...");
        }
        if (!decodingIsPaused)
        {
            //number of frames that have been written, but not read
            newFramesCount = videoFramesFifo.countNewFrames();
            
            //if there is enough space in audio and video fifo and the audio fifo does not have too many samples not read already
            if ( (audioFifo.getFreeSpace() > 2048
                && (audioFifo.getNumReady() < 192000 //this 4*48000 => 4 seconds audio at 48kHz
                && newFramesCount < videoFramesFifo.getSize() - 2) ) ) //-2 because this is a fifo
            {
                //since frames come in groups of successive audio frames OR video frames, we need to buffer at least one
                //whole group of each frame type before we can display images and play sound.
                if (countVideoFrameGroups >= 4 && countAudioFrameGroups >= 4) 
                {
                    if (!_firstDataHasArrived)
                    {
                        _firstDataHasArrived = true;
//                        DBG("Buffers are filled enough...");
                    }
                    waitUntilBuffersAreFullEnough.signal();
                }
                else if ( countAudioFrames >= 1 || countVideoFrames >= 1)
                {
                    if (!_firstDataHasArrived)
                    {
                        _firstDataHasArrived = true;
//                        DBG("First data has arrived...");
                    }
                    waitForFirstData.signal();
                }
                
                //keep buffering
                bufferingStopped = false;
                
                int error = readAndDecodePacket();
                if (error < 0 )
                {
                    if ( error == -541478725 ) // End Of File
                    {
                        if ( ! endOfFileReached )
                        {
//                            DBG("Decoding Thread has reached end of file.");
                            endOfFileReached = true;
                            waitUntilBuffersAreFullEnough.signal();
                        }
                    }
                    else
                    {
                        auto errorString = FFmpegHelpers::avErrorToString(error);
                        DBG("An error occurred when reading and decoding packet: " + juce::String(error)
                            + ", " + errorString );
                    }
                    //keep checking, position might be changed
                    wait(500);
                }
            }
            //else wait a little until there is enough space because something was read
            else
            {
                //only warn once
                if ( ! bufferingStopped )
                {
                    bufferingStopped = true;
//                    waitUntilBuffersAreFullEnough.signal();
//                    DBG("Buffering stopped, buffered video frames: " + juce::String(newFramesCount));
                }
                waitUntilBuffersAreFullEnough.signal();
                wait (20);
            }
        }

    }
//    DBG("VideoDecodeThread has stopped...");
}

int FFmpegVideoDecodeThread::readAndDecodePacket()
{
    //allocate packet, initialise it
    AVPacket* packet = av_packet_alloc();
    packet->data = NULL;
    packet->size = 0;
    
    auto error = av_read_frame (formatContext, packet);
    
    if (error < 0)
    {
        if (error != AVERROR_EOF/*-541478725*/)
        {
            DBG ("Error reading Packet: " + FFmpegHelpers::avErrorToString(error));
            av_packet_unref (packet);
            return error;
        }
        else
        {
            //if av_read_frame returns EOF, this does mean that reading the file has reached EOF. This does NOT mean
            //that decoding is finished, since the decoders can still contain data. Therefore the contexts must be
            //drained. Draining is initiated by sending NULL to contexts.
            //see https://www.ffmpeg.org/doxygen/3.4/group__lavc__encdec.html
            //or https://github.com/microsoft/FFmpegInterop/issues/217
            decodeAudioPacket (NULL);
            decodeVideoPacket (NULL);
            av_packet_unref (packet);
            return AVERROR_EOF;
        }
    }
    
    if (packet->size <= 0)
    {
        DBG ("packet empty");
        av_packet_unref (packet);
        return 0;
    }
    
    if (packet->stream_index == audioStreamIndex)
        decodeAudioPacket (packet);
    else if(packet->stream_index == videoStreamIndex)
        decodeVideoPacket (packet);
    //discard subtitles
//    else
//        DBG ("Packet is neither audio nor video... stream: " + juce::String (packet->stream_index));
    
    av_packet_unref (packet);
    return 1; //return SUCCESS
}

int FFmpegVideoDecodeThread::decodeAudioPacket (AVPacket* packet)
{
    //count groups of video frames
    if (countVideoFrames > 0 )
    {
        countVideoFrameGroups++;
//        DBG("Video Frames: " + juce::String(countVideoFrames) + ", Video Groups: " + juce::String(countVideoFrameGroups));
        countVideoFrames = 0;
    }
    
    int numOutputSamples = 0;
    int response = avcodec_send_packet(audioContext, packet);
    
    // decode audio frame
    // read frame until finished
    while (response >= 0) {
        response = avcodec_receive_frame(audioContext, audioFrame);
        // if not enough data or end of file reached, read more data
        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
        {
            break;
        }
        else if (response < 0 && response != AVERROR_EOF)
        {
            DBG("Error while receiving an audio frame from the decoder: " +  FFmpegHelpers::avErrorToString(response));
            break;
        }

        //get presentation timestamp of frame
        int64_t framePTS = audioFrame->best_effort_timestamp; //av_frame_get_best_effort_timestamp (audioFrame);
        double  framePTSsecs = static_cast<double>(framePTS) * sampleRateInverted;


        if ( audioFrame->extended_data == nullptr ) //if frame audio data does not exist
            continue;
        if (framePTSsecs < currentPositionSeconds)  //if audio frame is in past, skip it, see decodeVideoPaket
        {
            skippedAudioFramesCounter++;
            continue;
        }
        if (framePTSsecs < 0)
        {
            DBG("This is really weird and should not happen: Audio frame has negative pts: " + juce::String(framePTSsecs));
            continue;
        }
        
        //this is just debug stuff
        if ( skippedAudioFramesCounter > 0)
        {
            DBG(juce::String(skippedAudioFramesCounter) + " Audio Frames skipped after seeking...");
            skippedAudioFramesCounter = 0;
            countVideoFrameGroups = 0;
            countAudioFrameGroups = 0;
        }
        
        countAudioFrames++;

        const int channels   = audioFrame->ch_layout.nb_channels;
        const int numSamples = audioFrame->nb_samples;
        audioConvertBuffer.setSize(channels, numSamples, false, false, true);
        
        numOutputSamples = numSamples;
        //read audio data from frame, convert it and write it to audio conversion buffer
        swr_convert(audioConverterContext,
                    (uint8_t**)audioConvertBuffer.getArrayOfWritePointers(),
                    numSamples,
                    (const uint8_t**)audioFrame->extended_data,
                    numSamples);
        audioFifo.addToFifo (audioConvertBuffer);
    }
    
    return numOutputSamples;
}

int FFmpegVideoDecodeThread::decodeVideoPacket (AVPacket* packet)
{
    //count groups of audio frames
    if ( countAudioFrames > 0)
    {
        countAudioFrameGroups++;
        countAudioFrames = 0;
    }
    int response = avcodec_send_packet(videoContext, packet);

    if (response < 0 && response != AVERROR_EOF)
    {
        DBG(" -> Error while sending a video packet to the decoder: " + FFmpegHelpers::avErrorToString(response) );
    }

    double pts_sec = 0.0; 
    while (response >= 0) {
        //load next frame into frame at write index
        AVFrame* frame = videoFramesFifo.getFrameAtWriteIndex();
        response = avcodec_receive_frame(videoContext, frame);
        
        if (response >= 0)
        {
            //calculate PTS in seconds
            int64_t pts = frame->best_effort_timestamp;
            AVRational timeBase = av_make_q (1, AV_TIME_BASE);
            if (juce::isPositiveAndBelow(videoStreamIndex, static_cast<int> (formatContext->nb_streams)))
            {
                timeBase = formatContext->streams [videoStreamIndex]->time_base;
            }
            pts_sec = av_q2d (timeBase) * static_cast<double>(pts);
            
            //set seconds for frame in videoFrameFIFO
            videoFramesFifo.setSecondsAtWriteIndex(pts_sec);
            //advance write index only if this frames' position is larger or equal to current position.
            //This is important when playback position was changed, because seeking will go back
            //to previous i frame and start decoding from there. All frames before the current position must be
            //skipped.
            if ( pts_sec < currentPositionSeconds )
            {
                skippedVideoFramesCounter++;
                continue;
            }
            
            if (pts_sec < 0.0 )
            {
                DBG("This is really weird and should not happen: Video frame has negative pts: " + juce::String(pts_sec));
                continue;
            }
            
            //advance fifo write index
            videoFramesFifo.advanceWriteIndex();
            countVideoFrames++;

            //this is just debug stuff
            if ( skippedVideoFramesCounter > 0)
            {
                DBG(juce::String(skippedVideoFramesCounter) + " Video Frames skipped after seeking...");
                skippedVideoFramesCounter = 0;
                countVideoFrameGroups = 0;
                countAudioFrameGroups = 0;
            }
        }
        else if ( response == AVERROR_EOF )
        {
            DBG("EOF for avcodec_receive_frame.");
        }
    }
    return response;
}

void FFmpegVideoDecodeThread::pauseDecoding()
{
    if (/*!decodingShouldPause &&*/ isThreadRunning())
    {
        decodingShouldPause = true;
        //wait until thread has paused
        if ( !decodingIsPaused )
        {
            waitForDecodingToPause.reset();
//            DBG("Wait for decoding thread to pause...");
            waitForDecodingToPause.wait(-1);
        }
    }
}

void FFmpegVideoDecodeThread::continueDecoding()
{
    if (decodingIsPaused && isThreadRunning())
    {
        decodingIsPaused = false;
        decodingShouldPause = false;
        //signal to continue thread
        waitUntilContinue.signal();
    }
}


void FFmpegVideoDecodeThread::setPositionSeconds (const double newPositionSeconds, bool seek)
{
    //update position
    currentPositionSeconds = newPositionSeconds;
    // Position[Samples] = position[Seconds] * Sample Rate
    int64_t readPosSamples = static_cast<int64_t>(currentPositionSeconds *
        static_cast<double>(formatContext->streams[audioStreamIndex]->time_base.den));
    
    //if a transport source is using the video reader to seek
    if (seek)
    {
        DBG("\n>>>>>>>>>> seeking to position: " + juce::String (newPositionSeconds)
            +"(" + juce::String(getDuration()) + "), readPos: " +  juce::String (readPosSamples) );
        
        //start thread if it is seeked for the first time
        if(!isThreadRunning())
        {
//            DBG("starting decoding thread.");
            startThread();
            
            //let thread run for a while to see if data arrives
            waitForFirstData.reset();
            waitForFirstData.wait(-1);
        }
        
        //pause the decoding process safely, so it can finish the current decoding cycle
        pauseDecoding();
        
        //reset thread data about gathered data
        countVideoFrames = 0;
        countVideoFrameGroups = 0;
        countAudioFrames = 0;
        countAudioFrameGroups = 0;
        endOfFileReached = false;

        _firstDataHasArrived = false;  //debug stuff
        _isBufferFilledEnough = false; //debug stuff
        
        //flush context buffers
        avcodec_flush_buffers(videoContext);
        avcodec_flush_buffers(audioContext);

        //go to position
        int result = av_seek_frame (formatContext, audioStreamIndex, readPosSamples, AVSEEK_FLAG_BACKWARD);
        if (result < 0)
            DBG("Seek error: " + juce::String(result));

        // reset all FIFOs
        audioFifo.reset();
        videoFramesFifo.reset();

        //continue decoding to buffer data
        continueDecoding();
        
        //wait for data
        waitUntilBuffersAreFullEnough.reset();
        waitUntilBuffersAreFullEnough.wait(-1);

//        if(!endOfFileReached)
//        {
//            DBG("buffers filled enough after seeking to " + juce::String(newPositionSeconds) + "(s): ");
//        }
//        else
//        {
//            DBG("buffers filled but EOF reached after seeking to " + juce::String(newPositionSeconds) + "(s)");
//        }
        
    }
    
    videoListeners.call (&FFmpegVideoListener::positionSecondsChanged, newPositionSeconds);

    //if there are frames available
    if (videoFramesFifo.countNewFrames() > 0)
    {
        //Try to find the position of the current video frame in FIFO, starting at read position. The current frame might
        //not be the next frame in fifo. Frames before the current frame must be dropped.
        auto currentFramePositionOffset = videoFramesFifo.findOffsetForSeconds(newPositionSeconds);

        //if frame found or it was seeked
        if (currentFramePositionOffset > 0 || seek)
        {
            //TODO: see, if this really happens with some videos
            if (currentFramePositionOffset > 1)
                DBG ("Dropped " + juce::String (currentFramePositionOffset-1) + " frame(s).");
            
            //get frame
            AVFrame* nextFrame = videoFramesFifo.getFrameAtReadIndex();
            
            //Debug
//            double frameSeconds = static_cast<double>(nextFrame->pts) /
//                static_cast<double>(formatContext->streams[videoStreamIndex]->time_base.den);
//            if( (endOfFileReached && frameSeconds >= getDuration() - 0.25 ) /*|| frameSeconds <= 0.5*/ )
//            {
//                DBG(  "Update Frame " + juce::String(frameSeconds) + "/" + juce::String(getDuration()) + ", "
//                    + "pos: " + juce::String(currentPositionSeconds) + ", "
//                    + "fifo-index: " + juce::String(videoFramesFifo2.getReadIndex()) + ", "
//                    + "frames left: " + juce::String(videoFramesFifo2.countNewFrames()) + ""
//                    );
//            }
            
            videoFramesFifo.advanceReadIndex();
            
            //provide listeners with current frame
            videoListeners.call (&FFmpegVideoListener::displayNewFrame, nextFrame);
        }
    }
    else
    {
        DBG ("No frame available at " + juce::String(newPositionSeconds) + " : " + juce::String(getDuration()));
    }

    //if the decoding method has reached the end of file and if the last frame has been displayed
//    if(endOfFileReached && newPositionSeconds >= getDuration())
//    {
//        DBG("End at position: " + juce::String(newPositionSeconds));
//        videoListeners.call (&FFmpegVideoListener::videoEnded);
//    }
}

double FFmpegVideoDecodeThread::getCurrentPositionSeconds () const
{
    return currentPositionSeconds;
}

int FFmpegVideoDecodeThread::getVideoWidth () const
{
    if (videoContext) {
        return videoContext->width;
    }
    return 0;
}

int FFmpegVideoDecodeThread::getVideoHeight () const
{
    if (videoContext) {
        return videoContext->height;
    }
    return 0;
}

enum AVPixelFormat FFmpegVideoDecodeThread::getPixelFormat () const
{
    if (videoContext)
        return videoContext->pix_fmt;
    return AV_PIX_FMT_NONE;
}

enum AVSampleFormat FFmpegVideoDecodeThread::getSampleFormat () const
{
    if (audioContext)
        return audioContext->sample_fmt;
    return AV_SAMPLE_FMT_NONE;
}

double FFmpegVideoDecodeThread::getVideoAspectRatio () const
{
    return static_cast<double> (getVideoWidth()) /
        static_cast<double> (getVideoHeight());
}

double FFmpegVideoDecodeThread::getPixelAspectRatio () const
{
    if (videoContext && videoContext->sample_aspect_ratio.num > 0)
        return av_q2d (videoContext->sample_aspect_ratio);
    return 1.0;
}

double FFmpegVideoDecodeThread::getFramesPerSecond () const
{
    //if frame rate is set in video context
    if (videoContext && videoContext->framerate.num > 0)
    {
        return av_q2d (videoContext->framerate);
    }
    //else look in stream
    if (formatContext && juce::isPositiveAndBelow (videoStreamIndex, static_cast<int>(formatContext->nb_streams)))
    {
        AVRational avg_framerate = formatContext->streams [videoStreamIndex]->avg_frame_rate;
        if (avg_framerate.num > 0)
            return static_cast<double>(avg_framerate.num);
        else if (avg_framerate.den > 0)
            return static_cast<double>(avg_framerate.den);
        else
            return 0.0;
    }
    return 0;
}

double FFmpegVideoDecodeThread::getSampleRate () const
{
    if (audioContext) {
        return audioContext->sample_rate;
    }
    return 0;
}

AVRational FFmpegVideoDecodeThread::getAudioTimeBase() const
{
    if (formatContext)
    {
        if (juce::isPositiveAndBelow (audioStreamIndex, static_cast<int> (formatContext->nb_streams)))
        {
            return formatContext->streams [audioStreamIndex]->time_base;
        }
    }
    return av_make_q (0, 1);
}

AVRational FFmpegVideoDecodeThread::getVideoTimeBase() const
{
        if (formatContext)
        {
            if (juce::isPositiveAndBelow (videoStreamIndex, static_cast<int> (formatContext->nb_streams)))
            {
                return formatContext->streams [videoStreamIndex]->time_base;
            }
        }
        return av_make_q (0, 1);
}

double FFmpegVideoDecodeThread::getDuration () const
{
    if (formatContext)
    {
        auto ret = static_cast<double>(formatContext->duration)  *  CB_AV_TIME_BASE_INVERSE;
        return ret;
    }
    return 0;
}

int FFmpegVideoDecodeThread::getNumberOfAudioChannels () const
{
    if (audioContext) {
        return audioContext->ch_layout.nb_channels;
    }
    return 0;
}

AVCodecContext* FFmpegVideoDecodeThread::getVideoContext () const
{
    return videoContext;
}

AVCodecContext* FFmpegVideoDecodeThread::getAudioContext () const
{
    return audioContext;
}

bool FFmpegVideoDecodeThread::isVideoOpen() const
{
    return _isVideoOpen;
}

void FFmpegVideoDecodeThread::addVideoListener (FFmpegVideoListener* listener)
{
    videoListeners.add (listener);
}

void FFmpegVideoDecodeThread::removeVideoListener (FFmpegVideoListener* listener)
{
    videoListeners.remove (listener);
}

