/*******************************************************************************
 Implementation of M1 Object Detection module.
 Copyright (c) 2025 - Mach1
*******************************************************************************/

#include "m1_objectdetection.h"

#if M1_OBJECTDETECTION_ENABLED

namespace Mach1
{

//==============================================================================
struct ObjectDetector::Impl
{
    // Reference object data (stored at scaled size for matching)
    juce::Image referenceImage;          // Original reference
    juce::Image scaledReferenceImage;    // Scaled reference for matching
    int refWidth = 0;                    // Scaled reference width
    int refHeight = 0;                   // Scaled reference height
    int originalRefWidth = 0;            // Original reference width
    int originalRefHeight = 0;           // Original reference height
    
    // Detection parameters
    float confidenceThreshold = 0.30f;   // Slightly higher to avoid false positives
    float proximityWeight = 0.05f;       // Very low - almost no proximity bias
    
    // Scale factor for processing
    static constexpr int SCALE_FACTOR = 4;
    
    // Tracking quality metrics
    int consecutiveLowConfidenceFrames = 0;
    float lastGoodConfidence = 0.0f;
    
    // Tracking state
    juce::Point<float> lastKnownPosition;  // In scaled coordinates
    juce::Point<float> velocity;           // Movement velocity for prediction
    bool hasLastKnownPosition = false;
    int framesSinceGoodMatch = 0;          // Frames since we had a confident match
    
    // Multi-scale templates (wider range for significant size changes)
    static constexpr int NUM_SCALES = 9;
    struct ScaleTemplate {
        juce::Image image;
        int width = 0;
        int height = 0;
        float scale = 1.0f;
    };
    ScaleTemplate scaleTemplates[NUM_SCALES];  // 50%, 65%, 80%, 100%, 130%, 170%, 220%, 280%, 350%
    
    // Best matching scale from last frame (to prioritize nearby scales)
    int lastBestScaleIndex = 3;  // Start with 100%
    
    // Threading support
    class DetectionThread : public juce::Thread
    {
    public:
        DetectionThread(Impl* parent) : Thread("ObjectDetectionThread"), parentImpl(parent) {}
        
        void run() override
        {
            while (!threadShouldExit())
            {
                // Wait for new frame to process
                if (frameReadyEvent.wait(100)) // 100ms timeout
                {
                    if (threadShouldExit()) break;
                    
                    // Process the frame on this background thread
                    processFrame();
                }
            }
        }
        
        void submitFrame(const juce::Image& frame)
        {
            const juce::ScopedLock lock(frameLock);
            if (frame.isValid())
            {
                frameSkipCounter++;
                if (frameSkipCounter >= frameSkipCount)
                {
                    frameSkipCounter = 0;
                    currentFrame = frame.createCopy();
                    frameReadyEvent.signal();
                }
            }
        }
        
        void setFrameSkipCount(int count) { frameSkipCount = count; }
        void setCallback(ObjectDetector::DetectionCallback cb) { callback = cb; }
        
    private:
        void processFrame()
        {
            juce::Image frameToProcess;
            {
                const juce::ScopedLock lock(frameLock);
                if (!currentFrame.isValid()) return;
                frameToProcess = currentFrame.createCopy();
            }
            
            if (!parentImpl || !parentImpl->hasReferenceObjectInternal())
                return;
                
            auto startTime = juce::Time::getMillisecondCounterHiRes();
            
            // Scale down the frame for faster processing
            int scaledWidth = frameToProcess.getWidth() / SCALE_FACTOR;
            int scaledHeight = frameToProcess.getHeight() / SCALE_FACTOR;
            
            juce::Image scaledFrame = frameToProcess.rescaled(scaledWidth, scaledHeight, 
                                                              juce::Graphics::lowResamplingQuality);
            
            // Find best match using template matching
            auto detected = parentImpl->findBestMatchFast(scaledFrame);
            
            auto endTime = juce::Time::getMillisecondCounterHiRes();
            double processingTime = endTime - startTime;
            
            DBG("[ObjectDetection] Processing: " + juce::String(processingTime, 1) + 
                "ms, confidence: " + juce::String(detected.confidence, 3));
            
            // Scale the detected coordinates back up to original frame size
            juce::Point<float> detectedCenter;
            if (detected.confidence >= parentImpl->confidenceThreshold)
            {
                detectedCenter.setX(detected.centerPosition.getX() * SCALE_FACTOR);
                detectedCenter.setY(detected.centerPosition.getY() * SCALE_FACTOR);
                
                // Update tracking state (in scaled coordinates)
                parentImpl->lastKnownPosition = detected.centerPosition;
                parentImpl->hasLastKnownPosition = true;
                
                DBG("[ObjectDetection] FOUND at (" + juce::String(detectedCenter.getX(), 0) + 
                    ", " + juce::String(detectedCenter.getY(), 0) + ")");
            }
            else
            {
                DBG("[ObjectDetection] Not found (confidence " + 
                    juce::String(detected.confidence, 3) + " < " + 
                    juce::String(parentImpl->confidenceThreshold, 3) + ")");
            }
            
            // Call the callback on the message thread
            if (callback)
            {
                juce::MessageManager::callAsync([this, detectedCenter, frameToProcess, processingTime]() {
                    if (callback)
                    {
                        callback(detectedCenter, frameToProcess.getWidth(), frameToProcess.getHeight(), processingTime);
                    }
                });
            }
        }
        
