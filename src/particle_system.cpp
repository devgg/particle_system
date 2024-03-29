#pragma once

#include "particle_system.hpp"
#include "utility.hpp"
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.h>
#include <CL/cl_gl.h>

#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable
#pragma OPENCL EXTENSION cl_khr_gl_depth_images : enable

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define _USE_MATH_DEFINES
#include <math.h>
#include <array>
#include <iostream>


void particle_system::init() {
	if (!glfwInit())
		exit(EXIT_FAILURE);
	window = glfwCreateWindow(width, height, "Particle System", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		exit(EXIT_FAILURE);
	}

	glfwMakeContextCurrent(window);
	glewInit();
	glViewport(0, 0, width, height);

	auto key_callback = [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		particle_system* ps = static_cast<particle_system*>(glfwGetWindowUserPointer(window));
		if (key == GLFW_KEY_X && action != GLFW_REPEAT) {
			if (ps->sim) {
				ps->time -= glfwGetTime();
			} else {
				ps->time += glfwGetTime();
			}
			ps->sim = action != GLFW_RELEASE;
		}
	};

	auto mouse_button_callback = [](GLFWwindow* window, int button, int action, int mods) {
		particle_system* ps = static_cast<particle_system*>(glfwGetWindowUserPointer(window));
		ps->mouse_pressed = action == GLFW_PRESS;
		if (ps->mouse_pressed) {
			double mouse_x_new, mouse_y_new;
			glfwGetCursorPos(ps->window, &mouse_x_new, &mouse_y_new);
			ps->mouse_x = mouse_x_new;
			ps->mouse_y = mouse_y_new;
		}
	};

	auto cursor_position_callback = [](GLFWwindow* window, double x, double y) {
		particle_system* ps = static_cast<particle_system*>(glfwGetWindowUserPointer(window));
		if (ps->mouse_pressed) {
			glm::vec3 right = normalize(cross(ps->eye - ps->center, ps->up));
			glm::vec3 up = normalize(cross(right, ps->eye - ps->center));

			float x_delta = 0.001 * (x - ps->mouse_x);
			float y_delta = 0.001 * (y - ps->mouse_y);
			ps->eye = ps->center + length(ps->eye - ps->center) * normalize(normalize(ps->eye - ps->center) + x_delta * right + y_delta * up);

			ps->mouse_x = x;
			ps->mouse_y = y;
		}

	};

	auto scroll_callback = [](GLFWwindow* window, double x, double y) {
		particle_system* ps = static_cast<particle_system*>(glfwGetWindowUserPointer(window));
		glm::vec3 new_eye = ps->eye - static_cast<float>(0.5 * y)* normalize(ps->eye - ps->center);
		ps->eye = length(new_eye) > 1 ? new_eye : normalize(ps->eye);
	};

	glfwSetWindowUserPointer(window, this);
	glfwSetKeyCallback(window, key_callback);
	glfwSetMouseButtonCallback(window, mouse_button_callback);
	glfwSetCursorPosCallback(window, cursor_position_callback);
	glfwSetScrollCallback(window, scroll_callback);
	glEnable(GL_CULL_FACE);

	particle::cl::init_opencl(&device, &context, &command_queue);
	particle::gl::print_error(glGetError(), "particle_system::init");
	init_gl();
	init_cl();
}

void particle_system::init_gl() {

	glGenFramebuffers(1, &gl_framebuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer);

	GLuint gl_colorbuffer;
	glGenRenderbuffers(1, &gl_colorbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, gl_colorbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, gl_colorbuffer);

	glGenTextures(1, &gl_position_texture);
	glBindTexture(GL_TEXTURE_2D, gl_position_texture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, gl_position_texture, 0);

	GLuint gl_depthbuffer;
	glGenRenderbuffers(1, &gl_depthbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, gl_depthbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, width, height);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gl_depthbuffer);
	particle::gl::print_error_framebuffer(glCheckFramebufferStatus(GL_FRAMEBUFFER), "particle_system::init_gl");

	glGenTextures(1, &gl_light_texture);
	glBindTexture(GL_TEXTURE_3D, gl_light_texture);
	glTexImage3D(GL_TEXTURE_3D, 0, GL_RGBA32F, 32, 16, 256, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_REPEAT);


	particle::gl::print_error(glGetError(), "particle_system::init_gl");
	init_gl_particle();
	init_gl_world();
}

