//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

#include <chrono>
#include <thread>

#include "VkBootstrap.h"
#include "vk_images.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <vk_loader.h>
#include <glm/gtx/transform.hpp>
#include<controller/texture_controller.h>
#include<materials/materials.h>
#include<node/mesh_node.h>

#include<loader/loader_gltf.h>

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

    _window = SDL_CreateWindow(
        "Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);
    _components->_window = _window;
    _components->mainDeletionQueue = &_mainDeletionQueue;
    _components->drawImage = &_drawImage;
    _components->depthImage = &_depthImage;
    _components->drawExtent = &_drawExtent;
    
    init_vulkan();
    _components->surface = _surface;
    _swapchainController.init_swapchain(_components, _surface, _windowExtent);
    init_commands();
    init_sync_structures();
    init_descriptors();
    init_pipelines();
    init_imgui();
    init_default_data();
    mainCamera.init();

    std::string structurePath = { "..\\..\\assets\\littlest_neo_tokyo.glb" };
    auto structureFile = loader_gltf::loadGltf(this, structurePath);

    assert(structureFile.has_value());

    loadedScenes["littlest_neo_tokyo"] = *structureFile;
    scene->set_loadedGLTF(loadedScenes["littlest_neo_tokyo"]);
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

    _instance = vkb_inst.instance;
    _debug_messenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

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
		.set_surface(_surface)
		.select()
		.value();

    vkb::DeviceBuilder deviceBuilder{ physicalDevice };
    vkb::Device vkbDevice = deviceBuilder.build().value();

    _components->device= vkbDevice.device;
    _components->chosenGPU = physicalDevice.physical_device;

    // Get the graphics queue
    _graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    _graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    // initialize the memory allocator
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = _components->chosenGPU;
    allocatorInfo.device = _components->device;
    allocatorInfo.instance = _instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &_allocator);
    _components->allocator = _allocator;
    _mainDeletionQueue.push_function([&]() {
        vmaDestroyAllocator(_allocator);
    });

}

void VulkanEngine::init_descriptors() {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };
    globalDescriptorAllocator.init(_components->device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        _drawImageDescriptorLayout = builder.build(_components->device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // allocate the descriptor set for our compute draw image
    _drawImageDescriptors = globalDescriptorAllocator.allocate(_components->device, _drawImageDescriptorLayout);

    DescriptorWriter writer;
    writer.write_image(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.update_set(_components->device, _drawImageDescriptors);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _frames[i]._frameDescriptors.init(_components->device, 1000, frame_sizes);

        _mainDeletionQueue.push_function([&, i]() {
            _frames[i]._frameDescriptors.destroy_pools(_components->device);
        });
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        _gpuSceneDataDescriptorLayout = builder.build(_components->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        _singleImageDescriptorLayout = builder.build(_components->device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
}

void VulkanEngine::init_pipelines() {
    // Compute pipelines
    init_background_pipelines();

    init_mesh_pipeline();

    metalRoughMaterial.build_pipelines(this);
}

void VulkanEngine::init_background_pipelines() {
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.setLayoutCount = 1;
    computeLayout.pSetLayouts = &_drawImageDescriptorLayout;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_components->device, &computeLayout, nullptr, &_gradientPipelineLayout));

    //Load shader

    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _components->device, &computeDrawShader)) {
        fmt::print("Error when building the compute shader \n");
	}

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(sky.comp.spv), _components->device, &skyShader)) {
        fmt::print("Error when building the sky shader \n");
    }

    VkPipelineShaderStageCreateInfo computeStageInfo{};
    computeStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    computeStageInfo.pNext = nullptr;
    computeStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    computeStageInfo.module = computeDrawShader;
    computeStageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineInfo{};
    computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineInfo.pNext = nullptr;
    computePipelineInfo.layout = _gradientPipelineLayout;
    computePipelineInfo.stage = computeStageInfo;

    ComputeEffect gradient;
    gradient.layout = _gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};
    
    //default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);


    VK_CHECK(vkCreateComputePipelines(_components->device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &gradient.pipeline));

    computePipelineInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = _gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};

    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_components->device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &sky.pipeline));

    //Add 2 background effects into the array
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_components->device, computeDrawShader, nullptr);
    vkDestroyShaderModule(_components->device, skyShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_components->device, _gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_components->device, _gradientPipeline, nullptr);
        vkDestroyPipeline(_components->device, sky.pipeline, nullptr);
    });
}

