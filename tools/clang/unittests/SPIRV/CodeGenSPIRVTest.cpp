//===- unittests/SPIRV/CodeGenSPIRVTest.cpp ---- Run CodeGenSPIRV tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "FileTestFixture.h"
#include "WholeFileTestFixture.h"

namespace {
using clang::spirv::FileTest;
using clang::spirv::WholeFileTest;

// === Whole output tests ===

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruVertexShader) {
  runWholeFileTest("passthru-vs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruComputeShader) {
  runWholeFileTest("passthru-cs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, BezierHullShader) {
  runWholeFileTest("bezier.hull.hlsl2spv");
}

TEST_F(WholeFileTest, BezierDomainShader) {
  runWholeFileTest("bezier.domain.hlsl2spv");
}
TEST_F(WholeFileTest, EmptyStructInterfaceVS) {
  runWholeFileTest("empty-struct-interface.vs.hlsl2spv");
}

// === Partial output tests ===

// For types
TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }
TEST_F(FileTest, VectorTypes) { runFileTest("type.vector.hlsl"); }
TEST_F(FileTest, MatrixTypes) { runFileTest("type.matrix.hlsl"); }
TEST_F(FileTest, MatrixTypesMajornessZpr) {
  runFileTest("type.matrix.majorness.zpr.hlsl");
}
TEST_F(FileTest, MatrixTypesMajorness) {
  runFileTest("type.matrix.majorness.hlsl", Expect::Warning);
}
TEST_F(FileTest, StructTypes) { runFileTest("type.struct.hlsl"); }
TEST_F(FileTest, ClassTypes) { runFileTest("type.class.hlsl"); }
TEST_F(FileTest, ArrayTypes) { runFileTest("type.array.hlsl"); }
TEST_F(FileTest, TypedefTypes) { runFileTest("type.typedef.hlsl"); }
TEST_F(FileTest, SamplerTypes) { runFileTest("type.sampler.hlsl"); }
TEST_F(FileTest, TextureTypes) { runFileTest("type.texture.hlsl"); }
TEST_F(FileTest, RWTextureTypes) { runFileTest("type.rwtexture.hlsl"); }
TEST_F(FileTest, BufferType) { runFileTest("type.buffer.hlsl"); }
TEST_F(FileTest, CBufferType) { runFileTest("type.cbuffer.hlsl"); }
TEST_F(FileTest, ConstantBufferType) {
  runFileTest("type.constant-buffer.hlsl");
}
TEST_F(FileTest, TBufferType) { runFileTest("type.tbuffer.hlsl"); }
TEST_F(FileTest, TextureBufferType) { runFileTest("type.texture-buffer.hlsl"); }
TEST_F(FileTest, StructuredBufferType) {
  runFileTest("type.structured-buffer.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferType) {
  runFileTest("type.append-structured-buffer.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferType) {
  runFileTest("type.consume-structured-buffer.hlsl");
}
TEST_F(FileTest, ByteAddressBufferTypes) {
  runFileTest("type.byte-address-buffer.hlsl");
}
TEST_F(FileTest, PointStreamTypes) { runFileTest("type.point-stream.hlsl"); }
TEST_F(FileTest, LineStreamTypes) { runFileTest("type.line-stream.hlsl"); }
TEST_F(FileTest, TriangleStreamTypes) {
  runFileTest("type.triangle-stream.hlsl");
}

// For constants
TEST_F(FileTest, ScalarConstants) { runFileTest("constant.scalar.hlsl"); }
TEST_F(FileTest, 16BitDisabledScalarConstants) {
  runFileTest("constant.scalar.16bit.disabled.hlsl");
}
TEST_F(FileTest, 16BitEnabledScalarConstants) {
  // TODO: Fix spirv-val to make sure it respects the 16-bit extension.
  runFileTest("constant.scalar.16bit.enabled.hlsl", Expect::Success,
              /*runValidation*/ false);
}
TEST_F(FileTest, 64BitScalarConstants) {
  runFileTest("constant.scalar.64bit.hlsl");
}
TEST_F(FileTest, VectorConstants) { runFileTest("constant.vector.hlsl"); }
TEST_F(FileTest, MatrixConstants) { runFileTest("constant.matrix.hlsl"); }
TEST_F(FileTest, StructConstants) { runFileTest("constant.struct.hlsl"); }
TEST_F(FileTest, ArrayConstants) { runFileTest("constant.array.hlsl"); }

// For variables
TEST_F(FileTest, VarInitScalarVector) { runFileTest("var.init.hlsl"); }
TEST_F(FileTest, VarInitMatrixMxN) { runFileTest("var.init.matrix.mxn.hlsl"); }
TEST_F(FileTest, VarInitMatrixMx1) { runFileTest("var.init.matrix.mx1.hlsl"); }
TEST_F(FileTest, VarInitMatrix1xN) { runFileTest("var.init.matrix.1xn.hlsl"); }
TEST_F(FileTest, VarInitMatrix1x1) { runFileTest("var.init.matrix.1x1.hlsl"); }
TEST_F(FileTest, VarInitStruct) { runFileTest("var.init.struct.hlsl"); }
TEST_F(FileTest, VarInitArray) { runFileTest("var.init.array.hlsl"); }
TEST_F(FileTest, VarInitCbuffer) {
  runFileTest("var.init.cbuffer.hlsl", Expect::Warning);
}
TEST_F(FileTest, VarInitTbuffer) {
  runFileTest("var.init.tbuffer.hlsl", Expect::Warning);
}
TEST_F(FileTest, VarInitOpaque) { runFileTest("var.init.opaque.hlsl"); }
TEST_F(FileTest, VarInitCrossStorageClass) {
  runFileTest("var.init.cross-storage-class.hlsl");
}
TEST_F(FileTest, StaticVar) { runFileTest("var.static.hlsl"); }

// For prefix/postfix increment/decrement
TEST_F(FileTest, UnaryOpPrefixIncrement) {
  runFileTest("unary-op.prefix-inc.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixIncrementMatrix) {
  runFileTest("unary-op.prefix-inc.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixDecrement) {
  runFileTest("unary-op.prefix-dec.hlsl");
}
TEST_F(FileTest, UnaryOpPrefixDecrementMatrix) {
  runFileTest("unary-op.prefix-dec.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixIncrement) {
  runFileTest("unary-op.postfix-inc.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixIncrementMatrix) {
  runFileTest("unary-op.postfix-inc.matrix.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixDecrement) {
  runFileTest("unary-op.postfix-dec.hlsl");
}
TEST_F(FileTest, UnaryOpPostfixDecrementMatrix) {
  runFileTest("unary-op.postfix-dec.matrix.hlsl");
}

// For unary operators
TEST_F(FileTest, UnaryOpPlus) { runFileTest("unary-op.plus.hlsl"); }
TEST_F(FileTest, UnaryOpMinus) { runFileTest("unary-op.minus.hlsl"); }
TEST_F(FileTest, UnaryOpLogicalNot) {
  runFileTest("unary-op.logical-not.hlsl");
}

// For assignments
TEST_F(FileTest, BinaryOpAssign) { runFileTest("binary-op.assign.hlsl"); }
TEST_F(FileTest, BinaryOpAssignImage) {
  runFileTest("binary-op.assign.image.hlsl");
}
TEST_F(FileTest, BinaryOpAssignComposite) {
  runFileTest("binary-op.assign.composite.hlsl");
}

// For comma binary operator
TEST_F(FileTest, BinaryOpComma) { runFileTest("binary-op.comma.hlsl"); }

// For arithmetic binary operators
TEST_F(FileTest, BinaryOpScalarArithmetic) {
  runFileTest("binary-op.arithmetic.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithmetic) {
  runFileTest("binary-op.arithmetic.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMatrixArithmetic) {
  runFileTest("binary-op.arithmetic.matrix.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithmetic) {
  runFileTest("binary-op.arithmetic.mixed.hlsl");
}

// For arithmetic assignments
TEST_F(FileTest, BinaryOpScalarArithAssign) {
  runFileTest("binary-op.arith-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithAssign) {
  runFileTest("binary-op.arith-assign.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMatrixArithAssign) {
  runFileTest("binary-op.arith-assign.matrix.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithAssign) {
  runFileTest("binary-op.arith-assign.mixed.hlsl");
}

// For bitwise binary operators
TEST_F(FileTest, BinaryOpScalarBitwise) {
  runFileTest("binary-op.bitwise.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwise) {
  runFileTest("binary-op.bitwise.vector.hlsl");
}

// For bitwise assignments
TEST_F(FileTest, BinaryOpScalarBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.vector.hlsl");
}

// For comparison operators
TEST_F(FileTest, BinaryOpScalarComparison) {
  runFileTest("binary-op.comparison.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorComparison) {
  runFileTest("binary-op.comparison.vector.hlsl");
}

// For logical binary operators
TEST_F(FileTest, BinaryOpLogicalAnd) {
  runFileTest("binary-op.logical-and.hlsl");
}
TEST_F(FileTest, BinaryOpLogicalOr) {
  runFileTest("binary-op.logical-or.hlsl");
}

// For ternary operators
TEST_F(FileTest, TernaryOpConditionalOp) {
  runFileTest("ternary-op.cond-op.hlsl");
}

// For vector accessing/swizzling operators
TEST_F(FileTest, OpVectorSwizzle) { runFileTest("op.vector.swizzle.hlsl"); }
TEST_F(FileTest, OpVectorSwizzle1) {
  runFileTest("op.vector.swizzle.size1.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleAfterBufferAccess) {
  runFileTest("op.vector.swizzle.buffer-access.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleAfterTextureAccess) {
  runFileTest("op.vector.swizzle.texture-access.hlsl");
}
TEST_F(FileTest, OpVectorSwizzleConstScalar) {
  runFileTest("op.vector.swizzle.const-scalar.hlsl");
}
TEST_F(FileTest, OpVectorAccess) { runFileTest("op.vector.access.hlsl"); }

// For matrix accessing/swizzling operators
TEST_F(FileTest, OpMatrixAccessMxN) {
  runFileTest("op.matrix.access.mxn.hlsl");
}
TEST_F(FileTest, OpMatrixAccessMx1) {
  runFileTest("op.matrix.access.mx1.hlsl");
}
TEST_F(FileTest, OpMatrixAccess1xN) {
  runFileTest("op.matrix.access.1xn.hlsl");
}
TEST_F(FileTest, OpMatrixAccess1x1) {
  runFileTest("op.matrix.access.1x1.hlsl");
}

// For struct & array accessing operator
TEST_F(FileTest, OpStructAccess) { runFileTest("op.struct.access.hlsl"); }
TEST_F(FileTest, OpArrayAccess) { runFileTest("op.array.access.hlsl"); }

// For buffer accessing operator
TEST_F(FileTest, OpBufferAccess) { runFileTest("op.buffer.access.hlsl"); }
TEST_F(FileTest, OpRWBufferAccess) { runFileTest("op.rwbuffer.access.hlsl"); }
TEST_F(FileTest, OpCBufferAccess) { runFileTest("op.cbuffer.access.hlsl"); }
TEST_F(FileTest, OpConstantBufferAccess) {
  runFileTest("op.constant-buffer.access.hlsl");
}
TEST_F(FileTest, OpTBufferAccess) { runFileTest("op.tbuffer.access.hlsl"); }
TEST_F(FileTest, OpTextureBufferAccess) {
  runFileTest("op.texture-buffer.access.hlsl");
}
TEST_F(FileTest, OpStructuredBufferAccess) {
  runFileTest("op.structured-buffer.access.hlsl");
}
TEST_F(FileTest, OpRWStructuredBufferAccess) {
  runFileTest("op.rw-structured-buffer.access.hlsl");
}

// For Texture/RWTexture accessing operator (operator[])
TEST_F(FileTest, OpTextureAccess) { runFileTest("op.texture.access.hlsl"); }
TEST_F(FileTest, OpRWTextureAccessRead) {
  runFileTest("op.rwtexture.access.read.hlsl");
}
TEST_F(FileTest, OpRWTextureAccessWrite) {
  runFileTest("op.rwtexture.access.write.hlsl");
}

// For Texture.mips[][] operator
TEST_F(FileTest, OpTextureMipsAccess) {
  runFileTest("op.texture.mips-access.hlsl");
}
// For Texture2MD(Array).sample[][] operator
TEST_F(FileTest, OpTextureSampleAccess) {
  runFileTest("op.texture.sample-access.hlsl");
}

// For casting
TEST_F(FileTest, CastNoOp) { runFileTest("cast.no-op.hlsl"); }
TEST_F(FileTest, CastImplicit2Bool) { runFileTest("cast.2bool.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2Bool) { runFileTest("cast.2bool.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2SInt) { runFileTest("cast.2sint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2SInt) { runFileTest("cast.2sint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2UInt) { runFileTest("cast.2uint.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2UInt) { runFileTest("cast.2uint.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2FP) { runFileTest("cast.2fp.implicit.hlsl"); }
TEST_F(FileTest, CastExplicit2FP) { runFileTest("cast.2fp.explicit.hlsl"); }
TEST_F(FileTest, CastImplicit2LiteralInt) {
  runFileTest("cast.2literal-int.implicit.hlsl");
}
TEST_F(FileTest, CastImplicitFlatConversion) {
  runFileTest("cast.flat-conversion.implicit.hlsl");
}
TEST_F(FileTest, CastExplicitVecToMat) {
  runFileTest("cast.vec-to-mat.explicit.hlsl");
}

// For vector/matrix splatting and trunction
TEST_F(FileTest, CastTruncateVector) { runFileTest("cast.vector.trunc.hlsl"); }
TEST_F(FileTest, CastTruncateMatrix) { runFileTest("cast.matrix.trunc.hlsl"); }
TEST_F(FileTest, CastSplatVector) { runFileTest("cast.vector.splat.hlsl"); }
TEST_F(FileTest, CastSplatMatrix) { runFileTest("cast.matrix.splat.hlsl"); }

// For if statements
TEST_F(FileTest, IfStmtPlainAssign) { runFileTest("cf.if.plain.hlsl"); }
TEST_F(FileTest, IfStmtNestedIfStmt) { runFileTest("cf.if.nested.hlsl"); }
TEST_F(FileTest, IfStmtConstCondition) { runFileTest("cf.if.const-cond.hlsl"); }

// For switch statements
TEST_F(FileTest, SwitchStmtUsingOpSwitch) {
  runFileTest("cf.switch.opswitch.hlsl");
}
TEST_F(FileTest, SwitchStmtUsingIfStmt) {
  runFileTest("cf.switch.ifstmt.hlsl");
}

// For for statements
TEST_F(FileTest, ForStmtPlainAssign) { runFileTest("cf.for.plain.hlsl"); }
TEST_F(FileTest, ForStmtNestedForStmt) { runFileTest("cf.for.nested.hlsl"); }
TEST_F(FileTest, ForStmtContinue) { runFileTest("cf.for.continue.hlsl"); }
TEST_F(FileTest, ForStmtBreak) { runFileTest("cf.for.break.hlsl"); }

// For while statements
TEST_F(FileTest, WhileStmtPlain) { runFileTest("cf.while.plain.hlsl"); }
TEST_F(FileTest, WhileStmtNested) { runFileTest("cf.while.nested.hlsl"); }
TEST_F(FileTest, WhileStmtContinue) { runFileTest("cf.while.continue.hlsl"); }
TEST_F(FileTest, WhileStmtBreak) { runFileTest("cf.while.break.hlsl"); }

// For do statements
TEST_F(FileTest, DoStmtPlain) { runFileTest("cf.do.plain.hlsl"); }
TEST_F(FileTest, DoStmtNested) { runFileTest("cf.do.nested.hlsl"); }
TEST_F(FileTest, DoStmtContinue) { runFileTest("cf.do.continue.hlsl"); }
TEST_F(FileTest, DoStmtBreak) { runFileTest("cf.do.break.hlsl"); }

// For break statements (mix of breaks in loops and switch)
TEST_F(FileTest, BreakStmtMixed) { runFileTest("cf.break.mixed.hlsl"); }

// For discard statement
TEST_F(FileTest, Discard) { runFileTest("cf.discard.hlsl"); }

// For return statement
TEST_F(FileTest, EarlyReturn) { runFileTest("cf.return.early.hlsl"); }
TEST_F(FileTest, EarlyReturnFloat4) {
  runFileTest("cf.return.early.float4.hlsl");
}
TEST_F(FileTest, ReturnStruct) { runFileTest("cf.return.struct.hlsl"); }
TEST_F(FileTest, ReturnFromDifferentStorageClass) {
  runFileTest("cf.return.storage-class.hlsl");
}

// For control flows
TEST_F(FileTest, ControlFlowNestedIfForStmt) { runFileTest("cf.if.for.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalAnd) { runFileTest("cf.logical-and.hlsl"); }
TEST_F(FileTest, ControlFlowLogicalOr) { runFileTest("cf.logical-or.hlsl"); }
TEST_F(FileTest, ControlFlowConditionalOp) { runFileTest("cf.cond-op.hlsl"); }

// For functions
TEST_F(FileTest, FunctionCall) { runFileTest("fn.call.hlsl"); }
TEST_F(FileTest, FunctionDefaultArg) { runFileTest("fn.default-arg.hlsl"); }
TEST_F(FileTest, FunctionInOutParam) { runFileTest("fn.param.inout.hlsl"); }
TEST_F(FileTest, FunctionInOutParamVector) {
  runFileTest("fn.param.inout.vector.hlsl");
}
TEST_F(FileTest, FunctionFowardDeclaration) {
  runFileTest("fn.foward-declaration.hlsl");
}

// For OO features
TEST_F(FileTest, StructMethodCall) { runFileTest("oo.struct.method.hlsl"); }
TEST_F(FileTest, ClassMethodCall) { runFileTest("oo.class.method.hlsl"); }
TEST_F(FileTest, StructStaticMember) {
  runFileTest("oo.struct.static.member.hlsl");
}
TEST_F(FileTest, ClassStaticMember) {
  runFileTest("oo.struct.static.member.hlsl");
}
TEST_F(FileTest, StaticMemberInitializer) {
  runFileTest("oo.static.member.init.hlsl");
}
TEST_F(FileTest, MethodCallOnStaticVar) {
  runFileTest("oo.method.on-static-var.hlsl");
}

// For semantics
// SV_Position, SV_ClipDistance, and SV_CullDistance are covered in
// SpirvStageIOInterface* tests.
TEST_F(FileTest, SemanticVertexIDVS) {
  runFileTest("semantic.vertex-id.vs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDVS) {
  runFileTest("semantic.instance-id.vs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDHS) {
  runFileTest("semantic.instance-id.hs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDDS) {
  runFileTest("semantic.instance-id.ds.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDGS) {
  runFileTest("semantic.instance-id.gs.hlsl");
}
TEST_F(FileTest, SemanticInstanceIDPS) {
  runFileTest("semantic.instance-id.ps.hlsl");
}
TEST_F(FileTest, SemanticTargetPS) { runFileTest("semantic.target.ps.hlsl"); }
TEST_F(FileTest, SemanticDepthPS) { runFileTest("semantic.depth.ps.hlsl"); }
TEST_F(FileTest, SemanticDepthGreaterEqualPS) {
  runFileTest("semantic.depth-greater-equal.ps.hlsl");
}
TEST_F(FileTest, SemanticDepthLessEqualPS) {
  runFileTest("semantic.depth-less-equal.ps.hlsl");
}
TEST_F(FileTest, SemanticIsFrontFaceGS) {
  runFileTest("semantic.is-front-face.gs.hlsl");
}
TEST_F(FileTest, SemanticIsFrontFacePS) {
  runFileTest("semantic.is-front-face.ps.hlsl");
}
TEST_F(FileTest, SemanticDispatchThreadId) {
  runFileTest("semantic.dispatch-thread-id.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupID) { runFileTest("semantic.group-id.cs.hlsl"); }
TEST_F(FileTest, SemanticGroupThreadID) {
  runFileTest("semantic.group-thread-id.cs.hlsl");
}
TEST_F(FileTest, SemanticGroupIndex) {
  runFileTest("semantic.group-index.cs.hlsl");
}
TEST_F(FileTest, SemanticDomainLocationDS) {
  runFileTest("semantic.domain-location.ds.hlsl");
}
TEST_F(FileTest, SemanticTessFactorDS) {
  runFileTest("semantic.tess-factor.ds.hlsl");
}
TEST_F(FileTest, SemanticTessFactorSizeMismatchDS) {
  runFileTest("semantic.tess-factor.size-mismatch.ds.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorDS) {
  runFileTest("semantic.inside-tess-factor.ds.hlsl");
}
TEST_F(FileTest, SemanticTessFactorHS) {
  runFileTest("semantic.tess-factor.hs.hlsl");
}
TEST_F(FileTest, SemanticTessFactorSizeMismatchHS) {
  runFileTest("semantic.tess-factor.size-mismatch.hs.hlsl");
}
TEST_F(FileTest, SemanticInsideTessFactorHS) {
  runFileTest("semantic.inside-tess-factor.hs.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdHS) {
  runFileTest("semantic.primitive-id.hs.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdDS) {
  runFileTest("semantic.primitive-id.ds.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdGS) {
  runFileTest("semantic.primitive-id.gs.hlsl");
}
TEST_F(FileTest, SemanticPrimitiveIdPS) {
  runFileTest("semantic.primitive-id.ps.hlsl");
}
TEST_F(FileTest, SemanticOutputControlPointIdHS) {
  runFileTest("semantic.output-control-point-id.hs.hlsl");
}
TEST_F(FileTest, SemanticGSInstanceIDGS) {
  runFileTest("semantic.gs-instance-id.gs.hlsl");
}
TEST_F(FileTest, SemanticSampleIndexPS) {
  runFileTest("semantic.sample-index.ps.hlsl");
}
TEST_F(FileTest, SemanticStencilRefPS) {
  runFileTest("semantic.stencil-ref.ps.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexVS) {
  runFileTest("semantic.render-target-array-index.vs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexHS) {
  runFileTest("semantic.render-target-array-index.hs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexDS) {
  runFileTest("semantic.render-target-array-index.ds.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexGS) {
  runFileTest("semantic.render-target-array-index.gs.hlsl");
}
TEST_F(FileTest, SemanticRenderTargetArrayIndexPS) {
  runFileTest("semantic.render-target-array-index.ps.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexVS) {
  runFileTest("semantic.viewport-array-index.vs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexHS) {
  runFileTest("semantic.viewport-array-index.hs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexDS) {
  runFileTest("semantic.viewport-array-index.ds.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexGS) {
  runFileTest("semantic.viewport-array-index.gs.hlsl");
}
TEST_F(FileTest, SemanticViewportArrayIndexPS) {
  runFileTest("semantic.viewport-array-index.ps.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothPS) {
  runFileTest("semantic.barycentrics.ps.s.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothCentroidPS) {
  runFileTest("semantic.barycentrics.ps.s-c.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsSmoothSamplePS) {
  runFileTest("semantic.barycentrics.ps.s-s.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectivePS) {
  runFileTest("semantic.barycentrics.ps.np.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectiveCentroidPS) {
  runFileTest("semantic.barycentrics.ps.np-c.hlsl");
}
TEST_F(FileTest, SemanticBarycentricsNoPerspectiveSamplePS) {
  runFileTest("semantic.barycentrics.ps.np-s.hlsl");
}

TEST_F(FileTest, SemanticCoveragePS) {
  runFileTest("semantic.coverage.ps.hlsl");
}
TEST_F(FileTest, SemanticCoverageTypeMismatchPS) {
  runFileTest("semantic.coverage.type-mismatch.ps.hlsl");
}
TEST_F(FileTest, SemanticInnerCoveragePS) {
  runFileTest("semantic.inner-coverage.ps.hlsl", Expect::Failure);
}
TEST_F(FileTest, SemanticViewIDVS) { runFileTest("semantic.view-id.vs.hlsl"); }
TEST_F(FileTest, SemanticViewIDHS) { runFileTest("semantic.view-id.hs.hlsl"); }
TEST_F(FileTest, SemanticViewIDDS) { runFileTest("semantic.view-id.ds.hlsl"); }
TEST_F(FileTest, SemanticViewIDGS) { runFileTest("semantic.view-id.gs.hlsl"); }
TEST_F(FileTest, SemanticViewIDPS) { runFileTest("semantic.view-id.ps.hlsl"); }

TEST_F(FileTest, SemanticArbitrary) { runFileTest("semantic.arbitrary.hlsl"); }
TEST_F(FileTest, SemanticArbitraryDeclLocation) {
  runFileTest("semantic.arbitrary.location.decl.hlsl");
}
TEST_F(FileTest, SemanticArbitraryAlphaLocation) {
  runFileTest("semantic.arbitrary.location.alpha.hlsl");
}
TEST_F(FileTest, SemanticDuplication) {
  runFileTest("semantic.duplication.hlsl", Expect::Failure);
}
TEST_F(FileTest, SemanticOnStruct) { runFileTest("semantic.on-struct.hlsl"); }

// For texture methods
TEST_F(FileTest, TextureSample) { runFileTest("texture.sample.hlsl"); }
TEST_F(FileTest, TextureArraySample) {
  runFileTest("texture.array.sample.hlsl");
}
TEST_F(FileTest, TextureLoad) { runFileTest("texture.load.hlsl"); }
TEST_F(FileTest, TextureArrayLoad) { runFileTest("texture.array.load.hlsl"); }
TEST_F(FileTest, TextureGetDimensions) {
  runFileTest("texture.get-dimensions.hlsl");
}
TEST_F(FileTest, TextureGetSamplePosition) {
  runFileTest("texture.get-sample-position.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureCalculateLevelOfDetail) {
  runFileTest("texture.calculate-lod.hlsl");
}
TEST_F(FileTest, TextureCalculateLevelOfDetailUnclamped) {
  runFileTest("texture.calculate-lod-unclamped.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureGather) { runFileTest("texture.gather.hlsl"); }
TEST_F(FileTest, TextureArrayGather) {
  runFileTest("texture.array.gather.hlsl");
}
TEST_F(FileTest, TextureGatherRed) { runFileTest("texture.gather-red.hlsl"); }
TEST_F(FileTest, TextureArrayGatherRed) {
  runFileTest("texture.array.gather-red.hlsl");
}
TEST_F(FileTest, TextureGatherGreen) {
  runFileTest("texture.gather-green.hlsl");
}
TEST_F(FileTest, TextureArrayGatherGreen) {
  runFileTest("texture.array.gather-green.hlsl");
}
TEST_F(FileTest, TextureGatherBlue) { runFileTest("texture.gather-blue.hlsl"); }
TEST_F(FileTest, TextureArrayGatherBlue) {
  runFileTest("texture.array.gather-blue.hlsl");
}
TEST_F(FileTest, TextureGatherAlpha) {
  runFileTest("texture.gather-alpha.hlsl");
}
TEST_F(FileTest, TextureArrayGatherAlpha) {
  runFileTest("texture.array.gather-alpha.hlsl");
}
TEST_F(FileTest, TextureGatherCmp) { runFileTest("texture.gather-cmp.hlsl"); }
TEST_F(FileTest, TextureArrayGatherCmp) {
  runFileTest("texture.array.gather-cmp.hlsl");
}
TEST_F(FileTest, TextureGatherCmpRed) {
  runFileTest("texture.gather-cmp-red.hlsl");
}
TEST_F(FileTest, TextureArrayGatherCmpRed) {
  runFileTest("texture.array.gather-cmp-red.hlsl");
}
TEST_F(FileTest, TextureArrayGatherCmpGreen) {
  runFileTest("texture.gather-cmp-green.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureArrayGatherCmpBlue) {
  runFileTest("texture.gather-cmp-blue.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureArrayGatherCmpAlpha) {
  runFileTest("texture.gather-cmp-alpha.hlsl", Expect::Failure);
}
TEST_F(FileTest, TextureSampleLevel) {
  runFileTest("texture.sample-level.hlsl");
}
TEST_F(FileTest, TextureArraySampleLevel) {
  runFileTest("texture.array.sample-level.hlsl");
}
TEST_F(FileTest, TextureSampleBias) { runFileTest("texture.sample-bias.hlsl"); }
TEST_F(FileTest, TextureArraySampleBias) {
  runFileTest("texture.array.sample-bias.hlsl");
}
TEST_F(FileTest, TextureSampleGrad) { runFileTest("texture.sample-grad.hlsl"); }
TEST_F(FileTest, TextureArraySampleGrad) {
  runFileTest("texture.array.sample-grad.hlsl");
}
TEST_F(FileTest, TextureSampleCmp) { runFileTest("texture.sample-cmp.hlsl"); }
TEST_F(FileTest, TextureArraySampleCmp) {
  runFileTest("texture.array.sample-cmp.hlsl");
}
TEST_F(FileTest, TextureSampleCmpLevelZero) {
  runFileTest("texture.sample-cmp-level-zero.hlsl");
}
TEST_F(FileTest, TextureArraySampleCmpLevelZero) {
  runFileTest("texture.array.sample-cmp-level-zero.hlsl");
}

// For structured buffer methods
TEST_F(FileTest, StructuredBufferLoad) {
  runFileTest("method.structured-buffer.load.hlsl");
}
TEST_F(FileTest, StructuredBufferGetDimensions) {
  runFileTest("method.structured-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, RWStructuredBufferIncDecCounter) {
  runFileTest("method.rw-structured-buffer.counter.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferAppend) {
  runFileTest("method.append-structured-buffer.append.hlsl");
}
TEST_F(FileTest, AppendStructuredBufferGetDimensions) {
  runFileTest("method.append-structured-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferConsume) {
  runFileTest("method.consume-structured-buffer.consume.hlsl");
}
TEST_F(FileTest, ConsumeStructuredBufferGetDimensions) {
  runFileTest("method.consume-structured-buffer.get-dimensions.hlsl");
}

// For ByteAddressBuffer methods
TEST_F(FileTest, ByteAddressBufferLoad) {
  runFileTest("method.byte-address-buffer.load.hlsl");
}
TEST_F(FileTest, ByteAddressBufferStore) {
  runFileTest("method.byte-address-buffer.store.hlsl");
}
TEST_F(FileTest, ByteAddressBufferGetDimensions) {
  runFileTest("method.byte-address-buffer.get-dimensions.hlsl");
}
TEST_F(FileTest, RWByteAddressBufferAtomicMethods) {
  runFileTest("method.rw-byte-address-buffer.atomic.hlsl");
}

// For Buffer/RWBuffer methods
TEST_F(FileTest, BufferLoad) { runFileTest("method.buffer.load.hlsl"); }
TEST_F(FileTest, BufferGetDimensions) {
  runFileTest("method.buffer.get-dimensions.hlsl");
}

// For RWTexture methods
TEST_F(FileTest, RWTextureLoad) { runFileTest("method.rwtexture.load.hlsl"); }
TEST_F(FileTest, RWTextureGetDimensions) {
  runFileTest("method.rwtexture.get-dimensions.hlsl");
}

// For InputPatch and OutputPatch methods
TEST_F(FileTest, InputOutputPatchAccess) {
  runFileTest("method.input-output-patch.access.hlsl");
}

// For intrinsic functions
TEST_F(FileTest, IntrinsicsCountBits) {
  runFileTest("intrinsics.countbits.hlsl");
}
TEST_F(FileTest, IntrinsicsDot) { runFileTest("intrinsics.dot.hlsl"); }
TEST_F(FileTest, IntrinsicsMul) { runFileTest("intrinsics.mul.hlsl"); }
TEST_F(FileTest, IntrinsicsAll) { runFileTest("intrinsics.all.hlsl"); }
TEST_F(FileTest, IntrinsicsAny) { runFileTest("intrinsics.any.hlsl"); }
TEST_F(FileTest, IntrinsicsAsDouble) {
  runFileTest("intrinsics.asdouble.hlsl");
}
TEST_F(FileTest, IntrinsicsAsfloat) { runFileTest("intrinsics.asfloat.hlsl"); }
TEST_F(FileTest, IntrinsicsAsint) { runFileTest("intrinsics.asint.hlsl"); }
TEST_F(FileTest, IntrinsicsAsuint) { runFileTest("intrinsics.asuint.hlsl"); }
TEST_F(FileTest, IntrinsicsRound) { runFileTest("intrinsics.round.hlsl"); }
TEST_F(FileTest, IntrinsicsAbs) { runFileTest("intrinsics.abs.hlsl"); }
TEST_F(FileTest, IntrinsicsCross) { runFileTest("intrinsics.cross.hlsl"); }
TEST_F(FileTest, IntrinsicsCeil) { runFileTest("intrinsics.ceil.hlsl"); }
TEST_F(FileTest, IntrinsicsClamp) { runFileTest("intrinsics.clamp.hlsl"); }
TEST_F(FileTest, IntrinsicsClip) { runFileTest("intrinsics.clip.hlsl"); }
TEST_F(FileTest, IntrinsicsD3DCOLORtoUBYTE4) {
  runFileTest("intrinsics.D3DCOLORtoUBYTE4.hlsl");
}
TEST_F(FileTest, IntrinsicsDegrees) { runFileTest("intrinsics.degrees.hlsl"); }
TEST_F(FileTest, IntrinsicsDistance) {
  runFileTest("intrinsics.distance.hlsl");
}
TEST_F(FileTest, IntrinsicsRadians) { runFileTest("intrinsics.radians.hlsl"); }
TEST_F(FileTest, IntrinsicsDdx) { runFileTest("intrinsics.ddx.hlsl"); }
TEST_F(FileTest, IntrinsicsDdy) { runFileTest("intrinsics.ddy.hlsl"); }
TEST_F(FileTest, IntrinsicsDdxCoarse) {
  runFileTest("intrinsics.ddx-coarse.hlsl");
}
TEST_F(FileTest, IntrinsicsDdyCoarse) {
  runFileTest("intrinsics.ddy-coarse.hlsl");
}
TEST_F(FileTest, IntrinsicsDdxFine) { runFileTest("intrinsics.ddx-fine.hlsl"); }
TEST_F(FileTest, IntrinsicsDdyFine) { runFileTest("intrinsics.ddy-fine.hlsl"); }
TEST_F(FileTest, IntrinsicsDeterminant) {
  runFileTest("intrinsics.determinant.hlsl");
}
TEST_F(FileTest, IntrinsicsDst) { runFileTest("intrinsics.dst.hlsl"); }
TEST_F(FileTest, IntrinsicsExp) { runFileTest("intrinsics.exp.hlsl"); }
TEST_F(FileTest, IntrinsicsExp2) { runFileTest("intrinsics.exp2.hlsl"); }
TEST_F(FileTest, IntrinsicsF16ToF32) {
  runFileTest("intrinsics.f16tof32.hlsl");
}
TEST_F(FileTest, IntrinsicsF32ToF16) {
  runFileTest("intrinsics.f32tof16.hlsl");
}
TEST_F(FileTest, IntrinsicsFaceForward) {
  runFileTest("intrinsics.faceforward.hlsl");
}
TEST_F(FileTest, IntrinsicsFirstBitHigh) {
  runFileTest("intrinsics.firstbithigh.hlsl");
}
TEST_F(FileTest, IntrinsicsFirstBitLow) {
  runFileTest("intrinsics.firstbitlow.hlsl");
}
TEST_F(FileTest, IntrinsicsFloor) { runFileTest("intrinsics.floor.hlsl"); }
TEST_F(FileTest, IntrinsicsFma) { runFileTest("intrinsics.fma.hlsl"); }
TEST_F(FileTest, IntrinsicsFmod) { runFileTest("intrinsics.fmod.hlsl"); }
TEST_F(FileTest, IntrinsicsFrac) { runFileTest("intrinsics.frac.hlsl"); }
TEST_F(FileTest, IntrinsicsFrexp) { runFileTest("intrinsics.frexp.hlsl"); }
TEST_F(FileTest, IntrinsicsFwidth) { runFileTest("intrinsics.fwidth.hlsl"); }
TEST_F(FileTest, IntrinsicsDeviceMemoryBarrier) {
  runFileTest("intrinsics.devicememorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsAllMemoryBarrier) {
  runFileTest("intrinsics.allmemorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsAllMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.allmemorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsDeviceMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.devicememorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsGroupMemoryBarrier) {
  runFileTest("intrinsics.groupmemorybarrier.hlsl");
}
TEST_F(FileTest, IntrinsicsGroupMemoryBarrierWithGroupSync) {
  runFileTest("intrinsics.groupmemorybarrierwithgroupsync.hlsl");
}
TEST_F(FileTest, IntrinsicsIsFinite) {
  runFileTest("intrinsics.isfinite.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsPS) {
  runFileTest("intrinsics.interlocked-methods.ps.hlsl");
}
TEST_F(FileTest, IntrinsicsInterlockedMethodsCS) {
  runFileTest("intrinsics.interlocked-methods.cs.hlsl");
}
TEST_F(FileTest, IntrinsicsIsInf) { runFileTest("intrinsics.isinf.hlsl"); }
TEST_F(FileTest, IntrinsicsIsNan) { runFileTest("intrinsics.isnan.hlsl"); }
TEST_F(FileTest, IntrinsicsLength) { runFileTest("intrinsics.length.hlsl"); }
TEST_F(FileTest, IntrinsicsLdexp) { runFileTest("intrinsics.ldexp.hlsl"); }
TEST_F(FileTest, IntrinsicsLerp) { runFileTest("intrinsics.lerp.hlsl"); }
TEST_F(FileTest, IntrinsicsLog) { runFileTest("intrinsics.log.hlsl"); }
TEST_F(FileTest, IntrinsicsLog10) { runFileTest("intrinsics.log10.hlsl"); }
TEST_F(FileTest, IntrinsicsLog2) { runFileTest("intrinsics.log2.hlsl"); }
TEST_F(FileTest, IntrinsicsMin) { runFileTest("intrinsics.min.hlsl"); }
TEST_F(FileTest, IntrinsicsLit) { runFileTest("intrinsics.lit.hlsl"); }
TEST_F(FileTest, IntrinsicsModf) { runFileTest("intrinsics.modf.hlsl"); }
TEST_F(FileTest, IntrinsicsMad) { runFileTest("intrinsics.mad.hlsl"); }
TEST_F(FileTest, IntrinsicsMax) { runFileTest("intrinsics.max.hlsl"); }
TEST_F(FileTest, IntrinsicsMsad4) { runFileTest("intrinsics.msad4.hlsl"); }
TEST_F(FileTest, IntrinsicsNormalize) {
  runFileTest("intrinsics.normalize.hlsl");
}
TEST_F(FileTest, IntrinsicsPow) { runFileTest("intrinsics.pow.hlsl"); }
TEST_F(FileTest, IntrinsicsRsqrt) { runFileTest("intrinsics.rsqrt.hlsl"); }
TEST_F(FileTest, IntrinsicsFloatSign) {
  runFileTest("intrinsics.floatsign.hlsl");
}
TEST_F(FileTest, IntrinsicsIntSign) { runFileTest("intrinsics.intsign.hlsl"); }
TEST_F(FileTest, IntrinsicsRcp) { runFileTest("intrinsics.rcp.hlsl"); }
TEST_F(FileTest, IntrinsicsReflect) { runFileTest("intrinsics.reflect.hlsl"); }
TEST_F(FileTest, IntrinsicsRefract) { runFileTest("intrinsics.refract.hlsl"); }
TEST_F(FileTest, IntrinsicsReverseBits) {
  runFileTest("intrinsics.reversebits.hlsl");
}
TEST_F(FileTest, IntrinsicsSaturate) {
  runFileTest("intrinsics.saturate.hlsl");
}
TEST_F(FileTest, IntrinsicsSmoothStep) {
  runFileTest("intrinsics.smoothstep.hlsl");
}
TEST_F(FileTest, IntrinsicsStep) { runFileTest("intrinsics.step.hlsl"); }
TEST_F(FileTest, IntrinsicsSqrt) { runFileTest("intrinsics.sqrt.hlsl"); }
TEST_F(FileTest, IntrinsicsTranspose) {
  runFileTest("intrinsics.transpose.hlsl");
}
TEST_F(FileTest, IntrinsicsTrunc) { runFileTest("intrinsics.trunc.hlsl"); }

// For intrinsic trigonometric functions
TEST_F(FileTest, IntrinsicsSin) { runFileTest("intrinsics.sin.hlsl"); }
TEST_F(FileTest, IntrinsicsCos) { runFileTest("intrinsics.cos.hlsl"); }
TEST_F(FileTest, IntrinsicsSinCos) { runFileTest("intrinsics.sincos.hlsl"); }
TEST_F(FileTest, IntrinsicsTan) { runFileTest("intrinsics.tan.hlsl"); }
TEST_F(FileTest, IntrinsicsSinh) { runFileTest("intrinsics.sinh.hlsl"); }
TEST_F(FileTest, IntrinsicsCosh) { runFileTest("intrinsics.cosh.hlsl"); }
TEST_F(FileTest, IntrinsicsTanh) { runFileTest("intrinsics.tanh.hlsl"); }
TEST_F(FileTest, IntrinsicsAsin) { runFileTest("intrinsics.asin.hlsl"); }
TEST_F(FileTest, IntrinsicsAcos) { runFileTest("intrinsics.acos.hlsl"); }
TEST_F(FileTest, IntrinsicsAtan) { runFileTest("intrinsics.atan.hlsl"); }
TEST_F(FileTest, IntrinsicsAtan2) { runFileTest("intrinsics.atan2.hlsl"); }

// Unspported intrinsic functions
TEST_F(FileTest, IntrinsicsAbort) {
  runFileTest("intrinsics.abort.hlsl", Expect::Failure);
}
TEST_F(FileTest, IntrinsicsCheckAccessFullyMapped) {
  runFileTest("intrinsics.check-access-fully-mapped.hlsl");
}
TEST_F(FileTest, IntrinsicsGetRenderTargetSampleCount) {
  runFileTest("intrinsics.get-render-target-sample-count.hlsl",
              Expect::Failure);
}
TEST_F(FileTest, IntrinsicsGetRenderTargetSamplePosition) {
  runFileTest("intrinsics.get-render-target-sample-position.hlsl",
              Expect::Failure);
}

// For attributes
TEST_F(FileTest, AttributeNumThreads) {
  runFileTest("attribute.numthreads.hlsl");
}
TEST_F(FileTest, AttributeMissingNumThreads) {
  runFileTest("attribute.numthreads.missing.hlsl");
}
TEST_F(FileTest, AttributeDomainTri) {
  runFileTest("attribute.domain.tri.hlsl");
}
TEST_F(FileTest, AttributeDomainQuad) {
  runFileTest("attribute.domain.quad.hlsl");
}
TEST_F(FileTest, AttributeDomainIsoline) {
  runFileTest("attribute.domain.isoline.hlsl");
}
TEST_F(FileTest, AttributePartitioningInteger) {
  runFileTest("attribute.partitioning.integer.hlsl");
}
TEST_F(FileTest, AttributePartitioningFractionalEven) {
  runFileTest("attribute.partitioning.fractional-even.hlsl");
}
TEST_F(FileTest, AttributePartitioningFractionalOdd) {
  runFileTest("attribute.partitioning.fractional-odd.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyPoint) {
  runFileTest("attribute.outputtopology.point.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyTriangleCw) {
  runFileTest("attribute.outputtopology.triangle-cw.hlsl");
}
TEST_F(FileTest, AttributeOutputTopologyTriangleCcw) {
  runFileTest("attribute.outputtopology.triangle-ccw.hlsl");
}
TEST_F(FileTest, AttributeOutputControlPoints) {
  runFileTest("attribute.outputcontrolpoints.hlsl");
}
TEST_F(FileTest, AttributeMaxVertexCount) {
  runFileTest("attribute.max-vertex-count.hlsl");
}
TEST_F(FileTest, AttributeInstanceGS) {
  runFileTest("attribute.instance.gs.hlsl");
}
TEST_F(FileTest, AttributeInstanceMissingGS) {
  runFileTest("attribute.instance.missing.gs.hlsl");
}

// For geometry shader primitive types
TEST_F(FileTest, PrimitivePointGS) { runFileTest("primitive.point.gs.hlsl"); }
TEST_F(FileTest, PrimitiveLineGS) { runFileTest("primitive.line.gs.hlsl"); }
TEST_F(FileTest, PrimitiveTriangleGS) {
  runFileTest("primitive.triangle.gs.hlsl");
}
TEST_F(FileTest, PrimitiveLineAdjGS) {
  runFileTest("primitive.lineadj.gs.hlsl");
}
TEST_F(FileTest, PrimitiveTriangleAdjGS) {
  runFileTest("primitive.triangleadj.gs.hlsl");
}
TEST_F(FileTest, PrimitiveErrorGS) {
  runFileTest("primitive.error.gs.hlsl", Expect::Failure);
}

// SPIR-V specific
TEST_F(FileTest, SpirvStorageClass) { runFileTest("spirv.storage-class.hlsl"); }

TEST_F(FileTest, SpirvControlFlowMissingReturn) {
  runFileTest("spirv.cf.ret-missing.hlsl");
}

TEST_F(FileTest, SpirvEntryFunctionWrapper) {
  runFileTest("spirv.entry-function.wrapper.hlsl");
}
TEST_F(FileTest, SpirvEntryFunctionInOut) {
  runFileTest("spirv.entry-function.inout.hlsl");
}

TEST_F(FileTest, SpirvBuiltInHelperInvocation) {
  runFileTest("spirv.builtin.helper-invocation.hlsl");
}
TEST_F(FileTest, SpirvBuiltInHelperInvocationInvalidUsage) {
  runFileTest("spirv.builtin.helper-invocation.invalid.hlsl", Expect::Failure);
}
TEST_F(FileTest, SpirvBuiltInPointSizeInvalidUsage) {
  runFileTest("spirv.builtin.point-size.invalid.hlsl", Expect::Failure);
}

// For shader stage input/output interface
// For semantic SV_Position, SV_ClipDistance, SV_CullDistance
TEST_F(FileTest, SpirvStageIOInterfaceVS) {
  runFileTest("spirv.interface.vs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceHS) {
  runFileTest("spirv.interface.hs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceDS) {
  runFileTest("spirv.interface.ds.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfaceGS) {
  runFileTest("spirv.interface.gs.hlsl");
}
TEST_F(FileTest, SpirvStageIOInterfacePS) {
  runFileTest("spirv.interface.ps.hlsl");
}

TEST_F(FileTest, SpirvInterpolation) {
  runFileTest("spirv.interpolation.hlsl");
}
TEST_F(FileTest, SpirvInterpolationError) {
  runFileTest("spirv.interpolation.error.hlsl", Expect::Failure);
}

TEST_F(FileTest, SpirvLegalizationOpaqueStruct) {
  runFileTest("spirv.legal.opaque-struct.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferUsage) {
  runFileTest("spirv.legal.sbuffer.usage.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferMethods) {
  runFileTest("spirv.legal.sbuffer.methods.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounter) {
  runFileTest("spirv.legal.sbuffer.counter.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounterInStruct) {
  // Tests using struct/class having associated counters
  runFileTest("spirv.legal.sbuffer.counter.struct.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferCounterInMethod) {
  // Tests using methods whose enclosing struct/class having associated counters
  runFileTest("spirv.legal.sbuffer.counter.method.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationStructuredBufferInStruct) {
  runFileTest("spirv.legal.sbuffer.struct.hlsl", Expect::Success,
              /*runValidation=*/true, /*relaxLogicalPointer=*/true);
}
TEST_F(FileTest, SpirvLegalizationConstantBuffer) {
  runFileTest("spirv.legal.cbuffer.hlsl");
}
TEST_F(FileTest, SpirvLegalizationTextureBuffer) {
  runFileTest("spirv.legal.tbuffer.hlsl", Expect::Success,
              // TODO: fix the different type error for OpStore
              /*runValidation=*/false);
}

TEST_F(FileTest, VulkanAttributeErrors) {
  runFileTest("vk.attribute.error.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanAttributeInvalidUsages) {
  runFileTest("vk.attribute.invalid.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanCLOptionIgnoreUnusedResources) {
  runFileTest("vk.cloption.ignore-unused-resources.hlsl");
}

TEST_F(FileTest, VulkanCLOptionInvertYVS) {
  runFileTest("vk.cloption.invert-y.vs.hlsl");
}
TEST_F(FileTest, VulkanCLOptionInvertYDS) {
  runFileTest("vk.cloption.invert-y.ds.hlsl");
}
TEST_F(FileTest, VulkanCLOptionInvertYGS) {
  runFileTest("vk.cloption.invert-y.gs.hlsl");
}

// Vulkan specific
TEST_F(FileTest, VulkanLocation) { runFileTest("vk.location.hlsl"); }
TEST_F(FileTest, VulkanLocationInputExplicitOutputImplicit) {
  runFileTest("vk.location.exp-in.hlsl");
}
TEST_F(FileTest, VulkanLocationInputImplicitOutputExplicit) {
  runFileTest("vk.location.exp-out.hlsl");
}
TEST_F(FileTest, VulkanLocationTooLarge) {
  runFileTest("vk.location.large.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanLocationReassigned) {
  runFileTest("vk.location.reassign.hlsl", Expect::Failure);
}
TEST_F(FileTest, VulkanLocationPartiallyAssigned) {
  runFileTest("vk.location.mixed.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanExplicitBinding) {
  // Resource binding from [[vk::binding()]]
  runFileTest("vk.binding.explicit.hlsl");
}
TEST_F(FileTest, VulkanImplicitBinding) {
  // Resource binding from neither [[vk::binding()]] or :register()
  runFileTest("vk.binding.implicit.hlsl");
}
TEST_F(FileTest, VulkanRegisterBinding) {
  // Resource binding from :register()
  runFileTest("vk.binding.register.hlsl");
}
TEST_F(FileTest, VulkanRegisterBindingShift) {
  // Resource binding from :register() and with shift specified via
  // command line option
  runFileTest("vk.binding.cl.hlsl");
}
TEST_F(FileTest, VulkanExplicitBindingReassigned) {
  runFileTest("vk.binding.explicit.error.hlsl", Expect::Warning);
}
TEST_F(FileTest, VulkanRegisterBindingReassigned) {
  runFileTest("vk.binding.register.error.hlsl", Expect::Warning);
}
TEST_F(FileTest, VulkanRegisterBindingShiftReassigned) {
  runFileTest("vk.binding.cl.error.hlsl", Expect::Warning);
}
TEST_F(FileTest, VulkanStructuredBufferCounter) {
  // [[vk::counter_binding()]] for RWStructuredBuffer, AppendStructuredBuffer,
  // and ConsumeStructuredBuffer
  runFileTest("vk.binding.counter.hlsl");
}

TEST_F(FileTest, VulkanPushConstant) { runFileTest("vk.push-constant.hlsl"); }
TEST_F(FileTest, VulkanPushConstantOffset) {
  runFileTest("vk.push-constant.offset.hlsl");
}
TEST_F(FileTest, VulkanMultiplePushConstant) {
  runFileTest("vk.push-constant.multiple.hlsl", Expect::Failure);
}

TEST_F(FileTest, VulkanLayoutCBufferStd140) {
  runFileTest("vk.layout.cbuffer.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferNestedStd140) {
  runFileTest("vk.layout.cbuffer.nested.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutCBufferNestedEmptyStd140) {
  runFileTest("vk.layout.cbuffer.nested.empty.std140.hlsl");
}
TEST_F(FileTest, VulkanLayoutSBufferStd430) {
  runFileTest("vk.layout.sbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutSBufferNestedStd430) {
  runFileTest("vk.layout.sbuffer.nested.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutAppendSBufferStd430) {
  runFileTest("vk.layout.asbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutConsumeSBufferStd430) {
  runFileTest("vk.layout.csbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutTBufferStd430) {
  runFileTest("vk.layout.tbuffer.std430.hlsl");
}
TEST_F(FileTest, VulkanLayoutTextureBufferStd430) {
  runFileTest("vk.layout.texture-buffer.std430.hlsl");
}

TEST_F(FileTest, VulkanLayoutPushConstantStd430) {
  runFileTest("vk.layout.push-constant.std430.hlsl");
}

TEST_F(FileTest, VulkanLayoutCBufferPackOffset) {
  runFileTest("vk.layout.cbuffer.packoffset.hlsl", Expect::Warning);
}

// HS: for different Patch Constant Functions
TEST_F(FileTest, HullShaderPCFVoid) { runFileTest("hs.pcf.void.hlsl"); }
TEST_F(FileTest, HullShaderPCFTakesInputPatch) {
  runFileTest("hs.pcf.input-patch.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesOutputPatch) {
  runFileTest("hs.pcf.output-patch.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesPrimitiveId) {
  runFileTest("hs.pcf.primitive-id.1.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesPrimitiveIdButMainDoesnt) {
  runFileTest("hs.pcf.primitive-id.2.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesViewId) {
  runFileTest("hs.pcf.view-id.1.hlsl");
}
TEST_F(FileTest, HullShaderPCFTakesViewIdButMainDoesnt) {
  runFileTest("hs.pcf.view-id.2.hlsl");
}
// HS: for the structure of hull shaders
TEST_F(FileTest, HullShaderStructure) { runFileTest("hs.structure.hlsl"); }

// GS: emit vertex and emit primitive
TEST_F(FileTest, GeometryShaderEmit) { runFileTest("gs.emit.hlsl"); }

// CS: groupshared
TEST_F(FileTest, ComputeShaderGroupShared) {
  runFileTest("cs.groupshared.hlsl");
}

} // namespace
