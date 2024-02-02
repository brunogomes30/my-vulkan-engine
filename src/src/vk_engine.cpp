//> includes
#define VMA_IMPLEMENTATION
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#include "VkBootstrap.h"
#include "vk_images.h"


#include <vk_loader.h>
#include<controller/texture_controller.h>
#include<materials/materials.h>
#include<node/mesh_node.h>

#include<loader/loader_gltf.h>
#include<exceptions/window_resize_exception.h>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }
void VulkanEngine::init()
{
    _components = std::make_shared<EngineComponents>();
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _components->_window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);
    _components->mainDeletionQueue = &_mainDeletionQueue;
    _components->drawImage = &_drawImage;
    _components->depthImage = &_depthImage;
    _components->drawExtent = &_drawExtent;
    init_vulkan();
    _swapchainController.init_swapchain(_components, _windowExtent);
    init_commands();
    init_sync_structures();
    init_descriptors();
    _bufferAllocator.init(_components);
    _drawController.init(_components, &_descriptorController, &_pipelineController, &_swapchainController, &_bufferAllocator, &_materialController, &_uiController, &stats);
    _textureController.init(_components, &_bufferAllocator, &_commandController);
    _materialController.init(_components, &_textureController, &_bufferAllocator, &_descriptorController);
    init_pipelines();
    init_imgui();
    
    init_default_data();
    mainCamera.init();

    std::string structurePath = { "..\\..\\assets\\littlest_neo_tokyo.glb" };
    auto structureFile = loader_gltf::loadGltf(this, structurePath, &_materialController);

    assert(structureFile.has_value());

    loadedScenes["littlest_neo_tokyo"] = *structureFile;
    scene->set_loadedGLTF(loadedScenes["littlest_neo_tokyo"]);
    scene->init(&mainCamera);

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_vulkan() {
    vkb::InstanceBuilder builder;
    bool bUseValidationLayers = true;
    auto inst_ret = builder.set_app_name("BCGomes Vulkan Engine")
		.request_validation_layers(bUseValidationLayers)
		.use_default_debug_messenger()
		.require_api_version(1, 3, 0)
		.build();

    vkb::Instance vkb_inst = inst_ret.value();

    _components->instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_components->_window, _components->instance, &_components->surface);

    //Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features vk13features{};
    vk13features.dynamicRendering = true;
    vk13features.synchronization2 = true;

    //Vulkan 1.2 features
    VkPhysicalDeviceVulkan12Features vk12features{};
    vk12features.bufferDeviceAddress = true;
    vk12features.descriptorIndexing = true;

    // use vkbootstrap to select a gpu.
    // We want a gpu that can write to the SDL surface and supports Vulkan 1.3
    vkb::PhysicalDeviceSelector selector{ vkb_inst };
    vkb::PhysicalDevice physicalDevice = selector
		.set_minimum_version(1, 2)
        .set_required_features_13(vk13features)
        .set_required_features_12(vk12features)
		.set_surface(_components->surface)
		.select()
		.value();

    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    _components->device= vkbDevice.device;
    _components->chosenGPU = physicalDevice.physical_device;

    // Get the graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    _components->graphicsQueue = _graphicsQueue;
    _components->graphicsQueueFamily = _graphicsQueueFamily;

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _components->chosenGPU;
    allocatorInfo.device = _components->device;
    allocatorInfo.instance = _components->instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_components->allocator);
    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_components->allocator);
    });

}

void VulkanEngine::init_descriptors() {
    _descriptorController.init(_components);
    _descriptorController.init_descriptors();
}

void VulkanEngine::init_pipelines() {
    // Compute pipelines
    _pipelineController.init(_components, &_descriptorController);
    _pipelineController.init_background_pipelines(_drawController.backgroundEffects);
    _pipelineController.init_mesh_pipeline();

    _materialController.init_material_pipelines();
}



void VulkanEngine::init_default_data() {
    testMeshes = loadGltfMeshes(this, "..\\..\\assets\\basicmesh.glb").value();
    _materialController.init_default_data();
    //loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, mainDrawContext);
    
}

void VulkanEngine::init_commands() {
    _commandController.init(_components);
}

void VulkanEngine::init_sync_structures() {
    // One fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_components->device, &fenceCreateInfo, nullptr, &_components->frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(_components->device, &semaphoreCreateInfo, nullptr, &_components->frames[i].swapSemaphore));
        VK_CHECK(vkCreateSemaphore(_components->device, &semaphoreCreateInfo, nullptr, &_components->frames[i].renderSemaphore));
    }

    // Add immediate fence
    VK_CHECK(vkCreateFence(_components->device, &fenceCreateInfo, nullptr, &_commandController.immediateFence));
    _mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_components->device, _commandController.immediateFence, nullptr);
	});
}

