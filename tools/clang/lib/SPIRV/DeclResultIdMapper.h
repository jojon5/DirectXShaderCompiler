//===--- DeclResultIdMapper.h - AST Decl to SPIR-V <result-id> mapper ------==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_SPIRV_DECLRESULTIDMAPPER_H
#define LLVM_CLANG_LIB_SPIRV_DECLRESULTIDMAPPER_H

#include <vector>

#include "dxc/HLSL/DxilSemantic.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilSigPoint.h"
#include "spirv/1.0/spirv.hpp11"
#include "clang/SPIRV/ModuleBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/Optional.h"
#include "llvm/ADT/SmallVector.h"

#include "TypeTranslator.h"

namespace clang {
namespace spirv {

/// \brief The class containing HLSL and SPIR-V information about a Vulkan stage
/// (builtin/input/output) variable.
class StageVar {
public:
  inline StageVar(const hlsl::SigPoint *sig, const hlsl::Semantic *sema,
                  uint32_t type);

  const hlsl::SigPoint *getSigPoint() const { return sigPoint; }
  const hlsl::Semantic *getSemantic() const { return semantic; }

  uint32_t getSpirvTypeId() const { return typeId; }

  uint32_t getSpirvId() const { return valueId; }
  void setSpirvId(uint32_t id) { valueId = id; }

  bool isSpirvBuitin() const { return isBuiltin; }
  void setIsSpirvBuiltin() { isBuiltin = true; }

  spv::StorageClass getStorageClass() const { return storageClass; }
  void setStorageClass(spv::StorageClass sc) { storageClass = sc; }

  bool hasLocation() const { return location.hasValue(); }
  void setLocation(uint32_t loc) { location = llvm::Optional<uint32_t>(loc); }

private:
  /// HLSL SigPoint. It uniquely identifies each set of parameters that may be
  /// input or output for each entry point.
  const hlsl::SigPoint *sigPoint;
  /// HLSL semantic.
  const hlsl::Semantic *semantic;
  /// SPIR-V <type-id>.
  uint32_t typeId;
  /// SPIR-V <result-id>.
  uint32_t valueId;
  /// Indicates whether this stage variable should be a SPIR-V builtin.
  bool isBuiltin;
  /// SPIR-V storage class this stage variable belongs to.
  spv::StorageClass storageClass;
  /// Location assignment if input/output variable.
  llvm::Optional<uint32_t> location;
};

StageVar::StageVar(const hlsl::SigPoint *sig, const hlsl::Semantic *sema,
                   uint32_t type)
    : sigPoint(sig), semantic(sema), typeId(type), valueId(0), isBuiltin(false),
      storageClass(spv::StorageClass::Max), location(llvm::None) {}

/// \brief The class containing mappings from Clang frontend Decls to their
/// corresponding SPIR-V <result-id>s.
///
/// All symbols defined in the AST should be "defined" or registered in this
/// class and have their <result-id>s queried from this class. In the process
/// of defining a Decl, the SPIR-V module builder passed into the constructor
/// will be used to generate all SPIR-V instructions required.
///
/// This class acts as a middle layer to handle the mapping between HLSL
/// semantics and Vulkan stage (builtin/input/output) variables. Such mapping
/// is required because of the semantic differences between DirectX and
/// Vulkan and the essence of HLSL as the front-end language for DirectX.
/// A normal variable attached with some semantic will be translated into a
/// single stage variables if it is of non-struct type. If it is of struct
/// type, the fields with attached semantics will need to be translated into
/// stage variables per Vulkan's requirements.
///
/// In the following class, we call a Decl as *remapped* when it is translated
/// into a stage variable; otherwise, we call it as *normal*. Remapped decls
/// include:
/// * FunctionDecl if the return value is attached with a semantic
/// * ParmVarDecl if the parameter is attached with a semantic
/// * FieldDecl if the field is attached with a semantic.
class DeclResultIdMapper {
public:
  inline DeclResultIdMapper(const hlsl::ShaderModel &stage,
                            ModuleBuilder &builder, DiagnosticsEngine &diag);

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's return value and returns true on success.
  bool createStageVarFromFnReturn(const FunctionDecl *funcDecl);

