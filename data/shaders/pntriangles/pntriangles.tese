#version 450 core

// PN patch data
struct PnPatch {
 float b210;
 float b120;
 float b021;
 float b012;
 float b102;
 float b201;
 float b111;
 float n110;
 float n011;
 float n101;
};

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
layout(location = 6) in PnPatch iPnPatch[];
layout(location = 16) in vec3 iColor[];
layout(location = 17) in vec4 iTangent[];

layout(location = 0) out vec3 oNormal;
layout(location = 1) out vec3 oColor;
layout(location = 2) out vec2 oTexCoord;
layout(location = 3) out vec3 oViewVec;
layout(location = 4) out vec4 oTangent;
layout(location = 5) out vec3 oFragPos;

#define uvw gl_TessCoord

void main() {
    vec3 uvwSquared = uvw * uvw;
    vec3 uvwCubed   = uvwSquared * uvw;

    // extract control points
    vec3 b210 = vec3(iPnPatch[0].b210, iPnPatch[1].b210, iPnPatch[2].b210);
    vec3 b120 = vec3(iPnPatch[0].b120, iPnPatch[1].b120, iPnPatch[2].b120);
    vec3 b021 = vec3(iPnPatch[0].b021, iPnPatch[1].b021, iPnPatch[2].b021);
    vec3 b012 = vec3(iPnPatch[0].b012, iPnPatch[1].b012, iPnPatch[2].b012);
    vec3 b102 = vec3(iPnPatch[0].b102, iPnPatch[1].b102, iPnPatch[2].b102);
    vec3 b201 = vec3(iPnPatch[0].b201, iPnPatch[1].b201, iPnPatch[2].b201);
    vec3 b111 = vec3(iPnPatch[0].b111, iPnPatch[1].b111, iPnPatch[2].b111);

    // extract control normals
    vec3 n110 = normalize(vec3(iPnPatch[0].n110, iPnPatch[1].n110, iPnPatch[2].n110));
    vec3 n011 = normalize(vec3(iPnPatch[0].n011, iPnPatch[1].n011, iPnPatch[2].n011));
    vec3 n101 = normalize(vec3(iPnPatch[0].n101, iPnPatch[1].n101, iPnPatch[2].n101));

    // compute texcoords
    oTexCoord  = gl_TessCoord[2] * iTexCoord[0] + gl_TessCoord[0] * iTexCoord[1] + gl_TessCoord[1] * iTexCoord[2];

    // normal
    // Barycentric normal
    vec3 barNormal = gl_TessCoord[2] * iNormal[0] + gl_TessCoord[0] * iNormal[1] + gl_TessCoord[1] * iNormal[2];
    vec3 pnNormal  = iNormal[0] * uvwSquared[2] + iNormal[1] * uvwSquared[0] + iNormal[2] * uvwSquared[1]
                   + n110 * uvw[2] * uvw[0] + n011 * uvw[0] * uvw[1]+ n101 * uvw[2] * uvw[1];
    oNormal = tessAlpha*pnNormal + (1.0-tessAlpha) * barNormal;
    oNormal = mat3(primitive.model) * oNormal;

    // compute interpolated pos
    vec3 barPos = gl_TessCoord[2] * gl_in[0].gl_Position.xyz
                + gl_TessCoord[0] * gl_in[1].gl_Position.xyz
                + gl_TessCoord[1] * gl_in[2].gl_Position.xyz;

    // save some computations
    uvwSquared *= 3.0;

    // compute PN position
    vec3 pnPos  = gl_in[0].gl_Position.xyz * uvwCubed[2]
                + gl_in[1].gl_Position.xyz * uvwCubed[0]
                + gl_in[2].gl_Position.xyz * uvwCubed[1]
                + b210*uvwSquared[2] * uvw[0]
                + b120*uvwSquared[0] * uvw[2]
                + b201*uvwSquared[2] * uvw[1]
                + b021*uvwSquared[0] * uvw[1]
                + b102*uvwSquared[1] * uvw[2]
                + b012*uvwSquared[1] * uvw[0]
                + b111*6.0*uvw[0] * uvw[1]*uvw[2];

    // final position and normal
    vec3 finalPos = (1.0 - tessAlpha) * barPos + tessAlpha * pnPos;
    vec4 fragPos = primitive.model * vec4(finalPos, 1.0);
    oFragPos = fragPos.xyz;
    gl_Position = ubo.projection * ubo.view * fragPos;
    oViewVec = ubo.viewPos.xyz - oFragPos;

    oColor = gl_TessCoord.x * iColor[0] + gl_TessCoord.y * iColor[1] + gl_TessCoord.z * iColor[2];
    oTangent = gl_TessCoord.x * iTangent[0] + gl_TessCoord.y * iTangent[1] + gl_TessCoord.z * iTangent[2];
}