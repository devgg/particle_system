
#define WINDOW_WIDTH  2560
#define WINDOW_HEIGHT 1536

#define TILES_HORIZONTAL 32
#define TILES_VERTICAL 16

#define TILES_NUMBER (TILES_HORIZONTAL * TILES_VERTICAL)

#define TILE_WIDTH (WINDOW_WIDTH / TILES_HORIZONTAL)
#define TILE_HEIGHT (WINDOW_HEIGHT / TILES_VERTICAL)

#define LOCAL_WORK_SIZE 256

#define REDUCTIONS_PER_THREAD (TILE_WIDTH * TILE_HEIGHT / LOCAL_WORK_SIZE)

kernel void calculate_aabb(read_only image2d_t world_positions, global float* aabbs) {
	local float3 tile_reduction[2 * LOCAL_WORK_SIZE];

	uint GID = get_global_id(0);
	uint LID = get_local_id(0);
	
	uint tile_index = GID / LOCAL_WORK_SIZE;
	uint2 tile_base = (uint2) (TILE_WIDTH * (tile_index % TILES_HORIZONTAL), TILE_HEIGHT * (tile_index / TILES_HORIZONTAL)); 

	uint2 local_base = (uint2) ((LID * REDUCTIONS_PER_THREAD) % TILE_WIDTH, ((LID * REDUCTIONS_PER_THREAD) / TILE_WIDTH));

	float3 min_position = (float3) (NAN);
	float3 max_position = (float3) (NAN);
	for (int i = 0; i < REDUCTIONS_PER_THREAD; i++) {	// + LOCAL_WORK_SIZE to align memory access
		int2 current_index = (int2) (tile_base.x + ((local_base.x + i) % TILE_WIDTH), tile_base.y + local_base.y + ((local_base.x + i) / TILE_WIDTH));

		float3 world_position = read_imagef(world_positions, current_index).xyz;
		if (world_position.x != 0 || world_position.y != 0 || world_position.z != 0) {
			min_position = fmin(min_position, world_position);
			max_position = fmax(max_position, world_position);
		}
	}
	tile_reduction[LID] = min_position;
	tile_reduction[LID + LOCAL_WORK_SIZE] = max_position;
	barrier(CLK_LOCAL_MEM_FENCE);

	for (int i = 1; i < LOCAL_WORK_SIZE; i*=2) {
		if (LID % (2 * i) == 0) {
			tile_reduction[LID] = fmin(tile_reduction[LID], tile_reduction[LID + i]);
			tile_reduction[LID + LOCAL_WORK_SIZE] = fmax(tile_reduction[LID + LOCAL_WORK_SIZE], tile_reduction[LID + i + LOCAL_WORK_SIZE]);
		}
		barrier(CLK_LOCAL_MEM_FENCE);
	}
	if (LID == 0) {
		vstore3(tile_reduction[0], tile_index, aabbs);
		vstore3(tile_reduction[LOCAL_WORK_SIZE], tile_index + TILES_NUMBER, aabbs);
	}
}



float point_aabb_distance_squared(float3 p, float3 aabb_min, float3 aabb_max) {
	float distance_squared = 0;

	if (p.x < aabb_min.x) {
		distance_squared += pow(aabb_min.x - p.x, 2);
	} else if (p.x > aabb_max.x) {
		distance_squared += pow(p.x - aabb_max.x, 2);
	}

	if (p.y < aabb_min.y) {
		distance_squared += pow(aabb_min.y - p.y, 2);
	} else if (p.y > aabb_max.y) {
		distance_squared += pow(p.y - aabb_max.y, 2);
	}

	if (p.z < aabb_min.z) {
		distance_squared += pow(aabb_min.z - p.z, 2);
	} else if (p.z > aabb_max.z) {
		distance_squared += pow(p.z - aabb_max.z, 2);
	}
	
	return distance_squared;
}



#define STACK_SIZE (uint) 256
#define LIGHT_NUMBER_PER_TILE 256

kernel void cull_lights(global const float4* positions, global const float4* bvh, const uint bvh_size, global const float* aabbs, write_only image3d_t light_texture) {
	uint2 GID = (uint2) (get_global_id(0), get_global_id(1));
	
	float3 aabb_min = vload3(GID.x + GID.y * TILES_HORIZONTAL, aabbs);
	float3 aabb_max = vload3(GID.x + GID.y * TILES_HORIZONTAL + TILES_NUMBER, aabbs);
	if (isnan(aabb_min.x)) return;


	// go down bhv check for collisions with 2 * radius 
	// if collision go fruther down until no further collision
	// add last node or leave found



	// first try noch keine abstraction nur particle beleuchten!

	uint traversal_stack[STACK_SIZE] = {0};
//	uint level_stack[STACK_SIZE] = {bvh_size + 1};
	uint stack_counter = 1;

	uint light_stack[STACK_SIZE];
	uint light_stack_counter = 0;
	
	while (stack_counter > 0 && stack_counter < STACK_SIZE) {
		stack_counter--;
		float4 sphere = bvh[traversal_stack[stack_counter]];
		if(point_aabb_distance_squared(sphere.xyz, aabb_min, aabb_max) <= 9 +  pow(sphere.w, 2)) { // nicht  2 sondern current bhv level + pow(10 * 0.2f, 2))

			// if collision && hohe particle dichte dann light adden
			uint current_index = traversal_stack[stack_counter];
			if (2 * current_index + 1 < bvh_size) {
				traversal_stack[stack_counter] = 2 * current_index + 1;
				traversal_stack[min(stack_counter + 1, STACK_SIZE - 1)] = 2 * current_index + 2;
				stack_counter = min(stack_counter + 2, STACK_SIZE - 1);
			} else {
				current_index = 2 * (current_index - (bvh_size / 2));
				light_stack[light_stack_counter] = current_index;
				light_stack[light_stack_counter + 1] = current_index + 1;
				light_stack_counter += 2;
			}
		}
		if(stack_counter == STACK_SIZE - 1) {
			//p.radius = 0.5;
		}
	}


	uint light_index = 0;
	for (int i = 0; i < light_stack_counter && light_index < LIGHT_NUMBER_PER_TILE; i++) {
		float4 light = positions[light_stack[i]];
		if(point_aabb_distance_squared(light.xyz, aabb_min, aabb_max) < 9){//point_aabb_distance_squared(light.xyz, aabb_min, aabb_max) <= 10 * pow(light.w, 2)) { 
			write_imagef(light_texture, (int4) (GID.x, GID.y, light_index, 1), light);
			light_index++;
		}
	}


	if (light_index < LIGHT_NUMBER_PER_TILE) {
		write_imagef(light_texture, (int4) (GID.x, GID.y, light_index, 1), (float4) (NAN));
	}
}