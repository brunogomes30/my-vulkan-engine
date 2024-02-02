#pragma once
#include <draw/draw_context.h>
#include<scene/scene_data.h>
#include<stats/scene_stats.h>
#include<loader/loaded_gltf.h>
#include<camera/camera.h>
class Node;
class Scene {
private:
	DrawContext mainDrawContext;
	Camera* camera;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
	std::shared_ptr<LoadedGLTF> loadedGLTF;
	std::vector<uint32_t> opaque_draws;

public:

    SceneStats sceneStats;

	/*
	* Initializes the scene, sets extra data for the scene that won't be set in loadedGLTF
	*/
	void init(Camera* camera);
	void update_scene(GPUSceneData& sceneData);
	void draw_scene(VkCommandBuffer& cmd, VkDescriptorSet& globalDescriptor, const GPUSceneData& sceneData);
	void sort_drawables(const GPUSceneData& sceneData);
	void load_node(std::string name, std::shared_ptr<Node> node);
	void set_loadedGLTF(std::shared_ptr<LoadedGLTF> loadedGLTF){ this->loadedGLTF = loadedGLTF; }

	Camera* getCamera() const { return camera; }
};