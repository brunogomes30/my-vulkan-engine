// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_allocation_def.h>
#include <vk_pipelines.h>
#include<controller/texture_controller.h>
#include<materials/materials.h>

#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"	
#include <vk_loader.h>
#include<scene/scene.h>
#include<scene/scene_data.h>
#include<camera/camera.h>
#include<stats/engine_stats.h>
#include<ui/ui_controller.h>
#include<vulkan_engine/engine_components.h>
#include<vulkan_engine/swapchain_controller.h>
#include<vulkan_engine/pipeline_controller.h>
#include<vulkan_engine/draw_controller.h>
#include<vulkan_engine/command_controller.h>
#include<materials/material_controller.h>
#include<gizmos/gizmos_controller.h>
#include "vk_mem_alloc.h"


class VulkanEngine {
public:
	bool _isInitialized{ false };
	int _frameNumber{ 0 };
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };
	bool resize_requested;
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug message handle
	std::shared_ptr<EngineComponents> _components;

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;
	DeletionQueue _mainDeletionQueue;
	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	//Mesh
	std::vector<std::shared_ptr<MeshAsset>> testMeshes; // To be removed later

	Scene* scene = new Scene();
	Camera mainCamera;
	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	EngineStats stats;



	//initializes everything in the engine
	void init();
	//shuts down the engine
	void cleanup();
	//run main loop
	void run();

	FrameData& get_current_frame() { return _components->frames[_frameNumber % FRAME_OVERLAP]; };
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	static VulkanEngine& Get();

	// Temporary public TODO::
	BufferAllocator _bufferAllocator;
	MaterialController _materialController;
	TextureController _textureController;
	SwapchainController _swapchainController;

private:
	UIController _uiController;
	PipelineController _pipelineController;
	DescriptorController _descriptorController;
	DrawController _drawController;
	CommandController _commandController;
	GizmosController _gizmosController;

	void init_vulkan();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_imgui();
	void init_default_data();

};
