#pragma once
#include <draw/draw_context.h>
#include<scene/scene_data.h>
class Node;

class Scene {
private:
	DrawContext mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> loadedNodes;
public:
	//void init();
	void update_scene(GPUSceneData& sceneData);
	void draw_scene(VkCommandBuffer& cmd, VkDescriptorSet globalDescriptor);
	void load_node(std::string name, std::shared_ptr<Node> node);
};