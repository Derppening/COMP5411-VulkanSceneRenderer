#version 450 core

layout(triangles) in;
layout(line_strip, max_vertices = 6) out;

layout (set = 0, binding = 0) uniform UBOScene {
    mat4 projection;
    mat4 view;
    vec4 viewPos;
} ubo;
layout(push_constant) uniform PushConsts {
    mat4 model;
} primitive;

layout(location = 0) in vec3 inNormal[];

layout(location = 0) out vec3 outColor;

layout(constant_id = 0) const float NORMAL_LENGTH = 5.0f;

void main(void) {
    for (int i = 0; i < gl_in.length(); i++) {
        vec3 pos = gl_in[i].gl_Position.xyz;
        vec3 normal = normalize(inNormal[i]);

        gl_Position = ubo.projection * ubo.view * (primitive.model * vec4(pos, 1.0));
        outColor = vec3(1.0, 0.0, 0.0);
        EmitVertex();

        gl_Position = ubo.projection * ubo.view * (primitive.model * vec4(pos + normal * NORMAL_LENGTH, 1.0));
        outColor = vec3(0.0, 0.0, 1.0);
        EmitVertex();

        EndPrimitive();
    }
}