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
layout(set = 3, binding = 0) uniform LightSettings {
	float dirIntensity;
	float pointIntensity;
	float spotIntensity;
} lightSettings;
layout(set = 3, binding = 1) uniform DirLight {
	vec4 direction;

	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
} dirLight;
layout(set = 3, binding = 2) uniform PointLight {
	vec4 position;

	float constant;
	float linear;
	float quadratic;

	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
} pointLight;
layout(set = 3, binding = 3) uniform SpotLight {
	vec4 position;
	vec4 direction;
	float cutOff;
	float outerCutOff;

	float constant;
	float linear;
	float quadratic;

	vec4 ambient;
	vec4 diffuse;
	vec4 specular;
} spotLight;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec3 inLightVec;
layout (location = 5) in vec3 inHalfwayVec;
layout (location = 6) in vec4 inTangent;
layout (location = 7) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;

vec3 calcDirLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(-dirLight.direction.xyz);
	// diffuse
	float diff = max(dot(N, L), 0.0);
	// spec
	vec3 R = reflect(-L, N);
	float spec = pow(max(dot(V, R), 0.0), 32.0);
	// combine
	vec3 ambient = dirLight.ambient.xyz * color;
	vec3 diffuse = dirLight.diffuse.xyz * diff * color;
	vec3 specular = dirLight.specular.xyz * spec * color;
	return (ambient + diffuse + specular) * lightSettings.dirIntensity;
}

vec3 calcPointLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(pointLight.position.xyz - inFragPos);
	// diffuse shading
	float diff = max(dot(N, L), 0.0);
	// specular shading
	vec3 R = reflect(-L, N);
	float spec = pow(max(dot(V, R), 0.0), 32.0);
	// attenuation
	float distance = length(pointLight.position.xyz - inFragPos);
	float attenuation = 1.0 / (pointLight.constant + pointLight.linear * distance + pointLight.quadratic * (distance * distance));
	// combine results
	vec3 ambient = pointLight.ambient.xyz * color;
	vec3 diffuse = pointLight.diffuse.xyz * diff * color;
	vec3 specular = pointLight.specular.xyz * spec * color;
	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;
	return (ambient + diffuse + specular) * lightSettings.pointIntensity;
}

vec3 calcSpotLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(spotLight.position.xyz - inFragPos);
	// diffuse shading
	float diff = max(dot(N, L), 0.0);
	// specular shading
	vec3 R = reflect(-L, N);
	float spec = pow(max(dot(V, R), 0.0), 32.0);
	// attenuation
	float distance = length(spotLight.position.xyz - inFragPos);
	float attenuation = 1.0 / (spotLight.constant + spotLight.linear * distance + spotLight.quadratic * (distance * distance));
	// spotlight intensity
	float theta = dot(L, normalize(-spotLight.direction.xyz));
	float epsilon = spotLight.cutOff - spotLight.outerCutOff;
	float intensity = clamp((theta - spotLight.outerCutOff) / epsilon, 0.0, 1.0);
	// combine results
	vec3 ambient = spotLight.ambient.xyz * color;
	vec3 diffuse = spotLight.diffuse.xyz * diff * color;
	vec3 specular = spotLight.specular.xyz * spec * color;
	ambient *= attenuation * intensity;
	diffuse *= attenuation * intensity;
	specular *= attenuation * intensity;
	return (ambient + diffuse + specular) * lightSettings.spotIntensity;
}

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

	vec3 outColor = calcDirLight(color.rgb, N, V);
	outColor += calcPointLight(color.rgb, N, V);
	outColor += calcSpotLight(color.rgb, N, V);

	outFragColor = vec4(outColor, color.a);

//	vec3 diffuse = max(dot(N, L) * settings.diffuseIntensity, settings.minAmbientIntensity).rrr;
//
//	float specular = 0.0f;
//	if (settings.useBlinnPhong != 0) {
//		vec3 H = normalize(inHalfwayVec);
//		specular = pow(max(dot(N, H), 0.0), 96.0);
//	} else {
//		specular = pow(max(dot(R, V), 0.0), 32.0);
//	}
//
//	outFragColor = vec4(diffuse * color.rgb + specular * settings.specularIntensity, color.a);
//
//	if (settings.treatAsPointLight != 0) {
//		float distance = length(inLightVec);
//		float attenuation = 1.0 / (1.0 + settings.pointLightLinear * distance + settings.pointLightQuad * (distance * distance));
//		outFragColor = outFragColor * attenuation;
//	}
}