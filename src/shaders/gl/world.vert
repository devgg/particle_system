#version 420

layout(location = 0) in vec3 vertex_position;
layout(location = 1) in vec3 vertex_normal;
layout(location = 0) uniform mat4 PVM;


out vertex_block {
	vec3 position;
	vec3 normal;
} vertex;

void main() {
	gl_Position = PVM * vec4(vertex_position, 1.0);
	vertex.position = vertex_position;
	vertex.normal = vertex_normal;
}