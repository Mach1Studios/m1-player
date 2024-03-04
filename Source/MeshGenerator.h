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
 
public:
    MeshGenerator() {}

    // Generate a sphere mesh
    // Function to generate a sphere mesh
    MurVbo generateSphereMesh(int textureSizeX, int textureSizeY, int sphereSize = 100, int meshResolution = 64) {
        MurVbo result;

        float doubleRes = meshResolution * 2.f;
        float polarInc = pi / (meshResolution); // ringAngle
        float azimInc = 2 * pi / (doubleRes); // segAngle  

        for (float i = 0; i < meshResolution + 1; i++) {
            float tr = sin(pi - i * polarInc);
            float ny = cos(pi - i * polarInc);

            float tcoord_y = (i / meshResolution);

            for (float j = 0; j <= doubleRes; j++) {
                float nx = tr * sin(j * azimInc);
                float nz = tr * cos(j * azimInc);

                float tcoord_x = 1.0f - j / (doubleRes);

                result.addVertex({ nx * sphereSize, ny * sphereSize, nz * sphereSize });
                result.addTexCoord({ tcoord_x, tcoord_y });
            }
        }

        int nr = doubleRes + 1;

        int index1, index2, index3;

        for (float iy = 0; iy < meshResolution; iy++) {
            for (float ix = 0; ix < doubleRes; ix++) {

                // first tri //
                if (iy > 0) {
                    index1 = (iy + 0) * (nr)+(ix + 0);
                    index2 = (iy + 0) * (nr)+(ix + 1);
                    index3 = (iy + 1) * (nr)+(ix + 0);

                    result.addIndex(index1);
                    result.addIndex(index3);
                    result.addIndex(index2);
                }

                if (iy < meshResolution - 1) {
                    // second tri //
                    index1 = (iy + 0) * (nr)+(ix + 1);
                    index2 = (iy + 1) * (nr)+(ix + 1);
                    index3 = (iy + 1) * (nr)+(ix + 0);

                    result.addIndex(index1);
                    result.addIndex(index3);
                    result.addIndex(index2);

                }
            }
        }

        return result;
    }

    // Generate a circle
    MurVbo generateCircleMesh(float radius, float width, int segments = 32) {
        MurVbo vbo;
        std::vector<MurkaPoint3D> circleVerts;
        circleVerts.reserve(segments * 4);

        const float tau = 2.0f * M_PI;
        const float halfWidth = width * 0.5f;

        for (int i = 0; i < segments; ++i) {
            float theta = i * tau / segments;
            float nextTheta = (i + 1) * tau / segments;

            float x1 = radius * std::cos(theta);
            float y1 = radius * std::sin(theta);
            float x2 = radius * std::cos(nextTheta);
            float y2 = radius * std::sin(nextTheta);

            MurkaPoint3D p1(x1 + halfWidth * std::cos(theta + M_PI / 2.0f), y1 + halfWidth * std::sin(theta + M_PI / 2.0f), 0.0f);
            MurkaPoint3D p2(x1 - halfWidth * std::cos(theta + M_PI / 2.0f), y1 - halfWidth * std::sin(theta + M_PI / 2.0f), 0.0f);
            MurkaPoint3D p3(x2 - halfWidth * std::cos(nextTheta + M_PI / 2.0f), y2 - halfWidth * std::sin(nextTheta + M_PI / 2.0f), 0.0f);
            MurkaPoint3D p4(x2 + halfWidth * std::cos(nextTheta + M_PI / 2.0f), y2 + halfWidth * std::sin(nextTheta + M_PI / 2.0f), 0.0f);

            circleVerts.emplace_back(p1);
            circleVerts.emplace_back(p2);
            circleVerts.emplace_back(p3);
            circleVerts.emplace_back(p4);
        }

        vbo.setVertexData(circleVerts.data(), circleVerts.size());
        return vbo;
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
