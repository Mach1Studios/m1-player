#pragma once

#include <JuceHeader.h>

#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"

#include "Config.h"
#include "Mach1Decode.h"
#include "Mach1Encode.h"
#include "Mach1Transcode.h"
#include "Mach1TranscodeConstants.h"
#include "TypesForDataExchange.h"
#include "PlayerOSC.h"

#include "FFmpegVCMediaObject.h"
#include "UI/M1PlayerControls.h"

#include "UI/M1Checkbox.h"
#include "UI/M1Slider.h"
#include "UI/M1DropdownButton.h"
#include "UI/M1DropdownMenu.h"
#include "UI/VideoPlayerWidget.h"
#include "UI/RadioGroupWidget.h"

#include "m1_orientation_client/UI/M1Label.h"
#include "m1_orientation_client/UI/M1OrientationWindowToggleButton.h"
#include "m1_orientation_client/UI/M1OrientationClientWindow.h"

// Object Detection Module
#include "../Modules/m1_objectdetection/m1_objectdetection.h"

#include <iomanip>
#include <sstream>
#include <string>
#include <mutex>
#include <atomic>

//==============================================================================
// Audio Settings LookAndFeel
// TODO: Remove this and instead re-implement via Murka
class M1AudioSettingsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    M1AudioSettingsLookAndFeel()
    {
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(BACKGROUND_GREY));
        setColour(juce::PopupMenu::backgroundColourId, juce::Colour(BACKGROUND_COMPONENT));
        setColour(juce::PopupMenu::textColourId, juce::Colour(LABEL_TEXT_COLOR));
        setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(BACKGROUND_GREY));
        
        setColour(juce::ComboBox::backgroundColourId, juce::Colour(BACKGROUND_COMPONENT));
        setColour(juce::ComboBox::textColourId, juce::Colour(LABEL_TEXT_COLOR));
        setColour(juce::ComboBox::outlineColourId, juce::Colour(ENABLED_PARAM));
        setColour(juce::ComboBox::arrowColourId, juce::Colour(LABEL_TEXT_COLOR));
        
        setColour(juce::TextButton::buttonColourId, juce::Colour(BACKGROUND_COMPONENT));
        setColour(juce::TextButton::textColourOffId, juce::Colour(LABEL_TEXT_COLOR));
        setColour(juce::TextButton::buttonOnColourId, juce::Colour(BACKGROUND_GREY));
        
        setColour(juce::Label::textColourId, juce::Colour(LABEL_TEXT_COLOR));
        setColour(juce::Label::backgroundColourId, juce::Colours::transparentBlack);

        setColour(juce::ListBox::backgroundColourId, juce::Colour(BACKGROUND_COMPONENT));
        setColour(juce::ListBox::outlineColourId, juce::Colour(ENABLED_PARAM));
        setColour(juce::ListBox::textColourId, juce::Colour(LABEL_TEXT_COLOR));
    }

    void drawPopupMenuBackground(juce::Graphics& g, int width, int height) override
    {
        g.fillAll(findColour(juce::PopupMenu::backgroundColourId));
    }

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                     int buttonX, int buttonY, int buttonW, int buttonH,
                     juce::ComboBox& box) override
    {
        auto cornerSize = 3.0f;
        juce::Rectangle<int> boxBounds(0, 0, width, height);
        
        g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        g.setColour(box.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f, 0.5f), cornerSize, 1.0f);

        juce::Rectangle<int> arrowZone(width - 30, 0, 20, height);
        juce::Path path;
        path.startNewSubPath(arrowZone.getX() + 3.0f, arrowZone.getCentreY() - 2.0f);
        path.lineTo(static_cast<float>(arrowZone.getCentreX()), arrowZone.getCentreY() + 3.0f);
        path.lineTo(arrowZone.getRight() - 3.0f, arrowZone.getCentreY() - 2.0f);

        g.setColour(box.findColour(juce::ComboBox::arrowColourId));
        g.strokePath(path, juce::PathStrokeType(2.0f));
    }

    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                            bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto cornerSize = 3.0f;
        auto bounds = button.getLocalBounds().toFloat().reduced(0.5f, 0.5f);

        auto baseColour = backgroundColour.withMultipliedSaturation(button.hasKeyboardFocus(true) ? 1.3f : 0.9f)
                                       .withMultipliedAlpha(button.isEnabled() ? 1.0f : 0.5f);

        if (shouldDrawButtonAsDown || shouldDrawButtonAsHighlighted)
            baseColour = baseColour.contrasting(shouldDrawButtonAsDown ? 0.2f : 0.05f);

        g.setColour(baseColour);
        g.fillRoundedRectangle(bounds, cornerSize);

        g.setColour(button.findColour(juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle(bounds, cornerSize, 1.0f);
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                         bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        // This handles the "Hide Advanced Settings" button
        auto edge = 4;

        auto bounds = button.getLocalBounds();
        auto textBounds = bounds.withTrimmedLeft(edge);

        g.setColour(button.findColour(juce::ToggleButton::textColourId));
        g.drawFittedText(button.getButtonText(),
                         textBounds.getX(), textBounds.getY(),
                         textBounds.getWidth(), textBounds.getHeight(),
                         juce::Justification::centredLeft, 10);
    }

    void drawListBoxItem(juce::Graphics& g, int row, int width, int height,
                        juce::ListBox& listBox, bool rowIsSelected, int mouseOverRow,
                        bool /*rowIsClicked*/, bool /*shouldDrawAsEvenRow*/)
    {
        if (rowIsSelected)
            g.fillAll(juce::Colour(BACKGROUND_GREY));
        else if (row == mouseOverRow)
            g.fillAll(juce::Colour(BACKGROUND_GREY).withAlpha(0.5f));
    }

    void drawListBox(juce::Graphics& g, juce::ListBox& listBox,
                    const juce::Rectangle<int>& bounds, bool /*rowsInListAreDifferentHeights*/)
    {
        g.setColour(listBox.findColour(juce::ListBox::backgroundColourId));
        g.fillRect(bounds);

        g.setColour(listBox.findColour(juce::ListBox::outlineColourId));
        g.drawRect(bounds, 1);
    }
};

