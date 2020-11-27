#version 450 core

layout(constant_id = 3) const float tessLevel = 3.0f;

layout(vertices = 3) out;

layout(location = 0) in vec3 inNormal[];
layout(location = 1) in vec3 inColor[];
layout(location = 2) in vec2 inUV[];
layout(location = 3) in vec3 inViewVec[];
layout (location = 4) in vec4 inTangent[];
layout (location = 5) in vec3 inFragPos[];

layout(location = 0) out vec3 outNormal[3];
layout(location = 3) out vec2 outUV[3];
layout(location = 6) out vec3 outColor[3];
layout(location = 7) out vec3 outViewVec[3];
layout (location = 8) out vec4 outTangent[3];
layout (location = 9) out vec3 outFragPos[3];

void main(void) {
    if (gl_InvocationID == 0) {
        gl_TessLevelInner[0] = 1.0;
        gl_TessLevelOuter[0] = 1.0;
        gl_TessLevelOuter[1] = 1.0;
        gl_TessLevelOuter[2] = 1.0;
    }

    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
    outNormal[gl_InvocationID] = inNormal[gl_InvocationID];
    outUV[gl_InvocationID] = inUV[gl_InvocationID];
    outColor[gl_InvocationID] = inColor[gl_InvocationID];
    outViewVec[gl_InvocationID] = inViewVec[gl_InvocationID];
    outTangent[gl_InvocationID] = inTangent[gl_InvocationID];
    outFragPos[gl_InvocationID] = inFragPos[gl_InvocationID];
}