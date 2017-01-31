; RUN: %dxv %s | FileCheck %s

; CHECK: Semantic 'A' overlap at 0

target datalayout = "e-m:e-p:32:32-i64:64-f80:32-n8:16:32-a:0:32-S32"
target triple = "dxil-ms-dx"

; Function Attrs: nounwind
define void @main.flat(<4 x float>, <4 x float>, <4 x float>* nocapture readnone) #0 {
entry:
  %3 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 0, i32 undef)
  %4 = tail call float @dx.op.loadInput.f32(i32 4, i32 1, i32 0, i8 1, i32 undef)
  %5 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 0, i32 undef)
  %6 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 1, i32 undef)
  %7 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 2, i32 undef)
  %8 = tail call float @dx.op.loadInput.f32(i32 4, i32 0, i32 0, i8 3, i32 undef)
  %mul.i0 = fmul fast float %5, %4
  %mul.i1 = fmul fast float %6, %3
  %mul.i2 = fmul fast float %7, %3
  %mul.i3 = fmul fast float %8, %3
  %FAbs = tail call float @dx.op.unary.f32(i32 6, float %mul.i0)
  %FAbs1 = tail call float @dx.op.unary.f32(i32 6, float %mul.i1)
  %FAbs2 = tail call float @dx.op.unary.f32(i32 6, float %mul.i2)
  %FAbs3 = tail call float @dx.op.unary.f32(i32 6, float %mul.i3)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 0, float %FAbs)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 1, float %FAbs1)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 2, float %FAbs2)
  tail call void @dx.op.storeOutput.f32(i32 5, i32 0, i32 0, i8 3, float %FAbs3)
  ret void
}

; Function Attrs: nounwind readnone
declare float @dx.op.loadInput.f32(i32, i32, i32, i8, i32) #1

; Function Attrs: nounwind
declare void @dx.op.storeOutput.f32(i32, i32, i32, i8, float) #0

; Function Attrs: nounwind readnone
declare float @dx.op.unary.f32(i32, float) #1

attributes #0 = { nounwind }
attributes #1 = { nounwind readnone }

!llvm.ident = !{!0}
!dx.version = !{!1}
!dx.shaderModel = !{!2}
!dx.typeAnnotations = !{!3}
!dx.entryPoints = !{!12}

!0 = !{!"clang version 3.7 (tags/RELEASE_370/final)"}
!1 = !{i32 0, i32 7}
!2 = !{!"ps", i32 6, i32 0}
!3 = !{i32 1, void (<4 x float>, <4 x float>, <4 x float>*)* @main.flat, !4}
!4 = !{!5, !7, !7, !10}
!5 = !{i32 0, !6, !6}
!6 = !{}
!7 = !{i32 0, !8, !9}
!8 = !{i32 4, !"A", i32 7, i32 9}
!9 = !{i32 0}
!10 = !{i32 1, !11, !9}
!11 = !{i32 4, !"SV_TARGET", i32 7, i32 9}
!12 = !{void (<4 x float>, <4 x float>, <4 x float>*)* @main.flat, !"", !13, null, null}
!13 = !{!14, !17, null}
!14 = !{!15, !16}
!15 = !{i32 0, !"A", i8 9, i8 0, !9, i8 2, i32 1, i8 4, i32 0, i8 0, null}
!16 = !{i32 1, !"A", i8 9, i8 0, !9, i8 2, i32 1, i8 4, i32 1, i8 0, null}
!17 = !{!18}
!18 = !{i32 0, !"SV_Target", i8 9, i8 16, !9, i8 0, i32 1, i8 4, i32 0, i8 0, null}
