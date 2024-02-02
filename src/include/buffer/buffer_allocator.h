#pragma once
#include<vk_types.h>
#include<vulkan_engine/engine_components.h>
struct BufferAllocator {
private:
	VmaAllocator _allocator;
	std::shared_ptr<EngineComponents> _engineComponents;
public:
	void init(std::shared_ptr<EngineComponents> engineComponents);
	AllocatedBuffer create_buffer(size_t allocateSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);
	void destroy_buffer(const AllocatedBuffer& buffer);
};