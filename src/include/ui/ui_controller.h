#pragma once
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"	

class VulkanEngine;
class UIController {
private:
	VkExtent2D _swapchainExtent;
	VkDevice _device;

public:
	void setup(const VkExtent2D& swapchainExtent, const VkDevice& device, ImGui_ImplVulkan_InitInfo& init_info);
	void render();
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void cleanup();
};