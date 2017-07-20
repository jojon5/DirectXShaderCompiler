//===- unittests/SPIRV/StructureTest.cpp ------ SPIR-V structures tests ---===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/Structure.h"

#include "SPIRVTestUtils.h"
#include "clang/SPIRV/SPIRVContext.h"

namespace {

using namespace clang::spirv;

using ::testing::ContainerEq;

TEST(Structure, DefaultConstructedBasicBlockIsEmpty) {
  auto bb = BasicBlock();
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, TakeBasicBlockHaveAllContents) {
  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);

  auto bb = BasicBlock(42);
  bb.addInstruction(constructInst(spv::Op::OpReturn, {}));
  bb.take(&ib);

  std::vector<uint32_t> expected;
  appendVector(&expected, constructInst(spv::Op::OpLabel, {42}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, AfterClearBasicBlockIsEmpty) {
  auto bb = BasicBlock(42);
  bb.addInstruction(constructInst(spv::Op::OpNop, {}));
  EXPECT_FALSE(bb.isEmpty());
  bb.clear();
  EXPECT_TRUE(bb.isEmpty());
}

TEST(Structure, DefaultConstructedFunctionIsEmpty) {
  auto f = Function();
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, TakeFunctionHaveAllContents) {
  auto f = Function(1, 2, spv::FunctionControlMask::Inline, 3);
  f.addParameter(1, 42);

  auto bb = llvm::make_unique<BasicBlock>(10);
  bb->addInstruction(constructInst(spv::Op::OpReturn, {}));
  f.addBasicBlock(std::move(bb));

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  f.take(&ib);

  std::vector<uint32_t> expected;
  appendVector(&expected, constructInst(spv::Op::OpFunction, {1, 2, 1, 3}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionParameter, {1, 42}));
  appendVector(&expected, constructInst(spv::Op::OpLabel, {10}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, AfterClearFunctionIsEmpty) {
  auto f = Function(1, 2, spv::FunctionControlMask::Inline, 3);
  f.addParameter(1, 42);
  EXPECT_FALSE(f.isEmpty());
  f.clear();
  EXPECT_TRUE(f.isEmpty());
}

TEST(Structure, DefaultConstructedModuleIsEmpty) {
  auto m = SPIRVModule();
  EXPECT_TRUE(m.isEmpty());
}

TEST(Structure, AfterClearModuleIsEmpty) {
  auto m = SPIRVModule();
  m.setBound(12);
  EXPECT_FALSE(m.isEmpty());
  m.clear();
  EXPECT_TRUE(m.isEmpty());
}

TEST(Structure, TakeModuleHaveAllContents) {
  SPIRVContext context;
  auto m = SPIRVModule();

  // Will fix up the bound later.
  std::vector<uint32_t> expected = getModuleHeader(0);

  m.addCapability(spv::Capability::Shader);
  appendVector(&expected,
               constructInst(spv::Op::OpCapability,
                             {static_cast<uint32_t>(spv::Capability::Shader)}));

  m.addExtension("ext");
  const uint32_t extWord = 'e' | ('x' << 8) | ('t' << 16);
  appendVector(&expected, constructInst(spv::Op::OpExtension, {extWord}));

  const uint32_t extInstSetId = context.takeNextId();
  m.addExtInstSet(extInstSetId, "gl");
  const uint32_t glWord = 'g' | ('l' << 8);
  appendVector(&expected,
               constructInst(spv::Op::OpExtInstImport, {extInstSetId, glWord}));

  m.setAddressingModel(spv::AddressingModel::Logical);
  m.setMemoryModel(spv::MemoryModel::GLSL450);
  appendVector(
      &expected,
      constructInst(spv::Op::OpMemoryModel,
                    {static_cast<uint32_t>(spv::AddressingModel::Logical),
                     static_cast<uint32_t>(spv::MemoryModel::GLSL450)}));

  const uint32_t entryPointId = context.takeNextId();
  m.addEntryPoint(spv::ExecutionModel::Fragment, entryPointId, "main", {42});
  const uint32_t mainWord = 'm' | ('a' << 8) | ('i' << 16) | ('n' << 24);
  appendVector(
      &expected,
      constructInst(spv::Op::OpEntryPoint,
                    {static_cast<uint32_t>(spv::ExecutionModel::Fragment),
                     entryPointId, mainWord, /* addtional null in name */ 0,
                     42}));

  m.addExecutionMode(constructInst(
      spv::Op::OpExecutionMode,
      {entryPointId,
       static_cast<uint32_t>(spv::ExecutionMode::OriginUpperLeft)}));
  appendVector(
      &expected,
      constructInst(spv::Op::OpExecutionMode,
                    {entryPointId, static_cast<uint32_t>(
                                       spv::ExecutionMode::OriginUpperLeft)}));

  // TODO: source code debug information

  m.addDebugName(entryPointId, "main");
  appendVector(&expected, constructInst(spv::Op::OpName,
                                        {entryPointId, mainWord,
                                         /* additional null in name */ 0}));

  m.addDecoration(*Decoration::getRelaxedPrecision(context), entryPointId);

  appendVector(&expected,
               constructInst(
                   spv::Op::OpDecorate,
                   {entryPointId,
                    static_cast<uint32_t>(spv::Decoration::RelaxedPrecision)}));

  const auto *voidType = Type::getVoid(context);
  const uint32_t voidId = context.getResultIdForType(voidType);
  m.addType(voidType, voidId);
  appendVector(&expected, constructInst(spv::Op::OpTypeVoid, {voidId}));

  const auto *funcType = Type::getFunction(context, voidId, {voidId});
  const uint32_t funcTypeId = context.getResultIdForType(funcType);
  m.addType(funcType, funcTypeId);
  appendVector(&expected, constructInst(spv::Op::OpTypeFunction,
                                        {funcTypeId, voidId, voidId}));

  const auto *i32Type = Type::getInt32(context);
  const uint32_t i32Id = context.getResultIdForType(i32Type);
  m.addType(i32Type, i32Id);
  appendVector(&expected, constructInst(spv::Op::OpTypeInt, {i32Id, 32, 1}));

  const uint32_t constantId = context.takeNextId();
  m.addConstant(*i32Type,
                constructInst(spv::Op::OpConstant, {i32Id, constantId, 42}));
  appendVector(&expected,
               constructInst(spv::Op::OpConstant, {i32Id, constantId, 42}));
  // TODO: global variable

  const uint32_t funcId = context.takeNextId();
  auto f = llvm::make_unique<Function>(
      voidId, funcId, spv::FunctionControlMask::MaskNone, funcTypeId);
  const uint32_t bbId = context.takeNextId();
  auto bb = llvm::make_unique<BasicBlock>(bbId);
  bb->addInstruction(constructInst(spv::Op::OpReturn, {}));
  f->addBasicBlock(std::move(bb));
  m.addFunction(std::move(f));
  appendVector(&expected, constructInst(spv::Op::OpFunction,
                                        {voidId, funcId, 0, funcTypeId}));
  appendVector(&expected, constructInst(spv::Op::OpLabel, {bbId}));
  appendVector(&expected, constructInst(spv::Op::OpReturn, {}));
  appendVector(&expected, constructInst(spv::Op::OpFunctionEnd, {}));

  m.setBound(context.getNextId());
  expected[3] = context.getNextId();

  std::vector<uint32_t> result;
  auto ib = constructInstBuilder(result);
  m.take(&ib);

  EXPECT_THAT(result, ContainerEq(expected));
  EXPECT_TRUE(m.isEmpty());
}

} // anonymous namespace