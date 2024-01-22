#pragma once
#include<vk_types.h>
struct RenderObject;
struct Bounds {
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

namespace bounds {
    bool is_visible(const RenderObject& obj, const glm::mat4& viewproj);
}

