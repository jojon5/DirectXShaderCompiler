// RUN: %dxc -E main -T ps_6_0 %s | FileCheck %s

// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK: call float @dx.op.dot3
// CHECK-NOT: call float @dx.op.dot3

uint g_cond;
float main(float3 a : A, float3 b : B) : SV_Target {

  float result = 0;
  [unroll(3)]
  for (int i = 0; i < g_cond; i++) {
    result += dot(a*i, b);
  }
  return result;
}

