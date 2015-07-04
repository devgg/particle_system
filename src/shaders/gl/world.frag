#version 420

in vertex_block {
	vec3 position;
	vec3 normal;
} vertex;

layout(location = 1) uniform vec3 camera_position;
layout(binding = 0) uniform sampler3D light_positions;

layout(location = 0) out vec4 color;
layout(location = 1) out vec4 position;

vec4 phong(in vec3 position, in vec3 color, in vec3 n) {
    vec3 light_position = texelFetch(light_positions, ivec3(0, 0, 0), 0).xyz;

    vec3 k_a = 0.5f * color;
    vec3 k_d = 0.7f * color;
    vec3 k_s = 1.0f * vec3(1.0, 1.0, 1.0);
    float e = 5.0;

    vec3 v = normalize(camera_position - position);
    vec3 l = normalize(light_position - position);
    vec3 r = 2 * n * dot(n, l) - l;
	
    vec3 ambient = k_a;
    vec3 diffus = k_d * max(dot(n, l), 0.0);
    vec3 specular = k_s * pow(max(dot(r, v), 0.0), e);
    return vec4(ambient + diffus + specular, 1.0);
}


void main() {
	vec3 vertex_normal = normalize(vertex.normal);
	color = phong(vertex.position, vertex.normal, vertex.normal);
	position = vec4(vertex.position, 0);
}