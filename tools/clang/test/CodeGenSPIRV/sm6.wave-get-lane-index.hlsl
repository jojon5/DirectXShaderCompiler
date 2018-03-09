// Run: %dxc -T cs_6_0 -E main

RWStructuredBuffer<uint> values;

// CHECK: OpCapability SubgroupBallotKHR
// CHECK: OpExtension "SPV_KHR_shader_ballot"

// CHECK: OpEntryPoint GLCompute
// CHECK-SAME: %SubgroupLocalInvocationId

// CHECK: OpDecorate %SubgroupLocalInvocationId BuiltIn SubgroupLocalInvocationId

// CHECK: %SubgroupLocalInvocationId = OpVariable %_ptr_Input_uint Input

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
// CHECK: OpLoad %uint %SubgroupLocalInvocationId
    values[id.x] = WaveGetLaneIndex();
}