        Impl* parentImpl;
        juce::CriticalSection frameLock;
        juce::WaitableEvent frameReadyEvent;
        juce::Image currentFrame;
        int frameSkipCount = 5;
        int frameSkipCounter = 0;
        ObjectDetector::DetectionCallback callback;
    };
    
    std::unique_ptr<DetectionThread> detectionThread;
    bool asyncDetectionActive = false;
    
    // Fast template matching using direct pixel comparison
    DetectedObject findBestMatchFast(const juce::Image& scaledFrame);
    
    // Calculate similarity between two image regions
    float calculateSimilarityFast(const juce::Image& frame, int frameX, int frameY,
                                  const juce::Image& reference);
    
    // Color histogram for reference image (helps distinguish objects with same luminance)
    std::array<int, 16> referenceHueHistogram;  // 16-bin hue histogram
    float referenceAvgSaturation = 0.0f;
    
    // Calculate hue histogram for a region
    void calculateHueHistogram(const juce::Image& img, int x, int y, int w, int h,
                               std::array<int, 16>& histogram, float& avgSaturation);
    
    // Compare two histograms
    float compareHistograms(const std::array<int, 16>& hist1, const std::array<int, 16>& hist2);
    
    // Calculate variance of a region (to reject uniform areas like walls)
    float calculateRegionVariance(const juce::Image& frame, int x, int y, int w, int h);
    
    // Reference image variance (for comparison)
    float referenceVariance = 0.0f;
    
    bool hasReferenceObjectInternal() const
    {
        return scaledReferenceImage.isValid() && refWidth > 0 && refHeight > 0;
    }
};

//==============================================================================
ObjectDetector::ObjectDetector()
    : pimpl(std::make_unique<Impl>())
{
}

ObjectDetector::~ObjectDetector() 
{
    // Stop async processing if active
    stopAsyncDetection();
}

//==============================================================================
bool ObjectDetector::setReferenceObject(const std::vector<std::vector<uint8_t>>& imageData,
                                       int width, int height, int channels)
{
    // Create a JUCE image from the raw data
    juce::Image img(channels == 4 ? juce::Image::ARGB : juce::Image::RGB, width, height, true);
    juce::Image::BitmapData bitmapData(img, juce::Image::BitmapData::writeOnly);
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            int idx = y * width + x;
            if (idx < imageData.size() && imageData[idx].size() >= 3)
            {
                juce::Colour c(imageData[idx][0], imageData[idx][1], imageData[idx][2],
                              static_cast<juce::uint8>(channels == 4 && imageData[idx].size() >= 4 ? imageData[idx][3] : 255));
                bitmapData.setPixelColour(x, y, c);
            }
        }
    }
    
    return setReferenceObject(img);
}

