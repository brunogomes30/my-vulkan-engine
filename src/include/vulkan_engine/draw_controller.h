#pragma once
#include <vulkan_engine/engine_components.h>
#include<vulkan_engine/descriptor_controller.h>
#include<vulkan_engine/pipeline_controller.h>
#include<vulkan_engine/swapchain_controller.h>
#include<ui/ui_controller.h>
#include<materials/material_controller.h>
#include<buffer/buffer_allocator.h>
#include<stats/engine_stats.h>
#include<scene/scene_data.h>
#include <vk_types.h>
#include "VkBootstrap.h"
#include "vk_mem_alloc.h"
class DrawController {

public:


	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };


	void init(
		std::shared_ptr<EngineComponents> engineComponents,
		DescriptorController* descriptorController,
		PipelineController* pipelineController,
		SwapchainController* swapchainController,
		BufferAllocator* _bufferAllocator,
		MaterialController* materialController,
		UIController* uiController,
		EngineStats* stats);
	void draw(class Scene* scene, FrameData& frame);
	
	GPUSceneData sceneData;
private:

	std::shared_ptr<EngineComponents> _engineComponents;
	DescriptorController* _descriptorController;
	PipelineController* _pipelineController;
	SwapchainController* _swapchainController;
	BufferAllocator* _bufferAllocator;
	MaterialController* _materialController;
	UIController* _uiController;
	EngineStats* _stats;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;


	void draw_background(VkCommandBuffer cmd, FrameData& frameData);
	void draw_geometry(VkCommandBuffer cmd, Scene* scene, FrameData& frameData);

};