#pragma once

#include <GL/glew.h>
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <vector>


class particle_system {
	GLFWwindow* window;
	const unsigned int width = 2560;
	const unsigned int height = 1536;
	bool mouse_pressed = false;
	bool sim = false;
	float mouse_x = 0;
	float mouse_y = 0; 
	glm::vec3 eye;
	glm::vec3 center;
	glm::vec3 up;


	//ogl
	GLuint gl_framebuffer;
	GLuint gl_position_texture;
	GLuint gl_light_texture;

	GLuint gl_particle_program;
	GLuint gl_particle_vao[2];
	GLuint gl_positions[2];
	GLuint gl_particle_colors[2];

	GLuint gl_world_program;
	GLuint gl_world_vao;
	GLuint gl_world_positions;



	//ocl
	cl_device_id device;
	cl_context context;
	cl_command_queue command_queue;

	cl_program cl_particle_simulation_program;
	cl_program cl_bitonic_program;
	cl_program cl_bvh_program;
	cl_program cl_cull_program;

	cl_kernel move_kernel;
	cl_kernel resolve_collisions_kernel;
	cl_kernel init_indices_kernel;
	cl_kernel apply_indices_kernel;
	cl_kernel bitonic_sort_kernel;
	cl_kernel construct_bvh_kernel;
	cl_kernel cull_lights_kernel;
	cl_kernel calculate_aabb_kernel;

	size_t global_work_size;
	size_t local_work_size;

	std::vector<cl_float> h_particle_data;
	std::vector<cl_uint> h_level_sizes;
	unsigned int num_particles;
	unsigned int num_bvh_branch_nodes;
	std::vector<std::pair<cl_uint, cl_uint>> bvh_levels;

	std::vector<cl_float> h_world_positions;
	unsigned int num_triangles;

	cl_mem cl_world_depths;
	cl_mem cl_culled_lights;
	cl_mem cl_aabbs;
	cl_mem cl_particle_positions[2];
	cl_mem cl_particle_positions_old[2];
	cl_mem cl_particle_colors[2];
	cl_mem cl_particle_indices;
	cl_mem cl_bvh;

	cl_mem cl_world_positions;
	cl_mem cl_level_sizes;

	cl_float time;

	void init();
	void init_gl();
	void init_gl_particle();
	void init_gl_world();
	void init_cl();

	void prepass(const glm::mat4& projection, const glm::mat4& view);
	void render(const glm::mat4& projection, const glm::mat4& view);
	void simulate();
	void cull_lights();

	void move_particles();
	void sort_particles();
	void construct_bvh();
	void resolve_particle_collisions();
	
public:
	particle_system(size_t local_work_size, std::vector<cl_float> positions, std::vector<cl_float> radii);
	~particle_system();
	void enter_main_loop();
};

