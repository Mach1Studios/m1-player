#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "MurkaBasicWidgets.h"
#include "juce_murka/JuceMurkaBaseComponent.h"
#include "../ObjectTracker.h"

/**
 * MarqueeSelection - A Murka widget for drawing selection rectangles on video frames.
 * 
 * Features:
 * - Click and drag to create selection rectangle
 * - Visual feedback with animated dashed border
 * - Clear button to cancel selection
 * - Callbacks for selection events
 */
class MarqueeSelection : public View<MarqueeSelection>
{
public:
    // Styling options
    juce::Colour selectionColor = juce::Colour(0xFF00D4FF);       // Cyan selection border
    juce::Colour selectionFillColor = juce::Colour(0x3000D4FF);   // Semi-transparent cyan fill
    juce::Colour trackingColor = juce::Colour(0xFF00FF88);        // Green for active tracking
    juce::Colour lostTrackingColor = juce::Colour(0xFFFF4444);    // Red when tracking lost
    float borderWidth = 2.0f;
    float cornerRadius = 3.0f;
    
    // State
    bool selectionEnabled = false;  // Set to true to enable selection mode
    bool isSelecting = false;
    bool hasSelection = false;
    bool hideSelectionWhenTracking = true;  // Hide selection rectangle when tracking is active
    juce::Rectangle<float> selectionRect;
    
    // Tracking visualization
    bool showTrackingResult = false;
    juce::Rectangle<float> trackingRect;
    float trackingConfidence = 0.0f;
    bool trackingLost = false;
    
    // Animation
    float dashOffset = 0.0f;
    float pulsePhase = 0.0f;
    
    // Callbacks
    std::function<void(juce::Rectangle<float>)> onSelectionComplete;
    std::function<void()> onSelectionCancelled;
    
    void internalDraw(Murka& m)
    {
        // Update animation
        dashOffset += 0.5f;
        if (dashOffset > 20.0f) dashOffset = 0.0f;
        pulsePhase += 0.1f;
        if (pulsePhase > 2.0f * M_PI) pulsePhase -= 2.0f * M_PI;
        
        auto viewSize = getSize();
        
        // Handle mouse interaction for selection
        if (selectionEnabled)
        {
            if (mouseDownPressed(0) && inside())
            {
                // Start new selection
                isSelecting = true;
                hasSelection = false;
                selectionStartPoint = mousePosition();
                selectionRect = juce::Rectangle<float>(
                    selectionStartPoint.x, selectionStartPoint.y, 0, 0
                );
            }
            
            if (isSelecting && mouseDown(0))
            {
                // Update selection rectangle
                auto currentPos = mousePosition();
                float left = juce::jmin(selectionStartPoint.x, currentPos.x);
                float top = juce::jmin(selectionStartPoint.y, currentPos.y);
                float right = juce::jmax(selectionStartPoint.x, currentPos.x);
                float bottom = juce::jmax(selectionStartPoint.y, currentPos.y);
                
                // Clamp to view bounds
                left = juce::jmax(0.0f, left);
                top = juce::jmax(0.0f, top);
                right = juce::jmin(viewSize.x, right);
                bottom = juce::jmin(viewSize.y, bottom);
                
                selectionRect = juce::Rectangle<float>(left, top, right - left, bottom - top);
            }
            
            if (isSelecting && !mouseDown(0))
            {
                // Finish selection
                isSelecting = false;
                
                // Only keep selection if it's large enough
                if (selectionRect.getWidth() > 20 && selectionRect.getHeight() > 20)
                {
                    hasSelection = true;
                    if (onSelectionComplete)
                    {
                        // Convert to normalized coordinates (0-1)
                        juce::Rectangle<float> normalizedRect(
                            selectionRect.getX() / viewSize.x,
                            selectionRect.getY() / viewSize.y,
                            selectionRect.getWidth() / viewSize.x,
                            selectionRect.getHeight() / viewSize.y
                        );
                        onSelectionComplete(normalizedRect);
                    }
                }
                else
                {
                    selectionRect = juce::Rectangle<float>();
                }
            }
        }
        
        // Draw active tracking result
        if (showTrackingResult && !trackingRect.isEmpty())
        {
            float pulse = 0.7f + 0.3f * std::sin(pulsePhase);
            
            if (trackingLost)
            {
                // Draw lost tracking indicator (red pulsing)
                m.setColor(lostTrackingColor.getRed(), lostTrackingColor.getGreen(), 
                          lostTrackingColor.getBlue(), (int)(180 * pulse));
            }
            else
            {
                // Draw active tracking indicator (green)
                m.setColor(trackingColor.getRed(), trackingColor.getGreen(), 
                          trackingColor.getBlue(), (int)(200 * pulse));
            }
            
            // Draw tracking rectangle
            drawDashedRect(m, trackingRect, borderWidth * 1.5f);
            
            // Draw crosshair at center
            float cx = trackingRect.getCentreX();
            float cy = trackingRect.getCentreY();
            float crossSize = juce::jmin(trackingRect.getWidth(), trackingRect.getHeight()) * 0.2f;
            
            m.setColor(255, 255, 255, (int)(200 * pulse));
            m.drawLine(cx - crossSize, cy, cx + crossSize, cy);
            m.drawLine(cx, cy - crossSize, cx, cy + crossSize);
            
            // Draw confidence indicator
            if (!trackingLost && trackingConfidence > 0)
            {
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 4);
                std::string confText = juce::String(trackingConfidence * 100, 0).toStdString() + "%";
                m.setColor(255, 255, 255, 200);
                m.getCurrentFont()->drawString(confText, 
                    trackingRect.getX() + 5, 
                    trackingRect.getY() - 5);
            }
        }
        
