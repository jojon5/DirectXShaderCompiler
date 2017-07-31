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

TEST_F(WholeFileTest, EmptyVoidMain) {
  runWholeFileTest("empty-void-main.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruPixelShader) {
  runWholeFileTest("passthru-ps.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, PassThruVertexShader) {
  runWholeFileTest("passthru-vs.hlsl2spv", /*generateHeader*/ true);
}

TEST_F(WholeFileTest, ConstantPixelShader) {
  runWholeFileTest("constant-ps.hlsl2spv", /*generateHeader*/ true);
}

// === Partial output tests ===

TEST_F(FileTest, ScalarTypes) { runFileTest("type.scalar.hlsl"); }
TEST_F(FileTest, VectorTypes) { runFileTest("type.vector.hlsl"); }

TEST_F(FileTest, ScalarConstants) { runFileTest("constant.scalar.hlsl"); }
TEST_F(FileTest, VectorConstants) { runFileTest("constant.vector.hlsl"); }

TEST_F(FileTest, VariableInitialier) { runFileTest("var.init.hlsl"); }

TEST_F(FileTest, UnaryOpPrefixIncrement) {
  runFileTest("unary-op.prefix-inc.hlsl");
}

TEST_F(FileTest, BinaryOpAssign) { runFileTest("binary-op.assign.hlsl"); }

TEST_F(FileTest, BinaryOpScalarArithmetic) {
  runFileTest("binary-op.arithmetic.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithmetic) {
  runFileTest("binary-op.arithmetic.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithmetic) {
  runFileTest("binary-op.arithmetic.mixed.hlsl");
}

TEST_F(FileTest, BinaryOpScalarArithAssign) {
  runFileTest("binary-op.arith-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorArithAssign) {
  runFileTest("binary-op.arith-assign.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMixedArithAssign) {
  runFileTest("binary-op.arith-assign.mixed.hlsl");
}

TEST_F(FileTest, BinaryOpScalarBitwise) {
  runFileTest("binary-op.bitwise.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwise) {
  runFileTest("binary-op.bitwise.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMixedBitwise) {
  runFileTest("binary-op.bitwise.mixed.hlsl");
}

TEST_F(FileTest, BinaryOpScalarBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMixedBitwiseAssign) {
  runFileTest("binary-op.bitwise-assign.mixed.hlsl");
}

TEST_F(FileTest, BinaryOpScalarComparison) {
  runFileTest("binary-op.comparison.scalar.hlsl");
}
TEST_F(FileTest, BinaryOpVectorComparison) {
  runFileTest("binary-op.comparison.vector.hlsl");
}
TEST_F(FileTest, BinaryOpMixedComparison) {
  runFileTest("binary-op.comparison.mixed.hlsl");
}

TEST_F(FileTest, IfStmtPlainAssign) { runFileTest("if-stmt.plain.hlsl"); }

TEST_F(FileTest, IfStmtNestedIfStmt) { runFileTest("if-stmt.nested.hlsl"); }

TEST_F(FileTest, ForStmtPlainAssign) { runFileTest("for-stmt.plain.hlsl"); }

TEST_F(FileTest, ForStmtNestedForStmt) { runFileTest("for-stmt.nested.hlsl"); }

TEST_F(FileTest, ControlFlowNestedIfForStmt) { runFileTest("cf.if.for.hlsl"); }

TEST_F(FileTest, FunctionCall) { runFileTest("fn.call.hlsl"); }

} // namespace
