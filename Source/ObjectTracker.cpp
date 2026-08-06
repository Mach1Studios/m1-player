#include "ObjectTracker.h"

//==============================================================================
ObjectTracker::ObjectTracker()
    : detector(std::make_unique<Mach1::ObjectDetector>())
{
    // Set default parameters optimized for video tracking
    // Lower threshold for better detection (template matching typically gives lower scores)
    detector->setConfidenceThreshold(0.3f);
    detector->setProximityWeight(0.4f);
}

ObjectTracker::~ObjectTracker()
{
    stopTracking();
}

//==============================================================================
bool ObjectTracker::setReferenceFromSelection(const juce::Image& videoFrame, 
                                              const juce::Rectangle<int>& selectionRect)
{
    if (!videoFrame.isValid() || selectionRect.isEmpty())
        return false;

    // Ensure selection is within bounds
    auto clampedRect = selectionRect.getIntersection(videoFrame.getBounds());
    if (clampedRect.isEmpty() || clampedRect.getWidth() < 10 || clampedRect.getHeight() < 10)
    {
        DBG("[ObjectTracker] Selection too small or out of bounds");
        return false;
    }

    // Extract the selected region as reference
    referenceImage = videoFrame.getClippedImage(clampedRect);
    
    if (!detector->setReferenceObject(referenceImage))
    {
        DBG("[ObjectTracker] Failed to set reference object");
        referenceImage = juce::Image();
        return false;
    }

    DBG("[ObjectTracker] Reference object set: " + 
        juce::String(clampedRect.getWidth()) + "x" + 
        juce::String(clampedRect.getHeight()));
    
    return true;
}

void ObjectTracker::clearReference()
{
    stopTracking();
    detector = std::make_unique<Mach1::ObjectDetector>();
    detector->setConfidenceThreshold(0.3f);
    detector->setProximityWeight(0.4f);
    referenceImage = juce::Image();
    trackingMode = TrackingMode::Disabled;
    currentSelection = juce::Rectangle<float>();
    
    {
        const juce::ScopedLock lock(resultLock);
        latestResult = TrackingResult();
    }
    
    DBG("[ObjectTracker] Reference cleared");
}

bool ObjectTracker::hasReference() const
{
    return detector->hasReferenceObject();
}

//==============================================================================
bool ObjectTracker::startTracking(int frameSkipCount)
{
    if (!hasReference())
    {
        DBG("[ObjectTracker] Cannot start tracking without reference object");
        return false;
    }

    if (isTracking())
        return true; // Already tracking

    auto callback = [this](juce::Point<float> center, int width, int height, double time) {
        handleDetectionResult(center, width, height, time);
    };

    if (detector->startAsyncDetection(callback, frameSkipCount))
    {
        trackingMode = TrackingMode::Tracking;
        DBG("[ObjectTracker] Tracking started (frameSkip: " + juce::String(frameSkipCount) + ")");
        return true;
    }

    DBG("[ObjectTracker] Failed to start tracking");
    return false;
}

void ObjectTracker::stopTracking()
{
    if (detector->isAsyncDetectionActive())
    {
        detector->stopAsyncDetection();
        DBG("[ObjectTracker] Tracking stopped");
    }
    
    if (trackingMode == TrackingMode::Tracking)
        trackingMode = TrackingMode::Disabled;
}

bool ObjectTracker::isTracking() const
{
    return detector->isAsyncDetectionActive();
}

//==============================================================================
bool ObjectTracker::submitFrame(const juce::Image& videoFrame)
{
    if (!isTracking() || !videoFrame.isValid())
        return false;

    lastFrameWidth = videoFrame.getWidth();
    lastFrameHeight = videoFrame.getHeight();
    
    return detector->submitFrame(videoFrame);
}

ObjectTracker::TrackingResult ObjectTracker::getLatestResult() const
{
    const juce::ScopedLock lock(resultLock);
    return latestResult;
}

//==============================================================================
void ObjectTracker::setConfidenceThreshold(float threshold)
{
    detector->setConfidenceThreshold(threshold);
}

float ObjectTracker::getConfidenceThreshold() const
{
    return detector->getConfidenceThreshold();
}

void ObjectTracker::setProximityWeight(float weight)
{
    detector->setProximityWeight(weight);
}

float ObjectTracker::getProximityWeight() const
{
    return detector->getProximityWeight();
}

void ObjectTracker::resetTrackingState()
{
    detector->resetTracking();
    
    {
        const juce::ScopedLock lock(resultLock);
        latestResult.objectFound = false;
    }
    
    DBG("[ObjectTracker] Tracking state reset");
}

//==============================================================================
void ObjectTracker::beginSelection(juce::Point<float> startPoint)
{
    trackingMode = TrackingMode::Selecting;
    selectionStartPoint = startPoint;
    currentSelection = juce::Rectangle<float>(startPoint.x, startPoint.y, 0, 0);
}

void ObjectTracker::updateSelection(juce::Point<float> currentPoint)
{
    if (trackingMode != TrackingMode::Selecting)
        return;

    // Create rectangle from start and current points
    float left = juce::jmin(selectionStartPoint.x, currentPoint.x);
    float top = juce::jmin(selectionStartPoint.y, currentPoint.y);
    float right = juce::jmax(selectionStartPoint.x, currentPoint.x);
    float bottom = juce::jmax(selectionStartPoint.y, currentPoint.y);
    
    currentSelection = juce::Rectangle<float>(left, top, right - left, bottom - top);
}

void ObjectTracker::endSelection()
{
    if (trackingMode != TrackingMode::Selecting)
        return;

    // Selection will be processed when user confirms
    // Keep the selection visible until the reference is set
    trackingMode = TrackingMode::Disabled;
}

void ObjectTracker::cancelSelection()
{
    trackingMode = TrackingMode::Disabled;
    currentSelection = juce::Rectangle<float>();
    selectionStartPoint = juce::Point<float>();
}

//==============================================================================
void ObjectTracker::handleDetectionResult(juce::Point<float> detectedCenter, 
                                          int frameWidth, int frameHeight, 
                                          double processingTimeMs)
{
    TrackingResult result;
    result.processingTimeMs = processingTimeMs;
    
    if (!detectedCenter.isOrigin() && frameWidth > 0 && frameHeight > 0)
    {
        result.objectFound = true;
        result.centerPosition = detectedCenter;
        result.normalizedPosition.setX(detectedCenter.x / static_cast<float>(frameWidth));
        result.normalizedPosition.setY(detectedCenter.y / static_cast<float>(frameHeight));
        result.confidence = detector->getConfidenceThreshold(); // Approximate
        
        // Estimate bounds based on reference size
        if (referenceImage.isValid())
        {
            float refW = static_cast<float>(referenceImage.getWidth());
            float refH = static_cast<float>(referenceImage.getHeight());
            result.bounds = juce::Rectangle<float>(
                detectedCenter.x - refW / 2.0f,
                detectedCenter.y - refH / 2.0f,
                refW, refH
            );
        }
    }
    else
    {
        result.objectFound = false;
    }

    {
        const juce::ScopedLock lock(resultLock);
        latestResult = result;
    }

    // Notify callbacks on message thread
    if (result.objectFound)
    {
        if (onTrackingUpdate)
        {
            juce::MessageManager::callAsync([this, result]() {
                if (onTrackingUpdate)
                    onTrackingUpdate(result);
            });
        }
    }
    else
    {
        if (onTrackingLost)
        {
            juce::MessageManager::callAsync([this]() {
                if (onTrackingLost)
                    onTrackingLost();
            });
        }
    }
}
