#version 450 core

layout(location = 0) in vec3 inPos;

layout(set = 0, binding = 0, std140) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 projection;
} mvp;

void main() {
    gl_Position = mvp.projection * mvp.view * mvp.model * vec4(inPos, 1.0);
}