void particle_system::init_gl_particle() {
	GLuint vertex_shader = particle::gl::compile_shader("shaders/gl/particle.vert", GL_VERTEX_SHADER);
	GLuint fragment_shader = particle::gl::compile_shader("shaders/gl/particle.frag", GL_FRAGMENT_SHADER);
	gl_particle_program = glCreateProgram();
	glAttachShader(gl_particle_program, vertex_shader);
	glAttachShader(gl_particle_program, fragment_shader);
	glLinkProgram(gl_particle_program);
	glUseProgram(gl_particle_program);

	std::vector<GLfloat> particle_geometry = particle::create_sphere(1, 4);

	auto generate_random = []() {
		return static_cast<GLfloat>(rand()) / static_cast<GLfloat> (RAND_MAX);
	};
	std::vector<GLfloat> particle_colors;
	particle_colors.resize(3 * num_particles);
	for (size_t i = 0; i < 3 * num_particles; i += 3) {
		particle_colors[i] = generate_random();
		particle_colors[i + 1] = generate_random();
		particle_colors[i + 2] = generate_random();
	}


	glGenVertexArrays(2, gl_particle_vao);
	glGenBuffers(2, gl_positions);
	GLuint gl_particle_geometry;
	glGenBuffers(1, &gl_particle_geometry);
	glGenBuffers(2, gl_particle_colors);

	for (int i = 0; i < 2; i++) {
		glBindVertexArray(gl_particle_vao[i]);
		glBindBuffer(GL_ARRAY_BUFFER, gl_positions[i]);
		glBufferData(GL_ARRAY_BUFFER, num_particles * sizeof(cl_float4), h_particle_data.data(), GL_DYNAMIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
		glVertexAttribDivisorARB(0, 1);

		glBindBuffer(GL_ARRAY_BUFFER, gl_particle_geometry);
		glBufferData(GL_ARRAY_BUFFER, particle_geometry.size() * sizeof(GLfloat), particle_geometry.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

		glBindBuffer(GL_ARRAY_BUFFER, gl_particle_colors[i]);
		glBufferData(GL_ARRAY_BUFFER, particle_colors.size() * sizeof(GLfloat), particle_colors.data(), GL_STATIC_DRAW);
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
		glVertexAttribDivisorARB(2, 1);
	}

	particle::gl::print_error(glGetError(), "particle_system::init_gl_particle");
}

void particle_system::init_gl_world() {
	GLuint vertex_shader = particle::gl::compile_shader("shaders/gl/world.vert", GL_VERTEX_SHADER);
	GLuint fragment_shader = particle::gl::compile_shader("shaders/gl/world.frag", GL_FRAGMENT_SHADER);
	gl_world_program = glCreateProgram();
	glAttachShader(gl_world_program, vertex_shader);
	glAttachShader(gl_world_program, fragment_shader);
	glLinkProgram(gl_world_program);
	glUseProgram(gl_world_program);

	glGenVertexArrays(1, &gl_world_vao);
	glBindVertexArray(gl_world_vao);

	std::vector<std::vector<GLfloat>> world_positions;
	std::vector<std::vector<GLfloat>> world_normals;



//	world_positions.push_back(particle::create_sphere(2.f, 5, {8, 4, 0}));
//	world_normals.push_back(particle::create_sphere_normals(5));
//
//
	world_positions.push_back(particle::create_box({24, 0.2f, 24}, {4, 1, 0}, {0, 0, 1}, M_PI / 24.f));
	world_normals.push_back(particle::create_box_normals({0, 0, 1}, M_PI / 8.f));
	world_positions.push_back(particle::create_box({12, 0.2f, 24}, {-6, 8, 0}, {0, 0, 1}, -M_PI / 24.f));
	world_normals.push_back(particle::create_box_normals({0, 0, 1}, -M_PI / 8.f));
//
	float radius = 24;
	world_positions.push_back(particle::create_box({24, 1.f, 0.5f}, {0, 0, -radius}));
	world_normals.push_back(particle::create_box_normals());

	world_positions.push_back(particle::create_box({24, 1.f, 0.5f}, {0, 0, radius}));
	world_normals.push_back(particle::create_box_normals());

	world_positions.push_back(particle::create_box({0.5f, 1.f, 24}, {-radius, 0, 0}));
	world_normals.push_back(particle::create_box_normals());

	world_positions.push_back(particle::create_box({0.5f, 1.f, 24}, {radius, 0, 0}));
	world_normals.push_back(particle::create_box_normals());

	std::vector<GLfloat> h_world_normals;
	for (int i = 0; i < world_positions.size(); i++) {
		h_world_positions.insert(h_world_positions.end(), world_positions[i].begin(), world_positions[i].end());
		h_world_normals.insert(h_world_normals.end(), world_normals[i].begin(), world_normals[i].end());
	}
	
	num_triangles = h_world_positions.size() / 9;
	glGenBuffers(1, &gl_world_positions);
	glBindBuffer(GL_ARRAY_BUFFER, gl_world_positions);
	glBufferData(GL_ARRAY_BUFFER, h_world_positions.size() * sizeof(cl_float), h_world_positions.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	GLuint gl_world_normals;
	glGenBuffers(1, &gl_world_normals);
	glBindBuffer(GL_ARRAY_BUFFER, gl_world_normals);
	glBufferData(GL_ARRAY_BUFFER, h_world_normals.size() * sizeof(cl_float), h_world_normals.data(), GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

	particle::gl::print_error(glGetError(), "particle_system::init_gl_world");
}

void particle_system::init_cl() {	
	particle::cl::build_program(device, context, &cl_particle_simulation_program, "shaders/cl/particle_simulation.cl");
	particle::cl::build_program(device, context, &cl_bitonic_program, "shaders/cl/bitonic_sort.cl");
	particle::cl::build_program(device, context, &cl_bvh_program, "shaders/cl/bvh.cl");
	particle::cl::build_program(device, context, &cl_cull_program, "shaders/cl/cull_lights.cl");


	cl_int error = CL_SUCCESS;
	move_kernel = clCreateKernel(cl_particle_simulation_program, "move", nullptr);
	resolve_collisions_kernel = clCreateKernel(cl_particle_simulation_program, "resolve_collisions", nullptr);
	bitonic_sort_kernel = clCreateKernel(cl_bitonic_program, "bitonic_sort", nullptr);
	init_indices_kernel = clCreateKernel(cl_bitonic_program, "init_indices", nullptr);
	apply_indices_kernel = clCreateKernel(cl_bitonic_program, "apply_indices", nullptr);
	construct_bvh_kernel = clCreateKernel(cl_bvh_program, "construct_bvh", nullptr);
	cull_lights_kernel = clCreateKernel(cl_cull_program, "cull_lights", nullptr);
	calculate_aabb_kernel = clCreateKernel(cl_cull_program, "calculate_aabb", nullptr);
	
	for (int i = 0; i < 2; i++) {
		cl_particle_positions[i] = clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, gl_positions[i], nullptr);
		cl_particle_positions_old[i] = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_COPY_HOST_PTR, num_particles * sizeof(cl_float4), h_particle_data.data(), nullptr);
		cl_particle_colors[i] = clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, gl_particle_colors[i], nullptr);
	}
	cl_bvh = clCreateBuffer(context, CL_MEM_READ_WRITE | CL_MEM_ALLOC_HOST_PTR, num_bvh_branch_nodes * sizeof(cl_float4), nullptr, nullptr);
	cl_particle_indices = clCreateBuffer(context, CL_MEM_READ_WRITE, num_particles * sizeof(cl_uint), nullptr, nullptr);
	cl_world_positions = clCreateFromGLBuffer(context, CL_MEM_READ_WRITE, gl_world_positions, nullptr);
	cl_world_depths = clCreateFromGLTexture(context, CL_MEM_READ_ONLY, GL_TEXTURE_2D, 0, gl_position_texture, nullptr);
	cl_culled_lights = clCreateFromGLTexture(context, CL_MEM_READ_WRITE, GL_TEXTURE_3D, 0, gl_light_texture, nullptr);
	cl_aabbs = clCreateBuffer(context, CL_MEM_READ_WRITE, 32 * 16 * 2 * sizeof(cl_float3), nullptr, nullptr);


	cl_float time_delta_previous = 0;
	error |= clSetKernelArg(move_kernel, 2, sizeof(cl_uint), &num_particles);
	error |= clSetKernelArg(move_kernel, 3, sizeof(cl_float), &time_delta_previous);

	error |= clSetKernelArg(init_indices_kernel, 0, sizeof(cl_mem), &cl_particle_indices);
	error |= clSetKernelArg(init_indices_kernel, 1, sizeof(cl_mem), &num_particles);
	error |= clSetKernelArg(bitonic_sort_kernel, 1, sizeof(cl_mem), &cl_particle_indices);
	error |= clSetKernelArg(bitonic_sort_kernel, 2, sizeof(cl_uint), &num_particles);
	error |= clSetKernelArg(apply_indices_kernel, 0, sizeof(cl_mem), &cl_particle_indices);
	error |= clSetKernelArg(apply_indices_kernel, 7, sizeof(cl_mem), &num_particles);
	
	error |= clSetKernelArg(construct_bvh_kernel, 1, sizeof(cl_mem), &cl_bvh);

	error |= clSetKernelArg(resolve_collisions_kernel, 3, sizeof(cl_mem), &cl_bvh);
	error |= clSetKernelArg(resolve_collisions_kernel, 4, sizeof(cl_uint), &bvh_levels[1].second);
	error |= clSetKernelArg(resolve_collisions_kernel, 5, sizeof(cl_mem), &cl_world_positions);
	error |= clSetKernelArg(resolve_collisions_kernel, 6, sizeof(cl_uint), &num_triangles);

	error |= clSetKernelArg(cull_lights_kernel, 1, sizeof(cl_mem), &cl_bvh);
	error |= clSetKernelArg(cull_lights_kernel, 2, sizeof(cl_uint), &bvh_levels[1].second);
	error |= clSetKernelArg(cull_lights_kernel, 3, sizeof(cl_mem), &cl_aabbs);
	error |= clSetKernelArg(cull_lights_kernel, 4, sizeof(cl_mem), &cl_culled_lights);

	error |= clSetKernelArg(calculate_aabb_kernel, 0, sizeof(cl_mem), &cl_world_depths);
	error |= clSetKernelArg(calculate_aabb_kernel, 1, sizeof(cl_mem), &cl_aabbs);
	particle::cl::print_error(error, "particle_system::init_cl");
}

void particle_system::move_particles() {
	cl_int error = CL_SUCCESS;
	cl_float current_time = glfwGetTime();
	cl_float time_delta = current_time - time;
	time = current_time;
	error |= clSetKernelArg(move_kernel, 0, sizeof(cl_mem), &cl_particle_positions_old[0]);
	error |= clSetKernelArg(move_kernel, 1, sizeof(cl_mem), &cl_particle_positions[0]);
	error |= clSetKernelArg(move_kernel, 4, sizeof(cl_float), &time_delta);
	error |= clEnqueueNDRangeKernel(command_queue, move_kernel, 1, nullptr, &global_work_size, &local_work_size, NULL, nullptr, nullptr);
	error |= clSetKernelArg(move_kernel, 3, sizeof(cl_float), &time_delta);
	particle::cl::print_error(error, "particle_system::move_particles");
}

void particle_system::sort_particles() {
	cl_int error = CL_SUCCESS;
	error |= clEnqueueNDRangeKernel(command_queue, init_indices_kernel, 1, nullptr, &global_work_size, &local_work_size, NULL, nullptr, nullptr);
	
//	clEnqueueReadBuffer(command_queue, cl_particle_positions_old[0], true, 0, h_particle_data.size() * sizeof(cl_float), h_particle_data.data(), NULL, nullptr, nullptr);
//	for (int i = 0; i < h_particle_data.size() && i < 4 * 20; i += 4) {
//		std::cout << h_particle_data[i] << ' ' << h_particle_data[i + 1] << ' ' << h_particle_data[i + 2] << ' ' << h_particle_data[i + 3] << std::endl;
//	} std::cout << std::endl;

	{
		error |= clSetKernelArg(bitonic_sort_kernel, 0, sizeof(cl_mem), &cl_particle_positions[0]);
		int num_work_items = pow(2, ceil(log(num_particles) / log(2)));
		size_t global_work_size = particle::cl::get_global_work_size(num_work_items / 2, local_work_size);
		int iterations = ceil(log(num_particles) / log(2));
		int direction = 0;

		while (iterations > 1) {
			error |= clSetKernelArg(bitonic_sort_kernel, 5, sizeof(cl_uint), &direction);
			for (int i = 0; i < iterations; i++) {
				for (int j = 0; j <= i; j++) {
					cl_uint stride = pow(2, i - j);
					cl_uint merge = j == 0 ? 1 : 0;
					error |= clSetKernelArg(bitonic_sort_kernel, 3, sizeof(cl_uint), &stride);
					error |= clSetKernelArg(bitonic_sort_kernel, 4, sizeof(cl_uint), &merge);
					error |= clEnqueueNDRangeKernel(command_queue, bitonic_sort_kernel, 1, nullptr, &global_work_size, &local_work_size, NULL, nullptr, nullptr);

				}
			}
			iterations--;
			direction = (direction + 1) % 3;
		}
	}

	error |= clSetKernelArg(apply_indices_kernel, 1, sizeof(cl_mem), &cl_particle_positions[0]);
	error |= clSetKernelArg(apply_indices_kernel, 2, sizeof(cl_mem), &cl_particle_positions[1]);
	error |= clSetKernelArg(apply_indices_kernel, 3, sizeof(cl_mem), &cl_particle_positions_old[0]);
	error |= clSetKernelArg(apply_indices_kernel, 4, sizeof(cl_mem), &cl_particle_positions_old[1]);
	error |= clSetKernelArg(apply_indices_kernel, 5, sizeof(cl_mem), &cl_particle_colors[0]);
	error |= clSetKernelArg(apply_indices_kernel, 6, sizeof(cl_mem), &cl_particle_colors[1]);
	error |= clEnqueueNDRangeKernel(command_queue, apply_indices_kernel, 1, nullptr, &global_work_size, &local_work_size, NULL, nullptr, nullptr);
	std::swap(cl_particle_positions[0], cl_particle_positions[1]);
	std::swap(cl_particle_positions_old[0], cl_particle_positions_old[1]);
	std::swap(cl_particle_colors[0], cl_particle_colors[1]);

	std::swap(gl_particle_vao[0], gl_particle_vao[1]);
	particle::cl::print_error(error, "particle_system::sort_particles");

//	clEnqueueReadBuffer(command_queue, cl_particle_positions_old[0], true, 0, h_particle_data.size() * sizeof(cl_float), h_particle_data.data(), NULL, nullptr, nullptr);
//	for (int i = 0; i < h_particle_data.size() && i < 4 * 20; i += 4) {
//		std::cout << h_particle_data[i] << ' ' << h_particle_data[i + 1] << ' ' << h_particle_data[i + 2] << ' ' << h_particle_data[i + 3] << std::endl;
//	} std::cout << std::endl;
}

void particle_system::construct_bvh() {
	cl_int error = CL_SUCCESS;
	error |= clSetKernelArg(construct_bvh_kernel, 0, sizeof(cl_mem), &cl_particle_positions[0]);
	size_t gws = pow(2, ceil(log(num_particles) / log(2)) - 1);
	size_t lws = std::min(gws, local_work_size);
	for (size_t i = 0; i < bvh_levels.size() - 1; i++) {
		error |= clSetKernelArg(construct_bvh_kernel, 2, sizeof(cl_uint), &bvh_levels[i].first);
		error |= clSetKernelArg(construct_bvh_kernel, 3, sizeof(cl_uint), &bvh_levels[i].second);
		error |= clSetKernelArg(construct_bvh_kernel, 4, sizeof(cl_uint), &bvh_levels[i + 1].first);
		error |= clEnqueueNDRangeKernel(command_queue, construct_bvh_kernel, 1, nullptr, &gws, &lws, NULL, nullptr, nullptr);
		gws /= 2;
	}

//	std::vector<float> temp(4 * num_bvh_branch_nodes);	
//	error |= clEnqueueReadBuffer(command_queue, cl_bvh, true, 0, temp.size() * sizeof(cl_float), temp.data(), NULL, nullptr, nullptr);
//	for (int i = 0; i < 4 * num_bvh_branch_nodes && i < 4 * 20; i += 4) {
//		std::cout << temp[i] << ' ' << temp[i + 1] << ' ' << temp[i + 2] << ' ' << temp[i + 3] << std::endl;
//	} std::cout << std::endl;

	particle::cl::print_error(error, "particle_system::construct_bvh");
}

void particle_system::resolve_particle_collisions() {
	cl_int error = CL_SUCCESS;
	error |= clSetKernelArg(resolve_collisions_kernel, 2, sizeof(cl_mem), &cl_particle_positions_old[0]);
	for (int i = 0; i < 10; i++) {
		error |= clSetKernelArg(resolve_collisions_kernel, 0, sizeof(cl_mem), &cl_particle_positions[0]);
		error |= clSetKernelArg(resolve_collisions_kernel, 1, sizeof(cl_mem), &cl_particle_positions[1]);
		error |= clEnqueueNDRangeKernel(command_queue, resolve_collisions_kernel, 1, nullptr, &global_work_size, &local_work_size, NULL, nullptr, nullptr);
		std::swap(cl_particle_positions[0], cl_particle_positions[1]);
		std::swap(gl_positions[0], gl_positions[1]);
	}
	
	particle::cl::print_error(error, "particle_system::resolve_particle_collisions");
}

void particle_system::simulate() {
	glFinish();
	std::vector<cl_mem> cl_mem_objects = {cl_particle_positions[0], cl_particle_positions[1], cl_world_positions};
	cl_int error = clEnqueueAcquireGLObjects(command_queue, cl_mem_objects.size(), cl_mem_objects.data(), NULL, nullptr, nullptr);
	move_particles();
	sort_particles();
	construct_bvh();
	resolve_particle_collisions();
	error |= clEnqueueReleaseGLObjects(command_queue, cl_mem_objects.size(), cl_mem_objects.data(), NULL, nullptr, nullptr);
	clFinish(command_queue);
	particle::cl::print_error(error, "particle_system::simulate");
}

void particle_system::cull_lights() {
	glFinish();
	std::vector<cl_mem> cl_mem_objects = {cl_particle_positions[0], cl_world_depths, cl_culled_lights};
	cl_int error = clEnqueueAcquireGLObjects(command_queue, cl_mem_objects.size(), cl_mem_objects.data(), NULL, nullptr, nullptr);
	{
		size_t global_work_size[3] = {256 * 32 * 16, 1, 1};
		size_t local_work_size[3] = {256, 1, 1};
		error |= clEnqueueNDRangeKernel(command_queue, calculate_aabb_kernel, 1, nullptr, global_work_size, local_work_size, NULL, nullptr, nullptr);

		std::vector<cl_float> temp3(32 * 16 * 2 * 3);
		error |= clEnqueueReadBuffer(command_queue, cl_aabbs, CL_TRUE, 0, temp3.size() * sizeof(cl_float), temp3.data(), NULL, nullptr, nullptr);
		int a = 0;
	}

	{
		error |= clSetKernelArg(cull_lights_kernel, 0, sizeof(cl_mem), &cl_particle_positions[0]);
		size_t global_work_size[3] = {32, 16, 1};
		size_t local_work_size[3] = {32, 8, 1};
		error |= clEnqueueNDRangeKernel(command_queue, cull_lights_kernel, 3, nullptr, global_work_size, local_work_size, NULL, nullptr, nullptr);
	}

	std::vector<float> temp(16 * 16 * 8);	
//	error |= clEnqueueReadBuffer(command_queue, cl_world_depths, true, 0, temp.size() * sizeof(cl_float), temp.data(), NULL, nullptr, nullptr);
//	for (int i = 0; i < 16 * 16 * 8; i += 4) {
//		std::cout << temp[i] << ' ' << temp[i + 1] << ' ' << temp[i + 2] << ' ' << temp[i + 3] << std::endl;
//	} std::cout << std::endl;

	error |= clEnqueueReleaseGLObjects(command_queue, cl_mem_objects.size(), cl_mem_objects.data(), NULL, nullptr, nullptr);
	clFinish(command_queue);
	particle::cl::print_error(error, "particle_system::cull_lights");
}

void particle_system::prepass(const glm::mat4& projection, const glm::mat4& view) {
	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer);
	std::vector<GLenum> draw_buffers = {GL_NONE, GL_COLOR_ATTACHMENT1};
	glDrawBuffers(draw_buffers.size(), draw_buffers.data());

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	
	glUseProgram(gl_world_program);
	glBindVertexArray(gl_world_vao);
	glUniformMatrix4fv(0, 1, GL_FALSE, value_ptr(projection * view));
	glUniform3f(1, eye.x, eye.y, eye.z);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_3D, gl_light_texture);
	
	glDrawArrays(GL_TRIANGLES, 0, 3 * num_triangles);
	particle::gl::print_error(glGetError(), "particle_system::prepass");
}

