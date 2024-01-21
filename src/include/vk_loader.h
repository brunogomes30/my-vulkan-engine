#pragma once
#include <vk_types.h>
#include <unordered_map>
#include <filesystem>
#include <mesh/mesh_asset.h>

//forward declaration
class VulkanEngine;

std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadGltfMeshes(VulkanEngine* engine, std::filesystem::path filePath);