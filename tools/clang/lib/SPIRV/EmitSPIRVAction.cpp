//===--- EmitSPIRVAction.cpp - EmitSPIRVAction implementation -------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/EmitSPIRVAction.h"
#include "clang/AST/AST.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/Diagnostic.h"
#include "clang/Basic/FileManager.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace {
class SPIRVEmitter : public ASTConsumer,
                     public RecursiveASTVisitor<SPIRVEmitter> {
public:
  explicit SPIRVEmitter(raw_ostream *Out) : OutStream(*Out) {}

  void HandleTranslationUnit(ASTContext &Context) override {
    OutStream << "SPIR-V";
  }

private:
  raw_ostream &OutStream;
};
}

std::unique_ptr<ASTConsumer>
EmitSPIRVAction::CreateASTConsumer(CompilerInstance &CI, StringRef InFile) {
  return llvm::make_unique<SPIRVEmitter>(CI.getOutStream());
}
} // end namespace clang