#pragma once

#include <JuceHeader.h>

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "Config.h"
#include "UI/VideoPlayerWidget.h"
#include "UI/MainWidget.h"
#include "Mach1Decode.h"

#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public murka::JuceMurkaBaseComponent,
    public juce::AudioAppComponent,
    public juce::FileDragAndDropTarget,
    public foleys::TimeCodeAware::Listener,
	public murka::View<MainComponent>
{
    //==============================================================================

    foleys::VideoEngine videoEngine;

	std::shared_ptr<foleys::AVClip> clipVideo;
	std::shared_ptr<foleys::AVClip> clipAudio;

	juce::AudioBuffer<float> tempBuffer;

	juce::AudioTransportSource  transportSourceVideo;
	juce::AudioTransportSource  transportSourceAudio;

	double                      sampleRate = 0.0;
    int                         blockSize = 0;
    int                         ffwdSpeed = 2;

    Mach1Decode m1Decode;
    std::vector<float> spatialMixerCoeffs;
    std::vector<juce::LinearSmoothedValue<float>> smoothedChannelCoeffs;
	juce::AudioBuffer<float> readBufferAudio;
	juce::AudioBuffer<float> readBufferVideo;
	int detectedNumInputChannels;
    
	Mach1Point3D currentOrientation = { 0, 0, 0 };

public:
    //==============================================================================
    MainComponent();
	virtual ~MainComponent();

    //==============================================================================
    void initialise() override;
    void shutdown() override;
    void render() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile(juce::File filepath);

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void timecodeChanged(int64_t, double seconds) override;

    //==============================================================================
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

private:
    //==============================================================================
    // Your private member variables go here...
	MurImage imgVideo;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

// (This function is called by the app startup code to create our main component)
juce::Component* createMainContentComponent() { return (juce::OpenGLAppComponent*)new MainComponent(); }
