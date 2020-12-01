#version 450 core

layout (set = 1, binding = 0) uniform sampler2D samplerColorMap;
layout (set = 1, binding = 1) uniform sampler2D samplerNormalMap;
layout (set = 2, binding = 0, std140) uniform Settings {
	bool useBlinnPhong;
} settings;
layout(set = 3, binding = 0, std140) uniform LightSettings {
	float dirIntensity;
	float pointIntensity;
	float spotIntensity;
} lightSettings;
layout(set = 3, binding = 1, std140) uniform DirLight {
	vec3 direction;

	float ambient;
	float diffuse;
	float specular;
} dirLight;
layout(set = 3, binding = 2, std140) uniform PointLight {
	vec3 position;

	float constant;
	float linear;
	float quadratic;

	float ambient;
	float diffuse;
	float specular;
} pointLight;
layout(set = 3, binding = 3, std140) uniform SpotLight {
	vec3 position;
	vec3 direction;
	float cutOff;
	float outerCutOff;

	float constant;
	float linear;
	float quadratic;

	float ambient;
	float diffuse;
	float specular;
} spotLight;

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inViewVec;
layout (location = 4) in vec4 inTangent;
layout (location = 5) in vec3 inFragPos;

layout (location = 0) out vec4 outFragColor;

layout (constant_id = 0) const bool ALPHA_MASK = false;
layout (constant_id = 1) const float ALPHA_MASK_CUTOFF = 0.0f;

float calcSpec(vec3 N, vec3 L, vec3 R, vec3 V) {
	float spec = 0.0f;
	if (settings.useBlinnPhong) {
		vec3 H = normalize(L + V);
		spec = pow(max(dot(N, H), 0.0), 64.0);
	} else {
		spec = pow(max(dot(V, R), 0.0), 32.0);
	}
	return spec;
}

vec3 calcDirLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(-dirLight.direction);
	// diffuse
	float diff = max(dot(N, L), 0.0);
	// spec
	vec3 R = reflect(-L, N);
	float spec = calcSpec(N, L, R, V);
	// combine
	vec3 ambient = dirLight.ambient * color;
	vec3 diffuse = dirLight.diffuse * diff * color;
	vec3 specular = dirLight.specular * spec * color;
	return (ambient + diffuse + specular) * lightSettings.dirIntensity;
}

vec3 calcPointLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(pointLight.position - inFragPos);
	// diffuse shading
	float diff = max(dot(N, L), 0.0);
	// specular shading
	vec3 R = reflect(-L, N);
	float spec = calcSpec(N, L, R, V);
	// attenuation
	float distance = length(pointLight.position - inFragPos);
	float attenuation = 1.0 / (pointLight.constant + pointLight.linear * distance + pointLight.quadratic * (distance * distance));
	// combine results
	vec3 ambient = pointLight.ambient * color;
	vec3 diffuse = pointLight.diffuse * diff * color;
	vec3 specular = pointLight.specular * spec * color;
	ambient *= attenuation;
	diffuse *= attenuation;
	specular *= attenuation;
	return (ambient + diffuse + specular) * lightSettings.pointIntensity;
}

vec3 calcSpotLight(vec3 color, vec3 N, vec3 V) {
	vec3 L = normalize(spotLight.position - inFragPos);
	// diffuse shading
	float diff = max(dot(N, L), 0.0);
	// specular shading
	vec3 R = reflect(-L, N);
	float spec = pow(max(dot(V, R), 0.0), 32.0);
	// attenuation
	float distance = length(spotLight.position - inFragPos);
	float attenuation = 1.0 / (spotLight.constant + spotLight.linear * distance + spotLight.quadratic * (distance * distance));
	// spotlight intensity
	float theta = dot(L, normalize(-spotLight.direction));
	float epsilon = spotLight.cutOff - spotLight.outerCutOff;
	float intensity = clamp((theta - spotLight.outerCutOff) / epsilon, 0.0, 1.0);
	// combine results
	vec3 ambient = spotLight.ambient * color;
	vec3 diffuse = spotLight.diffuse * diff * color;
	vec3 specular = spotLight.specular * spec * color;
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

	vec3 V = normalize(inViewVec);

	vec3 outColor = calcDirLight(color.rgb, N, V);
	outColor += calcPointLight(color.rgb, N, V);
	outColor += calcSpotLight(color.rgb, N, V);

	outFragColor = vec4(outColor, color.a);
}