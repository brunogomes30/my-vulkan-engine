#pragma once
#include<loader/loaded_gltf.h>
#include<materials/material_controller.h>
class VulkanEngine;
namespace loader_gltf{
    std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanEngine* engine, std::string_view filePath, MaterialController* materialController);
}