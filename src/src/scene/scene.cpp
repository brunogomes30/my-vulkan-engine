#include<scene/scene.h>
#include<node/node.h>
#include<vk_types.h>
#include <glm/gtx/transform.hpp>
#include <scene/light/light.h>
#include <gizmos/gizmos_controller.h>	
#include <vk_types.h>
#include <chrono>

void Scene::init(Camera* camera, GizmosController* gizmosController) {
	this->camera = camera;

	//Temporary light until we can read from gltf
	lights.push_back(Light::CreatePointLight(glm::vec3(0, 0, 10), glm::vec4(0.7f), 0.5f));
	lights.push_back(Light::CreateDirectionalLight(glm::vec3(0, 1, 0.5), glm::vec4(0.4f, 0.4f, 0.4f, 1.0f))); // RGBA ABGR
	_gizmosController = gizmosController;
	

}


void Scene::update_scene(GPUSceneData& sceneData) {
	//begin clock
	auto start = std::chrono::system_clock::now();
	mainDrawContext.OpaqueSurfaces.clear();

	// camera projection
	//sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	sceneData.proj[1][1] *= -1;
	sceneData.viewproj = sceneData.proj * sceneData.view;

	//some default lighting parameters
	sceneData.ambientColor = glm::vec4(.1f);
	sceneData.sunlightColor = glm::vec4(1.f);
	sceneData.sunlightDirection = glm::vec4(0, 1, 0.5, 1.f);
	sceneData.lights[0] = this->lights[0].lightData;
	//sceneData.lights[1] = this->lights[1].lightData;
	sceneData.lightCount = lights.size();

	loadedGLTF->Draw(glm::mat4{ 1.f }, mainDrawContext);
	//end clock
	auto end = std::chrono::system_clock::now();
	std::chrono::duration<double> elapsed_seconds = end - start;
	sceneStats.scene_update_time = elapsed_seconds.count();
}

void Scene::draw_scene(VkCommandBuffer& cmd, VkDescriptorSet& globalDescriptor, const GPUSceneData& sceneData)
{	
    sceneStats.drawcall_count = 0;
    sceneStats.triangle_count = 0;
	sort_drawables(sceneData);
	VkBuffer lastIndexBuffer = VK_NULL_HANDLE;
	for (const uint32_t i : opaque_draws) {
		const RenderObject& draw = mainDrawContext.OpaqueSurfaces[i];
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->pipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 0, 1, &globalDescriptor, 0, nullptr);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, draw.material->pipeline->layout, 1, 1, &draw.material->materialSet, 0, nullptr);
		if (draw.indexBuffer != lastIndexBuffer) {
			vkCmdBindIndexBuffer(cmd, draw.indexBuffer, 0, VK_INDEX_TYPE_UINT32);
			lastIndexBuffer = draw.indexBuffer;
		}

		GPUDrawPushConstants pushConstants;
		pushConstants.vertexBuffer = draw.vertexBufferAddress;
		pushConstants.worldMatrix = draw.transform;
		vkCmdPushConstants(cmd, draw.material->pipeline->layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &pushConstants);

		vkCmdDrawIndexed(cmd, draw.indexCount, 1, draw.firstIndex, 0, 0);
        sceneStats.drawcall_count++;
        sceneStats.triangle_count += draw.indexCount / 3;
	}
}

void Scene::draw_gizmos(VkCommandBuffer& cmd, VkDescriptorSet& globalDescriptor, const GPUSceneData& sceneData) {
	//Draw lights as gizmos
	for (auto& l : lights) {
		// create square
		_gizmosController->draw_gizmo(cmd, GizmoShape::RECTANGLE, l.lightData.position);		
	}
}

void Scene::sort_drawables(const GPUSceneData& sceneData) {
	std::vector<uint32_t> opaque_draws;
	opaque_draws.reserve(mainDrawContext.OpaqueSurfaces.size());

	for (uint32_t i = 0; i < mainDrawContext.OpaqueSurfaces.size(); i++) {
		if (bounds::is_visible(mainDrawContext.OpaqueSurfaces[i], sceneData.viewproj)) {
			opaque_draws.push_back(i);
		}
		
	}

	// sort the opaque surfaces by material and mesh
	std::sort(opaque_draws.begin(), opaque_draws.end(), [&](const auto& iA, const auto& iB) {
		const RenderObject& A = mainDrawContext.OpaqueSurfaces[iA];
		const RenderObject& B = mainDrawContext.OpaqueSurfaces[iB];
		if (A.material == B.material) {
			return A.indexBuffer < B.indexBuffer;
		}
		else {
			return A.material < B.material;
		}
	});
	this->opaque_draws = opaque_draws;
}

void Scene::load_node(std::string name, std::shared_ptr<Node> node) {
	loadedNodes[name] = node;
}
