#pragma once

#include <JuceHeader.h>

struct M1PannerSettings {
    int port;
    int input_mode;
    float azi;
    float ele;
    float div;
    float gain;
};

struct find_panner {
    int port;

    find_panner(int port) : port(port) {}

    bool operator()(const M1PannerSettings& p) const {
        return p.port == port;
    }
};