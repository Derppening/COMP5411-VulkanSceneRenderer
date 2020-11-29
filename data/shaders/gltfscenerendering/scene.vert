#version 450 core

layout (location = 0) in vec3 inPos;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec3 inColor;
layout (location = 4) in vec4 inTangent;

layout (set = 0, binding = 0) uniform UBOScene {
	mat4 projection;
	mat4 view;
	vec4 viewPos;
} uboScene;
layout (set = 2, binding = 0) uniform Settings {
	bool useBlinnPhong;
} settings;

layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

layout(constant_id = 2) const bool preTransformPos = true;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec3 outViewVec;
layout (location = 4) out vec4 outTangent;
layout (location = 5) out vec3 outFragPos;

void main() {
	outColor = inColor;
	outUV = inUV;
	outTangent = inTangent;

	if (preTransformPos) {
		vec4 pos = primitive.model * vec4(inPos, 1.0);

		gl_Position = uboScene.projection * uboScene.view * pos;
		outNormal = mat3(primitive.model) * inNormal;
		outFragPos = pos.xyz;
		outViewVec = uboScene.viewPos.xyz - outFragPos;
	} else {
		gl_Position = vec4(inPos, 1.0);
		outNormal = inNormal;
	}
}