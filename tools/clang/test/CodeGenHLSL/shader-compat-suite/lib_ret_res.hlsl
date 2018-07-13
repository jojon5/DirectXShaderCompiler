// RUN: %dxc -T lib_6_3 -Zi -auto-binding-space 11 %s | FileCheck %s

// resources in return/params disallowed for lib_6_3
// CHECK: error: Exported function
// CHECK: GetSampler
// CHECK: must not contain a resource in parameter or return type

SamplerState    g_samLinear;

SamplerState GetSampler () {
  return g_samLinear;
}