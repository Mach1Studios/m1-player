#pragma once

#include "Mach1Encode.h"

struct PannerSettings {
    int port = 0;
    std::string displayName = "";
    
    /// State Definitions:
    ///  - -1 = mark for deletion
    ///  -  0 = off / inactive
    ///  -  1 = on / active
    ///  -  2 = focused
    int state = 0;
    
    struct Color {
        uint8 r = 0;
        uint8 g = 0;
        uint8 b = 0;
        uint8 a = 0;
    } color;
    
    /// This object contains:
    /// - `Mach1EncodeInputModeType`
    /// - `Mach1EncodeOutputModeType`
    /// - `Mach1EncodePannerMode`
    Mach1Encode m1Encode;
    
    float x = 0.;
    float y = 70.7;
    float azimuth = 0.;
    float elevation = 0.; // also known as `z`
    float diverge = 50.;
    float gain = 6.;
    float stereoOrbitAzimuth = 0.;
    float stereoSpread = 50.;
    float stereoInputBalance = 0.;
    bool autoOrbit = true;
    bool overlay = false;
    bool isotropicMode = false;
    bool equalpowerMode = false;
    
#ifdef ITD_PARAMETERS
    bool itdActive = false;
    int delayTime = 600;
    float delayDistance = 1.0;
#endif
};

struct MixerSettings {
    int monitor_input_channel_count;
    int monitor_output_channel_count;
    float yaw;
    float pitch;
    float roll;
    int monitor_mode;

    bool yawActive, pitchActive, rollActive = true;
};

struct HostTimelineData {
    // Currently implmenting via JUCE 6, however JUCE 7 will change require a change to this struct design
    bool isPlaying;
    double playheadPositionInSeconds;
    
    // TODO: Implement the following after upgrading project to JUCE 7
    // double hostBPM; // Used to calculate loop points in seconds
    // double loopStartPositionInSeconds; // for more detailed indication on timeline indicator
    // double loopEndPositionInSeconds; // for more detailed indication on timeline indicator
    // double editOriginPositionInSeconds;
};

struct find_panner {
    int port;

    find_panner(int port) : port(port) {}

    bool operator()(const PannerSettings& p) const {
        return p.port == port;
    }
};
