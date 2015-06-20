
kernel void init_indices(global uint* indices, uint num_particles) {
	uint GID = get_global_id(0);
	if (GID >= num_particles) return;
	indices[GID] = GID;
}

kernel void apply_indices(global const uint* indices, global const float4* positions_in, global float4* positions_out, global const float4* positions_old_in, global float4* positions_old_out, global const float* colors_in, global float* colors_out, uint num_particles) {
	uint GID = get_global_id(0);
	if (GID >= num_particles) return;

	positions_out[GID] = positions_in[indices[GID]];
	positions_old_out[GID] = positions_old_in[indices[GID]];
	vstore3(vload3(indices[GID], colors_in), GID, colors_out);
}


kernel void bitonic_sort(global const float4* positions, global uint* indices, const uint stride, const uint num_particles, const uint merge, const uint direction) {
	uint GID = get_global_id(0);
	
	uint2 ids;
	if (merge == 1) {
		ids.x = GID % stride + 2 * stride * (GID / stride);
		ids.y = 2 * stride * (GID / stride) + (2 * stride - ((ids.x % stride) + 1));
	} else  {
		ids.x = GID % stride + 2 * stride * (GID / stride);
		ids.y = ids.x + stride;
	}

	if (ids.y >= num_particles) return;
	uint2 local_indices = (uint2) (indices[ids.x], indices[ids.y]);

	float p1;
	float p2;
	switch(direction) {
		case 0: p1 = positions[local_indices.x].x;  p2 = positions[local_indices.y].x; break;
		case 1: p1 = positions[local_indices.x].y;  p2 = positions[local_indices.y].y; break;
		case 2: p1 = positions[local_indices.x].z;  p2 = positions[local_indices.y].z; break;
	}

	if (p1 > p2) {
		indices[ids.x] = local_indices.y;
		indices[ids.y] = local_indices.x;
	}
}