void VulkanEngine::init_mesh_pipeline() {
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(tex_image.frag.spv), _components->device, &meshFragShader)) {
        fmt::print("Error when building the triangle mesh fragment shader module");
    }
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(colored_triangle_mesh.vert.spv), _components->device, &meshVertexShader)) {
        fmt::print("Error when building the triangle mesh vertex shader module");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &_singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;


    VK_CHECK(vkCreatePipelineLayout(_components->device, &pipeline_layout_info, nullptr, &_meshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    //use the mesh layout we created
    pipelineBuilder._pipelineLayout = _meshPipelineLayout;
    //connecting the vertex and pixel shaders to the pipeline
    pipelineBuilder.set_shaders(meshVertexShader, meshFragShader);
    //it will draw triangles
    pipelineBuilder.set_input_topology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    //filled triangles
    pipelineBuilder.set_polygon_model(VK_POLYGON_MODE_FILL);
    //no backface culling
    pipelineBuilder.set_cull_mode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    //no multisampling
    pipelineBuilder.set_multisampling_none();
    //no blending
    pipelineBuilder.disable_blending();

    //pipelineBuilder.disable_depthtest();
    pipelineBuilder.enable_depthtest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    //connect the image format we will draw into, from draw image
    pipelineBuilder.set_color_attachment_format(_drawImage.imageFormat);
    pipelineBuilder.set_depth_format(_depthImage.imageFormat);

    //finally build the pipeline
    _meshPipeline = pipelineBuilder.build_pipeline(_components->device);

    //clean structures
    vkDestroyShaderModule(_components->device, meshFragShader, nullptr);
    vkDestroyShaderModule(_components->device, meshVertexShader, nullptr);

    _mainDeletionQueue.push_function([&]() {
        vkDestroyPipelineLayout(_components->device, _meshPipelineLayout, nullptr);
        vkDestroyPipeline(_components->device, _meshPipeline, nullptr);
    });

}

void VulkanEngine::init_default_data() {
    testMeshes = loadGltfMeshes(this, "..\\..\\assets\\basicmesh.glb").value();
    _textureController = TextureController(_allocator, _components->device, this);

    //DELETE later
    //3 default textures, white, grey, black. 1 pixel each
    uint32_t white = 0xFFFFFFFF;
    _whiteImage = _textureController.create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = 0xAAAAAAFF; 
    _greyImage = _textureController.create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = 0xFF000000;
    _blackImage = _textureController.create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    //checkerboard image
    uint32_t magenta = 0xFFFF00FF;
    std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }
    _errorCheckerboardImage = _textureController.create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_SAMPLED_BIT);

    VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;

    vkCreateSampler(_components->device, &sampl, nullptr, &_defaultSamplerNearest);

    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_components->device, &sampl, nullptr, &_defaultSamplerLinear);

    GLTFMetallic_Roughness::MaterialResources materialResources;
    //default the material textures
    materialResources.colorImage = _whiteImage;
    materialResources.colorSampler = _defaultSamplerLinear;
    materialResources.metalRoughImage = _whiteImage;
    materialResources.metalRoughSampler = _defaultSamplerLinear;

    //set the uniform buffer for the material data
    AllocatedBuffer materialConstants = create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //write the buffer
    GLTFMetallic_Roughness::MaterialConstants* sceneUniformData = (GLTFMetallic_Roughness::MaterialConstants*)materialConstants.allocation->GetMappedData();
    sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
    sceneUniformData->metal_rough_factors = glm::vec4{ 1,0.5,0,0 };

    _mainDeletionQueue.push_function([=, this]() {
        destroy_buffer(materialConstants);
    });

    materialResources.dataBuffer = materialConstants.buffer;
    materialResources.dataBufferOffset = 0;

    defaultData = metalRoughMaterial.write_material(_components->device, MaterialPass::MainColor, materialResources, globalDescriptorAllocator);
    std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
    newMaterial->data = defaultData;
    //loadedScenes["structure"]->Draw(glm::mat4{ 1.f }, mainDrawContext);
    
}


