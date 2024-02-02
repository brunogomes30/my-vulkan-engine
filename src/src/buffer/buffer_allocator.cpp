#include <buffer/buffer_allocator.h>
#include <vk_allocation_def.h>

void BufferAllocator::init(std::shared_ptr<EngineComponents> engineComponents) {
    _engineComponents = engineComponents;
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _engineComponents->chosenGPU;
    allocatorInfo.device = _engineComponents->device;
    allocatorInfo.instance = _engineComponents->instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
    _engineComponents->allocator = _allocator;
    _engineComponents->mainDeletionQueue->push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });
}

AllocatedBuffer BufferAllocator::create_buffer(size_t allocateSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage)
{
    // Allocate buffer
    VkBufferCreateInfo bufferInfo = { .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocateSize;

    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmallocInfo = {};
    vmallocInfo.usage = memoryUsage;
    vmallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    //allocate the buffer
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &vmallocInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.info));

    return newBuffer;
}

void BufferAllocator::destroy_buffer(const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}
