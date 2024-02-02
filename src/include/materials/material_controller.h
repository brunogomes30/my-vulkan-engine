#pragma once
#include<vulkan_engine/engine_components.h>
#include<controller/texture_controller.h>
#include<buffer/buffer_allocator.h>
#include "vk_mem_alloc.h"
#include<vulkan_engine/descriptor_controller.h>
#include<materials/materials.h>
class MaterialController {
public:

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	AllocatedImage errorCheckerboardImage;
	//Delete later
	AllocatedImage whiteImage;
	AllocatedImage blackImage;
	AllocatedImage greyImage;
	VkSampler defaultSamplerLinear;
	VkSampler defaultSamplerNearest;


	void init(std::shared_ptr<EngineComponents> engineComponents, TextureController* textureController, BufferAllocator* bufferAllocator, DescriptorController* descriptorController);
	void init_default_data();
	void init_material_pipelines();
private:

	TextureController* _textureController;
	BufferAllocator* _bufferAllocator;
	DescriptorController _descriptorController;
	std::shared_ptr<EngineComponents> _engineComponents;
	

};