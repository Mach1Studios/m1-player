file(GLOB source_files  Config.h
                        Main.cpp
                        MainComponent.h
                        MainComponent.cpp
                        SphereMeshGenerator.h
                        TransportOSCServer.h
                        UI/VideoPlayerWidget.h
                        UI/RadioGroupWidget.h)
target_sources(${PLUGIN_NAME} PRIVATE ${source_files})