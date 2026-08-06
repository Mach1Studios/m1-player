file(GLOB source_files  Config.h
                        Main.cpp
                        MainComponent.h
                        MainComponent.cpp
                        MediaPlayer.h
                        MediaPlayer.cpp
                        ObjectTracker.h
                        ObjectTracker.cpp
                        TypesForDataExchange.h
                        SphereMeshGenerator.h
                        PlayerOSC.h
                        PlayerOSC.cpp
                        UI/M1Slider.h
                        UI/M1Checkbox.h
                        UI/M1DropdownButton.h
                        UI/M1DropdownMenu.h
                        UI/VideoPlayerWidget.h
                        UI/MarqueeSelection.h
                        UI/RadioGroupWidget.h
                        UI/M1PlayerControlButton.h
                        UI/M1PlayerControls.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${source_files})