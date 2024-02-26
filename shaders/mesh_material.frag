#version 450

#extension GL_GOOGLE_include_directive : require

#include "include/scene_data.glsl"
#include "include/material_data.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inPosition;

layout (location = 0) out vec4 outFragColor;

//texture to access
//layout(set =0, binding = 0) uniform sampler2D displayTexture;

float map(float value, float min1, float max1, float min2, float max2) {
  return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

vec4 calculateDiffuse(vec3 normal, vec3 lightDirection, vec4 lightColor){
	float NdotL = max(dot(normal, lightDirection), 0.0f);
	vec4 diffuse = NdotL * lightColor;
	return diffuse;
} 

vec4 calculateSpecular(){
	return vec4(0.0f);
}

vec4 calculateAmbient(){
	return vec4(0.0f);
}

void main() 
{
	//outFragColor = texture(displayTexture, inUV);
	//vec4 normalMap = texture(materialData.normalMap, inUV);
	//vec3 N = normalize(normalMap.xyz);
	vec3 N = inNormal;
	
	vec3 V = normalize(sceneData.cameraPos.xyz - inPosition);

	vec3 L, H;
	Light light;
	vec4 lightValue = vec4(0.0f, 0.0f, 0.0f, 1.0);
	for(int i=0; i<sceneData.lightCount; i++)
	{
		light = sceneData.lights[i];
		L = normalize(sceneData.lights[i].position.xyz - inPosition);
		
		//diffuse
		lightValue += calculateDiffuse(N, L, light.color);
		//specular
		lightValue += calculateSpecular();
		//ambient
		lightValue += calculateAmbient();

	}
	vec3 normalMapped = normalize((inNormal + 1.0f)/2.0f);
	outFragColor = vec4(normalMapped, 1.0f);


	

	//outFragColor = vec4(color * lightValue + ambient ,1.0f);
}