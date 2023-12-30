//
//  M1 Workflow
//  M1 Player
//  SphereMeshGenerator.h
//

#ifndef SphereMeshGenerator_h
#define SphereMeshGenerator_h

#include "juce_murka/juce_murka.h"

class MeshGenerator {
    const float pi = 3.141592653589793238463f;

    void addSphereSegmentToMesh(MurVbo& mesh, float x1, float x2, float y1, float y2, int sphereSize, int textureSizeX, int textureSizeY) {
        float H1 = y1 * sphereSize;
        float H2 = y2 * sphereSize;
        float radialPhase1 = sqrt(sphereSize * H1 - pow(H1, 2)) / sphereSize;
        float radialPhase2 = sqrt(sphereSize * H2 - pow(H2, 2)) / sphereSize;

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

        float offset =  -0.25f;
        mesh.addTexCoord(MurkaPoint(fmod(x1 + offset, 1.0f), y1) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(fmod(x2 + offset, 1.0f), y1) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(fmod(x2 + offset, 1.0f), y2) * MurkaPoint(textureSizeX, textureSizeY));
        mesh.addTexCoord(MurkaPoint(fmod(x1 + offset, 1.0f), y2) * MurkaPoint(textureSizeX, textureSizeY));


        mesh.addIndex(initialIndex + 0);
        mesh.addIndex(initialIndex + 1);
        mesh.addIndex(initialIndex + 2);

        mesh.addIndex(initialIndex + 0);
        mesh.addIndex(initialIndex + 3);
        mesh.addIndex(initialIndex + 2);
    }

public:
    MeshGenerator() {

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

    // Generates a box mesh
    MurVbo generateBoxMesh(float width, float height, float depth) {
        MurVbo mesh;

        // Define vertices
        std::vector<MurkaPoint3D> vertices = {
            {width / 2, height / 2, depth / 2},   // 0: Front top right
            {-width / 2, height / 2, depth / 2},  // 1: Front top left
            {-width / 2, -height / 2, depth / 2}, // 2: Front bottom left
            {width / 2, -height / 2, depth / 2},  // 3: Front bottom right
            {width / 2, height / 2, -depth / 2},  // 4: Back top right
            {-width / 2, height / 2, -depth / 2}, // 5: Back top left
            {-width / 2, -height / 2, -depth / 2},// 6: Back bottom left
            {width / 2, -height / 2, -depth / 2}  // 7: Back bottom right
        };

        // Define indices (triangles)
        std::vector<unsigned int> indices = {
            0, 1, 2, 2, 3, 0, // Front face
            4, 5, 6, 6, 7, 4, // Back face
            5, 1, 2, 2, 6, 5, // Left face
            0, 4, 7, 7, 3, 0, // Right face
            5, 4, 7, 7, 6, 5, // Top face
            0, 1, 2, 2, 3, 0  // Bottom face
        };

        // Add vertices to mesh
        for (const auto& vertex : vertices) {
            mesh.addVertex(vertex);
        }

        // Add indices to mesh
        for (const auto& index : indices) {
            mesh.addIndex(index);
        }

        return mesh;
    }

};


#endif /* SphereMeshGenerator_h */
