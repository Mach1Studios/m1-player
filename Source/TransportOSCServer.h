//
//  m1-orientationmanager
//  Copyright © 2022 Mach1. All rights reserved.
//

#pragma once

#include <JuceHeader.h>

class TransportOSCServer : private juce::OSCReceiver::Listener<juce::OSCReceiver::RealtimeCallback>
{
    juce::OSCReceiver receiver;

	int serverPort = 0;

	void oscMessageReceived(const juce::OSCMessage& message) override {
        // TODO: Remove this, we get transport updates from orientationClient not here
        if (message.getAddressPattern() == "/transport") {
			correctTimeInSeconds = message[0].getFloat32();
			isPlaying = message[1].getInt32();
            
            if (isPlaying != wasPlaying) {
                DBG("[Playhead] isPlaying=" + std::to_string(isPlaying) + " , at " + std::to_string(correctTimeInSeconds) + " seconds");
            }
            
            // Update for debug messaging
            wasPlaying = isPlaying;
		}
	}

public:
	float correctTimeInSeconds = false;
	bool isPlaying = false;
    bool wasPlaying = false;

	TransportOSCServer() {
	}

	bool init(int serverPort) {
		// check the port
		juce::DatagramSocket socket(false);
		socket.setEnablePortReuse(false);
		if (socket.bindToPort(serverPort)) {
			socket.shutdown();

			receiver.connect(serverPort);
			receiver.addListener(this);

			this->serverPort = serverPort;

			return true;
		}
		return false;
	}

	void close() {
		receiver.removeListener(this);
		receiver.disconnect();
	}

	~TransportOSCServer() override {
		close();
	}
};
