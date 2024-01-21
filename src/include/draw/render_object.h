#pragma once
#include <materials/materials.h>
struct RenderObject {
	uint32_t indexCount;
	uint32_t firstIndex;
	VkBuffer indexBuffer;
	std::shared_ptr<MaterialInstance> material;

	glm::mat4 transform;
	VkDeviceAddress vertexBufferAddress;
};