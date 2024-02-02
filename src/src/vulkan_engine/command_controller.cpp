#include<vulkan_engine/command_controller.h>
#include <vk_initializers.h>



void CommandController::init(std::shared_ptr<EngineComponents> engineComponents)
{
	_engineComponents = engineComponents;

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _engineComponents->graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.pNext = nullptr;

    for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_engineComponents->device, &poolInfo, nullptr, &_engineComponents->frames[i].commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_engineComponents->frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_engineComponents->device, &cmdAllocInfo, &_engineComponents->frames[i].mainCommandBuffer));

    }

    // Add immediate command
    VK_CHECK(vkCreateCommandPool(_engineComponents->device, &poolInfo, nullptr, &immediateCommandPool));
    //Allocate command for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(immediateCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_engineComponents->device, &cmdAllocInfo, &immediateCommandBuffer));

    _engineComponents->mainDeletionQueue->push_function([=]() {
        vkDestroyCommandPool(_engineComponents->device, immediateCommandPool, nullptr);
    });
}

void CommandController::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(_engineComponents->device, 1, &immediateFence));
    VK_CHECK(vkResetCommandBuffer(immediateCommandBuffer, 0));

    VkCommandBuffer cmd = immediateCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    //Submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_engineComponents->graphicsQueue, 1, &submit, immediateFence));
    VK_CHECK(vkWaitForFences(_engineComponents->device, 1, &immediateFence, true, 9999999999));

}
