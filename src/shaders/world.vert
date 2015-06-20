#version 420

layout(location = 0) in vec3 vertex_position;
layout(location = 1) uniform mat4 PVM;

void main() {
	gl_Position = PVM * vec4(vertex_position, 1.0);
}