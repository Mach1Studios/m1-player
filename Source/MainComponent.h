#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"
#include "UI/VideoPlayerWidget.h"
#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"
#include <Mach1Decode.h>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent   : public murka::JuceMurkaBaseComponent,
    public juce::AudioAppComponent,
    public juce::FileDragAndDropTarget,
    public foleys::TimeCodeAware::Listener
{
    //==============================================================================

    foleys::VideoEngine videoEngine;

    std::shared_ptr<foleys::AVClip> clip;

    juce::AudioTransportSource  transportSource;
    double                      sampleRate = 0.0;
    int                         blockSize = 0;
    int                         ffwdSpeed = 2;

    Mach1Decode m1Decode;
    std::vector<float> spatialMixerCoeffs;
    std::vector<juce::LinearSmoothedValue<float>> smoothedChannelCoeffs;
    juce::AudioBuffer<float> tempBuffer;
    int detectedNumInputChannels;
    
public:
    //==============================================================================
    MainComponent();
    ~MainComponent();

    //==============================================================================
    void initialise() override;
    void shutdown() override;
    void render() override;

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile(juce::File name);

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
    MurImage imgLogo;
    
    M1OrientationClientWindow orientationControlWindow;
    bool showOrientationControlMenu = false;
    bool showedOrientationControlBefore = false;
    int DEBUG_orientationDeviceSelected = -1;
    bool DEBUG_trackYaw = true, DEBUG_trackPitch = true, DEBUG_trackRoll = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

// (This function is called by the app startup code to create our main component)
juce::Component* createMainContentComponent() { return (juce::OpenGLAppComponent*)new MainComponent(); }
