#pragma once
#include <vk_types.h>
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
};