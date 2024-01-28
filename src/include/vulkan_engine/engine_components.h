#pragma once
#include <vk_types.h>
#include<vk_descriptors.h>
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
	VkSurfaceKHR surface; // Vulkan window surface
	DeletionQueue* mainDeletionQueue;
	AllocatedImage* drawImage;
	AllocatedImage* depthImage;

	VmaAllocator allocator;
	VkExtent2D* drawExtent;
	struct SDL_Window* _window;

	FrameData frames[FRAME_OVERLAP];
};