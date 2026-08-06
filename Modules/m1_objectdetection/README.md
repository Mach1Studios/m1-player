# m1_objectdetection
Dependency for JUCE projects to add image or video object detection for auto-panning use cases after a user has supplied a reference object image for the target object to be detected and tracked.

## Implementation
This module provides a JUCE-compatible object detection system using template matching algorithms. The implementation includes:

### Core Classes
- `Mach1::ObjectDetector`: Main detection class
- `Mach1::DetectedObject`: Structure representing a detected object with position, bounds, and confidence

### Key Features
- **Template Matching**: Uses normalized cross-correlation for object detection
- **Proximity-Based Tracking**: Weights detections based on proximity to last known position
- **JUCE Integration**: Native support for `juce::Image` and JUCE data structures
- **Configurable Parameters**: Adjustable confidence threshold and proximity weighting
- **Multi-Object Detection**: Support for detecting multiple instances of the reference object

### Usage
```cpp
#include "m1_objectdetection.h"

// Create detector
Mach1::ObjectDetector detector;

// Set reference object from user selection
juce::Rectangle<int> selection(100, 100, 50, 50);
juce::Image referenceObject = videoFrame.getClippedImage(selection);
detector.setReferenceObject(referenceObject);

// Process video frames
for (auto& frame : videoFrames) {
    auto objectCenter = detector.detectObjectCenter(frame);
    if (!objectCenter.isOrigin()) {
        // Use objectCenter for auto-panning
        float panPosition = (objectCenter.getX() / frame.getWidth() - 0.5f) * 2.0f;
    }
}
```

### Configuration
- `setConfidenceThreshold(float)`: Set minimum confidence for valid detections (0.0-1.0)
- `setProximityWeight(float)`: Set weight for proximity-based tracking (0.0-1.0)
- `resetTracking()`: Reset tracking state