  /// \brief Creates the stage variables by parsing the semantics attached to
  /// the given function's parameter and returns true on success.
  bool createStageVarFromFnParam(const ParmVarDecl *paramDecl);

  /// \brief Registers a decl's <result-id> without generating any SPIR-V
  /// instruction. The given decl will be treated as normal decl.
  void registerDeclResultId(const NamedDecl *symbol, uint32_t resultId);

public:
  /// The struct containing SPIR-V information of a AST Decl.
  struct DeclSpirvInfo {
    uint32_t resultId;
    spv::StorageClass storageClass;
  };

  /// \brief Returns the SPIR-V information for the given decl.
  /// Returns nullptr if no such decl was previously registered.
  const DeclSpirvInfo *getDeclSpirvInfo(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given decl.
  ///
  /// This method will panic if the given decl is not registered.
  uint32_t getDeclResultId(const NamedDecl *decl) const;

  /// \brief Returns the <result-id> for the given decl if already registered;
  /// otherwise, treats the given decl as a normal decl and returns a newly
  /// assigned <result-id> for it.
  uint32_t getOrRegisterDeclResultId(const NamedDecl *decl);

  /// \brief Returns the <result-id> for the given remapped decl. Returns zero
  /// if it is not a registered remapped decl.
  uint32_t getRemappedDeclResultId(const NamedDecl *decl) const;

  /// Returns the storage class for the given expression. The expression is
  /// expected to be an lvalue. Otherwise this method may panic.
  spv::StorageClass resolveStorageClass(const Expr *expr) const;
  spv::StorageClass resolveStorageClass(const Decl *decl) const;

  /// \brief Returns all defined stage (builtin/input/ouput) variables in this
  /// mapper.
  std::vector<uint32_t> collectStageVars() const;

  /// \brief Decorates all stage input and output variables with proper
  /// location.
  ///
  /// This method will writes the location assignment into the module under
  /// construction.
  void finalizeStageIOLocations();

private:
  /// \brief Wrapper method to create an error message and report it
  /// in the diagnostic engine associated with this consumer.
  template <unsigned N> DiagnosticBuilder emitError(const char (&message)[N]) {
    const auto diagId =
        diags.getCustomDiagID(clang::DiagnosticsEngine::Error, message);
    return diags.Report(diagId);
  }

  /// Returns the type of the given decl. If the given decl is a FunctionDecl,
  /// returns its result type.
  QualType getFnParamOrRetType(const DeclaratorDecl *decl) const;

  /// Creates all the stage variables mapped from semantics on the given decl
  /// and returns true on success.
  ///
  /// Assumes the decl has semantic attached to itself or to its fields.
  bool createStageVars(const DeclaratorDecl *decl, bool forReturnValue);

  /// Creates the SPIR-V variable instruction for the given StageVar and returns
  /// the <result-id>. Also sets whether the StageVar is a SPIR-V builtin and
  /// its storage class accordingly.
  uint32_t createSpirvStageVar(StageVar *);

  /// \brief Returns the stage variable's semantic for the given Decl.
  static llvm::StringRef getStageVarSemantic(const NamedDecl *decl);

private:
  const hlsl::ShaderModel &shaderModel;
  ModuleBuilder &theBuilder;
  TypeTranslator typeTranslator;
  DiagnosticsEngine &diags;

  /// Mapping of all remapped decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, DeclSpirvInfo> remappedDecls;
  /// Mapping of all normal decls to their <result-id>s.
  llvm::DenseMap<const NamedDecl *, DeclSpirvInfo> normalDecls;
  /// Vector of all defined stage variables.
  llvm::SmallVector<StageVar, 8> stageVars;
};

DeclResultIdMapper::DeclResultIdMapper(const hlsl::ShaderModel &model,
                                       ModuleBuilder &builder,
                                       DiagnosticsEngine &diag)
    : shaderModel(model), theBuilder(builder), typeTranslator(builder, diag),
      diags(diag) {}

} // end namespace spirv
} // end namespace clang

#endif