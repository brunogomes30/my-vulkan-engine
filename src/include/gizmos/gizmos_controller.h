#pragma once

#include<vk_types.h>
enum GizmoShape {
	RECTANGLE,
};

class GizmosController {
private:
	GPUMeshBuffers _rectangle;
	VkPipeline _gizmoPipeline;
	VkPipelineLayout _gizmoPipelineLayout;

public:
	void init(class VulkanEngine* vulkanEngine, VkPipeline gizmoPipeline, VkPipelineLayout gizmoPipelineLayout);

	void draw_gizmo(VkCommandBuffer& cmd, const GizmoShape& shape, const glm::vec3& position);

	GPUMeshBuffers getGizmo(const GizmoShape& shape);
};