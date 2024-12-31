#pragma once

#include <JuceHeader.h>

#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "Config.h"
#include "Mach1Decode.h"
#include "Mach1Encode.h"
#include "Mach1Transcode.h"
#include "TypesForDataExchange.h"
#include "PlayerOSC.h"

#include "FFmpegVCMediaObject.h"
#include "UI/M1PlayerControls.h"

#include "UI/M1Checkbox.h"
#include "UI/M1Slider.h"
#include "UI/M1DropdownButton.h"
#include "UI/VideoPlayerWidget.h"
#include "UI/RadioGroupWidget.h"

#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"

#include <iomanip>
#include <sstream>
#include <string>

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class MainComponent : public murka::JuceMurkaBaseComponent,
    public juce::FileDragAndDropTarget,
    public juce::Timer,
    public juce::MenuBarModel,
    public juce::ChangeListener,
    public juce::AudioIODeviceCallback
{
    //==============================================================================
    MurImage imgLogo;
    MurImage imgVideo;

    Mach1::Orientation currentOrientation;
    Mach1::Orientation previousClientOrientation;
    MurkaPoint3D prev_mouse_offset = { 0, 0, 0 }; // used to track if the player mouse offset has a new value

    // Orientation Manager/Client
    M1OrientationClient m1OrientationClient;
    M1OrientationClientWindow* orientationControlWindow;
    bool showOrientationControlMenu = false;
    bool showedOrientationControlBefore = false;

    void draw_orientation_client(murka::Murka &m, M1OrientationClient &m1OrientationClient);
    
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
    bool drawReference = false;
    float mediaVolume = 1.0;
    
    // Consolidate the media and transport into a single object class
    FFmpegVCMediaObject currentMedia;

    bool b_standalone_mode = false;
    bool b_wants_to_switch_to_standalone = false;

    double                      sampleRate = 0.0;
    int                         blockSize = 0;
    int                         ffwdSpeed = 2;

    // Mach1Decode API
    Mach1Decode<float> m1Decode;
    std::vector<float> spatialMixerCoeffs;
    std::vector<juce::LinearSmoothedValue<float>> smoothedChannelCoeffs;
    juce::AudioBuffer<float> tempBuffer;
    juce::AudioBuffer<float> readBuffer;
    juce::AudioBuffer<float> intermediaryBuffer;
    int detectedNumInputChannels;

    // Mach1Transcode API
    Mach1Transcode<float> m1Transcode;
    std::vector<float> transcodeToDecodeCoeffs;
    std::vector< std::vector<float> > conversionMatrix;

    std::string transcodeSurroundInputFormatName;
    std::string transcodeSurroundOutputFormatName;
    const std::string DEFAULT_SURROUND_INPUT_FORMAT = "5.1_C";
    const std::string DEFAULT_SURROUND_OUTPUT_FORMAT = "M1Spatial-14";

    std::string transcodeAmbisonicInputFormatName;
    std::string transcodeAmbisonicOutputFormatName;
    const std::string DEFAULT_AMBISONIC_INPUT_FORMAT = "ACNSN3DO2A";
    const std::string DEFAULT_AMBISONIC_OUTPUT_FORMAT = "M1Spatial-14";

    int secondsWithoutMouseMove = 0;
    MurkaPoint lastScrollValue;
    bool bHideUI = false;
    bool bShowHelpUI = false;
    bool showSettingsMenu = false;
    std::unique_ptr<juce::FileChooser> file_chooser;
    
    // Communication to Monitor and the rest of the M1SpatialSystem
    void timerCallback() override;
    std::unique_ptr<PlayerOSC> playerOSC;

    void (MainComponent::*m_decode_strategy)(const AudioSourceChannelInfo&, const AudioSourceChannelInfo&);
    void (MainComponent::*m_transcode_strategy)(const AudioSourceChannelInfo&, const AudioSourceChannelInfo&);

    // Error display
    bool showErrorPopup = false;
    std::string errorMessage = "";
    std::string errorMessageInfo = "";
    float fadeDuration = 5.0f;
    float errorOpacity = 1.0f;
    std::chrono::time_point<std::chrono::steady_clock> errorStartTime;
    
    std::string formatTime(double seconds) {
        int hours = static_cast<int>(seconds) / 3600;
        int minutes = (static_cast<int>(seconds) % 3600) / 60;
        int secs = static_cast<int>(seconds) % 60;

        std::ostringstream oss;
        oss << std::setfill('0');
        if (hours > 0) {
            oss << hours << ':' << std::setw(2);
        }
        oss << minutes << ':' << std::setw(2) << secs;
        return oss.str();
    }
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

    void reconfigureAudioDecode();
    void reconfigureAudioTranscode();
    void setDetectedInputChannelCount(int numberOfInputChannels);

    void openFile(juce::File filepath);
    void setStatus(bool success, std::string message);

    //==============================================================================
    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate);

    void fallbackDecodeStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);
    void stereoDecodeStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);
    void monoDecodeStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);
    void readBufferDecodeStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);
    void intermediaryBufferDecodeStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);
    void intermediaryBufferTranscodeStrategy(const AudioSourceChannelInfo & bufferToFill, const AudioSourceChannelInfo & info);
    void nullStrategy(const AudioSourceChannelInfo& bufferToFill, const AudioSourceChannelInfo& info);

    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void releaseResources();
    void timecodeChanged(int64_t, double seconds);

    //==============================================================================
    void showFileChooser();
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int, int) override;

    std::string getTranscodeSurroundInputFormat() const;
    std::string getTranscodeSurroundOutputFormat() const;
    std::string getTranscodeAmbisonicInputFormat() const;
    void setTranscodeSurroundInputFormat(const std::string &name);
    void setTranscodeSurroundOutputFormat(const std::string &name);
    void setTranscodeAmbisonicInputFormat(const std::string &name);
    void setTranscodeAmbisonicOutputFormat(const std::string &name);
    std::string getTranscodeAmbisonicOutputFormat() const;

    // Add these MenuBarModel required methods
    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    // ChangeListener method
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

    // AudioIODeviceCallback interface methods
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                        int numInputChannels,
                                        float* const* outputChannelData,
                                        int numOutputChannels,
                                        int numSamples,
                                        const juce::AudioIODeviceCallbackContext& context) override;
    
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

protected:
    void syncWithDAWPlayhead();

private:
    std::unique_ptr<juce::AudioDeviceSelectorComponent> audioDeviceSelector;
    juce::AudioDeviceManager audioDeviceManager;

    const long long smallestDAWSyncInterval = 500;
    long long lastTimeDAWSyncHappened = 0;

    void audioDeviceManagerChanged();
    void createMenuBar();
    juce::ApplicationCommandManager commandManager;
    
    enum MenuIDs
    {
        SettingsMenuID = 1,
        // Add other menu IDs here as needed
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

// (This function is called by the app startup code to create our main component)
juce::Component* createMainContentComponent() { return (juce::OpenGLAppComponent*)new MainComponent(); }
