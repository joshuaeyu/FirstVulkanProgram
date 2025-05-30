#version 450

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main() {
//    vec4 mixed = mix(vec4(fragColor, 1.0), texture(texSampler, 1.5 * fragTexCoord), 0.5);
//    outColor = mixed;
    
    outColor = texture(texSampler, fragTexCoord);
}