/*
void VulkanEngine::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder  swapchainBuilder{ _components->chosenGPU, _components->device, _surface };
    _swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
		.set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT) 
		.build()
		.value();
    _swapchainExtent = vkbSwapchain.extent;

    _swapchain = vkbSwapchain.swapchain;
    _swapchainController.swapchainImages = vkbSwapchain.get_images().value();
    _swapchainImageViews = vkbSwapchain.get_image_views().value();
}

void VulkanEngine::destroy_swapchain() {
    vkDestroySwapchainKHR(_components->device, _swapchain, nullptr);

    for (auto imageView : _swapchainImageViews) {
	    vkDestroyImageView(_components->device, imageView, nullptr);
    }
}



void VulkanEngine::init_swapchain() {
    create_swapchain(_windowExtent.width, _windowExtent.height);

    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    //hardcoding the draw format to 32 bit float
    _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    _drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(_drawImage.imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(_allocator, &rimg_info, &rimg_allocinfo, &_drawImage.image, &_drawImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_components->device, &rview_info, nullptr, &_drawImage.imageView));

    _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    _depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(_depthImage.imageFormat, depthImageUsages, drawImageExtent);

    //allocate and create the image
    vmaCreateImage(_allocator, &dimg_info, &rimg_allocinfo, &_depthImage.image, &_depthImage.allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_components->device, &dview_info, nullptr, &_depthImage.imageView));

    //add to deletion queues
    _mainDeletionQueue.push_function([=]() {
        vkDestroyImageView(_components->device, _drawImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

        vkDestroyImageView(_components->device, _depthImage.imageView, nullptr);
        vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
    });
}

*/
void VulkanEngine::init_commands() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = _graphicsQueueFamily;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.pNext = nullptr;

    for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateCommandPool(_components->device, &poolInfo, nullptr, &_frames[i].commandPool));

        // allocate the default command buffer that we will use for rendering
        VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_frames[i].commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_components->device, &cmdAllocInfo, &_frames[i].mainCommandBuffer));

	}

    // Add immediate command
    VK_CHECK(vkCreateCommandPool(_components->device, &poolInfo, nullptr, &_immediateCommandPool));
    //Allocate command for immediate submits
    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immediateCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(_components->device, &cmdAllocInfo, &_immediateCommandBuffer));

    _mainDeletionQueue.push_function([=]() {
		vkDestroyCommandPool(_components->device, _immediateCommandPool, nullptr);
	});
}

void VulkanEngine::init_sync_structures() {
    // One fence to control when the gpu has finished rendering the frame
    // and 2 semaphores to synchronize rendering with swapchain
    // we want the fence to start signalled so we can wait on it on the first frame
    const VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    const VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();

    for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
        VK_CHECK(vkCreateFence(_components->device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));
        VK_CHECK(vkCreateSemaphore(_components->device, &semaphoreCreateInfo, nullptr, &_frames[i].swapSemaphore));
        VK_CHECK(vkCreateSemaphore(_components->device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));
    }

    // Add immediate fence
    VK_CHECK(vkCreateFence(_components->device, &fenceCreateInfo, nullptr, &_immediateFence));
    _mainDeletionQueue.push_function([=]() {
		vkDestroyFence(_components->device, _immediateFence, nullptr);
	});
}

