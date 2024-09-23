
class M1PlayerMediaObject {
private:
    foleys::VideoEngine videoEngine;
    std::shared_ptr<foleys::AVClip> clip;
    juce::AudioTransportSource  transportSource;
    int blockSize = 32;
    int sampleRate = 22050;

public:
    
    M1PlayerMediaObject() {
        videoEngine.getFormatManager().registerFormat(std::make_unique<foleys::FFmpegFormat>());
    }
    
    ~M1PlayerMediaObject() {
        // Release resources
        
    }
    
    void start() {
        transportSource.start();
    }
    
    void stop() {
        transportSource.stop();
    }
    
    bool isPlaying() {
        return transportSource.isPlaying();
    }
    
    void releaseResources() {
        if (clipLoaded()) {
            clip->releaseResources();
        }
        transportSource.setSource(nullptr);
    }
    
    int getNumChannels() {
        return clip->getNumChannels();
    }
    
    void prepareToPlay(int sessionBlockSize, int sessionSampleRate) {
        blockSize = sessionBlockSize;
        sampleRate = sessionSampleRate;
        if (clip.get() != nullptr && (clip->hasVideo() || clip->hasAudio())) {
            clip->prepareToPlay(blockSize, sampleRate);
            transportSource.prepareToPlay(blockSize, sampleRate);
        }
            

    }
    
    bool hasVideo() {
        return clip->hasVideo();
    }
    
    bool hasAudio() {
        return clip->hasAudio();
    }
    
    bool clipLoaded() {
        return clip.get() != nullptr;
    }
    
    void setTimelinePosition(int64 timecodeInSamples) {
        clip->setNextReadPosition(timecodeInSamples);
    }
    
    double getLengthInSeconds() {
        return transportSource.getLengthInSeconds();
    }
    
    void getNextAudioBlock (const AudioSourceChannelInfo& info) {
        return transportSource.getNextAudioBlock(info);
    }
    
    juce::URL getMediaFilePath() {
        return clip->getMediaFile();
    }
    
    int64 getNextReadPositionInSamples() {
        return clip->getNextReadPosition();
    }
    
    int64 getAudioSampleRate() {
        return clip->getSampleRate();
    }
    
    foleys::VideoFrame& getFrame(double currentTimeInSeconds) {
        return clip->getFrame(currentTimeInSeconds);
    }
    
    void setGain(float newGain) {
        transportSource.setGain(newGain);
    }
    
    float getGain() {
        return transportSource.getGain();
    }
    
    double getCurrentTimelinePositionInSeconds() {
        return transportSource.getCurrentPosition();
    }
    
    void setCurrentTimelinePositionInSeconds(double position) {
        
    }
    
    void setPositionNormalized(double newPositionNormalized) {
        transportSource.setPosition(newPositionNormalized);
    }
    
    bool open(juce::URL filepath) {
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