//==============================================================================

class MainComponent : public murka::JuceMurkaBaseComponent,
    public juce::FileDragAndDropTarget,
    public juce::Timer,
    public juce::MenuBarModel,
    public juce::ChangeListener,
    public juce::AudioIODeviceCallback
{
private:
    // UI Constants
    static constexpr int UI_HIDE_TIMEOUT_SECONDS = 5;

    //==============================================================================
    MurImage imgLogo;
    MurImage imgVideo;
    MurImage imgHideUI;
    MurImage imgUnhideUI;

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
    
    // Object Detection
    std::unique_ptr<Mach1::ObjectDetector> objectDetector;
    bool objectDetectionEnabled = false;
    bool hasReferenceObject = false;
    bool showObjectReticle = false;
    MurkaPoint objectReticlePosition;
    juce::Point<float> lastDetectedObjectCenter;
    bool callbackSetupComplete = false;  // Track if callback has been set up
    bool shouldClearSelection = false;  // Flag to clear selection rectangle in next draw call
    
    // Rectangle Selection for Object Detection
    bool isSelectingRectangle = false;
    juce::Rectangle<int> selectionRectangle;
    juce::Point<int> selectionStartPoint;
    juce::Point<int> selectionEndPoint;
    

    
    void updateObjectDetectionResult(const juce::Point<float>& detectedCenter, int frameWidth, int frameHeight, double processingTimeMs);

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
    
    std::vector<std::string> currentFormatOptions;
    std::string selectedInputFormat;
    std::string selectedOutputFormat = "M1Spatial-14"; // default
    std::atomic<bool> pendingFormatChange{false};

    juce::CriticalSection audioCallbackLock;
    juce::CriticalSection renderCallbackLock;

    std::map<int, std::vector<std::string>> matchingFormatNamesMap;

    std::vector<std::string> getMatchingFormatNames(int numChannels) {
        // Check if the numChannels already exists in the map
        auto it = matchingFormatNamesMap.find(numChannels);
        if (it != matchingFormatNamesMap.end()) {
            return it->second; // Return the existing list if numChannels is found
        }

        std::vector<std::string> matchingFormatNames;

        Mach1Transcode<float> m1TranscodeTemp;

        for (const auto& format : Mach1TranscodeConstants::formats) {
            if (format.numChannels == numChannels) {
                m1TranscodeTemp.setInputFormat(m1TranscodeTemp.getFormatFromString(format.name));

                /// INPUT PREFERRED OUTPUT OVERRIDE ASSIGNMENTS
                if (format.name == "3.0_LCR" || // NOTE: switch to M1Spatial-14 for center channel
                    format.name == "4.0_LCRS" || // NOTE: switch to M1Spatial-14 for center channel
                    format.name == "M1Horizon-4_2")
                {
                    m1TranscodeTemp.setOutputFormat(m1TranscodeTemp.getFormatFromString("M1Spatial-4"));
                }
                else if (format.name == "4.0_AFormat" ||
                    format.name == "Ambeo" ||
                    format.name == "TetraMic" ||
                    format.name == "SPS-200" ||
                    format.name == "ORTF3D" ||
                    format.name == "CoreSound-OctoMic" ||
                    format.name == "CoreSound-OctoMic_SIM")
                {
                    m1TranscodeTemp.setOutputFormat(m1TranscodeTemp.getFormatFromString("M1Spatial-8"));
                }
                else
                {
                    m1TranscodeTemp.setOutputFormat(m1TranscodeTemp.getFormatFromString("M1Spatial-14"));
                }

                if (m1TranscodeTemp.processConversionPath()) {
                    matchingFormatNames.push_back(format.name);
                }
            }
        }

        matchingFormatNamesMap[numChannels] = matchingFormatNames;
        return matchingFormatNames;
    }

    std::string getDefaultFormatForChannelCount(int numChannels) {
        switch (numChannels) {
            case 3:  return "3.0_LCR";
            case 4:  return "M1Spatial-4";
            case 5:  return "5.0_C";
            case 6:  return "5.1_C";
            case 7:  return "7.0_C";
            case 8:  return "M1Spatial-8";
            case 9:  return "ACNSN3DO2A";
            case 10: return "7.1.2_C";
            case 11: return "7.0.6_C";
            case 12: return "7.1.4_C";
            case 14: return "M1Spatial-14";
            case 16: return "ACNSN3DO3A";
            case 24: return "ACNSN3DO4A";
            case 36: return "ACNSN3DO5A";
            case 64: return "ACNSN3DO6A";
            default: return "";
        }
    }

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
    float errorOpacity = 0.0f;
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

    bool lastKnownMediaPlayState = false; // tracks the last known play state of the media for device changes
    double lastKnownMediaPosition = 0.0; // tracks the last known position of the media for device changes

    static constexpr int MAX_RECENT_FILES = 10;
    std::vector<juce::File> recentFiles;

    // Native OS Menu IDs
    enum MenuIDs
    {
        OpenFileMenuID = 1,
        SettingsMenuID = 2,
        View2DMenuID = 3,
        View3DMenuID = 4,
        FullScreenMenuID = 5,
        ToggleOverlayMenuID = 6,
        // Reserve IDs 7-9 for recent files
        RecentFileMenuID = 7
    };

    std::unique_ptr<juce::PropertiesFile> appProperties;
    void initializeAppProperties();
    void addToRecentFiles(const juce::File& file);
    void saveRecentFiles();
    void loadRecentFileList();

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
    void handleRectangleSelection(juce::Rectangle<int> selection);

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

    std::string getTranscodeInputFormat() const;
    std::string getTranscodeOutputFormat() const;
    void setTranscodeInputFormat(const std::string &name);
    void setTranscodeOutputFormat(const std::string &name);

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
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

// (This function is called by the app startup code to create our main component)
juce::Component* createMainContentComponent() { return (juce::OpenGLAppComponent*)new MainComponent(); }
