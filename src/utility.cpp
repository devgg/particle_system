#pragma once
#pragma OPENCL EXTENSION cl_khr_gl_sharing : enable

#include "utility.hpp"
#include <windows.h>
#include <CL/cl_gl.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <array>


namespace particle {
	std::string load_file(std::string file_name) {
		std::ifstream file(file_name);
		if (!file.is_open()) {
			std::cout << "could not open file " << file_name << std::endl;
			return nullptr;
		}
		std::stringstream buffer;
		buffer << file.rdbuf();
		return buffer.str();
	}


	std::vector<GLfloat> create_box(glm::vec3 dimensions, glm::vec3 position, glm::vec3 rotation_vector, float rotation_angle) {
		std::vector<GLfloat> box = {
			-dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, -dimensions.z,
			+dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, -dimensions.z,
			-dimensions.x, -dimensions.y, -dimensions.z,
			-dimensions.x, +dimensions.y, -dimensions.z,

			+dimensions.x, +dimensions.y, +dimensions.z,
			-dimensions.x, -dimensions.y, +dimensions.z,
			+dimensions.x, -dimensions.y, +dimensions.z,
			-dimensions.x, -dimensions.y, +dimensions.z,
			+dimensions.x, +dimensions.y, +dimensions.z,
			-dimensions.x, +dimensions.y, +dimensions.z,

			-dimensions.x, -dimensions.y, -dimensions.z,
			-dimensions.x, +dimensions.y, +dimensions.z,
			-dimensions.x, +dimensions.y, -dimensions.z,
			-dimensions.x, +dimensions.y, +dimensions.z,
			-dimensions.x, -dimensions.y, -dimensions.z,
			-dimensions.x, -dimensions.y, +dimensions.z,

			+dimensions.x, +dimensions.y, +dimensions.z,
			+dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, -dimensions.z,
			+dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, +dimensions.z,
			+dimensions.x, -dimensions.y, +dimensions.z,

			+dimensions.x, -dimensions.y, +dimensions.z,
			-dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, -dimensions.y, -dimensions.z,
			-dimensions.x, -dimensions.y, -dimensions.z,
			+dimensions.x, -dimensions.y, +dimensions.z,
			-dimensions.x, -dimensions.y, +dimensions.z,

			-dimensions.x, +dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, +dimensions.z,
			+dimensions.x, +dimensions.y, -dimensions.z,
			+dimensions.x, +dimensions.y, +dimensions.z,
			-dimensions.x, +dimensions.y, -dimensions.z,
			-dimensions.x, +dimensions.y, +dimensions.z
		};
		for (size_t i = 0; i < box.size(); i += 3) {
			glm::vec4 vertex(box[i], box[i + 1], box[i + 2], 1);
			vertex = glm::translate(glm::mat4(1), position) * glm::rotate(glm::mat4(1), rotation_angle, rotation_vector) * vertex;
			box[i] = vertex.x;
			box[i + 1] = vertex.y;
			box[i + 2] = vertex.z;
		}
		return box;
	}

	std::vector<GLfloat> create_sphere(float radius, float tesselation, glm::vec3 position) {
		std::vector<GLfloat> sphere = create_box({radius, radius, radius});
		for (size_t j = 0; j < tesselation; j++) {
			std::vector<GLfloat> new_sphere;
			for (size_t i = 0; i < sphere.size(); i += 9) {
				glm::vec3 v1(sphere[i], sphere[i + 1], sphere[i + 2]);
				glm::vec3 v2(sphere[i + 3], sphere[i + 4], sphere[i + 5]);
				glm::vec3 v3(sphere[i + 6], sphere[i + 7], sphere[i + 8]);

				auto push_back = [&new_sphere](glm::vec3 v) {
					new_sphere.push_back(v.x);
					new_sphere.push_back(v.y);
					new_sphere.push_back(v.z);
				};

				push_back(v3);
				push_back(v1);
				push_back((v1 + v2) / 2.f);

				push_back(v2);
				push_back(v3);
				push_back((v1 + v2) / 2.f);

			}
			sphere = new_sphere;
		}
		for (size_t i = 0; i < sphere.size(); i += 3) {
			glm::vec3 v(sphere[i], sphere[i + 1], sphere[i + 2]);
			v = radius * normalize(v);
			v = glm::vec3(glm::translate(glm::mat4(1), position) * glm::vec4(v, 1));
			sphere[i] = v.x;
			sphere[i + 1] = v.y;
			sphere[i + 2] = v.z;
		}
		return sphere;
	}

