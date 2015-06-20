

kernel void construct_bvh(global float4* positions, global float4* bvh, const uint start_index, const uint end_index, const uint higher_start_index) {
	uint GID = get_global_id(0);
	
	float4 spheres[2];
	float4 bounding_sphere;
	if (start_index == 0 && end_index != 1) {
		spheres[0] = positions[2 * GID];
		spheres[1] = positions[2 * GID + 1];
	} else {
		spheres[0] = bvh[start_index + 2 * GID];
		spheres[1] = bvh[start_index + 2 * GID + 1];
	}
	
	if (start_index + 2 * GID >= end_index) {
		bounding_sphere = (float4) (-1);
	} else if (start_index + 2 * GID + 1 == end_index) {
		bounding_sphere = spheres[0];
	} else {
		float3 difference = spheres[1].xyz - spheres[0].xyz;
		float3 p1 = spheres[0].xyz - normalize(difference) * spheres[0].w;
		float3 p2 = spheres[1].xyz + normalize(difference) * spheres[1].w;

		bounding_sphere =  (float4) (p1 + (p2 - p1) / 2, distance(p1, p2) / 2);
		if (spheres[0].w > bounding_sphere.w) bounding_sphere = spheres[0];
		if (spheres[1].w > bounding_sphere.w) bounding_sphere = spheres[1];
	}
	bvh[higher_start_index + GID] = bounding_sphere;
}