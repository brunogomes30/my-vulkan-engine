#pragma once

#include <vk_types.h>
class VulkanEngine;
class TextureController {
	public:
		TextureController() = default;
		TextureController(VmaAllocator& allocator, VkDevice& device, VulkanEngine* engine) : _allocator(allocator), _device(device), _engine(engine) {}

		AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
		AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
		void destroy_image(const AllocatedImage& img);

	private:
	VmaAllocator _allocator;
	VkDevice _device;
	VulkanEngine* _engine; // remove this dependency later

};