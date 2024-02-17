#include <vulkan_engine/draw_controller.h>
#include <vk_images.h>
#include<scene/scene.h>
#include <glm/gtx/transform.hpp>
#include <vk_initializers.h>
#include<exceptions/window_resize_exception.h>
#include <vulkan/vulkan_core.h>


void DrawController::init(
	std::shared_ptr<EngineComponents> engineComponents,
	DescriptorController* descriptorController,
	PipelineController* pipelineController,
	SwapchainController* swapchainController,
	BufferAllocator* bufferAllocator,
	MaterialController* materialController,
	UIController* uiController,
	EngineStats* stats)
{
	_engineComponents = engineComponents;
	_descriptorController = descriptorController;
	_pipelineController = pipelineController;
	_swapchainController = swapchainController;
	_bufferAllocator = bufferAllocator;
	_materialController = materialController;
	_uiController = uiController;
	_stats = stats;

	_graphicsQueue = _engineComponents->graphicsQueue;
	_graphicsQueueFamily = _engineComponents->graphicsQueueFamily;
}
void DrawController::draw(Scene* scene, FrameData& frameData)
{
	sceneData.proj = glm::perspective(glm::radians(70.f), (float)_engineComponents->drawExtent->width / (float)_engineComponents->drawExtent->height, 10000.f, 0.1f);
	Camera* camera = scene->getCamera();
	camera->update();
	sceneData.view = camera->getViewMatrix();
	sceneData.inverseView = glm::inverse(sceneData.view);
	sceneData.cameraPosition = glm::vec4(camera->position, 1.f);
	scene->update_scene(sceneData);
	_stats->scene_update_time = scene->sceneStats.scene_update_time;

	//wait until the gpu has finished rendering the last frame. Timeout of 1 second
	VK_CHECK(vkWaitForFences(_engineComponents->device, 1, &frameData.renderFence, true, 1000000000));

	frameData.deletionQueue.flush();
	frameData._frameDescriptors.clear_pools(_engineComponents->device);

	frameData.deletionQueue.flush();
	VK_CHECK(vkResetFences(_engineComponents->device, 1, &frameData.renderFence));

	uint32_t swapchainImageIndex;

	VkResult e = vkAcquireNextImageKHR(_engineComponents->device, _swapchainController->swapchain, 1000000000, frameData.swapSemaphore, nullptr, &swapchainImageIndex);
	if (e == VK_ERROR_OUT_OF_DATE_KHR) {
		throw WindowResizeException();
	}
	//VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, frameData.swapSemaphore, nullptr, &swapchainImageIndex));


	VkCommandBuffer cmd = frameData.mainCommandBuffer;

	// now that we are sure that the commands finished executing, we can safely
	// reset the command buffer to begin recording again.
	VK_CHECK(vkResetCommandBuffer(cmd, 0));

	//begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	_engineComponents->drawExtent->width = _engineComponents->drawImage->imageExtent.width;
	_engineComponents->drawExtent->height = _engineComponents->drawImage->imageExtent.height;

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

	// transition our main draw image into general layout so we can write into it
	// we will overwrite it all so we dont care about what was the older layout
	vkutil::transition_image(cmd, _engineComponents->drawImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);


	draw_background(cmd, frameData);

	vkutil::transition_image(cmd, _engineComponents->drawImage->image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

	vkutil::transition_image(cmd, _engineComponents->depthImage->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	draw_geometry(cmd, scene, frameData);

	//transition the draw image and the swapchain image into their correct transfer layouts
	vkutil::transition_image(cmd, _engineComponents->drawImage->image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
	vkutil::transition_image(cmd, _swapchainController->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	// execute a copy from the draw image into the swapchain
	vkutil::copy_image_to_image(cmd, _engineComponents->drawImage->image, _swapchainController->swapchainImages[swapchainImageIndex], *_engineComponents->drawExtent, _swapchainController->swapchainExtent);

	//Draw imgui
	// set swapchain image layout to Attachment Optimal so we can draw it
	vkutil::transition_image(cmd, _swapchainController->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	//draw imgui into the swapchain image
	_uiController->draw_imgui(cmd, _swapchainController->swapchainImageViews[swapchainImageIndex]);

	// set swapchain image layout to Present so we can show it on the screen TODO::::::
	vkutil::transition_image(cmd, _swapchainController->swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

	//finalize the command buffer (we can no longer add commands, but it can now be executed)
	VK_CHECK(vkEndCommandBuffer(cmd));

	// prepare the submission to the queue.
	// we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	// we will signal the _renderSemaphore, to signal that rendering has finished

	VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

	VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frameData.swapSemaphore);
	VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, frameData.renderSemaphore);

	VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

	// submit command buffer to the queue and execute it.
	// _renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, frameData.renderFence));


	// prepare present
	// this will put the swapchain image we just rendered to into the visible window.
	// we want to wait on the _renderSemaphore for that,
	// as its necessary that drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = {
		.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
		.pNext = nullptr,
		.pSwapchains = &_swapchainController->swapchain,
	};
	presentInfo.swapchainCount = 1;
	presentInfo.pWaitSemaphores = &frameData.renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;

	presentInfo.pImageIndices = &swapchainImageIndex;



	VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
	if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
		throw WindowResizeException();
	}
}

void DrawController::draw_background(VkCommandBuffer cmd, FrameData& frameData)
{

	VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

	ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];


	VkViewport viewport = {};
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = _engineComponents->drawExtent->width;
	viewport.height = _engineComponents->drawExtent->height;
	viewport.minDepth = 0.f;
	viewport.maxDepth = 1.f;

	vkCmdSetViewport(cmd, 0, 1, &viewport);
	VkRect2D scissor = {};
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.height = _engineComponents->drawExtent->height;
	scissor.extent.width = _engineComponents->drawExtent->width;

	vkCmdSetScissor(cmd, 0, 1, &scissor);

	// bind the gradient drawing compute pipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	// bind the descriptor set containing the draw image for the compute pipeline
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _pipelineController->gradientPipelineLayout, 0, 1, &_descriptorController->drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, _pipelineController->gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	// execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
	vkCmdDispatch(cmd, std::ceil(_engineComponents->drawExtent->width / 16), std::ceil(_engineComponents->drawExtent->height / 16), 1);
}

void DrawController::draw_geometry(VkCommandBuffer cmd, Scene* scene, FrameData& frameData)
{
	//begin clock
	_stats->drawcall_count = 0;
	_stats->triangle_count = 0;
	_stats->mesh_draw_time = 0;
	auto start = std::chrono::system_clock::now();
	//allocate a new uniform buffer for the scene data
	AllocatedBuffer gpuSceneDataBuffer = _bufferAllocator->create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//add it to the deletion queue of this frame so it gets deleted once its been used
	frameData.deletionQueue.push_function([=, this]() {
		_bufferAllocator->destroy_buffer(gpuSceneDataBuffer);
	});

	//write the buffer
	GPUSceneData* sceneUniformData;
	vmaMapMemory(_engineComponents->allocator, gpuSceneDataBuffer.allocation, (void**)&sceneUniformData);
	*sceneUniformData = sceneData;
	vmaUnmapMemory(_engineComponents->allocator, gpuSceneDataBuffer.allocation);
	//create a descriptor set that binds that buffer and update it
	VkDescriptorSet globalDescriptor = frameData._frameDescriptors.allocate(_engineComponents->device, _descriptorController->gpuSceneDataDescriptorLayout);

	DescriptorWriter writer;
	writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
	writer.update_set(_engineComponents->device, globalDescriptor);

	// begin a render pass connected to our draw image
	VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_engineComponents->drawImage->imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
	VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_engineComponents->depthImage->imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

	VkRenderingInfo renderInfo = vkinit::rendering_info(*_engineComponents->drawExtent, &colorAttachment, &depthAttachment);
	vkCmdBeginRendering(cmd, &renderInfo);

	

	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineController->meshPipeline);

	//bind a texture
	/*
	VkDescriptorSet imageSet = frameData._frameDescriptors.allocate(_engineComponents->device, _descriptorController->singleImageDescriptorLayout);
	{
		DescriptorWriter writer;
		writer.write_image(0, _materialController->errorCheckerboardImage.imageView, _materialController->defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

		writer.update_set(_engineComponents->device, imageSet);
	}
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineController->meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);
	*/
	GPUDrawPushConstants push_constants;
	push_constants.worldMatrix = glm::mat4{ 1.f };

	glm::mat4 view = glm::translate(glm::vec3{ 0,0,-5 });
	// camera projection
	glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_engineComponents->drawExtent->width / (float)_engineComponents->drawExtent->height, 10000.f, 0.1f);

	// invert the Y direction on projection matrix so that we are more similar
	// to opengl and gltf axis
	projection[1][1] *= -1;

	push_constants.worldMatrix = projection * view;

	//push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

	vkCmdPushConstants(cmd, _pipelineController->meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
	//vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

	scene->draw_scene(cmd, globalDescriptor, sceneData);

	auto end = std::chrono::system_clock::now();

	//convert to microseconds (integer), and then come back to miliseconds
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	_stats->add_mesh_draw_time(elapsed.count() / 1000.f);
	_stats->add_triangle_count(scene->sceneStats.triangle_count);
	_stats->add_drawcall_count(scene->sceneStats.drawcall_count);


	// Draw gizmos
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineController->gizmoPipeline);
	vkCmdPushConstants(cmd, _pipelineController->gizmoPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineController->gizmoPipelineLayout, 0, 1, &globalDescriptor, 0, nullptr);
	scene->draw_gizmos(cmd, globalDescriptor, sceneData);
	vkCmdEndRendering(cmd);
}
