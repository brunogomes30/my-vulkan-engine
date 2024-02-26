#include "gizmos/gizmos_controller.h"
#include <glm/gtx/transform.hpp>
#include "vk_types.h"
#include "vk_engine.h"
void GizmosController::init(VulkanEngine* vulkanEngine, VkPipeline gizmoPipeline, VkPipelineLayout gizmoPipelineLayout)
{
	_vulkanEngine = vulkanEngine;
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

	// init triangle
	std::array<Vertex, 3> tri_vertices;

	tri_vertices[0].position = { 0,0.5, 0 };
	tri_vertices[1].position = { 0.5,-0.5, 0 };
	tri_vertices[2].position = { -0.5,-0.5, 0 };

	tri_vertices[0].color = { 0,0, 0,1 };
	tri_vertices[1].color = { 0.5,0.5,0.5 ,1 };
	tri_vertices[2].color = { 1,0, 0,1 };

	std::array<uint32_t, 3> tri_indices;

	tri_indices[0] = 0;
	tri_indices[1] = 1;
	tri_indices[2] = 2;

	_triangle = vulkanEngine->uploadMesh(tri_indices, tri_vertices);

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

void GizmosController::set_meshes(std::vector<std::shared_ptr<MeshNode>> meshes)
{
		_meshes = meshes;
}

void GizmosController::draw_normals(VkCommandBuffer& cmd, float length)
{
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	
	for (auto& meshNode : _meshes) {
		for (auto& vertex : meshNode->mesh->vertices) {
			glm::mat4 matrix = meshNode->worldTransform;

			glm::vec4 start = matrix * glm::vec4(vertex.position, 1);
			glm::vec4 end = start + (matrix * glm::vec4(vertex.normal, 0)) * length;

			Vertex vertex1 = { glm::vec3(start.x, start.y, start.z)};
			Vertex vertex2 = { glm::vec3(end.x, end.y, end.z)};
			// get cross product
			Vertex vertex3 = { glm::vec3(start.x, start.y, start.z)};
			vertex3.position.x += 0.1f;
			
			vertices.push_back(vertex1);
			vertices.push_back(vertex2);
			vertices.push_back(vertex3);

			indices.push_back(vertices.size() - 3);
			indices.push_back(vertices.size() - 2);
			indices.push_back(vertices.size() - 1);

			indices.push_back(vertices.size() - 1);
			indices.push_back(vertices.size() - 2);
			indices.push_back(vertices.size() - 3);
		}
	}
	GPUMeshBuffers mesh = _vulkanEngine->uploadMesh(indices, vertices);
	
	// create line
			//GPUMeshBuffers line = CreateLine(start, end, { 1,1,1,1 });
	vkCmdBindIndexBuffer(cmd, mesh.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
	GPUDrawPushConstants pushConstants;
	pushConstants.vertexBuffer = mesh.vertexBufferAddress;
	glm::mat4 worldMatrix = glm::mat4(1.f);
	// face end
	pushConstants.worldMatrix = worldMatrix;
	vkCmdPushConstants(cmd, _gizmoPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

	vkCmdDrawIndexed(cmd, indices.size(), 1, 0, 0, 0);
}

GPUMeshBuffers GizmosController::getGizmo(const GizmoShape& shape)
{
	switch (shape) {
		case RECTANGLE:
			return _rectangle;
	}
	return _rectangle;
}

GPUMeshBuffers GizmosController::CreateLine(const glm::vec3& start, const glm::vec3& end, const glm::vec4& color)
{
	std::array<Vertex, 4> rect_vertices;

	rect_vertices[0].position = start;
	rect_vertices[1].position = end;
	rect_vertices[2].position = start;
	rect_vertices[3].position = end;

	rect_vertices[0].color = color;
	rect_vertices[1].color = color;
	rect_vertices[2].color = color;
	rect_vertices[3].color = color;

	std::array<uint32_t, 6> rect_indices;

	rect_indices[0] = 0;
	rect_indices[1] = 1;
	rect_indices[2] = 2;
		
	rect_indices[3] = 2;
	rect_indices[4] = 1;
	rect_indices[5] = 3;

	return _vulkanEngine->uploadMesh(rect_indices, rect_vertices);
}
