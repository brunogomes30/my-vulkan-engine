#pragma once

#include<vulkan_engine/engine_components.h>
class CommandController {
public:
	VkFence immediateFence;
	VkCommandPool immediateCommandPool;
	VkCommandBuffer immediateCommandBuffer;
	
	void init(std::shared_ptr<EngineComponents> engineComponents);
	void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
	std::shared_ptr<EngineComponents> _engineComponents;
};