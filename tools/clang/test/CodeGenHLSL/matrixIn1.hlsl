// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0
// CHECK: dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1
// CHECK: dx.op.loadInput.f32(i32 4, i32 0, i32 1, i8 0
// CHECK: dx.op.loadInput.f32(i32 4, i32 0, i32 1, i8 1

// fxc will generate v0.x v1.x v0.y v1.y

// CHECK: float %0)
// CHECK: float %2)
// CHECK: float %1)
// CHECK: float %3)

float4 main(float2x2 m : M) : SV_Target
{
  return m;
}