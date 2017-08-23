// Run: %dxc -T vs_6_0 -E main

// CHECK:      OpName %S "S"
// CHECK-NEXT: OpMemberName %S 0 "f1"
// CHECK-NEXT: OpMemberName %S 1 "f2"

// CHECK:      OpName %type_MyCbuffer "type.MyCbuffer"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 0 "a"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 1 "b"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 2 "c"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 3 "d"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 4 "s"
// CHECK-NEXT: OpMemberName %type_MyCbuffer 5 "t"

// CHECK:      OpName %var_MyCbuffer "var.MyCbuffer"

// CHECK:      OpName %type_AnotherCBuffer "type.AnotherCBuffer"
// CHECK-NEXT: OpMemberName %type_AnotherCBuffer 0 "m"
// CHECK-NEXT: OpMemberName %type_AnotherCBuffer 1 "n"

// CHECK:      OpName %var_AnotherCBuffer "var.AnotherCBuffer"

struct S {
    float  f1;
    float3 f2;
};

cbuffer MyCbuffer : register(b1) {
    bool     a;
    int      b;
    uint2    c;
    float3x4 d;
    S        s;
    float    t[4];
};

cbuffer AnotherCBuffer : register(b2) {
    float3 m;
    float4 n;
}

// CHECK: %type_MyCbuffer = OpTypeStruct %bool %int %v2uint %mat3v4float %S %_arr_float_uint_4

// CHECK: %type_AnotherCBuffer = OpTypeStruct %v3float %v4float

// CHECK: %var_MyCbuffer = OpVariable %_ptr_Uniform_type_MyCbuffer Uniform
// CHECK: %var_AnotherCBuffer = OpVariable %_ptr_Uniform_type_AnotherCBuffer Uniform

void main() {
}