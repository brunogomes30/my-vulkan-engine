#include<loader/loaded_gltf.h>
#include<vk_engine.h>
void LoadedGLTF::Draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    // create renderables from the scenenodes
    for (auto& n : topNodes) {
        n->Draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    VkDevice dv = creator->_components->device;

    descriptorPool.destroy_pools(dv);
    creator->_bufferAllocator.destroy_buffer(materialDataBuffer);

    for (auto& [k, v] : meshes) {

        creator->_bufferAllocator.destroy_buffer(v->meshBuffers.indexBuffer);
        creator->_bufferAllocator.destroy_buffer(v->meshBuffers.vertexBuffer);
    }

    for (auto& [k, v] : images) {

        if (v.image == creator->_materialController.errorCheckerboardImage.image) {
            //dont destroy the default images
            continue;
        }
        creator->_textureController.destroy_image(v);
    }

    for (auto& sampler : samplers) {
        vkDestroySampler(dv, sampler, nullptr);
    }
}
