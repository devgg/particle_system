#version 420

layout(location = 0) in vec4 particle_data;
layout(location = 1) in vec3 vertex_position;
layout(location = 2) in vec3 particle_color;
layout(location = 3) uniform mat4 PVM;

out vec3 particle_color_v;

void main() {
	particle_color_v = particle_color;
	gl_Position = PVM * vec4(particle_data.w * vertex_position + particle_data.xyz, 1.0);
}