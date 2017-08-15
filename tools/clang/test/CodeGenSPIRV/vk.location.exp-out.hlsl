// Run: %dxc -T vs_6_0 -E main

struct S {
                        int    a: A;
};

struct T {
                        float4 b: B;
                        S      s;
};

struct U {
    [[vk::location(1)]] float4 c: C; // In struct --- C -> 1
};

[[vk::location(2)]]                  // On function return --- R -> 2
float main(                    in  uint   m: M,
                                   T      t,
           [[vk::location(0)]] out float  n: N, // On function parameter --- N -> 0
                               out U      u,

                               out float4 pos: SV_Position
          ) : R {
    return 1.0;
}

// Alphabetical assignment
// CHECK: OpDecorate %in_var_A Location 0
// CHECK: OpDecorate %in_var_B Location 1
// CHECK: OpDecorate %in_var_M Location 2

// Explicit assignment
// CHECK: OpDecorate %out_var_R Location 2
// CHECK: OpDecorate %out_var_N Location 0
// CHECK: OpDecorate %out_var_C Location 1