bool ObjectDetector::setReferenceObject(const juce::Image& referenceImage)
{
    if (!referenceImage.isValid())
        return false;
    
    pimpl->referenceImage = referenceImage;
    pimpl->originalRefWidth = referenceImage.getWidth();
    pimpl->originalRefHeight = referenceImage.getHeight();
    
    // Scale reference to match the scaled frame size
    pimpl->refWidth = referenceImage.getWidth() / Impl::SCALE_FACTOR;
    pimpl->refHeight = referenceImage.getHeight() / Impl::SCALE_FACTOR;
    
    // Ensure minimum size for matching
    pimpl->refWidth = juce::jmax(8, pimpl->refWidth);
    pimpl->refHeight = juce::jmax(8, pimpl->refHeight);
    
    pimpl->scaledReferenceImage = referenceImage.rescaled(pimpl->refWidth, pimpl->refHeight,
                                                          juce::Graphics::lowResamplingQuality);
    
    // Create multi-scale templates for handling significant object distance changes
    // Scales: 50%, 65%, 80%, 100%, 130%, 170%, 220%, 280%, 350%
    // This allows tracking objects that grow up to 3.5x their original size
    const float scales[Impl::NUM_SCALES] = { 0.50f, 0.65f, 0.80f, 1.0f, 1.30f, 1.70f, 2.20f, 2.80f, 3.50f };
    
    for (int i = 0; i < Impl::NUM_SCALES; i++)
    {
        pimpl->scaleTemplates[i].scale = scales[i];
        pimpl->scaleTemplates[i].width = juce::jmax(6, (int)(pimpl->refWidth * scales[i]));
        pimpl->scaleTemplates[i].height = juce::jmax(6, (int)(pimpl->refHeight * scales[i]));
        pimpl->scaleTemplates[i].image = referenceImage.rescaled(
            pimpl->scaleTemplates[i].width, 
            pimpl->scaleTemplates[i].height,
            juce::Graphics::lowResamplingQuality);
    }
    
    pimpl->lastBestScaleIndex = 3;  // Reset to 100% (index 3 in our scale array)
    
    // Reset tracking state
    pimpl->hasLastKnownPosition = false;
    pimpl->velocity = juce::Point<float>(0, 0);
    pimpl->framesSinceGoodMatch = 0;
    
    // Calculate color histogram for reference
    pimpl->calculateHueHistogram(pimpl->scaledReferenceImage, 0, 0, pimpl->refWidth, pimpl->refHeight,
                                  pimpl->referenceHueHistogram, pimpl->referenceAvgSaturation);
    
    // Calculate variance for reference (people have more texture than walls)
    pimpl->referenceVariance = pimpl->calculateRegionVariance(pimpl->scaledReferenceImage, 
                                                               0, 0, pimpl->refWidth, pimpl->refHeight);
    
    DBG("[ObjectDetection] Reference set: original " + 
        juce::String(pimpl->originalRefWidth) + "x" + juce::String(pimpl->originalRefHeight) +
        " -> scaled " + juce::String(pimpl->refWidth) + "x" + juce::String(pimpl->refHeight));
    DBG("[ObjectDetection] Reference variance: " + juce::String(pimpl->referenceVariance, 4));
    DBG("[ObjectDetection] Multi-scale templates: " +
        juce::String(pimpl->scaleTemplates[0].width) + "x" + juce::String(pimpl->scaleTemplates[0].height) + " (50%) to " +
        juce::String(pimpl->scaleTemplates[Impl::NUM_SCALES-1].width) + "x" + 
        juce::String(pimpl->scaleTemplates[Impl::NUM_SCALES-1].height) + " (350%)");
    
    return true;
}

//==============================================================================
// ASYNCHRONOUS API (threaded processing)

bool ObjectDetector::startAsyncDetection(DetectionCallback callback, int frameSkipCount)
{
    if (pimpl->asyncDetectionActive)
        return false; // Already running
    
    if (!callback)
        return false; // Invalid callback
    
    DBG("[ObjectDetection] Starting async detection");
    
    pimpl->detectionThread = std::make_unique<Impl::DetectionThread>(pimpl.get());
    pimpl->detectionThread->setCallback(callback);
    pimpl->detectionThread->setFrameSkipCount(frameSkipCount);
    pimpl->detectionThread->startThread();
    pimpl->asyncDetectionActive = true;
    
    return true;
}

