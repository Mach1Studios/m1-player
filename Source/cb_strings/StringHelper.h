/*
The MIT License (MIT)

Copyright (c) 2022-2023, Clemens Bergthaller

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE
*/

/**
    FFmpegHelpers.h
 
    Author:  Clemens Bergthaller

    This class provides some helpful functions when working with FFmpeg and videos.
 */

#pragma once

#include <JuceHeader.h>

struct StringHelper
{
    static juce::String convertSecondsToTimeString(double seconds, bool withMillies=false)
    {

        int resultMinutes = (int) (seconds / 60.0);
        int resultSeconds = (int) ( seconds - (resultMinutes * 60.0) );
        if (withMillies)
        {
            int resultMillies = static_cast<int>( (seconds-(resultMinutes * 60.0)-resultSeconds) * 1000.0) ;
            char buffer [18];
            std::sprintf(buffer, "%02.2d:%02.2d:%03.3d", resultMinutes, resultSeconds, resultMillies);
            return juce::String(buffer);
        }
        else
        {
            char buffer [13];
            std::sprintf(buffer, "%02.2d:%02.2d", resultMinutes, resultSeconds);
            return juce::String(buffer);
        }

    }
    
    /*!
     converts an int to a juce::String. If the number is smaller than 10, zeroes will be padded
     */
    static juce::String convertIntToStringWithPaddingZeroes(int number)
    {
        char buffer [3];
        std::sprintf(buffer, "%02.2d", number);
        return juce::String(buffer);
    }
};
