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


// GGX/Trowbridge-Reitz Normal distribuition function
float D(float roughness, vec3 N, vec3 H){
	float numerator = pow(roughness, 2.0);
	float NdotH = max(dot(N, H), 0.0);
	float denominator = PI * pow(pow(NdotH, 2.0) * (pow(roughness, 2.0) - 1.0) + 1.0, 2.0);
	denominator = max(denominator, 0.0001);
	return numerator / denominator;
}

// Schlick-GGX Geometry shadowing function
float G1(float roughness, vec3 N, vec3 X){
	float numerator = max(dot(N, X), 0.0);
	float k = roughness / 2.0;
	float denominator = max(dot(N, X), 0.0) * (1.0 - k) + k;
	denominator = max(denominator, 0.0001);
	return numerator / denominator;
}

// Smith Model
float G(float alpha, vec3 N, vec3 V, vec3 L){
	return G1(alpha, N, V) * G1(alpha, N, L);
}

// Fresnel-Schilck functionw
vec3 F(vec3 F0, vec3 V, vec3 H){
	return F0 + (1.0 - F0) * pow(1.0 - max(dot(V, H), 0.0), 5.0);
}

vec3 PBR(vec3 N, vec3 V, vec3 L, vec3 H, vec3 F0, vec3 lightColor){
	vec3 Ks = F(F0, V, H);
	vec3 Kd = vec3(1.0) - Ks;
	//float alpha  = materialData.metalRoughTex.y;
	float alpha = 0.5;
	vec3 lambert = materialData.colorFactors.rgb / PI;

	vec3 cookTorranceNumerator = D(alpha, N, H) * G(alpha, N, V, L) * F(F0, V, H);
	float cookTorranceDenominator = 4.0 * max(dot(V, N), 0.0) * max(dot(N, L), 0.0);
	cookTorranceDenominator = max(cookTorranceDenominator, 0.0001);

	vec3 cookTorrance = cookTorranceNumerator / cookTorranceDenominator;
	
	vec3 BRDF = Kd * lambert + cookTorrance;
	vec3 outgoingLight = materialData.emission.rgb + BRDF * lightColor * max(dot(L, N), 0.0);

	return outgoingLight;
}

void main() 
{
	//outFragColor = texture(displayTexture, inUV);
	
	vec3 N = normalize(inNormal);
	vec3 V = normalize(sceneData.cameraPos.xyz - inPosition);

	vec3 L, H;
	Light light;
	vec3 lightValue = vec3(0.0f);
	for(int i=0; i<sceneData.lightCount; i++)
	{
		light = sceneData.lights[i];

		//L = normalize(sceneData.lights[i].position - gl_FragCoord.xyz) ? normalize(sceneData.lights[i].position - gl_FragCoord.xyz ) : normalize(sceneData.lights[i].direction);
		L = normalize(sceneData.lights[i].position.xyz - inPosition);
		H = normalize(V + L);
		vec3 color = PBR(N, V, L, H, vec3(0.3f), light.color.rgb);
		lightValue += color;
	}
	outFragColor = vec4(lightValue, 1.0f);


	

	//outFragColor = vec4(color * lightValue *  sceneData.sunlightColor.w + ambient ,1.0f);
}