	namespace gl {
		GLuint compile_shader(std::string file_name, GLenum shader_type) {
			std::string string = load_file(file_name);
			const GLchar* source = string.c_str();
			const GLint source_size = string.size();
			GLuint shader = glCreateShader(shader_type);
			glShaderSource(shader, 1, &source, &source_size);

			glCompileShader(shader);

			GLint status;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

			if (status != GL_TRUE) {
				std::string error_log;
				error_log.resize(1000);
				GLsizei error_length;
				glGetShaderInfoLog(shader, error_log.size(), &error_length, const_cast<GLchar*>(error_log.data()));
				error_log.resize(error_length);
				std::cout << error_log;
			}

			return shader;
		}

		void print_error(GLenum error, std::string message) {
			if (message != "") message += " - ";
			if (error != GL_NO_ERROR) std::cout << message << glewGetErrorString(error) << std::endl;
		}
	}


	namespace cl {
		void print_platform_info(cl_platform_id platform_id) {
			std::array<cl_platform_info, 5> platform_infos = {CL_PLATFORM_NAME, CL_PLATFORM_VENDOR, CL_PLATFORM_VERSION, CL_PLATFORM_PROFILE, CL_PLATFORM_EXTENSIONS};
			std::string platform_info_text;
			platform_info_text.resize(1000);
			size_t returned_size;
			size_t used_size = 0;
			for (cl_platform_info platform_info : platform_infos) {
				clGetPlatformInfo(platform_id, platform_info, platform_info_text.size() - used_size, (void*)(platform_info_text.data() + used_size), &returned_size);
				used_size += returned_size;
				platform_info_text[used_size - 1] = '\n';
			}
			platform_info_text.resize(used_size);
			std::cout << platform_info_text << std::endl;
		}

		void print_device_info(cl_device_id device_id) {
			cl_device_type device_type;
			clGetDeviceInfo(device_id, CL_DEVICE_TYPE, sizeof(cl_device_type), &device_type, nullptr);
			std::string device_type_text;
			switch (device_type) {
				case CL_DEVICE_TYPE_CPU: device_type_text = "CL_DEVICE_TYPE_CPU"; break;
				case CL_DEVICE_TYPE_GPU: device_type_text = "CL_DEVICE_TYPE_GPU"; break;
				case CL_DEVICE_TYPE_ACCELERATOR: device_type_text = "CL_DEVICE_TYPE_ACCELERATOR"; break;
				case CL_DEVICE_TYPE_DEFAULT: device_type_text = "CL_DEVICE_TYPE_DEFAULT"; break;
			}
			std::cout << device_type_text << std::endl;
		}

		void init_opencl(cl_device_id* device, cl_context* context, cl_command_queue* command_queue) {
			std::array<cl_platform_id, 8> platforms;
			clGetPlatformIDs(platforms.size(), platforms.data(), nullptr);
			print_platform_info(platforms[0]);

			std::array<cl_device_id, 8> devices;
			cl_uint num_devices;
			clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_ALL, devices.size(), devices.data(), &num_devices);
			for (size_t i = 0; i < num_devices; i++) {
				print_device_info(devices[i]);
			}

