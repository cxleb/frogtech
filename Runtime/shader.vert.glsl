#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inTextureCoord;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec2 vTextureCoord;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec3 vPosition;

layout(binding = 0) uniform UniformBufferObject {
    mat4 projectionMatrix;
    mat4 viewMatrix;
} ubo;

layout(push_constant) uniform PushConstant{
    mat4 modelMatrix;
} pc;

void main() 
{
    mat4 worldMatrix = ubo.viewMatrix * pc.modelMatrix;
    gl_Position = ubo.projectionMatrix * worldMatrix * vec4(inPosition, 1.0);
    vTextureCoord = inTextureCoord;
    vNormal = normalize(worldMatrix * vec4(inNormal, 0.0)).xyz;
    vPosition = (worldMatrix * vec4(inPosition, 1.0)).xyz;
    //
    //gl_Position = vec4(inPosition, 1.0);
    //vTextureCoord = inTextureCoord;
    //vNormal = inNormal;
    //vPosition = inPosition;
}