void ObjectDetector::stopAsyncDetection()
{
    if (!pimpl->asyncDetectionActive)
        return;
    
    DBG("[ObjectDetection] Stopping async detection");
    
    if (pimpl->detectionThread)
    {
        pimpl->detectionThread->signalThreadShouldExit();
        pimpl->detectionThread->stopThread(1000);
        pimpl->detectionThread.reset();
    }
    
    pimpl->asyncDetectionActive = false;
}

bool ObjectDetector::submitFrame(const juce::Image& frameImage)
{
    if (!pimpl->asyncDetectionActive || !pimpl->detectionThread)
        return false;
    
    pimpl->detectionThread->submitFrame(frameImage);
    return true;
}

bool ObjectDetector::isAsyncDetectionActive() const
{
    return pimpl->asyncDetectionActive;
}

//==============================================================================
std::vector<DetectedObject> ObjectDetector::detectObjects(const std::vector<std::vector<uint8_t>>& frameData,
                                                         int frameWidth, int frameHeight, int channels,
                                                         int maxMatches)
{
    // Convert to JUCE Image and use that
    juce::Image img(channels == 4 ? juce::Image::ARGB : juce::Image::RGB, frameWidth, frameHeight, true);
    juce::Image::BitmapData bitmapData(img, juce::Image::BitmapData::writeOnly);
    
    for (int y = 0; y < frameHeight; ++y)
    {
        for (int x = 0; x < frameWidth; ++x)
        {
            int idx = y * frameWidth + x;
            if (idx < frameData.size() && frameData[idx].size() >= 3)
            {
                juce::Colour c(frameData[idx][0], frameData[idx][1], frameData[idx][2],
                              static_cast<juce::uint8>(channels == 4 && frameData[idx].size() >= 4 ? frameData[idx][3] : 255));
                bitmapData.setPixelColour(x, y, c);
            }
        }
    }
    
    return detectObjects(img, maxMatches);
}

std::vector<DetectedObject> ObjectDetector::detectObjects(const juce::Image& frameImage, int maxMatches)
{
    std::vector<DetectedObject> results;
    
    if (!frameImage.isValid() || !hasReferenceObject())
        return results;
    
    // Scale the frame
    int scaledWidth = frameImage.getWidth() / Impl::SCALE_FACTOR;
    int scaledHeight = frameImage.getHeight() / Impl::SCALE_FACTOR;
    juce::Image scaledFrame = frameImage.rescaled(scaledWidth, scaledHeight, 
                                                   juce::Graphics::lowResamplingQuality);
    
    auto detected = pimpl->findBestMatchFast(scaledFrame);
    
    if (detected.confidence >= pimpl->confidenceThreshold)
    {
        // Scale coordinates back up
        detected.centerPosition.setX(detected.centerPosition.getX() * Impl::SCALE_FACTOR);
        detected.centerPosition.setY(detected.centerPosition.getY() * Impl::SCALE_FACTOR);
        detected.bounds = juce::Rectangle<float>(
            detected.bounds.getX() * Impl::SCALE_FACTOR,
            detected.bounds.getY() * Impl::SCALE_FACTOR,
            detected.bounds.getWidth() * Impl::SCALE_FACTOR,
            detected.bounds.getHeight() * Impl::SCALE_FACTOR
        );
        results.push_back(detected);
    }
    
    return results;
}

//==============================================================================
void ObjectDetector::setConfidenceThreshold(float threshold)
{
    pimpl->confidenceThreshold = juce::jlimit(0.0f, 1.0f, threshold);
}

float ObjectDetector::getConfidenceThreshold() const
{
    return pimpl->confidenceThreshold;
}

void ObjectDetector::setProximityWeight(float weight)
{
    pimpl->proximityWeight = juce::jlimit(0.0f, 1.0f, weight);
}

float ObjectDetector::getProximityWeight() const
{
    return pimpl->proximityWeight;
}

void ObjectDetector::resetTracking()
{
    pimpl->hasLastKnownPosition = false;
    pimpl->lastKnownPosition = juce::Point<float>();
    pimpl->velocity = juce::Point<float>(0, 0);
    pimpl->framesSinceGoodMatch = 0;
}

bool ObjectDetector::hasReferenceObject() const
{
    return pimpl->scaledReferenceImage.isValid() && pimpl->refWidth > 0 && pimpl->refHeight > 0;
}

