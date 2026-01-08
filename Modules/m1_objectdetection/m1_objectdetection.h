/*******************************************************************************
 The block below describes the properties of this module, and is read by
 the Projucer to automatically generate project code that uses it.
 For details about the syntax and how to create or use a module, see the
 JUCE Module Format.md file.

 BEGIN_JUCE_MODULE_DECLARATION

  ID:                 m1_objectdetection
  vendor:             Mach1
  version:            0.0.1
  name:               Mach1 Object Detection
  description:        Object detection and tracking for auto-panning use cases
  website:            https://mach1.tech
  license:            Proprietary
  dependencies:       juce_core juce_graphics juce_gui_basics

 END_JUCE_MODULE_DECLARATION

*******************************************************************************/

#pragma once

#include <vector>
#include <memory>
#include <functional>

#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Config: M1_OBJECTDETECTION_ENABLED
    Enables the object detection functionality. Set to 0 to disable.
*/
#ifndef M1_OBJECTDETECTION_ENABLED
  #define M1_OBJECTDETECTION_ENABLED 1
#endif

#if M1_OBJECTDETECTION_ENABLED

namespace Mach1
{

//==============================================================================
/** A structure representing a detected object with its position and confidence score. */
struct DetectedObject
{
    juce::Point<float> centerPosition;  /**< Center X,Y coordinate of the detected object */
    juce::Rectangle<float> bounds;      /**< Bounding rectangle of the detected object */
    float confidence;                   /**< Confidence score (0.0 to 1.0) */
    
    DetectedObject() : confidence(0.0f) {}
    DetectedObject(juce::Point<float> center, juce::Rectangle<float> rect, float conf)
        : centerPosition(center), bounds(rect), confidence(conf) {}
};

//==============================================================================
/** 
    Object detection and tracking class for auto-panning use cases.
    
    This class provides functionality to detect and track objects in video frames
    based on a reference object image supplied by the user.
    
    Supports asynchronous (threaded) operation mode.
*/
class ObjectDetector
{
public:
    //==============================================================================
    /** Callback function type for asynchronous object detection results.
        
        @param detectedCenter   Center coordinate of detected object (0,0 if not found)
        @param frameWidth       Width of the processed frame
        @param frameHeight      Height of the processed frame
        @param processingTimeMs Time taken to process the frame in milliseconds
    */
    using DetectionCallback = std::function<void(juce::Point<float> detectedCenter, 
                                                int frameWidth, int frameHeight, 
                                                double processingTimeMs)>;
    
    //==============================================================================
    ObjectDetector();
    ~ObjectDetector();
    
    //==============================================================================
    /** Sets the reference object image for detection.
        
        @param imageData        2D rectangular vector of pixels representing the target object
        @param width            Width of the reference image
        @param height           Height of the reference image
        @param channels         Number of color channels (1 for grayscale, 3 for RGB, 4 for RGBA)
        @returns                true if the reference image was successfully set
    */
    bool setReferenceObject(const std::vector<std::vector<uint8_t>>& imageData, 
                           int width, int height, int channels);
    
    /** Sets the reference object image from a JUCE Image.
        
        @param referenceImage   JUCE Image containing the reference object
        @returns                true if the reference image was successfully set
    */
    bool setReferenceObject(const juce::Image& referenceImage);
    
    //==============================================================================
    // ASYNCHRONOUS API (threaded processing)
    
    /** Starts asynchronous object detection processing.
        
        @param callback         Function to call when object detection completes
        @param frameSkipCount   Process every Nth frame (default: 5)
        @returns                true if threading was started successfully
    */
    bool startAsyncDetection(DetectionCallback callback, int frameSkipCount = 5);
    
    /** Stops asynchronous object detection processing. */
    void stopAsyncDetection();
    
    /** Submits a frame for asynchronous processing.
        
        @param frameImage       JUCE Image containing the current frame
        @returns                true if frame was successfully queued
    */
    bool submitFrame(const juce::Image& frameImage);
    
    /** Returns whether asynchronous processing is currently active. */
    bool isAsyncDetectionActive() const;
    
    //==============================================================================
    /** Returns a vector of closest matching objects with confidence scores.
        
        @param frameData        2D rectangular vector of pixels representing the current frame
        @param frameWidth       Width of the current frame
        @param frameHeight      Height of the current frame
        @param channels         Number of color channels
        @param maxMatches       Maximum number of matches to return
        @returns                Vector of DetectedObject structures with confidence scores
    */
    std::vector<DetectedObject> detectObjects(const std::vector<std::vector<uint8_t>>& frameData,
                                            int frameWidth, int frameHeight, int channels,
                                            int maxMatches = 5);
    
    /** Returns a vector of closest matching objects from a JUCE Image.
        
        @param frameImage       JUCE Image containing the current frame
        @param maxMatches       Maximum number of matches to return
        @returns                Vector of DetectedObject structures with confidence scores
    */
    std::vector<DetectedObject> detectObjects(const juce::Image& frameImage, int maxMatches = 5);
    
    //==============================================================================
    /** Sets the minimum confidence threshold for object detection.
        
        @param threshold        Minimum confidence score (0.0 to 1.0) for a detection to be considered valid
    */
    void setConfidenceThreshold(float threshold);
    
    /** Gets the current confidence threshold. */
    float getConfidenceThreshold() const;
    
    /** Sets the proximity weight factor for tracking continuity.
        
        @param weight           Weight factor (0.0 to 1.0) for proximity-based tracking
    */
    void setProximityWeight(float weight);
    
    /** Gets the current proximity weight factor. */
    float getProximityWeight() const;
    
    /** Resets the tracking state (clears last known object position). */
    void resetTracking();
    
    /** Returns whether a reference object has been set. */
    bool hasReferenceObject() const;

private:
    //==============================================================================
    struct Impl;
    std::unique_ptr<Impl> pimpl;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectDetector)
};

} // namespace Mach1

#endif // M1_OBJECTDETECTION_ENABLED 