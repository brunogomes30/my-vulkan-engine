#pragma once
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"	
#include<vulkan_engine/engine_components.h>
#include<vulkan_engine/swapchain_controller.h>
#include<vulkan_engine/command_controller.h>

class VulkanEngine;
class UIController {
private:
	std::shared_ptr<EngineComponents> _engineComponents;
	SwapchainController* _swapchainController;
	CommandController* _commandController;
	

public:
	void init(std::shared_ptr<EngineComponents> engineComponents, SwapchainController* swapchainController, CommandController* commandController);
	void render();
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void cleanup();
};