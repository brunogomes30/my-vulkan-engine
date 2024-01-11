#include <vk_descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type) {
	VkDescriptorSetLayoutBinding newbind = {};
	newbind.binding = binding;
	newbind.descriptorType = type;
	newbind.descriptorCount = 1;

	bindings.push_back(newbind);
}

void DescriptorLayoutBuilder::clear() {
	bindings.clear();
}

VkDescriptorSetLayout  DescriptorLayoutBuilder::build(VkDevice device, VkShaderStageFlags shaderStages) {
	for (auto& b : bindings) {
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
	};	
	info.pNext = nullptr;

	info.pBindings = bindings.data();
	info.bindingCount = (uint32_t) bindings.size();
	info.flags = 0;

	VkDescriptorSetLayout layout;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &layout));

	return layout;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios) {
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (PoolSizeRatio ratio : poolRatios) {
		poolSizes.push_back({
			.type = ratio.type,
			.descriptorCount = (uint32_t)(maxSets * ratio.ratio),
		});
	}

	VkDescriptorPoolCreateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.pNext = nullptr,
		.flags = 0,
		.maxSets = maxSets,
		.poolSizeCount = (uint32_t) poolSizes.size(),
		.pPoolSizes = poolSizes.data(),
	};

	vkCreateDescriptorPool(device, &info, nullptr, &pool);
}

void DescriptorAllocator::clear_descriptors(VkDevice device) {
	vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device) {
	vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout) {
	VkDescriptorSetAllocateInfo info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.pNext = nullptr,
		.descriptorPool = pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &layout,
	};

	VkDescriptorSet set;
	VK_CHECK(vkAllocateDescriptorSets(device, &info, &set));

	return set;
}
