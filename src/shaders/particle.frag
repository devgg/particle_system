#version 420

in vec3 particle_color_v;
out vec4 color;

void main() {
    color = vec4(particle_color_v, 1.0);
}