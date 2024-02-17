#pragma once
#include <vk_types.h>
#include <scene/light/light_data.h>
#define MAX_LIGHTS 8
struct GPUSceneData {
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 inverseView;
	alignas(16) glm::mat4 proj;
	alignas(16) glm::mat4 viewproj;
	alignas(16) glm::vec4 ambientColor;
	alignas(16) glm::vec4 sunlightDirection; // w for sun power
	alignas(16) glm::vec4 sunlightColor;
	alignas(16) glm::vec4 cameraPosition;
	alignas(4) int lightCount;
	alignas(16) GPULightData lights[MAX_LIGHTS];

};