void particle_system::render(const glm::mat4& projection, const glm::mat4& view) {
	glBindFramebuffer(GL_FRAMEBUFFER, gl_framebuffer);
	std::vector<GLenum> draw_buffers = {GL_COLOR_ATTACHMENT0, GL_NONE};
	glDrawBuffers(draw_buffers.size(), draw_buffers.data());

	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glClear(GL_COLOR_BUFFER_BIT);
	glDepthMask(GL_FALSE);

	glUseProgram(gl_world_program);
	glBindVertexArray(gl_world_vao);
	glUniformMatrix4fv(0, 1, GL_FALSE, value_ptr(projection * view));
	glUniform3f(1, eye.x, eye.y, eye.z);
	glDrawArrays(GL_TRIANGLES, 0, 3 * num_triangles);

	glDepthMask(GL_TRUE);

	glUseProgram(gl_particle_program);
	glBindVertexArray(gl_particle_vao[0]);
	glUniformMatrix4fv(3, 1, GL_FALSE, value_ptr(projection * view));
	glDrawArraysInstancedARB(GL_TRIANGLES, 0, pow(2, 4) * 36, num_particles);

	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

	glfwSwapBuffers(window);
	particle::gl::print_error(glGetError(), "particle_system::render");
}

void particle_system::enter_main_loop() {
	
	double last_time = glfwGetTime();
	unsigned int number_frames = 0;

	while (true) {
		glm::mat4 projection = glm::perspective(0.9272952f, static_cast<float>(width) / static_cast<float>(height), 0.001f, 1000.0f);
		glm::mat4 view = lookAt(eye, center, up);
		prepass(projection, view);
		cull_lights();
		render(projection, view);

		glfwPollEvents();
		if(sim) {
			simulate();
		}
		double current_time = glfwGetTime();
		if (current_time - last_time >= 1.0) {
			std::cout << number_frames << " fps" << std::endl;
			number_frames = 0;
			last_time = current_time;
		}
		number_frames++;
	}
}

