//===--- SpirvBasicBlock.cpp - SPIR-V Basic Block Implementation -*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/SpirvBasicBlock.h"
#include "clang/SPIRV/SpirvVisitor.h"

namespace clang {
namespace spirv {

SpirvBasicBlock::SpirvBasicBlock(uint32_t id, llvm::StringRef name)
    : labelId(id), labelName(name), mergeTarget(nullptr),
      continueTarget(nullptr) {}

bool SpirvBasicBlock::hasTerminator() const {
  return !instructions.empty() && isa<SpirvTerminator>(instructions.back());
}

bool SpirvBasicBlock::invokeVisitor(Visitor *visitor) {
  if (!visitor->visit(this, Visitor::Phase::Init))
    return false;

  for (auto *inst : instructions)
    if (!inst->invokeVisitor(visitor))
      return false;

  if (!visitor->visit(this, Visitor::Phase::Done))
    return false;

  return true;
}

} // end namespace spirv
} // end namespace clang
