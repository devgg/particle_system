/*
#define WINDOW_WIDTH  2560
#define WINDOW_HEIGHT 1536

#define TILES_HORIZONTAL 16
#define TILES_VERTICAL 16

#define TILE_WIDTH (WINDOW_WIDTH / TILES_HORIZONTAL)
#define TILE_HEIGHT (WINDOW_HEIGHT / TILES_VERTICAL)

kernel void calculate_aabb(read_only image2d_t world_positions, global float* aabbs) {
	local float3 tile_reduction[2 * 512];

	uint GID = get_global_id(0);
	uint LID = get_local_id(0);
	
	uint tile_index = GID % 512;
	uint2 tile_base = (uint2) (TILE_WIDTH * (tile_index % TILES_HORIZONTAL), WINDOW_WIDTH * (tile_index / TILES_HORIZONTAL)); 

	uint2 local_base = (uint2) ((LID * 30) % TILE_WIDTH, (LID * 30) / TILE_WIDTH);

	uint2 global_base = (tile_base + local_base);

	float3 min_position = (float3) (9999999.f);
	float3 max_position = (float3) (-9999999.f);

	for (int i = 0; i < 30; i++) {	// + 512 to align memory access
		int2 current_index = (int2) ((global_base.x + i) % TILE_WIDTH, global_base.y + WINDOW_WIDTH * ((global_base.x + i) / TILE_WIDTH));

		float3 world_position = read_imagef(world_positions, current_index).xyz;
		if (world_position.x != 0 || world_position.y != 0 || world_position.z != 0) {
			min_position = fmin(min_position, world_position);
			max_position = fmax(max_position, world_position);
		}
	}
	tile_reduction[LID] = min_position;
	tile_reduction[LID + 512] = max_position;
	barrier(CLK_LOCAL_MEM_FENCE);

	for (int i = 1; i < 512; i*=2) {
		if (LID % (2 * i) == 0) {
			tile_reduction[LID] = fmin(tile_reduction[LID], tile_reduction[LID + i]);
			tile_reduction[LID + 512] = fmax(tile_reduction[LID + 512], tile_reduction[LID + i + 512]);
		}
		barrier(CLK_LOCAL_MEM_FENCE);
	}
	if (LID == 0) {
		vstore3(tile_reduction[0], tile_index, aabbs);
		vstore3(tile_reduction[512], tile_index + 256, aabbs);
	}
}*/

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

	float3 min_position = (float3) (9999999);
	float3 max_position = (float3) (-9999999);
	for (int i = 0; i < REDUCTIONS_PER_THREAD; i++) {	// + 512 to align memory access
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


/*
#define WINDOW_WIDTH  320
#define WINDOW_HEIGHT 192

#define TILES_HORIZONTAL 4
#define TILES_VERTICAL 2

#define TILES_NUMBER (TILES_HORIZONTAL * TILES_VERTICAL)

#define TILE_WIDTH (WINDOW_WIDTH / TILES_HORIZONTAL)
#define TILE_HEIGHT (WINDOW_HEIGHT / TILES_VERTICAL)

#define LOCAL_WORK_SIZE 256

#define REDUCTIONS_PER_THREAD (TILE_WIDTH * TILE_HEIGHT / LOCAL_WORK_SIZE)

kernel void calculate_aabb(global const float* world_positions, global float* aabbs) {
	local float tile_reduction[2 * LOCAL_WORK_SIZE];

	uint GID = get_global_id(0);
	uint LID = get_local_id(0);
	
	uint tile_index = GID / LOCAL_WORK_SIZE;
	uint2 tile_base = (uint2) (TILE_WIDTH * (tile_index % TILES_HORIZONTAL), WINDOW_WIDTH * TILE_HEIGHT * (tile_index / TILES_HORIZONTAL)); 

	uint2 local_base = (uint2) ((LID * REDUCTIONS_PER_THREAD) % TILE_WIDTH, WINDOW_WIDTH * ((LID * REDUCTIONS_PER_THREAD) / TILE_WIDTH));

	float min_position = 9999999.f;
	float max_position = -9999999.f;
	for (int i = 0; i < REDUCTIONS_PER_THREAD; i++) {	// + 512 to align memory access
		int2 current_index = (int2) (tile_base.x + ((local_base.x + i) % TILE_WIDTH), tile_base.y + local_base.y + WINDOW_WIDTH * ((local_base.x + i) / TILE_WIDTH));

		//current index y wro ng
		float world_position = world_positions[current_index.x + current_index.y];
		if (world_position != 0) {
			min_position = fmin(min_position, world_position);
			max_position = fmax(max_position, world_position);
		}
	}
	tile_reduction[LID] = min_position;
	tile_reduction[LID + LOCAL_WORK_SIZE] = max_position;
	//if (LID == 0) {
	//	tile_reduction[0] = 255;
	//	tile_reduction[0 + LOCAL_WORK_SIZE] = 255;
	//}
	barrier(CLK_LOCAL_MEM_FENCE);

	for (int i = 1; i < LOCAL_WORK_SIZE; i*=2) {
		if (LID % (2 * i) == 0) {
			tile_reduction[LID] = fmin(tile_reduction[LID], tile_reduction[LID + i]);
			tile_reduction[LID + LOCAL_WORK_SIZE] = fmax(tile_reduction[LID + LOCAL_WORK_SIZE], tile_reduction[LID + i + LOCAL_WORK_SIZE]);
		}
		barrier(CLK_LOCAL_MEM_FENCE);
	}
	if (LID == 0) {
		aabbs[tile_index] = tile_reduction[0];
		aabbs[tile_index + TILES_NUMBER] = tile_reduction[LOCAL_WORK_SIZE];
		//vstore3((float3) (tile_reduction[0], 0, 0), tile_index, aabbs);
		//vstore3((float3) (tile_reduction[256], 0, 0), tile_index + 128, aabbs);
	}
}*/


kernel void cull_lights(write_only image3d_t light_texture) {
	uint3 GID = (uint3) (get_global_id(0), get_global_id(1), get_global_id(2));
	write_imagef(light_texture, (int4) (GID.x, GID.y, GID.z, 0), (float4) (10, 0, 0, 1));
}