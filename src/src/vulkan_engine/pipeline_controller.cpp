#include <vulkan_engine/pipeline_controller.h>
#include <vk_initializers.h>
#include "vk_pipelines.h"

void PipelineController::init(std::shared_ptr<EngineComponents> engineComponents, DescriptorController* descriptorController) {
    _engineComponents = engineComponents;
	_descriptorController = descriptorController;

}

void PipelineController::init_background_pipelines(std::vector<ComputeEffect>& backgroundEffects){
    VkPipelineLayoutCreateInfo computeLayout{};
    computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    computeLayout.pNext = nullptr;
    computeLayout.setLayoutCount = 1;
    computeLayout.pSetLayouts = &_descriptorController->drawImageDescriptorLayout;

    VkPushConstantRange pushConstant{};
    pushConstant.offset = 0;
    pushConstant.size = sizeof(ComputePushConstants);
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstant;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(_engineComponents->device, &computeLayout, nullptr, &gradientPipelineLayout));

    //Load shader

    VkShaderModule computeDrawShader;
    if (!vkutil::load_shader_module("../../shaders/gradient_color.comp.spv", _engineComponents->device, &computeDrawShader)) {
        fmt::print("Error when building the compute shader \n");
    }

    VkShaderModule skyShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(sky.comp.spv), _engineComponents->device, &skyShader)) {
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
    computePipelineInfo.layout = gradientPipelineLayout;
    computePipelineInfo.stage = computeStageInfo;

    ComputeEffect gradient;
    gradient.layout = gradientPipelineLayout;
    gradient.name = "gradient";
    gradient.data = {};

    //default colors
    gradient.data.data1 = glm::vec4(1, 0, 0, 1);
    gradient.data.data2 = glm::vec4(0, 0, 1, 1);


    VK_CHECK(vkCreateComputePipelines(_engineComponents->device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &gradient.pipeline));

    computePipelineInfo.stage.module = skyShader;

    ComputeEffect sky;
    sky.layout = gradientPipelineLayout;
    sky.name = "sky";
    sky.data = {};

    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(_engineComponents->device, VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &sky.pipeline));

    //Add 2 background effects into the array
    backgroundEffects.push_back(gradient);
    backgroundEffects.push_back(sky);

    vkDestroyShaderModule(_engineComponents->device, computeDrawShader, nullptr);
    vkDestroyShaderModule(_engineComponents->device, skyShader, nullptr);

    _engineComponents->mainDeletionQueue->push_function([&]() {
        vkDestroyPipelineLayout(_engineComponents->device, gradientPipelineLayout, nullptr);
        vkDestroyPipeline(_engineComponents->device, gradientPipeline, nullptr);
        vkDestroyPipeline(_engineComponents->device, sky.pipeline, nullptr);
    });
}

void PipelineController::init_mesh_pipeline(){
    VkShaderModule meshFragShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(tex_image.frag.spv), _engineComponents->device, &meshFragShader)) {
        fmt::print("Error when building the triangle mesh fragment shader module");
    }
    VkShaderModule meshVertexShader;
    if (!vkutil::load_shader_module(SHADERS_PATH(colored_triangle_mesh.vert.spv), _engineComponents->device, &meshVertexShader)) {
        fmt::print("Error when building the triangle mesh vertex shader module");
    }

    VkPushConstantRange bufferRange{};
    bufferRange.offset = 0;
    bufferRange.size = sizeof(GPUDrawPushConstants);
    bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
    pipeline_layout_info.pPushConstantRanges = &bufferRange;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pSetLayouts = &_descriptorController->singleImageDescriptorLayout;
    pipeline_layout_info.setLayoutCount = 1;


    VK_CHECK(vkCreatePipelineLayout(_engineComponents->device, &pipeline_layout_info, nullptr, &meshPipelineLayout));

    PipelineBuilder pipelineBuilder;

    //use the mesh layout we created
    pipelineBuilder._pipelineLayout = meshPipelineLayout;
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
    pipelineBuilder.set_color_attachment_format(_engineComponents->drawImage->imageFormat);
    pipelineBuilder.set_depth_format(_engineComponents->depthImage->imageFormat);

    //finally build the pipeline
    meshPipeline = pipelineBuilder.build_pipeline(_engineComponents->device);

    //clean structures
    vkDestroyShaderModule(_engineComponents->device, meshFragShader, nullptr);
    vkDestroyShaderModule(_engineComponents->device, meshVertexShader, nullptr);

    _engineComponents->mainDeletionQueue->push_function([&]() {
        vkDestroyPipelineLayout(_engineComponents->device, meshPipelineLayout, nullptr);
        vkDestroyPipeline(_engineComponents->device, meshPipeline, nullptr);
    });
}