#pragma once
#include <vk_types.h>
#include<vk_descriptors.h>
#include <vk_loader.h>
constexpr unsigned int FRAME_OVERLAP = 2;

struct FrameData {
	VkCommandPool commandPool;
	VkCommandBuffer mainCommandBuffer;
	VkSemaphore swapSemaphore, renderSemaphore;
	VkFence renderFence;
	DeletionQueue deletionQueue;

	DescriptorAllocatorGrowable _frameDescriptors;

};


struct EngineComponents{
	VkPhysicalDevice chosenGPU; // GPU chosen as the default device
	VkDevice device; // Vulkan device for commands
	VkQueue graphicsQueue;
	uint32_t graphicsQueueFamily;
	VkSurfaceKHR surface; // Vulkan window surface
	DeletionQueue* mainDeletionQueue;
	AllocatedImage* drawImage;
	AllocatedImage* depthImage;
	VkInstance instance; // Vulkan library handle
	VmaAllocator allocator;
	VkExtent2D* drawExtent;
	struct SDL_Window* _window;
	FrameData frames[FRAME_OVERLAP];
};