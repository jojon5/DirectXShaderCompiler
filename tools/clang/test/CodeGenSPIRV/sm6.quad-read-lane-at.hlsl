// Run: %dxc -T cs_6_0 -E main

// CHECK: ; Version: 1.3

struct S {
    float4 val1;
     uint3 val2;
       int val3;
};

RWStructuredBuffer<S> values;

// CHECK: OpCapability GroupNonUniformQuad

[numthreads(32, 1, 1)]
void main(uint3 id: SV_DispatchThreadID) {
    uint x = id.x;

    float4 val1 = values[x].val1;
     uint3 val2 = values[x].val2;
       int val3 = values[x].val3;

// CHECK:      [[val1:%\d+]] = OpLoad %v4float %val1
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadBroadcast %v4float %int_3 [[val1]] %uint_0
    values[x].val1 = QuadReadLaneAt(val1, 0);
// CHECK:      [[val2:%\d+]] = OpLoad %v3uint %val2
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadBroadcast %v3uint %int_3 [[val2]] %uint_1
    values[x].val2 = QuadReadLaneAt(val2, 1);
// CHECK:      [[val3:%\d+]] = OpLoad %int %val3
// CHECK-NEXT:      {{%\d+}} = OpGroupNonUniformQuadBroadcast %int %int_3 [[val3]] %uint_2
    values[x].val3 = QuadReadLaneAt(val3, 2);
}

