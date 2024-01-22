// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vk_descriptors.h>
#include "vk_mem_alloc.h"
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

#define SHADERS_PATH(VAR) "../../shaders/"#VAR

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

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
	VkSemaphore swapSemaphore, renderSemaphore;
	VkFence renderFence;
	DeletionQueue deletionQueue;

	DescriptorAllocatorGrowable _frameDescriptors;

};

constexpr unsigned int FRAME_OVERLAP = 2;

class VulkanEngine {
public:
	bool _isInitialized{ false };
	int _frameNumber {0};
	bool stop_rendering{ false };
	VkExtent2D _windowExtent{ 1700 , 900 };

	struct SDL_Window* _window{ nullptr };

	static VulkanEngine& Get();

	//Added variables below
	VkInstance _instance; // Vulkan library handle
	VkDebugUtilsMessengerEXT _debug_messenger; // Vulkan debug message handle
	VkPhysicalDevice _chosenGPU; // GPU chosen as the default device
	VkDevice _device; // Vulkan device for commands
	VkSurfaceKHR _surface; // Vulkan window surface

	VkSwapchainKHR  _swapchain;
	VkFormat _swapchainImageFormat;

	std::vector<VkImage> _swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;
	VkExtent2D _swapchainExtent;

	FrameData _frames[FRAME_OVERLAP];

	FrameData& get_current_frame() {return _frames[_frameNumber % FRAME_OVERLAP];};

	VkQueue _graphicsQueue;
	uint32_t _graphicsQueueFamily;


	DeletionQueue _mainDeletionQueue;
	VmaAllocator _allocator;

	//draw resources
	AllocatedImage _drawImage;
	AllocatedImage _depthImage;
	VkExtent2D _drawExtent;
	float renderScale = 1.f;

	DescriptorAllocatorGrowable globalDescriptorAllocator;

	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorLayout;


	VkPipeline _gradientPipeline;
	VkPipelineLayout _gradientPipelineLayout;

	// immediate submit structures
	VkFence _immediateFence;
	VkCommandPool _immediateCommandPool;
	VkCommandBuffer _immediateCommandBuffer;

	//Constants 
	std::vector<ComputeEffect> backgroundEffects;
	int currentBackgroundEffect{ 0 };

	//Mesh
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;

	GPUMeshBuffers rectangle;

	std::vector<std::shared_ptr<MeshAsset>> testMeshes;

	bool resize_requested;

	GPUSceneData sceneData;

	VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;
	VkDescriptorSetLayout _singleImageDescriptorLayout;

	TextureController _textureController;

	MaterialInstance defaultData;
	GLTFMetallic_Roughness metalRoughMaterial;

	Scene* scene = new Scene();
	Camera mainCamera;

	std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedScenes;

	EngineStats stats;
	


	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);
	GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);
	//Added functions below

	AllocatedBuffer create_buffer(size_t allocateSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);

	// Temporary public TODO::
	AllocatedImage _errorCheckerboardImage;
	//Delete later
	AllocatedImage _whiteImage;
	AllocatedImage _blackImage;
	AllocatedImage _greyImage;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;

private:

	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_sync_structures();
	void init_descriptors();
	void init_pipelines();
	void init_background_pipelines();
	void init_mesh_pipeline();
	void init_imgui();
	void init_default_data();

	

	
	void draw_background(VkCommandBuffer cmd);
	void draw_geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();

	void resize_swapchain();
	

};
