#include <GL/glew.h>
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
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

	positions[3 * (num_particles - 4)] = -4.48f;
	positions[3 * (num_particles - 4) + 1] = 3;
	positions[3 * (num_particles - 4) + 2] = 1;

	positions[3 * (num_particles - 3)] = -5.51;
	positions[3 * (num_particles - 3) + 1] = 4;
	positions[3 * (num_particles - 3) + 2] = 6.01;

	positions[3 * (num_particles - 2)] = -4.5f;
	positions[3 * (num_particles - 2) + 1] = 4;
	positions[3 * (num_particles - 2) + 2] = 0;

	positions[3 * (num_particles - 1)] = -4.48f;
	positions[3 * (num_particles - 1) + 1] = 3;
	positions[3 * (num_particles - 1) + 2] = 0;

	particle_system(256, positions, radii).enter_main_loop();
	return 0;
}

