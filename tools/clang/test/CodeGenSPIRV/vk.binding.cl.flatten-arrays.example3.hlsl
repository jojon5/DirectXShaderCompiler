// Run: %dxc -T ps_6_0 -E main -fspv-flatten-resource-arrays -O3

// This shader tests that loop unrolling is performed before resource array
// flattening is done.

struct PS_INPUT {
	float4 Pos		: SV_Position;
};
struct PS_OUTPUT {
	float4	Color	: SV_Target0;
};

SamplerState		S;

// CHECK: OpDecorate [[elem1:%\d+]] Binding 1
// CHECK: OpDecorate [[elem2:%\d+]] Binding 2
// CHECK: OpDecorate [[elem3:%\d+]] Binding 3
// CHECK: OpDecorate [[elem4:%\d+]] Binding 4

// CHECK: [[elem1]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[elem2]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[elem3]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant
// CHECK: [[elem4]] = OpVariable %_ptr_UniformConstant_type_2d_image UniformConstant

Texture2D<float>	T[4];

PS_OUTPUT main(const PS_INPUT In) {
	PS_OUTPUT	Out;
	Out.Color = float4(1,0,0,1);

  // Loop with [unroll] attribute.
  [unroll(4)]
	for (uint i=0; i<4; i++) {
		Out.Color.y += T[i].SampleLevel(S, In.Pos.xy, 0).x;
	}

	return Out;
}

