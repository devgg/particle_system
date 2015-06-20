
#define EPSILON 0.001f

kernel void move(global float4* positions_previous, global float4* positions, const uint num_particles, const float time_delta_previous, const float time_delta) {
	uint GID = get_global_id(0);
	if (GID >= num_particles || time_delta_previous == 0) return;
	float4 particle = positions[GID];

	float3 x0 = particle.xyz;
	float3 v0 = (x0 - positions_previous[GID].xyz) / time_delta_previous;
	float3 a = (float3) (0, -9.81f, 0);

	float3 x1 = x0 + time_delta * v0 + pow(time_delta, 2) * a / 2.f;

	x1.y = max(x1.y, 0.f);

	positions_previous[GID] = (float4) (x0, particle.w);
	positions[GID] = (float4) (x1, particle.w);
}

bool line_triangle_intersection(float3 x0, float3 x1, float3 v1, float3 v2, float3 v3, float *isectT, float3 *isectN) {

	float3 dir = x1 - x0;
	float3 e1 = v2 - v1;
	float3 e2 = v3 - v1;

	float3 s1 = cross(dir, e2);
	float divisor = dot(s1, e1);
	if (divisor == 0.f)
		return false;
	float invDivisor = 1.f / divisor;

	// Compute first barycentric coordinate
	float3 d = x0 - v1;
	float b1 = dot(d, s1) * invDivisor;
	if (b1 < -EPSILON || b1 > 1.f + EPSILON)
		return false;

	// Compute second barycentric coordinate
	float3 s2 = cross(d, e1);
	float b2 = dot(dir, s2) * invDivisor;
	if (b2 < -EPSILON || b1 + b2 > 1.f + EPSILON)
		return false;

	// Compute _t_ to intersection point
	float t = dot(e2, s2) * invDivisor;
	if (t < -EPSILON || t > 1.f + EPSILON)
		return false;

	// Store the closest found intersection so far
	*isectT = t;
	*isectN = cross(e1, e2);
	*isectN = normalize(*isectN);
	return true;
}


#define STACK_SIZE (uint) 256
kernel void resolve_collisions(global float4* positions_in, global float4* positions_out, global float4* positions_previous, global const float4* bvh, const uint bvh_size, global const float* world_triangles, const uint num_triangles) {
	uint GID = get_global_id(0);
	if (GID > bvh_size) return;

	float4 particle = positions_in[GID];
	float3 position = particle.xyz;
	float3 position_previous = positions_previous[GID].xyz;
	float3 velocity = position - position_previous;
	float radius = particle.w;
	float diameter = 2.f * radius;

	float3 correction_particles = (float3) (0);
	float3 correction_world = (float3) (0);
	float max_length = 0;
	
	uint branch_stack[STACK_SIZE] = {1, 2};
	uint branch_stack_counter = 2;
	uint leave_array[STACK_SIZE];
	uint leave_array_counter = 0;
	
	while (branch_stack_counter > 0 && leave_array_counter < STACK_SIZE) {
		branch_stack_counter--;
		float4 sphere = bvh[branch_stack[branch_stack_counter]];
		if(distance(position, sphere.xyz) < radius + sphere.w) {
			uint current_index = branch_stack[branch_stack_counter];
			if (2 * current_index + 1 < bvh_size) {
				branch_stack[branch_stack_counter] = 2 * current_index + 1;
				branch_stack[min(branch_stack_counter + 1, STACK_SIZE - 1)] = 2 * current_index + 2;
				branch_stack_counter = min(branch_stack_counter + 2, STACK_SIZE - 1);
			} else {
				current_index = 2 * (current_index - (bvh_size / 2));
				leave_array[leave_array_counter] = current_index;
				leave_array[leave_array_counter + 1] = current_index + 1;
				leave_array_counter += 2;
			}
		}
		if(branch_stack_counter == STACK_SIZE - 1) {
			radius = 0.5;
		}
	}
	if (leave_array_counter == STACK_SIZE) {
		radius = 0.5;
	}
	
	for (int i = 0; i < leave_array_counter; i++) {
		float4 collision_particle = positions_in[leave_array[i]];
		float3 difference = position - collision_particle.xyz;
		if ((length(difference) + EPSILON < radius + collision_particle.w) && (leave_array[i] != GID)) {
			float3 correction = (radius + collision_particle.w - length(difference)) / 2.0f * normalize(difference);
			correction_particles += correction;
			max_length = max(max_length, length(correction));
		}
	}
	/*
	for (int i = 0; i <= bvh_size; i++) {
		float3 vector = position - positions_in[i].xyz;
		if(i != GID && length(vector) < diameter) {
			float3 correction = (diameter - length(vector)) / 10.0f * normalize(vector);
			correction_particles += correction;
			max_length = max(max_length, length(correction));
		}
	}*/

	
	if (length(correction_particles) != 0) {
		//velocity = 0.5 * length(velocity) * normalize(correction_particles);

		float theta_correction = dot(correction_particles, -velocity);
		float3 velocity_correction = theta_correction * correction_particles;
		float3 velocity_correction_ortho = (1 - theta_correction) * (velocity - velocity_correction);
		//velocity = 0.5 * velocity_correction + 0.999 * velocity_correction_ortho;
	}

	//correction_particles = max_length * normalize(correction_particles);

	if (length(correction_particles) > radius - EPSILON) correction_particles = (float3) (radius - EPSILON) * normalize(correction_particles);
	position += correction_particles;

	// world collisions
	

	
	
	for (int i = 0; i < 3 * num_triangles; i +=  3) {
		const float3 v1 = vload3(i, world_triangles);
		const float3 v2 = vload3(i + 1, world_triangles);
		const float3 v3 = vload3(i + 2, world_triangles);
		const float3 normal = normalize(cross(v2 - v1, v3 - v1));

		float3 x0 = position_previous - (radius - EPSILON) * normal;
		float3 x1 = position - (radius - EPSILON) * normal;                // PROBLEM BECAUSE CORRECTION IS TO FAR? NO ... JUST EPSILON?

		float t;
		float3 n;
		if (line_triangle_intersection(x0, x1, v1, v2, v3, &t, &n)) {
			correction_world -= (dot(n, x1 - x0) - EPSILON) * n;
		}
	}



	/*
	if ((position + correction_particles).y <= 0) {
		position.y = 0;
		float3 normal = (float3) (0, 1, 0);
		float theta = dot(normal, correction_particles);
		if (theta  < 0) {
			correction_particles -= theta * normal;
		}
	}*/

	position += correction_world;
	positions_out[GID] = (float4) (position, radius);
	//positions_previous[GID] = (float4) (position_previous, radius);
	
}	





