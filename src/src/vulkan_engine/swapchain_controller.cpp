#include <vulkan_engine/swapchain_controller.h>

#include <SDL.h>
#include <SDL_vulkan.h>//Could be removed later, doesn't make much sense to have it here

#include <vk_initializers.h>
#include "VkBootstrap.h"
void SwapchainController::init_swapchain(std::shared_ptr<EngineComponents> engineComponents, VkExtent2D& windowExtent)
{
    _engineComponents = engineComponents;
    _windowExtent = windowExtent;
    create_swapchain(_windowExtent.width, _windowExtent.height);

    VkExtent3D drawImageExtent = {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    //hardcoding the draw format to 32 bit float
    engineComponents->drawImage->imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    engineComponents->drawImage->imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(engineComponents->drawImage->imageFormat, drawImageUsages, drawImageExtent);

    //for the draw image, we want to allocate it from gpu local memory
    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    //allocate and create the image
    vmaCreateImage(_engineComponents->allocator, &rimg_info, &rimg_allocinfo, &engineComponents->drawImage->image, &engineComponents->drawImage->allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo rview_info = vkinit::imageview_create_info(engineComponents->drawImage->imageFormat, engineComponents->drawImage->image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(_engineComponents->device, &rview_info, nullptr, &_engineComponents->drawImage->imageView));

    engineComponents->depthImage->imageFormat = VK_FORMAT_D32_SFLOAT;
    engineComponents->depthImage->imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(engineComponents->depthImage->imageFormat, depthImageUsages, drawImageExtent);

    //allocate and create the image
    vmaCreateImage(_engineComponents->allocator, &dimg_info, &rimg_allocinfo, &engineComponents->depthImage->image, &engineComponents->depthImage->allocation, nullptr);

    //build a image-view for the draw image to use for rendering
    VkImageViewCreateInfo dview_info = vkinit::imageview_create_info(engineComponents->depthImage->imageFormat, engineComponents->depthImage->image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(_engineComponents->device, &dview_info, nullptr, &_engineComponents->depthImage->imageView));

    //add to deletion queues
    _engineComponents->mainDeletionQueue->push_function([=]() {
        vkDestroyImageView(_engineComponents->device, engineComponents->drawImage->imageView, nullptr);
        vmaDestroyImage(_engineComponents->allocator, engineComponents->drawImage->image, engineComponents->drawImage->allocation);

        vkDestroyImageView(_engineComponents->device, engineComponents->depthImage->imageView, nullptr);
        vmaDestroyImage(_engineComponents->allocator, engineComponents->depthImage->image, engineComponents->depthImage->allocation);
    });
}

void SwapchainController::resize_swapchain(SDL_Window* window, float renderScale) {
    vkDeviceWaitIdle(_engineComponents->device);

    destroy_swapchain();

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    _engineComponents->drawExtent->height = std::min(swapchainExtent.height, _engineComponents->drawImage->imageExtent.height) * renderScale;
    _engineComponents->drawExtent->width = std::min(swapchainExtent.width, _engineComponents->drawImage->imageExtent.width) * renderScale;

    create_swapchain(_windowExtent.width, _windowExtent.height);
}

void SwapchainController::destroy_swapchain() {
    vkDestroySwapchainKHR(_engineComponents->device, swapchain, nullptr);

    for (auto imageView : swapchainImageViews) {
        vkDestroyImageView(_engineComponents->device, imageView, nullptr);
    }
}

void SwapchainController::create_swapchain(uint32_t width, uint32_t height) {
    vkb::SwapchainBuilder  swapchainBuilder{ _engineComponents->chosenGPU, _engineComponents->device, _engineComponents->surface };
    swapchainImageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain = swapchainBuilder
        .set_desired_format(VkSurfaceFormatKHR{ .format = swapchainImageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR })
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();
    swapchainExtent = vkbSwapchain.extent;

    swapchain = vkbSwapchain.swapchain;
    swapchainImages = vkbSwapchain.get_images().value();
    swapchainImageViews = vkbSwapchain.get_image_views().value();
}