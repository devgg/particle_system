
#define EPSILON 0.001f

kernel void move(global float4* positions_old, global float4* positions, const uint num_particles, const float time_delta_old, const float time_delta) {
	uint GID = get_global_id(0);
	if (GID >= num_particles || time_delta_old == 0) return;
	float4 particle = positions[GID];

	float3 x0 = particle.xyz;
	float3 v0 = (x0 - positions_old[GID].xyz) / time_delta_old;
	float3 a = (float3) (0, -9.81f, 0);

	float3 x1 = x0 + time_delta * v0 + pow(time_delta, 2) * a / 2.f;

	x1.y = max(x1.y, 0.f);

	positions_old[GID] = (float4) (x0, particle.w);
	positions[GID] = (float4) (x1, particle.w);
}

bool line_triangle_intersection(float3 x0, float3 x1, float3 v1, float3 v2, float3 v3, float3* n) {

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
	*n = cross(e1, e2);
	*n = normalize(*n);
	return true;
}

bool line_sphere_intersection(float3 x0, float3 x1, float3 s, float radius, float3* n) {
	float3 line_direction = x1 - x0;
	float3 line_direction_normalized = normalize(line_direction);
	float3 sphere_to_origin = x0 - s;
	
	float u = dot(line_direction, -sphere_to_origin) / dot(line_direction, line_direction);
	if (u < 0 || u > 1) return false;

	float a = dot(line_direction_normalized, line_direction_normalized);
	float b = 2 * dot(line_direction_normalized, sphere_to_origin);
	float c = dot(sphere_to_origin, sphere_to_origin) - pow(radius, 2);


	float radicand = pow(b, 2) - 4 * a * c;
	if (radicand < 0) return false;

	if (radicand == 0) {
		*n = s - x0 + (-b / (2 * a)) * line_direction_normalized;
	} else {
		float root = sqrt(radicand);
		float u1 = (-b - root) / (2 * a);
		float u2 = (-b + root) / (2 * a);
		float3 p1 = x0 + u1 * line_direction_normalized;
		float3 p2 = x0 + u2 * line_direction_normalized;
		*n = 2 * s - (p1 + p2);
	}
	*n = normalize(*n);
	return true;
}


#define STACK_SIZE (uint) 256
kernel void resolve_collisions(global float4* positions_in, global float4* positions_out, global const float4* positions_old, global const float4* bvh, const uint bvh_size, global const float* world_triangles, const uint num_triangles) {
	uint GID = get_global_id(0);
	if (GID > bvh_size) return;

	float4 particle = positions_in[GID];
	float3 position = particle.xyz;
	float3 position_old = positions_old[GID].xyz;
	float3 velocity = position - position_old;
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
	position += length(correction_particles) > radius - EPSILON ? (float3) (radius - EPSILON) * normalize(correction_particles) : correction_particles;


	// world collisions
	for (int i = 0; i < 3 * num_triangles; i +=  3) {
		const float3 v1 = vload3(i, world_triangles);
		const float3 v2 = vload3(i + 1, world_triangles);
		const float3 v3 = vload3(i + 2, world_triangles);
		const float3 normal = normalize(cross(v2 - v1, v3 - v1));

		float3 x0 = position_old - (radius - EPSILON) * normal;
		float3 x1 = position - (radius - EPSILON) * normal; 

		float3 n;
		if (line_triangle_intersection(x0, x1, v1, v2, v3,&n)) {
			position -= dot(n, x1 - x0) * n;
		} else if (line_sphere_intersection(v1, v2, position, radius, &n)
				|| line_sphere_intersection(v1, v3, position, radius, &n)
				|| line_sphere_intersection(v2, v3, position, radius, &n)) {
			position += distance(position, position_old) * n;
		} 
	}

	// LINE SPHERE TO GET NORMAL IF 2 POINTS ADD THEM IF 1 IT IS NORMAL

	position += correction_world;
	positions_out[GID] = (float4) (position, radius);
}