			clGetDeviceIDs(platforms[0], CL_DEVICE_TYPE_GPU, devices.size(), devices.data(), &num_devices);
			if (num_devices == 0) return;
			*device = devices[0];

			cl_context_properties properties[] = {
				CL_GL_CONTEXT_KHR, reinterpret_cast<cl_context_properties>(wglGetCurrentContext()),
				CL_WGL_HDC_KHR, reinterpret_cast<cl_context_properties>(wglGetCurrentDC()),
				CL_CONTEXT_PLATFORM, reinterpret_cast<cl_context_properties>(platforms[0]),
				NULL
			};

			*context = clCreateContext(properties, 1, device,
				[](const char* errinfo, const void* private_info, size_t cb, void* user_data) {
				std::cout << "error: " << errinfo << std::endl;
			}, nullptr, nullptr);

			*command_queue = clCreateCommandQueue(*context, *device, NULL, nullptr);
		}

		void print_build_log(cl_device_id device, cl_program program) {
			cl_build_status buildStatus;
			clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &buildStatus, nullptr);

			//there were some errors.
			size_t logSize;
			clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, 0, nullptr, &logSize);
			std::string buildLog(logSize, ' ');

			clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_LOG, logSize, &buildLog[0], nullptr);
			buildLog[logSize] = '\0';

			if (buildStatus != CL_SUCCESS)
				std::cout << "There were build errors!" << std::endl;
			std::cout << "Build log:" << std::endl;
			std::cout << buildLog << std::endl;
		}


		void build_program(cl_device_id device, cl_context context, cl_program* program, std::string file_name) {
			std::string string = load_file(file_name);
			const char* source = string.c_str();

			cl_int errcode_ret;
			*program = clCreateProgramWithSource(context, 1, &source, nullptr, &errcode_ret);
			cl_int error = clBuildProgram(*program, 1, &device, nullptr, nullptr, nullptr);
			print_build_log(device, *program);
			print_error(error, "particle::cl::build_program");
		}

		size_t get_global_work_size(size_t data_count, size_t local_work_size) {
			size_t r = data_count % local_work_size;
			if (r == 0)
				return data_count;
			else
				return data_count + local_work_size - r;
		}

		void print_error(cl_int error, std::string message) {
			if (error == CL_SUCCESS) return;
			if (message != "") message += " - ";
			std::string error_string;
			switch (error) {
				// run-time and JIT compiler errors
				case 0: error_string = "CL_SUCCESS"; break;
				case -1: error_string = "CL_DEVICE_NOT_FOUND"; break;
				case -2: error_string = "CL_DEVICE_NOT_AVAILABLE"; break;
				case -3: error_string = "CL_COMPILER_NOT_AVAILABLE"; break;
				case -4: error_string = "CL_MEM_OBJECT_ALLOCATION_FAILURE"; break;
				case -5: error_string = "CL_OUT_OF_RESOURCES"; break;
				case -6: error_string = "CL_OUT_OF_HOST_MEMORY"; break;
				case -7: error_string = "CL_PROFILING_INFO_NOT_AVAILABLE"; break;
				case -8: error_string = "CL_MEM_COPY_OVERLAP"; break;
				case -9: error_string = "CL_IMAGE_FORMAT_MISMATCH"; break;
				case -10: error_string = "CL_IMAGE_FORMAT_NOT_SUPPORTED"; break;
				case -11: error_string = "CL_BUILD_PROGRAM_FAILURE"; break;
				case -12: error_string = "CL_MAP_FAILURE"; break;
				case -13: error_string = "CL_MISALIGNED_SUB_BUFFER_OFFSET"; break;
				case -14: error_string = "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST"; break;
				case -15: error_string = "CL_COMPILE_PROGRAM_FAILURE"; break;
				case -16: error_string = "CL_LINKER_NOT_AVAILABLE"; break;
				case -17: error_string = "CL_LINK_PROGRAM_FAILURE"; break;
				case -18: error_string = "CL_DEVICE_PARTITION_FAILED"; break;
				case -19: error_string = "CL_KERNEL_ARG_INFO_NOT_AVAILABLE"; break;

					// compile-time errors
				case -30: error_string = "CL_INVALID_VALUE"; break;
				case -31: error_string = "CL_INVALID_DEVICE_TYPE"; break;
				case -32: error_string = "CL_INVALID_PLATFORM"; break;
				case -33: error_string = "CL_INVALID_DEVICE"; break;
				case -34: error_string = "CL_INVALID_CONTEXT"; break;
				case -35: error_string = "CL_INVALID_QUEUE_PROPERTIES"; break;
				case -36: error_string = "CL_INVALID_COMMAND_QUEUE"; break;
				case -37: error_string = "CL_INVALID_HOST_PTR"; break;
				case -38: error_string = "CL_INVALID_MEM_OBJECT"; break;
				case -39: error_string = "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR"; break;
				case -40: error_string = "CL_INVALID_IMAGE_SIZE"; break;
				case -41: error_string = "CL_INVALID_SAMPLER"; break;
				case -42: error_string = "CL_INVALID_BINARY"; break;
				case -43: error_string = "CL_INVALID_BUILD_OPTIONS"; break;
				case -44: error_string = "CL_INVALID_PROGRAM"; break;
				case -45: error_string = "CL_INVALID_PROGRAM_EXECUTABLE"; break;
				case -46: error_string = "CL_INVALID_KERNEL_NAME"; break;
				case -47: error_string = "CL_INVALID_KERNEL_DEFINITION"; break;
				case -48: error_string = "CL_INVALID_KERNEL"; break;
				case -49: error_string = "CL_INVALID_ARG_INDEX"; break;
				case -50: error_string = "CL_INVALID_ARG_VALUE"; break;
				case -51: error_string = "CL_INVALID_ARG_SIZE"; break;
				case -52: error_string = "CL_INVALID_KERNEL_ARGS"; break;
				case -53: error_string = "CL_INVALID_WORK_DIMENSION"; break;
				case -54: error_string = "CL_INVALID_WORK_GROUP_SIZE"; break;
				case -55: error_string = "CL_INVALID_WORK_ITEM_SIZE"; break;
				case -56: error_string = "CL_INVALID_GLOBAL_OFFSET"; break;
				case -57: error_string = "CL_INVALID_EVENT_WAIT_LIST"; break;
				case -58: error_string = "CL_INVALID_EVENT"; break;
				case -59: error_string = "CL_INVALID_OPERATION"; break;
				case -60: error_string = "CL_INVALID_GL_OBJECT"; break;
				case -61: error_string = "CL_INVALID_BUFFER_SIZE"; break;
				case -62: error_string = "CL_INVALID_MIP_LEVEL"; break;
				case -63: error_string = "CL_INVALID_GLOBAL_WORK_SIZE"; break;
				case -64: error_string = "CL_INVALID_PROPERTY"; break;
				case -65: error_string = "CL_INVALID_IMAGE_DESCRIPTOR"; break;
				case -66: error_string = "CL_INVALID_COMPILER_OPTIONS"; break;
				case -67: error_string = "CL_INVALID_LINKER_OPTIONS"; break;
				case -68: error_string = "CL_INVALID_DEVICE_PARTITION_COUNT"; break;

					// extension errors
				case -1000: error_string = "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR"; break;
				case -1001: error_string = "CL_PLATFORM_NOT_FOUND_KHR"; break;
				case -1002: error_string = "CL_INVALID_D3D10_DEVICE_KHR"; break;
				case -1003: error_string = "CL_INVALID_D3D10_RESOURCE_KHR"; break;
				case -1004: error_string = "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR"; break;
				case -1005: error_string = "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR"; break;
				default: error_string = "Unknown OpenCL error"; break;
			}
			std::cout << message << error_string << std::endl;
		}
	}
}