#pragma once
#include<scene/light/light_data.h>
class Light {

public:
	GPULightData lightData;

	Light() {
		lightData.position = glm::vec3(0.0f);
		lightData.color = glm::vec4(1.0f);
		lightData.type = GPULightData::POINT;
		lightData.attenuation = 0.0f;
		//lightData.direction = glm::vec3(0.0f);
		lightData.angle = 0.0f;
	}

	static Light CreatePointLight(const glm::vec3& position, const glm::vec4& color, float attenuation);

	static Light CreateDirectionalLight(const glm::vec3& direction, const glm::vec4& color);

	static Light CreateSpotLight(const glm::vec3& position, const glm::vec3& direction, const glm::vec4& color, float attenuation, float angle);
};