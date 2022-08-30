
//
//  M1 Workflow
//  M1 Player
//  SphereMeshGenerator.h
//

#ifndef SphereMeshGenerator_h
#define SphereMeshGenerator_h

#include "juce_murka/juce_murka.h"

class SphereMeshGenerator {
    const float pi = 3.141592653589793238463f;

public:
    SphereMeshGenerator() {

    }

    void addSphereSegmentToMesh(MurVbo& mesh, float x1, float x2, float y1, float y2, int sphereSize, int textureSizeX, int textureSizeY) {

        float radialPhase1 = sqrt(cos((y1 + pi / 2) * pi));
        float radialPhase2 = sqrt(cos((y2 + pi / 2) * pi));


        float H1 = y1 * sphereSize;
        float H2 = y2 * sphereSize;
        radialPhase1 = sqrt(sphereSize * H1 - pow(H1, 2)) / sphereSize;
        radialPhase2 = sqrt(sphereSize * H2 - pow(H2, 2)) / sphereSize;

        MurkaPoint3D p1 = MurkaPoint3D(cos(x1 * pi * 2) * radialPhase1 * sphereSize,
            y1 * sphereSize - sphereSize / 2,
            sin(x1 * pi * 2) * radialPhase1 * sphereSize);

        MurkaPoint3D p2 = MurkaPoint3D(cos(x2 * pi * 2) * radialPhase1 * sphereSize,
            y1 * sphereSize - sphereSize / 2,
            sin(x2 * pi * 2) * radialPhase1 * sphereSize);

        MurkaPoint3D p3 = MurkaPoint3D(cos(x2 * pi * 2) * radialPhase2 * sphereSize,
            y2 * sphereSize - sphereSize / 2,
            sin(x2 * pi * 2) * radialPhase2 * sphereSize);

        MurkaPoint3D p4 = MurkaPoint3D(cos(x1 * pi * 2) * radialPhase2 * sphereSize,
            y2 * sphereSize - sphereSize / 2,
            sin(x1 * pi * 2) * radialPhase2 * sphereSize);

        int initialIndex = mesh.getVertices().size();


        mesh.addVertex(p1);
        mesh.addVertex(p2);
        mesh.addVertex(p3);
        mesh.addVertex(p4);


        mesh.addTexCoord(MurkaPoint(x1, 1 - y1) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(x2, 1 - y1) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(x2, 1 - y2) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(x1, 1 - y2) * MurkaPoint(textureSizeX, textureSizeY));


        mesh.addIndex(initialIndex + 0);
        mesh.addIndex(initialIndex + 1);
        mesh.addIndex(initialIndex + 2);

        mesh.addIndex(initialIndex + 0);
        mesh.addIndex(initialIndex + 3);
        mesh.addIndex(initialIndex + 2);
    }

    MurVbo generateSphereMesh(int textureSizeX, int textureSizeY, int sphereSize = 100, int meshResolution = 64) {
        MurVbo result;

        float step = 1.0 / (float)meshResolution;
        for (int i = 0; i < meshResolution; i++) { // from -3
            float phaseY = (float)i / (float)meshResolution;
            for (int j = 0; j < meshResolution; j++) {
                float phaseX = (float)j / (float)meshResolution;
                addSphereSegmentToMesh(result, phaseX, phaseX + step, phaseY, phaseY + step, sphereSize, textureSizeX, textureSizeY);
            }
        }

        return result;
    }
};


#endif /* SphereMeshGenerator_h */
