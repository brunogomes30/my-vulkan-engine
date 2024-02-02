#pragma once
#include <vulkan_engine/engine_components.h>
#include <vulkan_engine/descriptor_controller.h>
struct ComputePushConstants {
	glm::vec4 data1;
	glm::vec4 data2;
	glm::vec4 data3;
	glm::vec4 data4;
};

struct ComputeEffect {
	const char* name;

	VkPipeline pipeline;
	VkPipelineLayout layout;

	ComputePushConstants data;
};

class PipelineController {

public:
	VkPipeline gradientPipeline;
	VkPipelineLayout gradientPipelineLayout;

	VkPipelineLayout meshPipelineLayout;
	VkPipeline meshPipeline;

	void init(std::shared_ptr<EngineComponents> engineComponents, DescriptorController* descriptorController);
	void init_background_pipelines(std::vector<ComputeEffect>& backgroundEffects);
	void init_mesh_pipeline();
private:
	DescriptorController* _descriptorController;

	std::shared_ptr<EngineComponents> _engineComponents;
	

	int currentBackgroundEffect{ 0 };
};