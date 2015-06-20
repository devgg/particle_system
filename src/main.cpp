#include <GL/glew.h>
#include <CL/cl.h>
#include <vector>
#include <ctime>
#include "particle_system.hpp"

int main() {

	const cl_uint num_particles = 8 * 1024;

	srand(static_cast<unsigned>(time(nullptr)));
	auto generate_random = []() {
		return static_cast<float>(rand()) / static_cast<float> (RAND_MAX);
	};

	std::vector<cl_float> positions;
	positions.resize(3 * num_particles);
	std::vector<cl_float> radii;
	radii.resize(num_particles);

	float radius = num_particles / 1024.f /2.f;
	for (int i = 0; i < 3 * num_particles; i+=3) {
		positions[i] = 2 * radius * generate_random() - radius;
		positions[i + 1] = 5 * generate_random() + 10;
		positions[i + 2] = 2 * radius * generate_random() - radius;
		radii[i / 3] = 0.1f;
	}
	particle_system(256, positions, radii).enter_main_loop();
	return 0;
}

