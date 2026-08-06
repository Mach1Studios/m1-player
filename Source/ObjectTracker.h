#pragma once

#include <JuceHeader.h>
#include <m1_objectdetection/m1_objectdetection.h>

/**
 * ObjectTracker - A wrapper class for integrating object detection with the M1-Player.
 * 
 * This class manages the object detection lifecycle, provides a simple API for:
 * - Setting a reference object from a marquee selection
 * - Processing video frames asynchronously
 * - Getting tracked object position for visualization
 * - Converting tracked positions to audio panning values
 */
class ObjectTracker
{
public:
    //==============================================================================
    /** Tracking mode enumeration */
    enum class TrackingMode
    {
        Disabled,       // No tracking active
        Selecting,      // User is drawing marquee selection
        Tracking        // Actively tracking the selected object
    };

    /** Tracking result structure */
    struct TrackingResult
    {
        bool objectFound = false;
        juce::Point<float> centerPosition;          // Pixel position in video frame
        juce::Point<float> normalizedPosition;      // 0.0 to 1.0 normalized position
        juce::Rectangle<float> bounds;              // Bounding rectangle
        float confidence = 0.0f;
        double processingTimeMs = 0.0;
    };

    //==============================================================================
    ObjectTracker();
    ~ObjectTracker();

    //==============================================================================
    /** Sets the reference object from a selection on a video frame.
        @param videoFrame The current video frame image
        @param selectionRect The rectangle selected by the user (in video frame coordinates)
        @returns true if the reference was set successfully
    */
    bool setReferenceFromSelection(const juce::Image& videoFrame, 
                                   const juce::Rectangle<int>& selectionRect);

    /** Clears the current reference object and stops tracking. */
    void clearReference();

    /** Returns whether a reference object has been set. */
    bool hasReference() const;

    //==============================================================================
    /** Starts asynchronous tracking.
        @param frameSkipCount Process every Nth frame (default: 3 for better responsiveness)
        @returns true if tracking started successfully
    */
    bool startTracking(int frameSkipCount = 3);

    /** Stops asynchronous tracking. */
    void stopTracking();

    /** Returns the current tracking mode. */
    TrackingMode getTrackingMode() const { return trackingMode; }

    /** Sets the tracking mode. */
    void setTrackingMode(TrackingMode mode) { trackingMode = mode; }

    /** Returns whether tracking is currently active. */
    bool isTracking() const;

    //==============================================================================
    /** Submits a video frame for asynchronous processing.
        @param videoFrame The current video frame
        @returns true if the frame was submitted successfully
    */
    bool submitFrame(const juce::Image& videoFrame);

    /** Gets the latest tracking result. Thread-safe. */
    TrackingResult getLatestResult() const;

    //==============================================================================
    /** Sets the confidence threshold for detection.
        @param threshold Value between 0.0 and 1.0 (default: 0.5)
    */
    void setConfidenceThreshold(float threshold);

    /** Gets the current confidence threshold. */
    float getConfidenceThreshold() const;

    /** Sets the proximity weight for tracking continuity.
        @param weight Value between 0.0 and 1.0 (default: 0.4)
    */
    void setProximityWeight(float weight);

    /** Gets the current proximity weight. */
    float getProximityWeight() const;

    /** Resets the tracking state (useful when seeking in video). */
    void resetTrackingState();

    //==============================================================================
    // Selection State Management
    
    /** Called when user starts drawing a selection. */
    void beginSelection(juce::Point<float> startPoint);

    /** Called as user drags to update selection. */
    void updateSelection(juce::Point<float> currentPoint);

    /** Called when user finishes drawing selection. */
    void endSelection();

    /** Cancels the current selection. */
    void cancelSelection();

    /** Returns the current selection rectangle (in normalized 0-1 coordinates). */
    juce::Rectangle<float> getCurrentSelection() const { return currentSelection; }

    /** Returns whether a selection is currently being drawn. */
    bool isSelecting() const { return trackingMode == TrackingMode::Selecting; }

    //==============================================================================
    /** Callback when tracking result is updated. Called on message thread. */
    std::function<void(const TrackingResult&)> onTrackingUpdate;

    /** Callback when tracking is lost. Called on message thread. */
    std::function<void()> onTrackingLost;

private:
    //==============================================================================
    std::unique_ptr<Mach1::ObjectDetector> detector;
    TrackingMode trackingMode = TrackingMode::Disabled;
    
    // Selection state
    juce::Point<float> selectionStartPoint;
    juce::Rectangle<float> currentSelection;
    
    // Thread-safe result storage
    mutable juce::CriticalSection resultLock;
    TrackingResult latestResult;
    
    // Frame dimensions (updated with each submitted frame)
    std::atomic<int> lastFrameWidth { 0 };
    std::atomic<int> lastFrameHeight { 0 };
    
    // Reference image for visualization
    juce::Image referenceImage;
    
    //==============================================================================
    void handleDetectionResult(juce::Point<float> detectedCenter, 
                               int frameWidth, int frameHeight, 
                               double processingTimeMs);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ObjectTracker)
};
