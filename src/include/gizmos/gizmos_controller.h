#pragma once

#include<vk_types.h>
#include<node/mesh_node.h>
enum GizmoShape {
	RECTANGLE,
};

class GizmosController {
private:
	class VulkanEngine* _vulkanEngine; // I don't like this
	GPUMeshBuffers _rectangle;
	GPUMeshBuffers _triangle;
	VkPipeline _gizmoPipeline;
	VkPipelineLayout _gizmoPipelineLayout;

	
	std::vector<std::shared_ptr<MeshNode>> _meshes;

public:
	bool willDrawNormals = false;

	void init(class VulkanEngine* vulkanEngine, VkPipeline gizmoPipeline, VkPipelineLayout gizmoPipelineLayout);
	void draw_gizmo(VkCommandBuffer& cmd, const GizmoShape& shape, const glm::vec3& position);

	void set_meshes(std::vector<std::shared_ptr<MeshNode>> meshes);
	void draw_normals(VkCommandBuffer& cmd, float length);
	




	GPUMeshBuffers getGizmo(const GizmoShape& shape);

	GPUMeshBuffers CreateLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color);
};