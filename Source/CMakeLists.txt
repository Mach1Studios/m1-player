file(GLOB source_files  Config.h
                        Main.cpp
                        MainComponent.h
                        MainComponent.cpp
                        FFmpegVCMediaObject.h
                        FFmpegVCMediaObject.cpp
                        M1PlayerMediaObject.h
                        TypesForDataExchange.h
                        SphereMeshGenerator.h
                        TransportOSCServer.h
                        PlayerOSC.h
                        PlayerOSC.cpp
                        UI/M1Slider.h
                        UI/M1Checkbox.h
                        UI/M1DropdownButton.h
                        UI/VideoPlayerWidget.h
                        UI/RadioGroupWidget.h
                        UI/M1PlayerControlButton.h
                        UI/M1PlayerControls.h
                        cb_ffmpeg/AudioBufferFIFO.h
                        cb_ffmpeg/FFmpegHelpers.h
                        cb_ffmpeg/FfmpegHelpers.cpp
                        cb_ffmpeg/FFmpegVideoComponent.h
                        cb_ffmpeg/FFmpegVideoComponent.cpp
                        cb_ffmpeg/FFmpegVideoDecodeThread.h
                        cb_ffmpeg/FFmpegVideoDecodeThread.cpp
                        cb_ffmpeg/FFmpegVideoFramesFIFO.h
                        cb_ffmpeg/FFmpegVideoListener.h
                        cb_ffmpeg/FFmpegVideoReader.h
                        cb_ffmpeg/FFmpegVideoReader.cpp
                        cb_ffmpeg/FFmpegVideoScaler.h
                        cb_strings/StringHelper.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${source_files})