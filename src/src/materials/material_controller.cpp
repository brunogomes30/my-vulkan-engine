
#include<materials/material_controller.h>
#include<materials/materials.h>
void MaterialController::init(std::shared_ptr<EngineComponents> engineComponents, TextureController* textureController, BufferAllocator* bufferAllocator, DescriptorController* descriptorController)
{

	_engineComponents = engineComponents;
	_textureController = textureController;
	_bufferAllocator = bufferAllocator;
	_descriptorController = *descriptorController;

}

void MaterialController::init_default_data() {
	//3 default textures, white, grey, black. 1 pixel each
	uint32_t white = 0xFFFFFFFF;
	whiteImage = _textureController->create_image((void*)&white, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t grey = 0xAAAAAAFF;
	greyImage = _textureController->create_image((void*)&grey, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	uint32_t black = 0xFF000000;
	blackImage = _textureController->create_image((void*)&black, VkExtent3D{ 1, 1, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	//checkerboard image
	uint32_t magenta = 0xFFFF00FF;
	std::array<uint32_t, 16 * 16 > pixels; //for 16x16 checkerboard texture
	for (int x = 0; x < 16; x++) {
		for (int y = 0; y < 16; y++) {
			pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
		}
	}
	errorCheckerboardImage = _textureController->create_image(pixels.data(), VkExtent3D{ 16, 16, 1 }, VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_USAGE_SAMPLED_BIT);

	VkSamplerCreateInfo sampl = { .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };

	sampl.magFilter = VK_FILTER_NEAREST;
	sampl.minFilter = VK_FILTER_NEAREST;

	vkCreateSampler(_engineComponents->device, &sampl, nullptr, &defaultSamplerNearest);

	sampl.magFilter = VK_FILTER_LINEAR;
	sampl.minFilter = VK_FILTER_LINEAR;
	vkCreateSampler(_engineComponents->device, &sampl, nullptr, &defaultSamplerLinear);

	GLTFMetallic_Roughness::MaterialResources materialResources;
	//default the material textures
	materialResources.colorImage = whiteImage;
	materialResources.colorSampler = defaultSamplerLinear;
	materialResources.metalRoughImage = whiteImage;
	materialResources.metalRoughSampler = defaultSamplerLinear;
	materialResources.hasNormalMap = false;

	//set the uniform buffer for the material data
	AllocatedBuffer materialConstants = _bufferAllocator->create_buffer(sizeof(GLTFMetallic_Roughness::MaterialConstants), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

	//write the buffer
	GLTFMetallic_Roughness::MaterialConstants* sceneUniformData;
	vmaMapMemory(_engineComponents->allocator, materialConstants.allocation, (void**)&sceneUniformData);
	sceneUniformData->colorFactors = glm::vec4{ 1,1,1,1 };
	sceneUniformData->metal_rough_factors = glm::vec4{ 1,0.5,0,0 };
	vmaUnmapMemory(_engineComponents->allocator, materialConstants.allocation);

	_engineComponents->mainDeletionQueue->push_function([=]() {
		_textureController->destroy_image(whiteImage);
		_textureController->destroy_image(greyImage);
		_textureController->destroy_image(blackImage);
		_textureController->destroy_image(errorCheckerboardImage);
		vkDestroySampler(_engineComponents->device, defaultSamplerLinear, nullptr);
		vkDestroySampler(_engineComponents->device, defaultSamplerNearest, nullptr);
		_bufferAllocator->destroy_buffer(materialConstants);
	});

	materialResources.dataBuffer = materialConstants.buffer;
	materialResources.dataBufferOffset = 0;

	defaultData = metalRoughMaterial.write_material(_engineComponents->device, MaterialPass::MainColor, materialResources, _descriptorController.globalDescriptorAllocator);
	std::shared_ptr<GLTFMaterial> newMaterial = std::make_shared<GLTFMaterial>();
	newMaterial->data = defaultData;
}

void MaterialController::init_material_pipelines()
{
	metalRoughMaterial.build_pipelines(_engineComponents, &_descriptorController);
}