void VulkanEngine::init_imgui() {
    _uiController.init(_components, &_swapchainController, &_commandController);

}



GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;
    newSurface.vertexBuffer = _bufferAllocator.create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT  
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // find address of the vertex buffer
    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_components->device, &deviceAddressInfo);

    // create index buffer
    newSurface.indexBuffer = _bufferAllocator.create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


    AllocatedBuffer staging = _bufferAllocator.create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    //copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    //copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    _commandController.immediate_submit([&](VkCommandBuffer cmd) {
        VkBufferCopy vertexCopy{ 0 };
        vertexCopy.dstOffset = 0;
        vertexCopy.srcOffset = 0;
        vertexCopy.size = vertexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

        VkBufferCopy indexCopy{ 0 };
        indexCopy.dstOffset = 0;
        indexCopy.srcOffset = vertexBufferSize;
        indexCopy.size = indexBufferSize;

        vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
    });

    _bufferAllocator.destroy_buffer(staging);
    return newSurface;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {
        // make sure the gpu has stopped doing its things
        vkDeviceWaitIdle(_components->device);

        loadedScenes.clear();
        _mainDeletionQueue.flush();

        //Clean up sync structures
        for (int i = 0; i < FRAME_OVERLAP; i++) {

            //already written from before
            vkDestroyCommandPool(_components->device, _components->frames[i].commandPool, nullptr);

            //destroy sync objects
            vkDestroyFence(_components->device, _components->frames[i].renderFence, nullptr);
            vkDestroySemaphore(_components->device, _components->frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_components->device, _components->frames[i].swapSemaphore, nullptr);
        }

        vkDeviceWaitIdle(_components->device);

        for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_components->device, _components->frames[i].commandPool, nullptr);
		}

        _swapchainController.destroy_swapchain();

        vkDestroySurfaceKHR(_components->instance, _components->surface, nullptr);
        vkDestroyDevice(_components->device, nullptr);

        vkb::destroy_debug_utils_messenger(_components->instance, _debug_messenger);
        vkDestroyInstance(_components->instance, nullptr);
        SDL_DestroyWindow(_components->_window);

    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        //begin clock
        auto start = std::chrono::system_clock::now();
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                    stop_rendering = true;
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED) {
                    stop_rendering = false;
                }
            }
            //send SDL event to imgui for handling
            mainCamera.processSDLEvent(e);
            ImGui_ImplSDL2_ProcessEvent(&e);
            
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (resize_requested) {
            _swapchainController.resize_swapchain(_components->_window, renderScale);
            resize_requested = false;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_components->_window);
        ImGui::NewFrame();
        //some imgui UI to test
        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
            ComputeEffect& selected = _drawController.backgroundEffects[_drawController.currentBackgroundEffect];

            ImGui::Text("Selected Effect ", selected.name);
            ImGui::SliderInt("Effect index", &_drawController.currentBackgroundEffect, 0, _drawController.backgroundEffects.size() - 1);

            ImGui::InputFloat4("data1", (float*)&selected.data.data1);
            ImGui::InputFloat4("data2", (float*)&selected.data.data2);
            ImGui::InputFloat4("data3", (float*)&selected.data.data3);
            ImGui::InputFloat4("data4", (float*)&selected.data.data4);

            ImGui::End();
        }
        ImGui::Begin("Stats");

        ImGui::Text("frametime %f ms", stats.frametime);
        ImGui::Text("draw time %f ms", stats.mesh_draw_time);
        ImGui::Text("update time %f ms", stats.scene_update_time);
        ImGui::Text("triangles %i", stats.triangle_count);
        ImGui::Text("draws %i", stats.drawcall_count);
        ImGui::End();


        //make imgui calculate internal draw structures
        ImGui::Render();

        FrameData& currentFrame = get_current_frame();
        try {
            _drawController.draw(scene, currentFrame);
        }
        catch (WindowResizeException e) {
            resize_requested = true;
        }
        //draw();
        // increase the number of frames drawn
        _frameNumber++;



        
        //get clock again, compare with start clock
        auto end = std::chrono::system_clock::now();

        //convert to microseconds (integer), and then come back to miliseconds
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frametime = elapsed.count() / 1000.f;
    }
}

