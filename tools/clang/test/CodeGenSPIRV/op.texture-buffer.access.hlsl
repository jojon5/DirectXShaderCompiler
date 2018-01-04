// Run: %dxc -T vs_6_0 -E main

struct S {
    float  f;
};

struct T {
    float    a;
    float2   b;
    float3x4 c;
    S        s;
    float    t[4];
};


TextureBuffer<T> MyTB : register(t1);

float main() : A {
// CHECK:      [[a:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[a]]

// CHECK:      [[b:%\d+]] = OpAccessChain %_ptr_Uniform_v2float %MyTB %int_1
// CHECK-NEXT: [[b0:%\d+]] = OpAccessChain %_ptr_Uniform_float [[b]] %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[b0]]

// CHECK:      [[c12:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_2 %uint_1 %uint_2
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[c12]]

// CHECK:      [[s:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_3 %int_0
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[s]]

// CHECK:      [[t:%\d+]] = OpAccessChain %_ptr_Uniform_float %MyTB %int_4 %uint_3
// CHECK-NEXT: {{%\d+}} = OpLoad %float [[t]]
  return MyTB.a + MyTB.b.x + MyTB.c[1][2] + MyTB.s.f + MyTB.t[3];
}
