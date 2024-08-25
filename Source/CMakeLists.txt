file(GLOB source_files  Config.h
                        Main.cpp
                        MainComponent.h
                        MainComponent.cpp
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
                        UI/M1PlayerControls.h)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${source_files})