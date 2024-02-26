#pragma once
#include <vk_types.h>
#include<draw/bounds.h>
struct GeoSurface {
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};
struct MeshAsset {
    std::string name;

    std::vector<GeoSurface> surfaces;
    std::vector<Vertex> vertices;
    GPUMeshBuffers meshBuffers;
};