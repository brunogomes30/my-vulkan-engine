#version 450

#extension GL_GOOGLE_include_directive : require

#include "include/scene_data.glsl"
#include "include/material_data.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec2 inUV;
layout (location = 2) in vec3 inPosition;
layout (location = 3) in vec3 vertNormal;

layout (location = 0) out vec4 outFragColor;

//texture to access
//layout(set =0, binding = 0) uniform sampler2D displayTexture;


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
	
	vec3 N = vertNormal;
	vec3 V = normalize(sceneData.cameraPos.xyz - inPosition);

	vec3 L, H;
	Light light;
	vec4 lightValue = vec4(0.0f);
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
	outFragColor = lightValue;


	

	//outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient ,1.0f);
}