particle_system::particle_system(size_t local_work_size, std::vector<cl_float> positions, std::vector<cl_float> radii):
	global_work_size(particle::cl::get_global_work_size(positions.size(), local_work_size)),
	local_work_size(local_work_size),
	num_particles(positions.size() / 3),
	num_bvh_branch_nodes(pow(2, ceil(log(num_particles) / log(2))) - 1),
	eye(15, 12, 0), center(-10, 0, 0), up(0, 1, 0) {
	for (size_t i = 0; i < radii.size(); i++) {
		h_particle_data.push_back(positions[i * 3]);
		h_particle_data.push_back(positions[i * 3 + 1]);
		h_particle_data.push_back(positions[i * 3 + 2]);
		h_particle_data.push_back(radii[i]);
	}
	bvh_levels.push_back({0, num_particles});
	cl_uint level_size = pow(2, ceil(log(num_particles) / log(2)) - 1);
	cl_uint num_nodes = ceil(num_particles / 2.f);
	cl_uint start_index = level_size - 1;
	while (level_size > 0) {
		bvh_levels.push_back({start_index, start_index + num_nodes});
		level_size = floor(level_size / 2.f);
		num_nodes = ceil(num_nodes / 2.f);
		start_index -= level_size;
		
	}
	init();
}

particle_system::~particle_system() {}