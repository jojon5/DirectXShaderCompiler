// RUN: %dxc -T lib_6_2 %s | FileCheck %s

// CHECK: call i1 @"dx.op.reportHit.%struct.Attr*"(i32 158

struct Attr {
   float2 t;
   int3 t2;
};

float emit(float THit : t, uint HitKind : h, Attr a : A) {
  return ReportHit(THit, HitKind, a);
}