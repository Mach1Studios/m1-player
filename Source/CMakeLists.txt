file(GLOB source_files  Config.h
                        Main.cpp
                        MainComponent.h
                        MainComponent.cpp
                        SphereMeshGenerator.h
                        UI/VideoPlayerWidget.h)
target_sources(${PLUGIN_NAME} PRIVATE ${source_files})