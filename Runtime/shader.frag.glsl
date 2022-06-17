#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec2 vTextureCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vPosition;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D albedoSampler;

void main() 
{
	vec3 normal = normalize(vNormal);
	vec4 color = vec4(0.3, 0.6, 1.0, 1.0);

	vec3 lightPosition = vec3(0, -1, 010.0f);
	vec3 cameraPosition = vec3(0, 0, -10.0f);
	
	vec3 toLightDir = normalize(lightPosition - vPosition);
	vec3 toCameraDir = normalize(cameraPosition - vPosition);


	float diffuse = max(dot(normal, toLightDir), 0.2);

	float specular = max(dot(normal, toLightDir), 0.0) > 0.0 ? pow(max(dot(toCameraDir, reflect(toLightDir, normal)), 0.0), 32) : 0.0;

	outColor = texture(albedoSampler, vTextureCoord) * (specular + diffuse);

	outColor.rgb = pow(outColor.rgb, vec3(1.0 / 2.2));

}