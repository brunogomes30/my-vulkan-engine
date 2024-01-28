#include <vulkan_engine/descriptor_controller.h>

void DescriptorController::init(std::shared_ptr<EngineComponents> engineComponents)
{
	_engineComponents = engineComponents;
}

void DescriptorController::init_descriptors()
{
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
    };
    globalDescriptorAllocator.init(_engineComponents->device, 10, sizes);

    // make the descriptor set layout for our compute draw
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        drawImageDescriptorLayout = builder.build(_engineComponents->device, VK_SHADER_STAGE_COMPUTE_BIT);
    }

    // allocate the descriptor set for our compute draw image
    drawImageDescriptors = globalDescriptorAllocator.allocate(_engineComponents->device, drawImageDescriptorLayout);

    DescriptorWriter writer;
    writer.write_image(0, _engineComponents->drawImage->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

    writer.update_set(_engineComponents->device, drawImageDescriptors);

    for (int i = 0; i < FRAME_OVERLAP; i++) {
        // create a descriptor pool
        std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
        };

        _engineComponents->frames[i]._frameDescriptors = DescriptorAllocatorGrowable{};
        _engineComponents->frames[i]._frameDescriptors.init(_engineComponents->device, 1000, frame_sizes);

        _engineComponents->mainDeletionQueue->push_function([&, i]() {
            _engineComponents->frames[i]._frameDescriptors.destroy_pools(_engineComponents->device);
        });
    }

    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        gpuSceneDataDescriptorLayout = builder.build(_engineComponents->device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }
    {
        DescriptorLayoutBuilder builder;
        builder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
        singleImageDescriptorLayout = builder.build(_engineComponents->device, VK_SHADER_STAGE_FRAGMENT_BIT);
    }
}
