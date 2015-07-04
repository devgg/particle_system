
#define EPSILON 0.000001f

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

typedef struct {
	float3 position_old;
	float3 position_new;
	float3 velocity;
	float radius;
} particle;

void correct_position_new(particle* p, float3 correction) {
	(*p).position_new += correction;
	(*p).velocity += correction;
}

particle init_particle(float4 position, float4 position_old) {
	return (particle) {position_old.xyz, position.xyz, position.xyz - position_old.xyz, position.w};
}

typedef struct triangle {
	float3 corners[3];
	float3 n;
	float hesse_distance;
	float3 edges[3];
} triangle;

triangle init_triangle(float3 v1, float3 v2, float3 v3) {
	float3 n = normalize(cross(v2 - v1, v3 - v1));
	return (triangle) {{v1, v2, v3}, n, dot(v1, n), {v2 - v1, v3 - v2, v1 - v3}};
}

bool vertex_triangle_intersection(float3 v, triangle t) {
	float b1, b2, b3;
	float3 v0 = t.corners[1] - t.corners[0];
	float3 v1 = t.corners[2] - t.corners[0];
	float3 v2 = v - t.corners[0];
	float d00 = dot(v0, v0);
	float d01 = dot(v0, v1);
	float d11 = dot(v1, v1);
	float d20 = dot(v2, v0);
	float d21 = dot(v2, v1);
	float denom = d00 * d11 - d01 * d01;
	b1 = (d11 * d20 - d01 * d21) / denom;
	b2 = (d00 * d21 - d01 * d20) / denom;
	b3 = 1.f - (b1 + b2);
	return b1 >= 0 && b2 >= 0 && b3 >= 0;
}


bool swept_sphere_triangle_intersection(particle p_world, triangle t_world, float3* n) {

	particle p = init_particle((float4) (p_world.position_new / p_world.radius, 1), (float4) (p_world.position_old / p_world.radius, 1));
	triangle t = init_triangle(t_world.corners[0] / p_world.radius, t_world.corners[1] / p_world.radius, t_world.corners[2] / p_world.radius);
	float distance_old = dot(t.n, p.position_old) - t.hesse_distance;
	float speed_in_direction = dot(t.n, p.velocity);
	if (speed_in_direction != 0) {
		float t0 = (1 - distance_old) / speed_in_direction;
		float t1 = -(1 + distance_old) / speed_in_direction;
		if ((t0 < 0 || t0 > 1) && (t1 < 0 || t1 > 1)) return false;
		if (vertex_triangle_intersection(p.position_old - t.n + t0 * p.velocity , t)) {
			*n = t.n;
			return true; 
		}
	} else if (fabs(distance_old) > 1) {
		return false;
	}

	// test against corners
	float3 intersection_position;
	float t_intersection = -1;
	for (int i = 0; i < 3; i++) {
		float3 particle_to_corner = t.corners[i] - p.position_old;

		float a = dot(p.velocity, p.velocity);
		float b = 2 * dot(p.velocity, (-particle_to_corner));
		float c = pow(length(particle_to_corner), 2)  - 1;

		float radicand = pow(b, 2) - 4 * a * c;
		if (radicand < 0) break;

		float root = sqrt(radicand);
		float t1 = (-b - root) / (2 * a);
		float t2 = (-b + root) / (2 * a);
			
		if (t1 > t2) {
			float temp = t1;
			t1 = t2;
			t2 = temp;
		}

		if (t1 <= -EPSILON || t1 >= 1 + EPSILON) {
			t1 = t2;
		}

		if (t1 <= -EPSILON || t1 >= 1 + EPSILON) {
			break;
		}

		if (t_intersection == -1 || t1 < t_intersection) {
			t_intersection = t1;
			intersection_position = t.corners[i];
			*n = normalize((p.position_old + t1 * p.velocity) - intersection_position);
		}
	}
	
	// test against edges
	float velocity_length_squared = pow(length(p.velocity), 2);
	for (int i = 0; i < 3; i++) {
		float3 particle_to_start = t.corners[i] - p.position_old;
		float edge_length_squared = pow(length(t.edges[i]), 2);
		float edge_dot_velocity = dot(t.edges[i], p.velocity);
		float edge_dot_particle_to_start = dot(t.edges[i], particle_to_start);

		float a = edge_length_squared * -velocity_length_squared + pow(edge_dot_velocity, 2);
		float b = edge_length_squared * 2 * dot(p.velocity, particle_to_start) - 2 * ((edge_dot_velocity) * (edge_dot_particle_to_start));
		float c = edge_length_squared * (1 - pow(length(particle_to_start), 2)) + pow(edge_dot_particle_to_start, 2);

		float radicand = pow(b, 2) - 4 * a * c;
		if (radicand < 0) break;

		float root = sqrt(radicand);
		float t1 = (-b - root) / (2 * a);
		float t2 = (-b + root) / (2 * a);
			
		if (t1 > t2) {
			float temp = t1;
			t1 = t2;
			t2 = temp;
		}

		if (t1 <= -EPSILON || t1 >= 1 + EPSILON) {
			t1 = t2;
			if (t1 <= -EPSILON || t1 >= 1 + EPSILON) {
				break;
			}
		}

		if (t_intersection == -1 || t1 < t_intersection) {
			float f = (edge_dot_velocity * t1 - edge_dot_particle_to_start) / edge_length_squared;
			if (f >= 0 && f <= 1) {
				t_intersection = t1;
				intersection_position = t.corners[i] + f * t.edges[i];
				*n = normalize((p.position_old + t1 * p.velocity) - intersection_position);
			}
		}
	}
	
	return t_intersection >= -EPSILON;
}


