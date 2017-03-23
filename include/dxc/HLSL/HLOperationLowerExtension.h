///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// HLOperationLowerExtension.h                                                //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Functions to lower HL operations coming from HLSL extensions to DXIL      //
// operations.                                                               //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////
#pragma once

#include "dxc/HLSL/HLSLExtensionsCodegenHelper.h"
#include "llvm/ADT/StringRef.h"
#include <string>
#include <unordered_map>

namespace llvm {
  class Value;
  class CallInst;
  class Function;
  class StringRef;
  class Instruction;
}

namespace hlsl {
  class OP;

  // Lowers HLSL extensions from HL operation to DXIL operation.
  class ExtensionLowering {
  public:
    // Strategy used for lowering extensions.
    enum class Strategy {
      Unknown,        // Do not know how to lower. This is an error condition.
      NoTranslation,  // Propagate the call arguments as is down to dxil.
      Replicate,      // Scalarize the vector arguments and replicate the call.
      Pack,           // Convert the vector arguments into structs.
      Resource,       // Convert return value to resource return and explode vectors.
    };

    typedef std::unordered_map<llvm::Instruction *, llvm::Value *> HandleMap;

    // Create the lowering using the given strategy and custom codegen helper.
    ExtensionLowering(llvm::StringRef strategy, HLSLExtensionsCodegenHelper *helper, const HandleMap &handleMap, OP& hlslOp);
    ExtensionLowering(Strategy strategy, HLSLExtensionsCodegenHelper *helper, const HandleMap &handleMap, OP& hlslOp);

    // Translate the HL op call to a DXIL op call.
    // Returns a new value if translation was successful.
    // Returns nullptr if translation failed or made no changes.
    llvm::Value *Translate(llvm::CallInst *CI);
    
    // Translate the strategy string to an enum. The strategy string is
    // added as a custom attribute on the high level extension function.
    // It is translated as follows:
    //  "r" -> Replicate
    //  "n" -> NoTranslation
    //  "c" -> Custom
    static Strategy GetStrategy(llvm::StringRef strategy);

    // Translate the strategy enum into a name. This is the inverse of the
    // GetStrategy() function.
    static llvm::StringRef GetStrategyName(Strategy strategy);

    // Get the name that will be used for the extension function call after
    // lowering.
    std::string GetExtensionName(llvm::CallInst *CI);

  private:
    Strategy m_strategy;
    HLSLExtensionsCodegenHelper *m_helper;
    const HandleMap &m_handleMap;
    OP &m_hlslOp;

    llvm::Value *Unknown(llvm::CallInst *CI);
    llvm::Value *NoTranslation(llvm::CallInst *CI);
    llvm::Value *Replicate(llvm::CallInst *CI);
    llvm::Value *Pack(llvm::CallInst *CI);
    llvm::Value *Resource(llvm::CallInst *CI);
  };
}