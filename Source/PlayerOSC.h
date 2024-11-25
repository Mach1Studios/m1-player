#pragma once

#include <JuceHeader.h>

#include "TypesForDataExchange.h"

class PlayerOSC : private juce::OSCSender, private juce::OSCReceiver, private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
    bool init(int helperPort);
    bool initFromSettings(std::string jsonSettingsFilePath);
    int helperPort = 0, port = 0;
    bool isConnected = false; // used to track connection with helper utility
    bool isActivePlayer = true; // used to track if this is the primary player instance
    std::function<void(juce::OSCMessage msg)> messageReceived;
    void oscMessageReceived(const juce::OSCMessage& msg) override;
    int num_monitor_instances = 0;

    juce::uint32 lastMessageTime = 0; // time of last update received

private:
    float playerPositionInSeconds = 0;
    float playerFrameRate = 0;
    bool playerIsPlaying = false;
    //int HH, MM, SS, FS;
    int playerLastUpdate = 0; // time of last update from DAW side
    
public:
    PlayerOSC();
    ~PlayerOSC();

    void update();
    void AddListener(std::function<void(juce::OSCMessage msg)> messageReceived);
    bool Send(const juce::OSCMessage& msg);
    bool IsConnected();
    void setAsActivePlayer(bool is_active);
    bool IsActivePlayer();
    int getNumberOfMonitors();
    bool sendPlayerYPR(float yaw, float pitch, float roll);
    bool connectToHelper();
    bool disconnectToHelper();
    
    // Transport
    float getPlayerPositionInSeconds() {
        return playerPositionInSeconds;
    }

    bool getPlayerIsPlaying() {
        return playerIsPlaying;
    }

    float getPlayerLastUpdate() {
        return playerLastUpdate;
    }
};