//==============================================================================
// Color histogram functions

void ObjectDetector::Impl::calculateHueHistogram(const juce::Image& img, int x, int y, int w, int h,
                                                  std::array<int, 16>& histogram, float& avgSaturation)
{
    histogram.fill(0);
    avgSaturation = 0.0f;
    int pixelCount = 0;
    
    if (!img.isValid()) return;
    
    juce::Image::BitmapData bitmap(img, juce::Image::BitmapData::readOnly);
    
    // Sample every 3rd pixel for speed
    for (int py = y; py < y + h && py < img.getHeight(); py += 3)
    {
        for (int px = x; px < x + w && px < img.getWidth(); px += 3)
        {
            juce::Colour c = bitmap.getPixelColour(px, py);
            float hue = c.getHue();
            float saturation = c.getSaturation();
            
            // Only count pixels with some saturation (ignore grays)
            if (saturation > 0.1f)
            {
                int bin = (int)(hue * 15.99f); // 0-15
                histogram[bin]++;
                avgSaturation += saturation;
                pixelCount++;
            }
        }
    }
    
    if (pixelCount > 0)
    {
        avgSaturation /= pixelCount;
    }
}

float ObjectDetector::Impl::compareHistograms(const std::array<int, 16>& hist1, const std::array<int, 16>& hist2)
{
    // Calculate histogram intersection (normalized)
    int sum1 = 0, sum2 = 0, intersection = 0;
    
    for (int i = 0; i < 16; i++)
    {
        sum1 += hist1[i];
        sum2 += hist2[i];
        intersection += juce::jmin(hist1[i], hist2[i]);
    }
    
    if (sum1 == 0 || sum2 == 0)
        return 1.0f; // No color info, consider it a match
    
    return (float)intersection / (float)juce::jmax(sum1, sum2);
}

float ObjectDetector::Impl::calculateRegionVariance(const juce::Image& frame, int x, int y, int w, int h)
{
    if (!frame.isValid() || w <= 0 || h <= 0)
        return 0.0f;
    
    // Clamp bounds
    int endX = juce::jmin(x + w, frame.getWidth());
    int endY = juce::jmin(y + h, frame.getHeight());
    x = juce::jmax(0, x);
    y = juce::jmax(0, y);
    
    if (endX <= x || endY <= y)
        return 0.0f;
    
    juce::Image::BitmapData bitmap(frame, juce::Image::BitmapData::readOnly);
    
    // First pass: calculate mean luminance (sample every 3rd pixel for speed)
    double sum = 0.0;
    int count = 0;
    
    for (int py = y; py < endY; py += 3)
    {
        for (int px = x; px < endX; px += 3)
        {
            juce::Colour c = bitmap.getPixelColour(px, py);
            float lum = c.getFloatRed() * 0.299f + c.getFloatGreen() * 0.587f + c.getFloatBlue() * 0.114f;
            sum += lum;
            count++;
        }
    }
    
    if (count == 0) return 0.0f;
    
    double mean = sum / count;
    
    // Second pass: calculate variance
    double variance = 0.0;
    for (int py = y; py < endY; py += 3)
    {
        for (int px = x; px < endX; px += 3)
        {
            juce::Colour c = bitmap.getPixelColour(px, py);
            float lum = c.getFloatRed() * 0.299f + c.getFloatGreen() * 0.587f + c.getFloatBlue() * 0.114f;
            double diff = lum - mean;
            variance += diff * diff;
        }
    }
    
    return static_cast<float>(variance / count);
}

//==============================================================================
// Fast template matching implementation

