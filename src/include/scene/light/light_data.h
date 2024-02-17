#pragma once

struct GPULightData {
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec4 color;
	alignas(4) enum LightType {
		POINT = 0,
		SPOT = 1,
		DIRECTIONAL = 2
	} type;
	alignas(4) float attenuation;


	//glm::vec3 direction; // only applicable to directional and spot lights
	alignas(4) float angle; // only applicable to spot lights
};