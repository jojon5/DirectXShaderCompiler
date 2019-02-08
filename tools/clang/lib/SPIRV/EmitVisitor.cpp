//===--- EmitVisitor.cpp - SPIR-V Emit Visitor Implementation ----*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/EmitVisitor.h"
#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvFunction.h"
#include "clang/SPIRV/SpirvInstruction.h"
#include "clang/SPIRV/SpirvModule.h"
#include "clang/SPIRV/String.h"

namespace clang {
namespace spirv {

void EmitVisitor::initInstruction(spv::Op op) {
  curInst.clear();
  curInst.push_back(static_cast<uint32_t>(op));
}

void EmitVisitor::finalizeInstruction() {
  curInst[0] |= static_cast<uint32_t>(curInst.size()) << 16;
  spirvBinary.insert(spirvBinary.end(), curInst.begin(), curInst.end());
}

void EmitVisitor::encodeString(llvm::StringRef value) {
  const auto &words = string::encodeSPIRVString(value);
  curInst.insert(curInst.end(), words.begin(), words.end());
}

bool EmitVisitor::visit(SpirvModule *m, Phase phase) {
  // No pre or post ops for SpirvModule.
  return true;
}

bool EmitVisitor::visit(SpirvFunction *fn, Phase phase) {
  assert(fn);

  // Before emitting the function
  if (phase == Visitor::Phase::Init) {
    // Emit OpFunction
    initInstruction(spv::Op::OpFunction);
    curInst.push_back(fn->getReturnTypeId());
    curInst.push_back(fn->getResultId());
    curInst.push_back(
        static_cast<uint32_t>(spv::FunctionControlMask::MaskNone));
    curInst.push_back(fn->getFunctionTypeId());
    finalizeInstruction();
  }
  // After emitting the function
  else if (phase == Visitor::Phase::Done) {
    // Emit OpFunctionEnd
    initInstruction(spv::Op::OpFunctionEnd);
    finalizeInstruction();
  }

  return true;
}

bool EmitVisitor::visit(SpirvBasicBlock *bb, Phase phase) {
  assert(bb);

  // Before emitting the basic block.
  if (phase == Visitor::Phase::Init) {
    // Emit OpLabel
    initInstruction(spv::Op::OpLabel);
    curInst.push_back(getNextId());
    finalizeInstruction();
  }
  // After emitting the basic block
  else if (phase == Visitor::Phase::Done) {
    assert(bb->hasTerminator());
  }
  return true;
}

bool EmitVisitor::visit(SpirvCapability *cap) {
  initInstruction(cap->getopcode());
  curInst.push_back(static_cast<uint32_t>(cap->getCapability()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtension *ext) {
  initInstruction(ext->getopcode());
  encodeString(ext->getExtensionName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInstImport *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultId());
  encodeString(inst->getExtendedInstSetName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvMemoryModel *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getAddressingModel()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryModel()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvEntryPoint *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getExecModel()));
  curInst.push_back(static_cast<uint32_t>(inst->getEntryPointId()));
  encodeString(inst->getEntryPointName());
  curInst.insert(curInst.end(), inst->getInterface().begin(),
                 inst->getInterface().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExecutionMode *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getEntryPointId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionMode()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvString *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultId());
  encodeString(inst->getString());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSource *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getSourceLanguage()));
  curInst.push_back(static_cast<uint32_t>(inst->getVersion()));
  if (inst->hasFileId())
    curInst.push_back(inst->getFileId());
  if (!inst->getSource().empty()) {
    // Note: in order to improve performance and avoid multiple copies, we
    // encode this (potentially large) string directly into spirvBinary.
    const auto &words = string::encodeSPIRVString(inst->getSource());
    const auto numWordsInInstr = curInst.size() + words.size();
    curInst[0] |= static_cast<uint32_t>(numWordsInInstr) << 16;
    spirvBinary.insert(spirvBinary.end(), curInst.begin(), curInst.end());
    spirvBinary.insert(spirvBinary.end(), words.begin(), words.end());
  }
  return true;
}

bool EmitVisitor::visit(SpirvName *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(static_cast<uint32_t>(inst->getTarget()));
  if (inst->isForMember())
    curInst.push_back(inst->getMember());
  encodeString(inst->getName());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvModuleProcessed *inst) {
  initInstruction(inst->getopcode());
  encodeString(inst->getProcess());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvDecoration *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getTarget());
  if (inst->isMemberDecoration())
    curInst.push_back(inst->getMemberIndex());
  curInst.push_back(static_cast<uint32_t>(inst->getDecoration()));
  curInst.insert(curInst.end(), inst->getParams().begin(),
                 inst->getParams().end());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvVariable *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getStorageClass()));
  if (inst->hasInitializer())
    curInst.push_back(inst->getInitializer());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvFunctionParameter *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvLoopMerge *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getMergeBlock());
  curInst.push_back(inst->getContinueTarget());
  curInst.push_back(static_cast<uint32_t>(inst->getLoopControlMask()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSelectionMerge *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getMergeBlock());
  curInst.push_back(static_cast<uint32_t>(inst->getSelectionControlMask()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBranch *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getTargetLabel());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBranchConditional *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getCondition());
  curInst.push_back(inst->getTrueLabel());
  curInst.push_back(inst->getFalseLabel());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvKill *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvReturn *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSwitch *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getSelector());
  curInst.push_back(inst->getDefaultLabel());
  for (const auto &target : inst->getTargets()) {
    curInst.push_back(target.first);
    curInst.push_back(target.second);
  }
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvUnreachable *inst) {
  initInstruction(inst->getopcode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvAccessChain *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase());
  for (const auto index : inst->getIndexes())
    curInst.push_back(index);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvAtomic *inst) {
  const auto op = inst->getopcode();
  initInstruction(op);
  if (op != spv::Op::OpAtomicStore && op != spv::Op::OpAtomicFlagClear) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(inst->getResultId());
  }
  curInst.push_back(inst->getPointer());
  curInst.push_back(static_cast<uint32_t>(inst->getScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  if (inst->hasComparator())
    curInst.push_back(static_cast<uint32_t>(inst->getMemorySemanticsUnequal()));
  if (inst->hasValue())
    curInst.push_back(inst->getValue());
  if (inst->hasComparator())
    curInst.push_back(inst->getComparator());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBarrier *inst) {
  initInstruction(inst->getopcode());
  if (inst->isControlBarrier())
    curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemoryScope()));
  curInst.push_back(static_cast<uint32_t>(inst->getMemorySemantics()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getOperand1());
  curInst.push_back(inst->getOperand2());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldExtract *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase());
  curInst.push_back(inst->getOffset());
  curInst.push_back(inst->getCount());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvBitFieldInsert *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getBase());
  curInst.push_back(inst->getInsert());
  curInst.push_back(inst->getOffset());
  curInst.push_back(inst->getCount());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvComposite *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  for (const auto constituent : inst->getConstituents())
    curInst.push_back(constituent);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvCompositeExtract *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getComposite());
  for (const auto constituent : inst->getIndexes())
    curInst.push_back(constituent);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvExtInst *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getInstructionSetId());
  curInst.push_back(inst->getInstruction());
  for (const auto operand : inst->getOperands())
    curInst.push_back(operand);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvFunctionCall *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getFunction());
  for (const auto arg : inst->getArgs())
    curInst.push_back(arg);
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  curInst.push_back(inst->getArg1());
  curInst.push_back(inst->getArg2());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformElect *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvNonUniformUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getExecutionScope()));
  if (inst->hasGroupOp())
    curInst.push_back(static_cast<uint32_t>(inst->getGroupOp()));
  curInst.push_back(inst->getArg());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvImageOp *inst) {
  initInstruction(inst->getopcode());

  if (!inst->isImageWrite()) {
    curInst.push_back(inst->getResultTypeId());
    curInst.push_back(inst->getResultId());
  }

  curInst.push_back(inst->getImage());
  curInst.push_back(inst->getCoordinate());

  if (inst->isImageWrite())
    curInst.push_back(inst->getTexelToWrite());

  if (inst->hasDref())
    curInst.push_back(inst->getDref());
  if (inst->hasComponent())
    curInst.push_back(inst->getComponent());
  if (inst->getImageOperandsMask() != spv::ImageOperandsMask::MaskNone) {
    curInst.push_back(static_cast<uint32_t>(inst->getImageOperandsMask()));
    if (inst->hasBias())
      curInst.push_back(inst->getBias());
    if (inst->hasLod())
      curInst.push_back(inst->getLod());
    if (inst->hasGrad()) {
      curInst.push_back(inst->getGradDx());
      curInst.push_back(inst->getGradDy());
    }
    if (inst->hasConstOffset())
      curInst.push_back(inst->getConstOffset());
    if (inst->hasOffset())
      curInst.push_back(inst->getOffset());
    if (inst->hasConstOffsets())
      curInst.push_back(inst->getConstOffsets());
    if (inst->hasSample())
      curInst.push_back(inst->getSample());
    if (inst->hasMinLod())
      curInst.push_back(inst->getMinLod());
  }
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvImageQuery *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage());
  if (inst->hasCoordinate())
    curInst.push_back(inst->getCoordinate());
  if (inst->hasLod())
    curInst.push_back(inst->getLod());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvImageSparseTexelsResident *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getResidentCode());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvImageTexelPointer *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage());
  curInst.push_back(inst->getCoordinate());
  curInst.push_back(inst->getSample());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvLoad *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getPointer());
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSampledImage *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getImage());
  curInst.push_back(inst->getSampler());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSelect *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getCondition());
  curInst.push_back(inst->getTrueObject());
  curInst.push_back(inst->getFalseObject());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantBinaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(inst->getOperand1());
  curInst.push_back(inst->getOperand2());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvSpecConstantUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(static_cast<uint32_t>(inst->getSpecConstantopcode()));
  curInst.push_back(inst->getOperand());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvStore *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getPointer());
  curInst.push_back(inst->getObject());
  if (inst->hasMemoryAccessSemantics())
    curInst.push_back(static_cast<uint32_t>(inst->getMemoryAccess()));
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvUnaryOp *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getOperand());
  finalizeInstruction();
  return true;
}

bool EmitVisitor::visit(SpirvVectorShuffle *inst) {
  initInstruction(inst->getopcode());
  curInst.push_back(inst->getResultTypeId());
  curInst.push_back(inst->getResultId());
  curInst.push_back(inst->getVec1());
  curInst.push_back(inst->getVec2());
  for (const auto component : inst->getComponents())
    curInst.push_back(component);
  finalizeInstruction();
  return true;
}

} // end namespace spirv
} // end namespace clang
