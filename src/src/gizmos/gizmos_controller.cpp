#include "gizmos/gizmos_controller.h"
#include <glm/gtx/transform.hpp>
#include "vk_types.h"
#include "vk_engine.h"
void GizmosController::init(VulkanEngine* vulkanEngine, VkPipeline gizmoPipeline, VkPipelineLayout gizmoPipelineLayout)
{
	_gizmoPipeline = gizmoPipeline;
	_gizmoPipelineLayout = gizmoPipelineLayout;

	// init rectangle
	std::array<Vertex, 4> rect_vertices;

	rect_vertices[0].position = { 0.5,-0.5, 0 };
	rect_vertices[1].position = { 0.5,0.5, 0 };
	rect_vertices[2].position = { -0.5,-0.5, 0 };
	rect_vertices[3].position = { -0.5,0.5, 0 };

	rect_vertices[0].color = { 0,0, 0,1 };
	rect_vertices[1].color = { 0.5,0.5,0.5 ,1 };
	rect_vertices[2].color = { 1,0, 0,1 };
	rect_vertices[3].color = { 0,1, 0,1 };

	std::array<uint32_t, 6> rect_indices;

	rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;

	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

	_rectangle = vulkanEngine->uploadMesh(rect_indices, rect_vertices);
}

void GizmosController::draw_gizmo(VkCommandBuffer& cmd, const GizmoShape& shape, const glm::vec3& position)
{
	GPUMeshBuffers mesh = getGizmo(shape);
	vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	GPUDrawPushConstants pushConstants;
	pushConstants.vertexBuffer = mesh.vertexBufferAddress;
	glm::mat4 worldMatrix = glm::translate(glm::mat4(1.f), position);
	pushConstants.worldMatrix = worldMatrix;

	vkCmdPushConstants(cmd, _gizmoPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

	vkCmdDrawIndexed(cmd, 6, 1, 0, 0, 0);
}

GPUMeshBuffers GizmosController::getGizmo(const GizmoShape& shape)
{
	switch (shape) {
		case RECTANGLE:
			return _rectangle;
	}
	return _rectangle;
}
