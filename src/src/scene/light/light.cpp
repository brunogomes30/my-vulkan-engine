#include <scene/light/light.h>

Light Light::CreatePointLight(const glm::vec3& position, const glm::vec4& color, float attenuation) {
	Light light;
	light.lightData.position = position;
	light.lightData.color = color;
	light.lightData.type = GPULightData::POINT;
	light.lightData.attenuation = attenuation;
	return light;
}

Light Light::CreateDirectionalLight(const glm::vec3& direction, const glm::vec4& color) {
	Light light;
	light.lightData.position = direction;
	light.lightData.color = color;
	light.lightData.type = GPULightData::DIRECTIONAL;
	light.lightData.attenuation = 0.0f;
	return light;
}

Light Light::CreateSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec4& color, float attenuation, float angle) {
	Light light;
	light.lightData.position = position;
	light.lightData.color = color;
	light.lightData.type = GPULightData::SPOT;
	light.lightData.attenuation = attenuation;
	//light.lightData.direction = direction;
	light.lightData.angle = angle;
	return light;
}
