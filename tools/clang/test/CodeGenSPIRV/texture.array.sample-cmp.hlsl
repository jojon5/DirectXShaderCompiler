// Run: %dxc -T ps_6_0 -E main

SamplerComparisonState gSampler : register(s5);

Texture1DArray   <float4> t1 : register(t1);
Texture2DArray   <float3> t2 : register(t2);
TextureCubeArray <float>  t3 : register(t3);

// CHECK: OpCapability MinLod

// CHECK: [[v2fc:%\d+]] = OpConstantComposite %v2float %float_0_1 %float_1
// CHECK: [[v3fc:%\d+]] = OpConstantComposite %v3float %float_0_1 %float_0_2 %float_1
// CHECK: [[v4fc:%\d+]] = OpConstantComposite %v4float %float_0_1 %float_0_2 %float_0_3 %float_1

float4 main(int2 offset: A, float comparator: B) : SV_Target {
// CHECK:              [[t1:%\d+]] = OpLoad %type_1d_image_array %t1
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image [[t1]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v2fc]] [[comparator]] ConstOffset %int_1
    float val1 = t1.SampleCmp(gSampler, float2(0.1, 1), comparator, 1);

// CHECK:              [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Offset [[offset]]
    float val2 = t2.SampleCmp(gSampler, float3(0.1, 0.2, 1), comparator, offset);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v4fc]] [[comparator]]
    float val3 = t3.SampleCmp(gSampler, float4(0.1, 0.2, 0.3, 1), comparator);

    float clamp;
// CHECK:           [[clamp:%\d+]] = OpLoad %float %clamp
// CHECK-NEXT:         [[t2:%\d+]] = OpLoad %type_2d_image_array %t2
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT:     [[offset:%\d+]] = OpLoad %v2int %offset
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_0 [[t2]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v3fc]] [[comparator]] Offset|MinLod [[offset]] [[clamp]]
    float val4 = t2.SampleCmp(gSampler, float3(0.1, 0.2, 1), comparator, offset, clamp);

// CHECK:              [[t3:%\d+]] = OpLoad %type_cube_image_array %t3
// CHECK-NEXT:   [[gSampler:%\d+]] = OpLoad %type_sampler %gSampler
// CHECK-NEXT: [[comparator:%\d+]] = OpLoad %float %comparator
// CHECK-NEXT: [[sampledImg:%\d+]] = OpSampledImage %type_sampled_image_1 [[t3]] [[gSampler]]
// CHECK-NEXT:            {{%\d+}} = OpImageSampleDrefImplicitLod %float [[sampledImg]] [[v4fc]] [[comparator]] MinLod %float_1_5
    float val5 = t3.SampleCmp(gSampler, float4(0.1, 0.2, 0.3, 1), comparator, /*clamp*/ 1.5);
    
    return 1.0;
}