void VulkanEngine::init_imgui() {
    //_uiController.setup(_swapchainExtent);
    
    // 1: create descriptor pool for IMGUI
    //  the size of the pool is very oversize, but it's copied from imgui demo
    //  itself.
    VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
    pool_info.pPoolSizes = pool_sizes;

    VkDescriptorPool imguiPool;
    VK_CHECK(vkCreateDescriptorPool(_components->device, &pool_info, nullptr, &imguiPool));

    // 2: initialize imgui library

    // this initializes the core structures of imgui
    ImGui::CreateContext();

    // this initializes imgui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    // this initializes imgui for Vulkan
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = _instance;
    init_info.PhysicalDevice = _components->chosenGPU;
    init_info.Device = _components->device;
    init_info.Queue = _graphicsQueue;
    init_info.DescriptorPool = imguiPool;
    init_info.MinImageCount = 3;
    init_info.ImageCount = 3;
    init_info.UseDynamicRendering = true;
    init_info.ColorAttachmentFormat = _swapchainController.swapchainImageFormat;

    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

    ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

    // execute a gpu command to upload imgui font textures
    immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(cmd); });

    // clear font textures from cpu data
    ImGui_ImplVulkan_DestroyFontUploadObjects();

    // add the destroy the imgui created structures
    _mainDeletionQueue.push_function([=]() {
        vkDestroyDescriptorPool(_components->device, imguiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
    });
}

AllocatedBuffer VulkanEngine::create_buffer(size_t allocateSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage) {
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

void VulkanEngine::destroy_buffer(const AllocatedBuffer& buffer) {
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices) {
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;
    newSurface.vertexBuffer = create_buffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_TRANSFER_DST_BIT  
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY
    );

    // find address of the vertex buffer
    VkBufferDeviceAddressInfo deviceAddressInfo{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurface.vertexBuffer.buffer };
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_components->device, &deviceAddressInfo);

    // create index buffer
    newSurface.indexBuffer = create_buffer(indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);


    AllocatedBuffer staging = create_buffer(vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.allocation->GetMappedData();

    //copy vertex buffer
    memcpy(data, vertices.data(), vertexBufferSize);
    //copy index buffer
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    immediate_submit([&](VkCommandBuffer cmd) {
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

    destroy_buffer(staging);
    return newSurface;
}

void VulkanEngine::immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VK_CHECK(vkResetFences(_components->device, 1, &_immediateFence));
    VK_CHECK(vkResetCommandBuffer(_immediateCommandBuffer, 0));

    VkCommandBuffer cmd = _immediateCommandBuffer;

    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdInfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdInfo, nullptr, nullptr);

    //Submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immediateFence));
    VK_CHECK(vkWaitForFences(_components->device, 1, &_immediateFence, true, 9999999999));
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
            vkDestroyCommandPool(_components->device, _frames[i].commandPool, nullptr);

            //destroy sync objects
            vkDestroyFence(_components->device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_components->device, _frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_components->device, _frames[i].swapSemaphore, nullptr);
        }

        vkDeviceWaitIdle(_components->device);

        for (unsigned int i = 0; i < FRAME_OVERLAP; i++) {
			vkDestroyCommandPool(_components->device, _frames[i].commandPool, nullptr);
		}

        _swapchainController.destroy_swapchain();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_components->device, nullptr);

        vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
        vkDestroyInstance(_instance, nullptr);
        SDL_DestroyWindow(_window);

    }

      

    // clear engine pointer
    loadedEngine = nullptr;
}


void VulkanEngine::draw_background(VkCommandBuffer cmd)
{
    //make a clear-color from frame number. This will flash with a 120 frame period.
    VkClearColorValue clearValue;
    float flash = abs(sin(_frameNumber / 120.f));
    clearValue = { { 0.0f, 0.0f, flash, 1.0f } };

    VkImageSubresourceRange clearRange = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);

    ComputeEffect& effect = backgroundEffects[currentBackgroundEffect];


    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = _drawExtent.width;
    viewport.height = _drawExtent.height;
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    VkRect2D scissor = {};
    scissor.offset.x = 0;
    scissor.offset.y = 0;
    scissor.extent.height = _drawExtent.height;
    scissor.extent.width = _drawExtent.width;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    // bind the gradient drawing compute pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

    // bind the descriptor set containing the draw image for the compute pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

    vkCmdPushConstants(cmd, _gradientPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

    // execute the compute pipeline dispatch. We are using 16x16 workgroup size so we need to divide by it
    vkCmdDispatch(cmd, std::ceil(_drawExtent.width / 16), std::ceil(_drawExtent.height / 16), 1);
}