        // Draw selection rectangle if selecting or has selection
        // But hide it when tracking is active (showTrackingResult means we're tracking)
        bool shouldShowSelection = (isSelecting || hasSelection) && !selectionRect.isEmpty();
        if (hideSelectionWhenTracking && showTrackingResult)
            shouldShowSelection = false;
            
        if (shouldShowSelection)
        {
            // Draw semi-transparent fill
            m.setColor(selectionFillColor.getRed(), selectionFillColor.getGreen(),
                      selectionFillColor.getBlue(), selectionFillColor.getAlpha());
            m.drawRectangle(selectionRect.getX(), selectionRect.getY(),
                           selectionRect.getWidth(), selectionRect.getHeight());
            
            // Draw animated dashed border
            m.setColor(selectionColor.getRed(), selectionColor.getGreen(),
                      selectionColor.getBlue(), 255);
            drawDashedRect(m, selectionRect, borderWidth);
            
            // Draw corner handles
            float handleSize = 8.0f;
            drawHandle(m, selectionRect.getTopLeft(), handleSize);
            drawHandle(m, selectionRect.getTopRight(), handleSize);
            drawHandle(m, selectionRect.getBottomLeft(), handleSize);
            drawHandle(m, selectionRect.getBottomRight(), handleSize);
            
            // Draw dimension text
            if (isSelecting)
            {
                m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE - 2);
                std::string dimText = juce::String((int)selectionRect.getWidth()).toStdString() + 
                                     " x " + 
                                     juce::String((int)selectionRect.getHeight()).toStdString();
                m.setColor(255, 255, 255, 220);
                m.getCurrentFont()->drawString(dimText,
                    selectionRect.getX() + 5,
                    selectionRect.getBottom() + 15);
            }
        }
        
        // Draw selection mode indicator
        if (selectionEnabled && !isSelecting && !hasSelection)
        {
            // Draw crosshair cursor hint
            auto mousePos = mousePosition();
            if (inside())
            {
                m.setColor(selectionColor.getRed(), selectionColor.getGreen(),
                          selectionColor.getBlue(), 150);
                m.drawLine(mousePos.x - 10, mousePos.y, mousePos.x + 10, mousePos.y);
                m.drawLine(mousePos.x, mousePos.y - 10, mousePos.x, mousePos.y + 10);
            }
        }
    }
    
    /** Sets the tracking result for visualization.
        @param rect The bounding rectangle of the tracked object (in view coordinates)
        @param confidence Confidence value 0-1
        @param lost Whether tracking was lost
    */
    void setTrackingResult(const juce::Rectangle<float>& rect, float confidence, bool lost)
    {
        trackingRect = rect;
        trackingConfidence = confidence;
        trackingLost = lost;
        showTrackingResult = true;
    }
    
    /** Clears the tracking result visualization. */
    void clearTrackingResult()
    {
        showTrackingResult = false;
        trackingRect = juce::Rectangle<float>();
    }
    
    /** Clears the current selection. */
    void clearSelection()
    {
        hasSelection = false;
        isSelecting = false;
        selectionRect = juce::Rectangle<float>();
        if (onSelectionCancelled)
            onSelectionCancelled();
    }
    
    /** Gets the selection rectangle in normalized coordinates (0-1). */
    juce::Rectangle<float> getNormalizedSelection()
    {
        auto viewSize = getSize();
        if (viewSize.x <= 0 || viewSize.y <= 0)
            return juce::Rectangle<float>();
            
        return juce::Rectangle<float>(
            selectionRect.getX() / viewSize.x,
            selectionRect.getY() / viewSize.y,
            selectionRect.getWidth() / viewSize.x,
            selectionRect.getHeight() / viewSize.y
        );
    }

private:
    MurkaPoint selectionStartPoint;
    
    void drawDashedRect(Murka& m, const juce::Rectangle<float>& rect, float lineWidth)
    {
        // Draw dashed rectangle
        float dashLen = 8.0f;
        float gapLen = 4.0f;
        float totalLen = dashLen + gapLen;
        
        // Top edge
        float offset = std::fmod(dashOffset, totalLen);
        for (float x = rect.getX() - offset; x < rect.getRight(); x += totalLen)
        {
            float startX = juce::jmax(rect.getX(), x);
            float endX = juce::jmin(rect.getRight(), x + dashLen);
            if (startX < endX)
                m.drawLine(startX, rect.getY(), endX, rect.getY());
        }
        
        // Bottom edge
        for (float x = rect.getX() - offset; x < rect.getRight(); x += totalLen)
        {
            float startX = juce::jmax(rect.getX(), x);
            float endX = juce::jmin(rect.getRight(), x + dashLen);
            if (startX < endX)
                m.drawLine(startX, rect.getBottom(), endX, rect.getBottom());
        }
        
        // Left edge
        for (float y = rect.getY() - offset; y < rect.getBottom(); y += totalLen)
        {
            float startY = juce::jmax(rect.getY(), y);
            float endY = juce::jmin(rect.getBottom(), y + dashLen);
            if (startY < endY)
                m.drawLine(rect.getX(), startY, rect.getX(), endY);
        }
        
        // Right edge
        for (float y = rect.getY() - offset; y < rect.getBottom(); y += totalLen)
        {
            float startY = juce::jmax(rect.getY(), y);
            float endY = juce::jmin(rect.getBottom(), y + dashLen);
            if (startY < endY)
                m.drawLine(rect.getRight(), startY, rect.getRight(), endY);
        }
    }
    
    void drawHandle(Murka& m, juce::Point<float> point, float size)
    {
        m.enableFill();
        m.drawRectangle(point.x - size/2, point.y - size/2, size, size);
    }
};
