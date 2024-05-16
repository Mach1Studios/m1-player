#pragma once

#include <JuceHeader.h>

#include "juce_murka/Murka/MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "Config.h"
#include "Mach1Decode.h"
#include "Mach1Encode.h"
#include "TypesForDataExchange.h"
#include "TransportOSCServer.h"
#include "PlayerOSC.h"

#include "UI/VideoPlayerWidget.h"
#include "UI/RadioGroupWidget.h"

#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public murka::JuceMurkaBaseComponent,
    public juce::AudioAppComponent,
    public juce::FileDragAndDropTarget,
    public foleys::TimeCodeAware::Listener,
    public juce::Timer
{
    //==============================================================================
    MurImage imgLogo;
    MurImage imgVideo;

    M1OrientationClient m1OrientationClient;
    Mach1::Orientation currentOrientation;
    Mach1::Orientation previousClientOrientation;
    MurkaPoint3D prev_mouse_offset = { 0, 0, 0 }; // used to track if the player mouse offset has a new value
    M1OrientationClientWindow* orientationControlWindow;
    bool showOrientationControlMenu = false;
    bool showedOrientationControlBefore = false;
    
    // collect existing local panners for display
    std::vector<PannerSettings> panners;
    
    // search plugin by registered port number
    // TODO: potentially improve this with uuid concept
    struct find_plugin {
        int port;
        find_plugin(int port) : port(port) {}
        bool operator () ( const PannerSettings& p ) const
        {
            return p.port == port;
        }
    };

    float lastUpdateForPlayer = 0.0f;

    double currentPlayerWidgetFov = 0;
    bool drawReference = false;

    foleys::VideoEngine videoEngine;
    
    // TODO: make a check that changes this flag
    bool b_standalone_mode = false;

    std::shared_ptr<foleys::AVClip> clipVideo;
    std::shared_ptr<foleys::AVClip> clipAudio;

    juce::AudioBuffer<float> tempBuffer;

    juce::AudioTransportSource  transportSource;

    double                      sampleRate = 0.0;
    int                         blockSize = 0;
    int                         ffwdSpeed = 2;

    Mach1Decode m1Decode;
    std::vector<float> spatialMixerCoeffs;
    std::vector<juce::LinearSmoothedValue<float>> smoothedChannelCoeffs;
    juce::AudioBuffer<float> readBuffer;
    int detectedNumInputChannels;
    
    MurkaPoint lastScrollValue;
    bool bHideUI = false;
    
    // Communication to Monitor and the rest of the M1SpatialSystem
    void timerCallback() override;
    PlayerOSC playerOSC;
 
public:
    //==============================================================================
    MainComponent();
    virtual ~MainComponent();

    //==============================================================================
    void initialise() override;
    void shutdown() override;
    void draw();

    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

    void openFile(juce::File filepath);
    void setStatus(bool success, std::string message);

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;
    void timecodeChanged(int64_t, double seconds) override;

    //==============================================================================
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

protected:
    void syncWithDAWPlayhead();

private:
    //==============================================================================
    // Your private member variables go here...

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

// (This function is called by the app startup code to create our main component)
juce::Component* createMainContentComponent() { return (juce::OpenGLAppComponent*)new MainComponent(); }
