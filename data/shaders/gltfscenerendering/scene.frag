#version 450

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 2, binding = 0) uniform Settings {
	int useBlinnPhong;

	float minAmbientIntensity;

	int treatAsPointLight;
	float diffuseIntensity;
	float specularIntensity;
	float pointLightLinear;
	float pointLightQuad;
} settings;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec3 inHalfwayVec;
layout (location = 6) in vec4 inTangent;

layout (location = 0) out vec4 outFragColor;

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;

void main() 
{
	vec4 color = texture(samplerColorMap, inUV) * vec4(inColor, 1.0);

	if (ALPHA_MASK) {
		if (color.a < ALPHA_MASK_CUTOFF) {
			discard;
		}
	}

	vec3 N = normalize(inNormal);
	vec3 T = normalize(inTangent.xyz);
	vec3 B = cross(inNormal, inTangent.xyz) * inTangent.w;
	mat3 TBN = mat3(T, B, N);
	N = TBN * normalize(texture(samplerNormalMap, inUV).xyz * 2.0 - vec3(1.0));

	vec3 L = normalize(inLightVec);
	vec3 V = normalize(inViewVec);
	vec3 R = reflect(-L, N);
	vec3 diffuse = max(dot(N, L) * settings.diffuseIntensity, settings.minAmbientIntensity).rrr;
	float specular = 0.0f;
	if (settings.useBlinnPhong != 0) {
		vec3 H = normalize(inHalfwayVec);
		specular = pow(max(dot(N, H), 0.0), 96.0);
	} else {
		specular = pow(max(dot(R, V), 0.0), 32.0);
	}
	outFragColor = vec4(diffuse * color.rgb + specular * settings.specularIntensity, color.a);
	if (settings.treatAsPointLight != 0) {
		float distance = length(inLightVec);
		float attenuation = 1.0 / (1.0 + settings.pointLightLinear * distance + settings.pointLightQuad * (distance * distance));
		outFragColor = outFragColor * attenuation;
	}
}