void VulkanEngine::draw_geometry(VkCommandBuffer cmd) {
     //begin clock
    stats.drawcall_count = 0;
    stats.triangle_count = 0;
    stats.mesh_draw_time = 0;
    auto start = std::chrono::system_clock::now();
    //allocate a new uniform buffer for the scene data
    AllocatedBuffer gpuSceneDataBuffer = create_buffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    //add it to the deletion queue of this frame so it gets deleted once its been used
    get_current_frame().deletionQueue.push_function([=, this]() {
        destroy_buffer(gpuSceneDataBuffer);
    });

    //write the buffer
    GPUSceneData* sceneUniformData = (GPUSceneData*)gpuSceneDataBuffer.allocation->GetMappedData();
    *sceneUniformData = sceneData;

    //create a descriptor set that binds that buffer and update it
    VkDescriptorSet globalDescriptor = get_current_frame()._frameDescriptors.allocate(_components->device, _gpuSceneDataDescriptorLayout);

    DescriptorWriter writer;
    writer.write_buffer(0, gpuSceneDataBuffer.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.update_set(_components->device, globalDescriptor);

    // begin a render pass connected to our draw image
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_GENERAL);
    VkRenderingAttachmentInfo depthAttachment = vkinit::depth_attachment_info(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(_drawExtent, &colorAttachment, &depthAttachment);
    vkCmdBeginRendering(cmd, &renderInfo);
    
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);

    //bind a texture
    VkDescriptorSet imageSet = get_current_frame()._frameDescriptors.allocate(_components->device, _singleImageDescriptorLayout);
    {
        DescriptorWriter writer;
        writer.write_image(0, _errorCheckerboardImage.imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

        writer.update_set(_components->device, imageSet);
    }
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &imageSet, 0, nullptr);

    GPUDrawPushConstants push_constants;
    push_constants.worldMatrix = glm::mat4{ 1.f };

    glm::mat4 view = glm::translate(glm::vec3{ 0,0,-5 });
    // camera projection
    glm::mat4 projection = glm::perspective(glm::radians(70.f), (float)_drawExtent.width / (float)_drawExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    push_constants.worldMatrix = projection * view;

    push_constants.vertexBuffer = testMeshes[2]->meshBuffers.vertexBufferAddress;

    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &push_constants);
    vkCmdBindIndexBuffer(cmd, testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);

    scene->draw_scene(cmd, globalDescriptor, sceneData);
    
    auto end = std::chrono::system_clock::now();

    //convert to microseconds (integer), and then come back to miliseconds
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    stats.add_mesh_draw_time(elapsed.count() / 1000.f);
    stats.add_triangle_count(scene->sceneStats.triangle_count);
    stats.add_drawcall_count(scene->sceneStats.drawcall_count);
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
     // ####
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderInfo = vkinit::rendering_info(_swapchainController.swapchainExtent, &colorAttachment, nullptr);

    vkCmdBeginRendering(cmd, &renderInfo);

    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    sceneData.proj = glm::perspective(glm::radians(70.f), (float)_windowExtent.width / (float)_windowExtent.height, 10000.f, 0.1f);
    mainCamera.update();
    sceneData.view = mainCamera.getViewMatrix();
    scene->update_scene(sceneData);
    stats.scene_update_time = scene->sceneStats.scene_update_time;
    
    //wait until the gpu has finished rendering the last frame. Timeout of 1 second
    VK_CHECK(vkWaitForFences(_components->device, 1, &get_current_frame().renderFence, true, 1000000000));

    get_current_frame().deletionQueue.flush();
    get_current_frame()._frameDescriptors.clear_pools(_components->device);

    get_current_frame().deletionQueue.flush();
    VK_CHECK(vkResetFences(_components->device, 1, &get_current_frame().renderFence));

    uint32_t swapchainImageIndex;

    VkResult e = vkAcquireNextImageKHR(_components->device, _swapchainController.swapchain, 1000000000, get_current_frame().swapSemaphore, nullptr, &swapchainImageIndex);
    if (e == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;
        return;
    }
    //VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, get_current_frame().swapSemaphore, nullptr, &swapchainImageIndex));

    
    VkCommandBuffer cmd = get_current_frame().mainCommandBuffer;

    // now that we are sure that the commands finished executing, we can safely
    // reset the command buffer to begin recording again.
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //begin the command buffer recording. We will use this command buffer exactly once, so we want to let vulkan know that
    VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    _drawExtent.width = _drawImage.imageExtent.width;
    _drawExtent.height = _drawImage.imageExtent.height;

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    // transition our main draw image into general layout so we can write into it
    // we will overwrite it all so we dont care about what was the older layout
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    draw_background(cmd);

    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
       
    vkutil::transition_image(cmd, _depthImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    draw_geometry(cmd);

    //transition the draw image and the swapchain image into their correct transfer layouts
    vkutil::transition_image(cmd, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    vkutil::transition_image(cmd, _swapchainController.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // execute a copy from the draw image into the swapchain
    vkutil::copy_image_to_image(cmd, _drawImage.image, _swapchainController.swapchainImages[swapchainImageIndex], _drawExtent, _swapchainController.swapchainExtent);
    
    //Draw imgui
    // set swapchain image layout to Attachment Optimal so we can draw it
    vkutil::transition_image(cmd, _swapchainController.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //draw imgui into the swapchain image
    draw_imgui(cmd, _swapchainController.swapchainImageViews[swapchainImageIndex]);

    // set swapchain image layout to Present so we can show it on the screen TODO::::::
    vkutil::transition_image(cmd, _swapchainController.swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //finalize the command buffer (we can no longer add commands, but it can now be executed)
    VK_CHECK(vkEndCommandBuffer(cmd));

    // prepare the submission to the queue.
    // we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
    // we will signal the _renderSemaphore, to signal that rendering has finished

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);

    VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapSemaphore);
    VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().renderSemaphore);

    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

    // submit command buffer to the queue and execute it.
    // _renderFence will now block until the graphic commands finish execution
    VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, get_current_frame().renderFence));


    // prepare present
    // this will put the swapchain image we just rendered to into the visible window.
    // we want to wait on the _renderSemaphore for that,
    // as its necessary that drawing commands have finished before the image is displayed to the user
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .pSwapchains = &_swapchainController.swapchain,
    };
    presentInfo.swapchainCount = 1;
    presentInfo.pWaitSemaphores = &get_current_frame().renderSemaphore;
    presentInfo.waitSemaphoreCount = 1;

    presentInfo.pImageIndices = &swapchainImageIndex;


    // increase the number of frames drawn
    _frameNumber++;

    VkResult presentResult = vkQueuePresentKHR(_graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
        resize_requested = true;
    }

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
            _swapchainController.resize_swapchain(_window, renderScale);
            resize_requested = false;
        }

        // imgui new frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(_window);
        ImGui::NewFrame();
        //some imgui UI to test
        if (ImGui::Begin("background")) {
            ImGui::SliderFloat("Render Scale", &renderScale, 0.3f, 1.f);
            ComputeEffect& selected = backgroundEffects[currentBackgroundEffect];

            ImGui::Text("Selected Effect ", selected.name);
            ImGui::SliderInt("Effect index", &currentBackgroundEffect, 0, backgroundEffects.size() - 1);

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

        draw();



        
        //get clock again, compare with start clock
        auto end = std::chrono::system_clock::now();

        //convert to microseconds (integer), and then come back to miliseconds
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        stats.frametime = elapsed.count() / 1000.f;
    }
}

/*

void VulkanEngine::resize_swapchain() {
    vkDeviceWaitIdle(_components->device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _drawExtent.height = std::min(_swapchainExtent.height, _drawImage.imageExtent.height) * renderScale;
    _drawExtent.width = std::min(_swapchainExtent.width, _drawImage.imageExtent.width) * renderScale;

    create_swapchain(_windowExtent.width, _windowExtent.height);

    resize_requested = false;
}

*/