#define STACK_SIZE (uint) 256
kernel void resolve_collisions(global float4* positions_in, global float4* positions_out, global const float4* positions_old, global const float4* bvh, const uint bvh_size, global const float* world_triangles, const uint num_triangles) {
	uint GID = get_global_id(0);
	if (GID > bvh_size) return;

	particle p = init_particle(positions_in[GID], positions_old[GID]);

	float3 correction_particles = (float3) (0);
	float max_length = 0;
	
	uint branch_stack[STACK_SIZE] = {1, 2};
	uint branch_stack_counter = 2;
	uint leave_array[STACK_SIZE];
	uint leave_array_counter = 0;
	
	while (branch_stack_counter > 0 && leave_array_counter < STACK_SIZE) {
		branch_stack_counter--;
		float4 sphere = bvh[branch_stack[branch_stack_counter]];
		if(distance(p.position_new, sphere.xyz) < p.radius + sphere.w) {
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
			p.radius = 0.5;
		}
	}
	if (leave_array_counter == STACK_SIZE) {
		p.radius = 0.5;
	}
	
	for (int i = 0; i < leave_array_counter; i++) {
		float4 collision_particle = positions_in[leave_array[i]];
		float3 difference = p.position_new - collision_particle.xyz;
		if ((length(difference) + EPSILON < p.radius + collision_particle.w) && (leave_array[i] != GID)) {
			float3 correction = (p.radius + collision_particle.w - length(difference)) / 2.0f * normalize(difference);
			correction_particles += correction;
			max_length = max(max_length, length(correction));
		}
	}
	
	/*
	for (int i = 0; i <= bvh_size; i++) {
		float3 vector = p.position_new - positions_in[i].xyz;
		if(i != GID && length(vector) < 2 * p.radius) {
			float3 correction = (2 * p.radius - length(vector)) / 10.0f * normalize(vector);
			correction_particles += correction;
			max_length = max(max_length, length(correction));
		}
	}*/

	
	if (length(correction_particles) != 0) {
		//velocity = 0.5 * length(velocity) * normalize(correction_particles);

		float theta_correction = dot(correction_particles, -p.velocity);
		float3 velocity_correction = theta_correction * correction_particles;
		float3 velocity_correction_ortho = (1 - theta_correction) * (p.velocity - velocity_correction);
		//velocity = 0.5 * velocity_correction + 0.999 * velocity_correction_ortho;
	}
	correct_position_new(&p, length(correction_particles) > p.radius - EPSILON ? (float3) (p.radius - EPSILON) * normalize(correction_particles) : correction_particles);


	

	// world collisions
	for (int i = 0; i < 3 * num_triangles; i += 3) {
		float3 n;
		if (swept_sphere_triangle_intersection(p, init_triangle(vload3(i, world_triangles), vload3(i + 1, world_triangles), vload3(i + 2, world_triangles)), &n)) {
			correct_position_new(&p, -((dot(n, p.velocity) - EPSILON) * n)); // nicht riichtig! wie weit muss man wirklich von der oberfläche weg?! -> intersection point ...
		}
	}

	positions_out[GID] = (float4) (p.position_new, p.radius);
}

















		
		/*
		const float3 v1 = vload3(i, world_triangles);
		const float3 v2 = vload3(i + 1, world_triangles);
		const float3 v3 = vload3(i + 2, world_triangles);
		const float3 normal = normalize(cross(v2 - v1, v3 - v1));

		float3 x0 = p.position_old - p.radius * normal;
		float3 x1 = p.position_new - p.radius * normal; 

		//float3 x0 = position_old - (radius - EPSILON) * normal;
		///float3 x1 = position - (radius - EPSILON) * normal; 

		float3 n;
		if (line_triangle_intersection(x0, x1, v1, v2, v3,&n)) {
			correct_position_new(&p, -((dot(n, x1 - x0) - EPSILON) * n));
		} else if (line_sphere_intersection(v1, v2, p.position_new, p.radius, &n)
				|| line_sphere_intersection(v1, v3, p.position_new, p.radius, &n)
				|| line_sphere_intersection(v2, v3, p.position_new, p.radius, &n)) {
			correct_position_new(&p, distance(p.position_new, p.position_old) * n);
		} */