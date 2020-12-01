#version 450 core

layout (set = 0, binding = 0, std140) uniform UBOScene {
	mat4 projection;
	mat4 view;
	vec4 viewPos;
} ubo;
layout(push_constant) uniform PushConsts {
	mat4 model;
} primitive;

layout(constant_id = 4) const float tessAlpha = 1.0f;

layout(triangles, fractional_odd_spacing, cw) in;

layout(location = 0) in vec3 iNormal[];
layout(location = 3) in vec2 iTexCoord[];
layout(location = 6) in vec3 iColor[];
layout(location = 7) in vec3 iViewVec[];
layout (location = 8) in vec4 iTangent[];
layout (location = 9) in vec3 iFragPos[];

layout(location = 0) out vec3 oNormal;
layout (location = 1) out vec3 oColor;
layout(location = 2) out vec2 oTexCoord;
layout (location = 3) out vec3 oViewVec;
layout (location = 4) out vec4 oTangent;
layout (location = 5) out vec3 oFragPos;

void main(void)
{
	vec4 pos = (gl_TessCoord.x * gl_in[0].gl_Position) +
			   (gl_TessCoord.y * gl_in[1].gl_Position) +
			   (gl_TessCoord.z * gl_in[2].gl_Position);
	vec4 fragPos = primitive.model * pos;
	oFragPos = fragPos.xyz;
	gl_Position = ubo.projection * ubo.view * fragPos;

	oNormal = gl_TessCoord.x*iNormal[0] + gl_TessCoord.y*iNormal[1] + gl_TessCoord.z*iNormal[2];
	oNormal = mat3(primitive.model) * oNormal;
	oTexCoord = gl_TessCoord.x*iTexCoord[0] + gl_TessCoord.y*iTexCoord[1] + gl_TessCoord.z*iTexCoord[2];
	oColor = gl_TessCoord.x * iColor[0] + gl_TessCoord.y * iColor[1] + gl_TessCoord.z * iColor[2];
	oViewVec = ubo.viewPos.xyz - oFragPos;
	oTangent = gl_TessCoord.x * iTangent[0] + gl_TessCoord.y * iTangent[1] + gl_TessCoord.z * iTangent[2];
}