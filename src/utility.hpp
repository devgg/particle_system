#pragma once

#include <GL/glew.h>
#define CL_USE_DEPRECATED_OPENCL_2_0_APIS
#include <CL/cl.h>
#include <glm/gtc/matrix_transform.hpp>

#include <string>
#include <vector>


namespace particle {
	std::string load_file(std::string file_name);
	std::vector<GLfloat> create_box(glm::vec3 dimensions, glm::vec3 position = glm::vec3(0), glm::vec3 rotation_vector = glm::vec3(1), float rotation_angle = 0);
	std::vector<GLfloat> create_sphere(float radius, float tesselation, glm::vec3 position = glm::vec3(0));
	std::vector<GLfloat> create_box_normals(glm::vec3 rotation_vector = glm::vec3(1), float rotation_angle = 0);
	std::vector<GLfloat> create_sphere_normals(float tesselation);

	namespace gl {
		GLuint compile_shader(std::string file_name, GLenum shader_type);
		void print_error(GLenum error, std::string message = "");
		void print_error_framebuffer(GLenum error, std::string message = "");
	}

	namespace cl {
		void print_platform_info(cl_platform_id platform_id);
		void print_device_info(cl_device_id device_id);
		void init_opencl(cl_device_id* device, cl_context* context, cl_command_queue* command_queue);
		void print_build_log(cl_device_id device, cl_program program);
		void build_program(cl_device_id device, cl_context context, cl_program* program, std::string file_name);
		size_t get_global_work_size(size_t data_count, size_t local_work_size);
		void print_error(cl_int error, std::string message = "");
	}
}