/*
	uint local_level_sizes[10];
	uint level_base_indices[10];
	uint current_level_indices[10];
	level_base_indices[0] = 0;
	local_level_sizes[num_levels - 1] = level_sizes[num_levels - 1];
	for(int i = 0; i < num_levels - 1; i++) {
		current_level_indices[i] = 0;
		local_level_sizes[i] = level_sizes[i];
		level_base_indices[i + 1] = level_base_indices[i] + local_level_sizes[i];
	}*/
	/*
	for (uint i = 0; LID + i < level_base_indices[num_levels - 1] + local_level_sizes[num_levels - 1]; i += LS) {
		positions_local[LID + i] = positions_in[LID + i];
	}
	barrier(CLK_LOCAL_MEM_FENCE);*/
	/*

	uint current_level = num_levels - 1;
	current_level_indices[current_level] = level_base_indices[current_level];
	*/
	/*
	for (int i = 0; i < level_sizes[0]; i++) {
		float3 vector = position - positions_in[i].xyz;
		if(i != GID && length(vector) < diameter) {
			float3 correction = (diameter - length(vector)) / 10.0f * normalize(vector);
			correction_particles += correction;
			max_length = max(max_length, length(correction));
		}
	}*/



/*
	uint stack[256];
	uint stack_counter;
	for (stack_counter = 0; stack_counter < local_level_sizes[num_levels - 1]; stack_counter++) {
		stack[stack_counter] = level_base_indices[num_levels - 1] + stack_counter;
	}
	
	while(stack_counter > 0) {
		stack_counter--;
		float4 sphere = positions_in[stack[stack_counter]];
		float3 vector = position - sphere.xyz;
		if (length(vector) < (radius + sphere.w)) {
			uint current_level = num_levels - 1;
			while (stack[stack_counter] < level_base_indices[current_level]) {
				current_level--;
			}
			if (current_level > 0) {
				uint space_left = min(max(256 - stack_counter, (uint) 0), (uint) 4); // clamp
				uint lower_base_index = level_base_indices[current_level - 1] + 4 * (stack[stack_counter] - level_base_indices[current_level]);
				for (int i = 0; i < space_left; i++) {
					stack[stack_counter] = lower_base_index + i;
					stack_counter++;
				}
			} else if (stack[stack_counter] != GID) {
				float3 correction = (radius + sphere.w - length(vector)) / 2.0f * normalize(vector);
				correction_particles += correction;
				max_length = max(max_length, length(correction));
			}
		}
	}*/



/*
	while(current_level < num_levels) {
		uint i = 0;
		while (true) {
			float4 sphere = positions_local[current_level_indices[current_level]];
			float3 vector = position - sphere.xyz;
			
			if(i == 4 || current_level_indices[current_level] == level_base_indices[current_level] + local_level_sizes[current_level]) {
				current_level++;
				break;
			}
			
			if (length(vector) < (radius + sphere.w)) {
				if (current_level == 0) {
					if (current_level_indices[current_level] != GID) {
						float3 correction = (radius + sphere.w - length(vector)) / 2.0f * normalize(vector);
						correction_particles += correction;
						max_length = max(max_length, length(correction));
					}
				} else { 
					current_level_indices[current_level - 1] = level_base_indices[current_level - 1] + 4 * (current_level_indices[current_level] - level_base_indices[current_level]);
					current_level_indices[current_level]++;
					current_level--;
					break;
				}
			}
			current_level_indices[current_level]++;
			i++;
		}
	}
	*/