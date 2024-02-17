


struct Light{
	vec4 position;
	vec4 color;
	int type; // POINT = 0; SPOT = 1; DIRECTIONAL = 2;
	float attenuation;
	float spotlightCutoff;
};

layout(set = 0, binding = 0) uniform  SceneData{   
	mat4 view;
	mat4 inverseView;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
	vec4 cameraPos;
	int lightCount;
	Light lights[8];
} sceneData;