float ObjectDetector::Impl::calculateSimilarityFast(const juce::Image& frame, int frameX, int frameY,
                                                    const juce::Image& reference)
{
    if (!frame.isValid() || !reference.isValid())
        return 0.0f;
    
    int refW = reference.getWidth();
    int refH = reference.getHeight();
    
    // Check bounds
    if (frameX < 0 || frameY < 0 || 
        frameX + refW > frame.getWidth() || 
        frameY + refH > frame.getHeight())
        return 0.0f;
    
    juce::Image::BitmapData frameBitmap(frame, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData refBitmap(reference, juce::Image::BitmapData::readOnly);
    
    // Calculate Sum of Squared Differences (SSD) normalized
    // Also compute color similarity and frame region variance
    double sumDiff = 0.0;
    double sumRef = 0.0;
    double colorDiff = 0.0;  // Track color channel differences
    double frameLumSum = 0.0;
    double frameLumSqSum = 0.0;
    int pixelCount = 0;
    
    // Sample every 2nd pixel for speed
    for (int y = 0; y < refH; y += 2)
    {
        for (int x = 0; x < refW; x += 2)
        {
            juce::Colour framePixel = frameBitmap.getPixelColour(frameX + x, frameY + y);
            juce::Colour refPixel = refBitmap.getPixelColour(x, y);
            
            // Use luminance for primary comparison
            float frameLum = framePixel.getFloatRed() * 0.299f + 
                            framePixel.getFloatGreen() * 0.587f + 
                            framePixel.getFloatBlue() * 0.114f;
            float refLum = refPixel.getFloatRed() * 0.299f + 
                          refPixel.getFloatGreen() * 0.587f + 
                          refPixel.getFloatBlue() * 0.114f;
            
            float diff = frameLum - refLum;
            sumDiff += diff * diff;
            sumRef += refLum * refLum;
            
            // Track frame luminance for variance calculation
            frameLumSum += frameLum;
            frameLumSqSum += frameLum * frameLum;
            
            // Add color channel comparison (helps distinguish objects with same brightness)
            float rDiff = framePixel.getFloatRed() - refPixel.getFloatRed();
            float gDiff = framePixel.getFloatGreen() - refPixel.getFloatGreen();
            float bDiff = framePixel.getFloatBlue() - refPixel.getFloatBlue();
            colorDiff += (rDiff * rDiff + gDiff * gDiff + bDiff * bDiff) / 3.0f;
            
            pixelCount++;
        }
    }
    
    if (sumRef < 0.001 || pixelCount == 0)
        return 0.0f;
    
    // Calculate variance of the frame region
    double frameMean = frameLumSum / pixelCount;
    double frameVariance = (frameLumSqSum / pixelCount) - (frameMean * frameMean);
    
    // REJECT LOW-VARIANCE REGIONS (walls, floors, etc.)
    // If the frame region has much less texture than the reference, it's probably not the object
    float minVarianceRatio = 0.3f;  // Frame must have at least 30% of reference variance
    if (referenceVariance > 0.001f)
    {
        float varianceRatio = static_cast<float>(frameVariance) / referenceVariance;
        if (varianceRatio < minVarianceRatio)
        {
            // This region is too uniform - likely a wall or floor
            return 0.0f;
        }
        
        // Penalize regions with lower variance than reference
        // (they're less likely to be the textured object we're looking for)
        if (varianceRatio < 0.7f)
        {
            float penalty = (varianceRatio - minVarianceRatio) / (0.7f - minVarianceRatio);
            // Apply a soft penalty based on variance difference
            sumDiff *= (2.0 - penalty);  // Increase the difference for low-variance regions
        }
    }
    
    // Normalized luminance similarity (1.0 = perfect match, 0.0 = no match)
    float lumSimilarity = static_cast<float>(1.0 - std::sqrt(sumDiff / pixelCount));
    
    // Normalized color similarity
    float colorSimilarity = static_cast<float>(1.0 - std::sqrt(colorDiff / pixelCount));
    
    // Combine luminance and color (70% luminance, 30% color)
    float similarity = lumSimilarity * 0.7f + colorSimilarity * 0.3f;
    
    return juce::jmax(0.0f, similarity);
}

DetectedObject ObjectDetector::Impl::findBestMatchFast(const juce::Image& scaledFrame)
{
    DetectedObject bestMatch;
    float bestSimilarity = 0.0f;
    int bestScaleIndex = lastBestScaleIndex;
    
    if (!scaledReferenceImage.isValid() || refWidth == 0 || refHeight == 0)
        return bestMatch;
    
    int frameW = scaledFrame.getWidth();
    int frameH = scaledFrame.getHeight();
    
    // Search step size - larger = faster but less accurate
    int step = juce::jmax(2, juce::jmin(refWidth, refHeight) / 4);
    
    // Lambda to search a region with a given template
    auto searchRegion = [&](int startX, int startY, int endX, int endY, int searchStep, 
                            int scaleIdx, bool applyProximityBonus) {
        const auto& scale = scaleTemplates[scaleIdx];
        if (!scale.image.isValid()) return;
        
        for (int y = startY; y <= endY; y += searchStep)
        {
            for (int x = startX; x <= endX; x += searchStep)
            {
                if (x + scale.width > frameW || y + scale.height > frameH)
                    continue;
                    
                float similarity = calculateSimilarityFast(scaledFrame, x, y, scale.image);
                
                // Only apply proximity bonus if we're confident in tracking
                if (applyProximityBonus && hasLastKnownPosition && framesSinceGoodMatch < 3)
                {
                    juce::Point<float> currentCenter(x + scale.width / 2.0f, y + scale.height / 2.0f);
                    juce::Point<float> predictedPos = lastKnownPosition + velocity;
                    float distance = predictedPos.getDistanceFrom(currentCenter);
                    float maxDistance = (float)juce::jmax(refWidth, refHeight) * 4.0f;
                    if (distance < maxDistance)
                    {
                        float proximityBonus = (1.0f - distance / maxDistance) * proximityWeight * 0.05f;
                        similarity += proximityBonus;
                    }
                }
                
                if (similarity > bestSimilarity)
                {
                    bestSimilarity = similarity;
                    bestMatch.centerPosition = juce::Point<float>(x + scale.width / 2.0f, y + scale.height / 2.0f);
                    // Report bounds using the actual matched template size
                    bestMatch.bounds = juce::Rectangle<float>((float)x, (float)y, 
                                                               (float)scale.width, (float)scale.height);
                    bestMatch.confidence = similarity;
                    bestScaleIndex = scaleIdx;
                }
            }
        }
    };
    
    // Determine search strategy based on tracking history
    // Be VERY aggressive about doing full search when tracking is uncertain
    // Only use focused search if we've had a match in the last frame
    bool doFullSearch = !hasLastKnownPosition || framesSinceGoodMatch > 0;
    
    if (!doFullSearch)
    {
        // Focused search around predicted position (last position + velocity)
        juce::Point<float> predictedPos = lastKnownPosition + velocity;
        
        // Larger base search radius to handle fast movement
        int baseRadius = juce::jmax(refWidth, refHeight) * 4;
        int searchRadius = baseRadius + (framesSinceGoodMatch * refWidth * 2);
        searchRadius = juce::jmin(searchRadius, juce::jmax(frameW, frameH) / 2);
        
        // Get the largest template size to ensure we don't go out of bounds
        int maxTemplateW = scaleTemplates[NUM_SCALES - 1].width;
        int maxTemplateH = scaleTemplates[NUM_SCALES - 1].height;
        
        int focusStartX = juce::jmax(0, (int)predictedPos.getX() - refWidth/2 - searchRadius);
        int focusEndX = juce::jmin(frameW - maxTemplateW, (int)predictedPos.getX() - refWidth/2 + searchRadius);
        int focusStartY = juce::jmax(0, (int)predictedPos.getY() - refHeight/2 - searchRadius);
        int focusEndY = juce::jmin(frameH - maxTemplateH, (int)predictedPos.getY() - refHeight/2 + searchRadius);
        
        // Search scales near the last best scale first, then expand
        // This prioritizes the current size but allows for size changes
        int focusStep = juce::jmax(1, step / 2);
        
        // Search in order of likelihood: last best scale, then neighbors, then all
        std::vector<int> scaleOrder;
        scaleOrder.push_back(lastBestScaleIndex);
        for (int offset = 1; offset < NUM_SCALES; offset++)
        {
            if (lastBestScaleIndex + offset < NUM_SCALES)
                scaleOrder.push_back(lastBestScaleIndex + offset);
            if (lastBestScaleIndex - offset >= 0)
                scaleOrder.push_back(lastBestScaleIndex - offset);
        }
        
        for (int scaleIdx : scaleOrder)
        {
            searchRegion(focusStartX, focusStartY, focusEndX, focusEndY, focusStep, scaleIdx, true);
        }
        
        // If we found a good match, refine it
        if (bestSimilarity >= confidenceThreshold)
        {
            // Pixel-level refinement
            int refineX = (int)bestMatch.bounds.getX();
            int refineY = (int)bestMatch.bounds.getY();
            int refineRadius = step;
            const auto& bestScale = scaleTemplates[bestScaleIndex];
            
            for (int y = juce::jmax(0, refineY - refineRadius); 
                 y <= juce::jmin(frameH - bestScale.height, refineY + refineRadius); y++)
            {
                for (int x = juce::jmax(0, refineX - refineRadius); 
                     x <= juce::jmin(frameW - bestScale.width, refineX + refineRadius); x++)
                {
                    float similarity = calculateSimilarityFast(scaledFrame, x, y, bestScale.image);
                    if (similarity > bestSimilarity)
                    {
                        bestSimilarity = similarity;
                        bestMatch.centerPosition = juce::Point<float>(x + bestScale.width / 2.0f, y + bestScale.height / 2.0f);
                        bestMatch.bounds = juce::Rectangle<float>((float)x, (float)y, 
                                                                   (float)bestScale.width, (float)bestScale.height);
                        bestMatch.confidence = similarity;
                    }
                }
            }
            
            // Update velocity based on movement
            juce::Point<float> newVelocity = bestMatch.centerPosition - lastKnownPosition;
            velocity = velocity * 0.5f + newVelocity * 0.5f; // Smooth the velocity
            framesSinceGoodMatch = 0;
            lastBestScaleIndex = bestScaleIndex;
            
            DBG("[ObjectDetection] Tracked at scale " + juce::String(scaleTemplates[bestScaleIndex].scale, 2) + 
                " (" + juce::String(bestScaleIndex) + ")");
            
            return bestMatch;
        }
    }
    
    // Full frame search (either first frame, or lost tracking)
    framesSinceGoodMatch++;
    
    DBG("[ObjectDetection] Full frame search (framesSinceGoodMatch=" + juce::String(framesSinceGoodMatch) + ")");
    
    // When doing full search, DON'T apply any proximity bonus - start fresh
    for (int scaleIdx = 0; scaleIdx < NUM_SCALES; scaleIdx++)
    {
        const auto& scale = scaleTemplates[scaleIdx];
        if (scale.image.isValid())
        {
            searchRegion(0, 0, frameW - scale.width, frameH - scale.height, step, scaleIdx, false);
        }
    }
    
    // Refine if we found something
    if (bestSimilarity > 0.2f)
    {
        int refineX = (int)bestMatch.bounds.getX();
        int refineY = (int)bestMatch.bounds.getY();
        int refineRadius = step;
        const auto& bestScale = scaleTemplates[bestScaleIndex];
        
        for (int y = juce::jmax(0, refineY - refineRadius); 
             y <= juce::jmin(frameH - bestScale.height, refineY + refineRadius); y++)
        {
            for (int x = juce::jmax(0, refineX - refineRadius); 
                 x <= juce::jmin(frameW - bestScale.width, refineX + refineRadius); x++)
            {
                float similarity = calculateSimilarityFast(scaledFrame, x, y, bestScale.image);
                if (similarity > bestSimilarity)
                {
                    bestSimilarity = similarity;
                    bestMatch.centerPosition = juce::Point<float>(x + bestScale.width / 2.0f, y + bestScale.height / 2.0f);
                    bestMatch.bounds = juce::Rectangle<float>((float)x, (float)y, 
                                                               (float)bestScale.width, (float)bestScale.height);
                    bestMatch.confidence = similarity;
                }
            }
        }
    }
    
    // If this is a good match after full search, reset tracking state
    if (bestSimilarity >= confidenceThreshold)
    {
        if (hasLastKnownPosition)
        {
            velocity = bestMatch.centerPosition - lastKnownPosition;
        }
        else
        {
            velocity = juce::Point<float>(0, 0);
        }
        framesSinceGoodMatch = 0;
        lastBestScaleIndex = bestScaleIndex;
        
        DBG("[ObjectDetection] Re-acquired at scale " + juce::String(scaleTemplates[bestScaleIndex].scale, 2));
    }
    
    return bestMatch;
}

} // namespace Mach1

#endif // M1_OBJECTDETECTION_ENABLED