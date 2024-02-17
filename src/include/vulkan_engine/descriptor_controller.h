#pragma once
#include <vulkan_engine/engine_components.h>
#include <vk_descriptors.h>
class DescriptorController {
public:
	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet drawImageDescriptors; 
	VkDescriptorSetLayout drawImageDescriptorLayout; 

	VkDescriptorSetLayout gpuSceneDataDescriptorLayout; 
	VkDescriptorSetLayout gizmoDescriptorLayout; 
	VkDescriptorSetLayout singleImageDescriptorLayout; 


	void init(std::shared_ptr<EngineComponents> engineComponents);
	void init_descriptors();
private:
	std::shared_ptr<EngineComponents> _engineComponents;
	
};