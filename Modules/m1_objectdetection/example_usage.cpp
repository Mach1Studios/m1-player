/*******************************************************************************
 Example usage of M1 Object Detection module
 This file demonstrates how to use the ObjectDetector class for auto-panning
*******************************************************************************/

#include "m1_objectdetection.h"
#include <juce_core/juce_core.h>
#include <juce_graphics/juce_graphics.h>

// Example of how to use the ObjectDetector in a video player context
class VideoPlayerWithObjectDetection
{
public:
    VideoPlayerWithObjectDetection()
    {
        // Initialize the object detector
        detector = std::make_unique<Mach1::ObjectDetector>();
        
        // Set detection parameters
        detector->setConfidenceThreshold(0.6f);  // 60% confidence threshold
        detector->setProximityWeight(0.4f);      // 40% weight for proximity-based tracking
    }
    
    void setReferenceObjectFromSelection(const juce::Image& frameImage, 
                                       juce::Rectangle<int> selection)
    {
        // Extract the selected region as reference object
        if (selection.isEmpty() || !frameImage.isValid())
            return;
        
        // Create a sub-image from the selection
        juce::Image referenceObject = frameImage.getClippedImage(selection);
        
        // Set as reference object for detection
        if (detector->setReferenceObject(referenceObject))
        {
            juce::Logger::writeToLog("Reference object set successfully");
        }
        else
        {
            juce::Logger::writeToLog("Failed to set reference object");
        }
    }
    
    juce::Point<float> processVideoFrame(const juce::Image& currentFrame)
    {
        if (!currentFrame.isValid())
            return juce::Point<float>();
        
        // Detect the object in the current frame
        auto objectCenter = detector->detectObjectCenter(currentFrame);
        
        if (!objectCenter.isOrigin())
        {
            // Convert to normalized coordinates (0.0 to 1.0)
            float normalizedX = objectCenter.getX() / currentFrame.getWidth();
            float normalizedY = objectCenter.getY() / currentFrame.getHeight();
            
            // Log the detection
            juce::Logger::writeToLog(
                "Object detected at: (" + 
                juce::String(normalizedX, 3) + ", " + 
                juce::String(normalizedY, 3) + ")"
            );
            
            return juce::Point<float>(normalizedX, normalizedY);
        }
        
        return juce::Point<float>();
    }
    
    std::vector<Mach1::DetectedObject> findMultipleObjects(const juce::Image& currentFrame, 
                                                        int maxObjects = 3)
    {
        if (!currentFrame.isValid())
            return {};
        
        // Find multiple instances of the object
        auto detectedObjects = detector->detectObjects(currentFrame, maxObjects);
        
        // Log all detections
        for (size_t i = 0; i < detectedObjects.size(); ++i)
        {
            const auto& obj = detectedObjects[i];
            juce::Logger::writeToLog(
                "Object " + juce::String(i + 1) + 
                " detected at: (" + 
                juce::String(obj.centerPosition.getX(), 1) + ", " + 
                juce::String(obj.centerPosition.getY(), 1) + 
                ") with confidence: " + 
                juce::String(obj.confidence, 3)
            );
        }
        
        return detectedObjects;
    }
    
    void resetObjectTracking()
    {
        detector->resetTracking();
        juce::Logger::writeToLog("Object tracking reset");
    }
    
    bool hasReferenceObject() const
    {
        return detector->hasReferenceObject();
    }

private:
    std::unique_ptr<Mach1::ObjectDetector> detector;
};

// Example of how to integrate with audio panning
class AutoPanningController
{
public:
    AutoPanningController(VideoPlayerWithObjectDetection& videoPlayer)
        : videoPlayer(videoPlayer)
    {
    }
    
    void updatePanningFromVideoFrame(const juce::Image& frame)
    {
        if (!videoPlayer.hasReferenceObject())
            return;
        
        // Get the object position
        auto objectPos = videoPlayer.processVideoFrame(frame);
        
        if (!objectPos.isOrigin())
        {
            // Convert normalized position to pan value (-1.0 to 1.0)
            float panValue = (objectPos.getX() - 0.5f) * 2.0f;
            panValue = juce::jlimit(-1.0f, 1.0f, panValue);
            
            // Apply panning (this would connect to your audio engine)
            applyAudioPanning(panValue);
        }
    }
    
private:
    VideoPlayerWithObjectDetection& videoPlayer;
    
    void applyAudioPanning(float panValue)
    {
        // This would connect to your Mach1 Spatial System or audio engine
        juce::Logger::writeToLog(
            "Auto-panning audio to: " + juce::String(panValue, 3)
        );
    }
};