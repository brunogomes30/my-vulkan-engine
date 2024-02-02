#pragma once

#include <vk_types.h>
#include<fastgltf/parser.hpp>
#include<buffer/buffer_allocator.h>
#include<vulkan_engine/command_controller.h>
class VulkanEngine;
class TextureController {
	public:

		void init(std::shared_ptr<EngineComponents> engineComponents, BufferAllocator* bufferAllocator, CommandController* commandController);

		AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
		AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
		std::optional<AllocatedImage> load_image(fastgltf::Asset& asset, fastgltf::Image& image);
		void destroy_image(const AllocatedImage& img);

	private:
		BufferAllocator* _bufferAllocator;
		VmaAllocator _allocator;
		CommandController* _commandController;
		VkDevice _device;
		std::shared_ptr<EngineComponents> _engineComponents; // remove this dependency later

};