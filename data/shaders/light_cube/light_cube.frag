#version 450 core

layout(push_constant) uniform PushConsts {
    vec3 color;
} pushConsts;

layout(location = 0) out vec4 outFragColor;

void main() {
    outFragColor = vec4(pushConsts.color, 1.0f);
}
