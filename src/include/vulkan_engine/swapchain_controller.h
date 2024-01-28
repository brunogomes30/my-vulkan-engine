#pragma once
#include<vk_types.h>
#include<vulkan_engine/engine_components.h>
class SwapchainController {

private:
	std::shared_ptr<EngineComponents> _engineComponents;
	VkSurfaceKHR _surface;
	VkExtent2D _windowExtent;
	

public:
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	VkSwapchainKHR  swapchain;
	VkExtent2D swapchainExtent;
	VkFormat swapchainImageFormat;

public:
	void init_swapchain(std::shared_ptr<EngineComponents> engineComponents, VkExtent2D& windowExtent);
	void create_swapchain(uint32_t width, uint32_t height);
	void destroy_swapchain();
	void resize_swapchain(struct SDL_Window* window, float renderScale);
};