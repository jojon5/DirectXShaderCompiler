//===--- DeclResultIdMapper.cpp - DeclResultIdMapper impl --------*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "DeclResultIdMapper.h"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <unordered_map>

#include "dxc/HLSL/DxilConstants.h"
#include "dxc/HLSL/DxilTypeSystem.h"
#include "clang/AST/Expr.h"
#include "clang/AST/HlslTypes.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "llvm/ADT/SmallBitVector.h"
#include "llvm/ADT/StringSet.h"

namespace clang {
namespace spirv {

namespace {

/// \brief Returns the stage variable's register assignment for the given Decl.
const hlsl::RegisterAssignment *getResourceBinding(const NamedDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *reg = dyn_cast<hlsl::RegisterAssignment>(annotation)) {
      return reg;
    }
  }
  return nullptr;
}

/// \brief Returns the resource category for the given type.
ResourceVar::Category getResourceCategory(QualType type) {
  if (TypeTranslator::isTexture(type) || TypeTranslator::isRWTexture(type))
    return ResourceVar::Category::Image;
  if (TypeTranslator::isSampler(type))
    return ResourceVar::Category::Sampler;
  return ResourceVar::Category::Other;
}

/// \brief Returns true if the given declaration has a primitive type qualifier.
/// Returns false otherwise.
inline bool hasGSPrimitiveTypeQualifier(const Decl *decl) {
  return decl->hasAttr<HLSLTriangleAttr>() ||
         decl->hasAttr<HLSLTriangleAdjAttr>() ||
         decl->hasAttr<HLSLPointAttr>() || decl->hasAttr<HLSLLineAttr>() ||
         decl->hasAttr<HLSLLineAdjAttr>();
}

/// \brief Deduces the parameter qualifier for the given decl.
hlsl::DxilParamInputQual deduceParamQual(const DeclaratorDecl *decl,
                                         bool asInput) {
  const auto type = decl->getType();

  if (hlsl::IsHLSLInputPatchType(type))
    return hlsl::DxilParamInputQual::InputPatch;
  if (hlsl::IsHLSLOutputPatchType(type))
    return hlsl::DxilParamInputQual::OutputPatch;
  // TODO: Add support for multiple output streams.
  if (hlsl::IsHLSLStreamOutputType(type))
    return hlsl::DxilParamInputQual::OutStream0;

  // The inputs to the geometry shader that have a primitive type qualifier
  // must use 'InputPrimitive'.
  if (hasGSPrimitiveTypeQualifier(decl))
    return hlsl::DxilParamInputQual::InputPrimitive;

  return asInput ? hlsl::DxilParamInputQual::In : hlsl::DxilParamInputQual::Out;
}

/// \brief Deduces the HLSL SigPoint for the given decl appearing in the given
/// shader model.
const hlsl::SigPoint *deduceSigPoint(const DeclaratorDecl *decl, bool asInput,
                                     const hlsl::ShaderModel::Kind kind,
                                     bool forPCF) {
  return hlsl::SigPoint::GetSigPoint(hlsl::SigPointFromInputQual(
      deduceParamQual(decl, asInput), kind, forPCF));
}

/// Returns the type of the given decl. If the given decl is a FunctionDecl,
/// returns its result type.
inline QualType getTypeOrFnRetType(const DeclaratorDecl *decl) {
  if (const auto *funcDecl = dyn_cast<FunctionDecl>(decl)) {
    return funcDecl->getReturnType();
  }
  return decl->getType();
}
} // anonymous namespace

std::string StageVar::getSemanticStr() const {
  // A special case for zero index, which is equivalent to no index.
  // Use what is in the source code.
  // TODO: this looks like a hack to make the current tests happy.
  // Should consider remove it and fix all tests.
  if (semanticIndex == 0)
    return semanticStr;

  std::ostringstream ss;
  ss << semanticName.str() << semanticIndex;
  return ss.str();
}

DeclResultIdMapper::SemanticInfo
DeclResultIdMapper::getStageVarSemantic(const ValueDecl *decl) {
  for (auto *annotation : decl->getUnusualAnnotations()) {
    if (auto *sema = dyn_cast<hlsl::SemanticDecl>(annotation)) {
      llvm::StringRef semanticStr = sema->SemanticName;
      llvm::StringRef semanticName;
      uint32_t index = 0;
      hlsl::Semantic::DecomposeNameAndIndex(semanticStr, &semanticName, &index);
      const auto *semantic = hlsl::Semantic::GetByName(semanticName);
      return {semanticStr, semantic, semanticName, index, sema->Loc};
    }
  }
  return {};
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t storedValue,
                                              bool forPCF) {
  QualType type = getTypeOrFnRetType(decl);

  // Output stream types (PointStream, LineStream, TriangleStream) are
  // translated as their underlying struct types.
  if (hlsl::IsHLSLStreamOutputType(type))
    type = hlsl::GetHLSLResourceResultType(type);

  const auto *sigPoint =
      deduceSigPoint(decl, /*asInput=*/false, shaderModel.GetKind(), forPCF);

  // HS output variables are created using the other overload. For the rest,
  // none of them should be created as arrays.
  assert(sigPoint->GetKind() != hlsl::DXIL::SigPointKind::HSCPOut);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, decl, /*asInput=*/false, type,
                         /*arraySize=*/0, "out.var", llvm::None, &storedValue,
                         // Write back of stage output variables in GS is
                         // manually controlled by .Append() intrinsic method,
                         // implemented in writeBackOutputStream(). So
                         // noWriteBack should be set to true for GS.
                         shaderModel.IsGS(), &inheritSemantic);
}

bool DeclResultIdMapper::createStageOutputVar(const DeclaratorDecl *decl,
                                              uint32_t arraySize,
                                              uint32_t invocationId,
                                              uint32_t storedValue) {
  assert(shaderModel.IsHS());

  QualType type = getTypeOrFnRetType(decl);

  const auto *sigPoint =
      hlsl::SigPoint::GetSigPoint(hlsl::DXIL::SigPointKind::HSCPOut);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, decl, /*asInput=*/false, type, arraySize,
                         "out.var", invocationId, &storedValue,
                         /*noWriteBack=*/false, &inheritSemantic);
}

bool DeclResultIdMapper::createStageInputVar(const ParmVarDecl *paramDecl,
                                             uint32_t *loadedValue,
                                             bool forPCF) {
  uint32_t arraySize = 0;
  QualType type = paramDecl->getType();

  // Deprive the outermost arrayness for HS/DS/GS and use arraySize
  // to convey that information
  if (hlsl::IsHLSLInputPatchType(type)) {
    arraySize = hlsl::GetHLSLInputPatchCount(type);
    type = hlsl::GetHLSLInputPatchElementType(type);
  } else if (hlsl::IsHLSLOutputPatchType(type)) {
    arraySize = hlsl::GetHLSLOutputPatchCount(type);
    type = hlsl::GetHLSLOutputPatchElementType(type);
  }
  if (hasGSPrimitiveTypeQualifier(paramDecl)) {
    const auto *typeDecl = astContext.getAsConstantArrayType(type);
    arraySize = static_cast<uint32_t>(typeDecl->getSize().getZExtValue());
    type = typeDecl->getElementType();
  }

  const auto *sigPoint = deduceSigPoint(paramDecl, /*asInput=*/true,
                                        shaderModel.GetKind(), forPCF);

  SemanticInfo inheritSemantic = {};

  return createStageVars(sigPoint, paramDecl, /*asInput=*/true, type, arraySize,
                         "in.var", llvm::None, loadedValue,
                         /*noWriteBack=*/false, &inheritSemantic);
}

const DeclResultIdMapper::DeclSpirvInfo *
DeclResultIdMapper::getDeclSpirvInfo(const ValueDecl *decl) const {
  auto it = astDecls.find(decl);
  if (it != astDecls.end())
    return &it->second;

  return nullptr;
}

SpirvEvalInfo DeclResultIdMapper::getDeclResultId(const ValueDecl *decl,
                                                  bool checkRegistered) {
  if (const auto *info = getDeclSpirvInfo(decl))
    if (info->indexInCTBuffer >= 0) {
      // If this is a VarDecl inside a HLSLBufferDecl, we need to do an extra
      // OpAccessChain to get the pointer to the variable since we created
      // a single variable for the whole buffer object.

      const uint32_t varType = typeTranslator.translateType(
          // Should only have VarDecls in a HLSLBufferDecl.
          cast<VarDecl>(decl)->getType(),
          // We need to set decorateLayout here to avoid creating SPIR-V
          // instructions for the current type without decorations.
          info->info.getLayoutRule());

      const uint32_t elemId = theBuilder.createAccessChain(
          theBuilder.getPointerType(varType, info->info.getStorageClass()),
          info->info, {theBuilder.getConstantInt32(info->indexInCTBuffer)});

      return SpirvEvalInfo(elemId)
          .setStorageClass(info->info.getStorageClass())
          .setLayoutRule(info->info.getLayoutRule());
    } else {
      return *info;
    }

  if (checkRegistered) {
    emitFatalError("found unregistered decl", decl->getLocation())
        << decl->getName();
  }

  return 0;
}

uint32_t DeclResultIdMapper::createFnParam(const ParmVarDecl *param) {
  bool isAlias = false;
  auto &info = astDecls[param].info;
  const uint32_t type = getTypeForPotentialAliasVar(param, &isAlias, &info);
  const uint32_t ptrType =
      theBuilder.getPointerType(type, spv::StorageClass::Function);
  const uint32_t id = theBuilder.addFnParam(ptrType, param->getName());
  info.setResultId(id);

  // The counter variable may be created before by forward declaration.
  if (!counterVars.count(param))
    // Create alias counter variable if suitable
    if (isAlias && TypeTranslator::isRWAppendConsumeSBuffer(param->getType()))
      createCounterVar(param, /*isAlias=*/true);

  return id;
}

void DeclResultIdMapper::createFnParamCounterVar(const ParmVarDecl *param) {
  if (counterVars.count(param))
    return;

  if (TypeTranslator::isRWAppendConsumeSBuffer(param->getType()))
    createCounterVar(param, /*isAlias=*/true);
}

uint32_t DeclResultIdMapper::createFnVar(const VarDecl *var,
                                         llvm::Optional<uint32_t> init) {
  bool isAlias = false;
  auto &info = astDecls[var].info;
  const uint32_t type = getTypeForPotentialAliasVar(var, &isAlias, &info);
  const uint32_t id = theBuilder.addFnVar(type, var->getName(), init);
  info.setResultId(id);

  // Create alias counter variable if suitable
  if (isAlias && TypeTranslator::isRWAppendConsumeSBuffer(var->getType()))
    createCounterVar(var, /*isAlias=*/true);

  return id;
}

uint32_t DeclResultIdMapper::createFileVar(const VarDecl *var,
                                           llvm::Optional<uint32_t> init) {
  bool isAlias = false;
  auto &info = astDecls[var].info;
  const uint32_t type = getTypeForPotentialAliasVar(var, &isAlias, &info);
  const uint32_t id = theBuilder.addModuleVar(type, spv::StorageClass::Private,
                                              var->getName(), init);
  info.setResultId(id);
  if (!isAlias)
    info.setStorageClass(spv::StorageClass::Private);

  // Create alias counter variable if suitable
  if (isAlias && TypeTranslator::isRWAppendConsumeSBuffer(var->getType()))
    createCounterVar(var, /*isAlias=*/true);

  return id;
}

uint32_t DeclResultIdMapper::createExternVar(const VarDecl *var) {
  auto storageClass = spv::StorageClass::UniformConstant;
  auto rule = LayoutRule::Void;
  bool isACRWSBuffer = false; // Whether its {Append|Consume|RW}StructuredBuffer

  if (var->getAttr<HLSLGroupSharedAttr>()) {
    // For CS groupshared variables
    storageClass = spv::StorageClass::Workgroup;
  } else if (auto *t = var->getType()->getAs<RecordType>()) {
    const llvm::StringRef typeName = t->getDecl()->getName();

    // These types are all translated into OpTypeStruct with BufferBlock
    // decoration. They should follow standard storage buffer layout,
    // which GLSL std430 rules statisfies.
    if (typeName == "StructuredBuffer" || typeName == "ByteAddressBuffer" ||
        typeName == "RWByteAddressBuffer") {
      storageClass = spv::StorageClass::Uniform;
      rule = LayoutRule::GLSLStd430;
    } else if (typeName == "RWStructuredBuffer" ||
               typeName == "AppendStructuredBuffer" ||
               typeName == "ConsumeStructuredBuffer") {
      storageClass = spv::StorageClass::Uniform;
      rule = LayoutRule::GLSLStd430;
      isACRWSBuffer = true;
    }
  }

  const auto varType = typeTranslator.translateType(var->getType(), rule);
  const uint32_t id = theBuilder.addModuleVar(varType, storageClass,
                                              var->getName(), llvm::None);
  astDecls[var] =
      SpirvEvalInfo(id).setStorageClass(storageClass).setLayoutRule(rule);

  const auto *regAttr = getResourceBinding(var);
  const auto *bindingAttr = var->getAttr<VKBindingAttr>();
  const auto *counterBindingAttr = var->getAttr<VKCounterBindingAttr>();

  resourceVars.emplace_back(id, getResourceCategory(var->getType()), regAttr,
                            bindingAttr, counterBindingAttr);

  if (isACRWSBuffer) {
    // For {Append|Consume|RW}StructuredBuffer, we need to always create another
    // variable for its associated counter.
    createCounterVar(var, /*isAlias=*/false);
  }

  return id;
}

uint32_t DeclResultIdMapper::createVarOfExplicitLayoutStruct(
    const DeclContext *decl, const ContextUsageKind usageKind,
    llvm::StringRef typeName, llvm::StringRef varName) {
  // cbuffers are translated into OpTypeStruct with Block decoration.
  // tbuffers are translated into OpTypeStruct with BufferBlock decoration.
  // PushConstants are translated into OpTypeStruct with Block decoration.
  //
  // Both cbuffers and tbuffers have the SPIR-V Uniform storage class. cbuffers
  // follow GLSL std140 layout rules, and tbuffers follow GLSL std430 layout
  // rules. PushConstants follow GLSL std430 layout rules.

  auto &context = *theBuilder.getSPIRVContext();
  const LayoutRule layoutRule = usageKind == ContextUsageKind::CBuffer
                                    ? LayoutRule::GLSLStd140
                                    : LayoutRule::GLSLStd430;
  const auto *blockDec = usageKind == ContextUsageKind::TBuffer
                             ? Decoration::getBufferBlock(context)
                             : Decoration::getBlock(context);

  auto decorations = typeTranslator.getLayoutDecorations(decl, layoutRule);
  decorations.push_back(blockDec);

  // Collect the type and name for each field
  llvm::SmallVector<uint32_t, 4> fieldTypes;
  llvm::SmallVector<llvm::StringRef, 4> fieldNames;
  uint32_t fieldIndex = 0;
  for (const auto *subDecl : decl->decls()) {
    // Ignore implicit generated struct declarations/constructors/destructors.
    if (subDecl->isImplicit())
      continue;

    // The field can only be FieldDecl (for normal structs) or VarDecl (for
    // HLSLBufferDecls).
    assert(isa<VarDecl>(subDecl) || isa<FieldDecl>(subDecl));
    const auto *declDecl = cast<DeclaratorDecl>(subDecl);
    // All fields are qualified with const. It will affect the debug name.
    // We don't need it here.
    auto varType = declDecl->getType();
    varType.removeLocalConst();

    fieldTypes.push_back(typeTranslator.translateType(
        varType, layoutRule, declDecl->hasAttr<HLSLRowMajorAttr>()));
    fieldNames.push_back(declDecl->getName());

    // tbuffer/TextureBuffers are non-writable SSBOs. OpMemberDecorate
    // NonWritable must be applied to all fields.
    if (usageKind == ContextUsageKind::TBuffer) {
      decorations.push_back(Decoration::getNonWritable(
          *theBuilder.getSPIRVContext(), fieldIndex));
    }
    ++fieldIndex;
  }

  // Get the type for the whole struct
  const uint32_t structType =
      theBuilder.getStructType(fieldTypes, typeName, fieldNames, decorations);

  // Register the <type-id> for this decl
  ctBufferPCTypeIds[decl] = structType;

  const auto sc = usageKind == ContextUsageKind::PushConstant
                      ? spv::StorageClass::PushConstant
                      : spv::StorageClass::Uniform;

  // Create the variable for the whole struct
  return theBuilder.addModuleVar(structType, sc, varName);
}

uint32_t DeclResultIdMapper::createCTBuffer(const HLSLBufferDecl *decl) {
  const auto usageKind =
      decl->isCBuffer() ? ContextUsageKind::CBuffer : ContextUsageKind::TBuffer;
  const std::string structName = "type." + decl->getName().str();
  const std::string varName = "var." + decl->getName().str();
  const uint32_t bufferVar =
      createVarOfExplicitLayoutStruct(decl, usageKind, structName, varName);

  // We still register all VarDecls seperately here. All the VarDecls are
  // mapped to the <result-id> of the buffer object, which means when querying
  // querying the <result-id> for a certain VarDecl, we need to do an extra
  // OpAccessChain.
  int index = 0;
  for (const auto *subDecl : decl->decls()) {
    const auto *varDecl = cast<VarDecl>(subDecl);
    astDecls[varDecl] = {SpirvEvalInfo(bufferVar)
                             .setStorageClass(spv::StorageClass::Uniform)
                             .setLayoutRule(decl->isCBuffer()
                                                ? LayoutRule::GLSLStd140
                                                : LayoutRule::GLSLStd430),
                         index++};
  }
  resourceVars.emplace_back(
      bufferVar, ResourceVar::Category::Other, getResourceBinding(decl),
      decl->getAttr<VKBindingAttr>(), decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::createCTBuffer(const VarDecl *decl) {
  const auto *recordType = decl->getType()->getAs<RecordType>();
  assert(recordType);
  const auto *context = cast<HLSLBufferDecl>(decl->getDeclContext());
  const auto usageKind = context->isCBuffer() ? ContextUsageKind::CBuffer
                                              : ContextUsageKind::TBuffer;

  const char *ctBufferName =
      context->isCBuffer() ? "ConstantBuffer." : "TextureBuffer.";
  const std::string structName = "type." + std::string(ctBufferName) +
                                 recordType->getDecl()->getName().str();
  const uint32_t bufferVar = createVarOfExplicitLayoutStruct(
      recordType->getDecl(), usageKind, structName, decl->getName());

  // We register the VarDecl here.
  astDecls[decl] =
      SpirvEvalInfo(bufferVar)
          .setStorageClass(spv::StorageClass::Uniform)
          .setLayoutRule(context->isCBuffer() ? LayoutRule::GLSLStd140
                                              : LayoutRule::GLSLStd430);
  resourceVars.emplace_back(
      bufferVar, ResourceVar::Category::Other, getResourceBinding(context),
      decl->getAttr<VKBindingAttr>(), decl->getAttr<VKCounterBindingAttr>());

  return bufferVar;
}

uint32_t DeclResultIdMapper::createPushConstant(const VarDecl *decl) {
  const auto *recordType = decl->getType()->getAs<RecordType>();
  assert(recordType);

  const std::string structName =
      "type.PushConstant." + recordType->getDecl()->getName().str();
  const uint32_t var = createVarOfExplicitLayoutStruct(
      recordType->getDecl(), ContextUsageKind::PushConstant, structName,
      decl->getName());

  // Register the VarDecl
  astDecls[decl] = SpirvEvalInfo(var)
                       .setStorageClass(spv::StorageClass::PushConstant)
                       .setLayoutRule(LayoutRule::GLSLStd430);
  // Do not push this variable into resourceVars since it does not need
  // descriptor set.

  return var;
}

uint32_t DeclResultIdMapper::getOrRegisterFnResultId(const FunctionDecl *fn) {
  if (const auto *info = getDeclSpirvInfo(fn))
    return info->info;

  auto &info = astDecls[fn].info;

  bool isAlias = false;
  const uint32_t type = getTypeForPotentialAliasVar(fn, &isAlias, &info);

  const uint32_t id = theBuilder.getSPIRVContext()->takeNextId();
  info.setResultId(id);
  if (isAlias)
    // No need to dereference to get the pointer. Alias function returns
    // themselves are already pointers to values.
    info.setValTypeId(0);
  else
    // All other cases should be normal rvalues.
    info.setRValue();

  // Create alias counter variable if suitable
  if (TypeTranslator::isRWAppendConsumeSBuffer(fn->getReturnType()))
    createCounterVar(fn, /*isAlias=*/true);

  return id;
}

const CounterIdAliasPair &
DeclResultIdMapper::getCounterIdAliasPair(const ValueDecl *decl) {
  const auto counter = counterVars.find(decl);
  assert(counter != counterVars.end());
  return counter->second;
}

void DeclResultIdMapper::createCounterVar(const ValueDecl *decl, bool isAlias) {
  const std::string counterName = "counter.var." + decl->getName().str();
  uint32_t counterType = typeTranslator.getACSBufferCounter();
  // {RW|Append|Consume}StructuredBuffer are all in Uniform storage class.
  // Alias counter variables should be created into the Private storage class.
  const spv::StorageClass sc =
      isAlias ? spv::StorageClass::Private : spv::StorageClass::Uniform;

  if (isAlias) {
    // Apply an extra level of pointer for alias counter variable
    counterType =
        theBuilder.getPointerType(counterType, spv::StorageClass::Uniform);
  }

  const uint32_t counterId =
      theBuilder.addModuleVar(counterType, sc, counterName);

  if (!isAlias) {
    // Non-alias counter variables should be put in to resourceVars so that
    // descriptors can be allocated for them.
    resourceVars.emplace_back(counterId, ResourceVar::Category::Other,
                              getResourceBinding(decl),
                              decl->getAttr<VKBindingAttr>(),
                              decl->getAttr<VKCounterBindingAttr>(), true);
  }

  counterVars[decl] = {counterId, isAlias};
}

uint32_t
DeclResultIdMapper::getCTBufferPushConstantTypeId(const DeclContext *decl) {
  const auto found = ctBufferPCTypeIds.find(decl);
  assert(found != ctBufferPCTypeIds.end());
  return found->second;
}

std::vector<uint32_t> DeclResultIdMapper::collectStageVars() const {
  std::vector<uint32_t> vars;

  for (auto var : glPerVertex.getStageInVars())
    vars.push_back(var);
  for (auto var : glPerVertex.getStageOutVars())
    vars.push_back(var);

  for (const auto &var : stageVars)
    vars.push_back(var.getSpirvId());

  return vars;
}

namespace {
/// A class for managing stage input/output locations to avoid duplicate uses of
/// the same location.
class LocationSet {
public:
  /// Maximum number of locations supported
  // Typically we won't have that many stage input or output variables.
  // Using 64 should be fine here.
  const static uint32_t kMaxLoc = 64;

  LocationSet() : usedLocs(kMaxLoc, false), nextLoc(0) {}

  /// Uses the given location.
  void useLoc(uint32_t loc) { usedLocs.set(loc); }

  /// Uses the next available location.
  uint32_t useNextLoc() {
    while (usedLocs[nextLoc])
      nextLoc++;
    usedLocs.set(nextLoc);
    return nextLoc++;
  }

  /// Returns true if the given location number is already used.
  bool isLocUsed(uint32_t loc) { return usedLocs[loc]; }

private:
  llvm::SmallBitVector usedLocs; ///< All previously used locations
  uint32_t nextLoc;              ///< Next available location
};

/// A class for managing resource bindings to avoid duplicate uses of the same
/// set and binding number.
class BindingSet {
public:
  /// Tries to use the given set and binding number. Returns true if possible,
  /// false otherwise and writes the source location of where the binding number
  /// is used to *usedLoc.
  bool tryToUseBinding(uint32_t binding, uint32_t set,
                       ResourceVar::Category category, SourceLocation tryLoc,
                       SourceLocation *usedLoc) {
    const auto cat = static_cast<uint32_t>(category);
    // Note that we will create the entry for binding in bindings[set] here.
    // But that should not have bad effects since it defaults to zero.
    if ((usedBindings[set][binding] & cat) == 0) {
      usedBindings[set][binding] |= cat;
      whereUsed[set][binding] = tryLoc;
      return true;
    }
    *usedLoc = whereUsed[set][binding];
    return false;
  }

  /// Uses the next avaiable binding number in set 0.
  uint32_t useNextBinding(uint32_t set, ResourceVar::Category category) {
    auto &binding = usedBindings[set];
    auto &next = nextBindings[set];
    while (binding.count(next))
      ++next;
    binding[next] = static_cast<uint32_t>(category);
    return next++;
  }

private:
  ///< set number -> (binding number -> resource category)
  llvm::DenseMap<uint32_t, llvm::DenseMap<uint32_t, uint32_t>> usedBindings;
  ///< set number -> (binding number -> source location)
  llvm::DenseMap<uint32_t, llvm::DenseMap<uint32_t, SourceLocation>> whereUsed;
  ///< set number -> next available binding number
  llvm::DenseMap<uint32_t, uint32_t> nextBindings;
};
} // namespace

bool DeclResultIdMapper::checkSemanticDuplication(bool forInput) {
  llvm::StringSet<> seenSemantics;
  bool success = true;
  for (const auto &var : stageVars) {
    auto s = var.getSemanticStr();

    if (forInput && var.getSigPoint()->IsInput()) {
      if (seenSemantics.count(s)) {
        emitError("input semantic '%0' used more than once", {}) << s;
        success = false;
      }
      seenSemantics.insert(s);
    } else if (!forInput && var.getSigPoint()->IsOutput()) {
      if (seenSemantics.count(s)) {
        emitError("output semantic '%0' used more than once", {}) << s;
        success = false;
      }
      seenSemantics.insert(s);
    }
  }

  return success;
}

bool DeclResultIdMapper::finalizeStageIOLocations(bool forInput) {
  if (!checkSemanticDuplication(forInput))
    return false;

  // Returns false if the given StageVar is an input/output variable without
  // explicit location assignment. Otherwise, returns true.
  const auto locAssigned = [forInput, this](const StageVar &v) {
    if (forInput == isInputStorageClass(v))
      // No need to assign location for builtins. Treat as assigned.
      return v.isSpirvBuitin() || v.getLocationAttr() != nullptr;
    // For the ones we don't care, treat as assigned.
    return true;
  };

  // If we have explicit location specified for all input/output variables,
  // use them instead assign by ourselves.
  if (std::all_of(stageVars.begin(), stageVars.end(), locAssigned)) {
    LocationSet locSet;
    bool noError = true;

    for (const auto &var : stageVars) {
      // Skip those stage variables we are not handling for this call
      if (forInput != isInputStorageClass(var))
        continue;

      // Skip builtins
      if (var.isSpirvBuitin())
        continue;

      const auto *attr = var.getLocationAttr();
      const auto loc = attr->getNumber();
      const auto attrLoc = attr->getLocation(); // Attr source code location

      if (loc >= LocationSet::kMaxLoc) {
        emitError("stage %select{output|input}0 location #%1 too large",
                  attrLoc)
            << forInput << loc;
        return false;
      }

      // Make sure the same location is not assigned more than once
      if (locSet.isLocUsed(loc)) {
        emitError("stage %select{output|input}0 location #%1 already assigned",
                  attrLoc)
            << forInput << loc;
        noError = false;
      }
      locSet.useLoc(loc);

      theBuilder.decorateLocation(var.getSpirvId(), loc);
    }

    return noError;
  }

  std::vector<const StageVar *> vars;
  LocationSet locSet;

  for (const auto &var : stageVars) {
    if (forInput != isInputStorageClass(var))
      continue;

    if (!var.isSpirvBuitin()) {
      if (var.getLocationAttr() != nullptr) {
        // We have checked that not all of the stage variables have explicit
        // location assignment.
        emitError("partial explicit stage %select{output|input}0 location "
                  "assignment via [[vk::location(X)]] unsupported",
                  {})
            << forInput;
        return false;
      }

      // Only SV_Target, SV_Depth, SV_DepthLessEqual, SV_DepthGreaterEqual,
      // SV_StencilRef, SV_Coverage are allowed in the pixel shader.
      // Arbitrary semantics are disallowed in pixel shader.
      if (var.getSemantic() &&
          var.getSemantic()->GetKind() == hlsl::Semantic::Kind::Target) {
        theBuilder.decorateLocation(var.getSpirvId(), var.getSemanticIndex());
        locSet.useLoc(var.getSemanticIndex());
      } else {
        vars.push_back(&var);
      }
    }
  }

  // If alphabetical ordering was requested, sort by semantic string.
  // Since HS includes 2 sets of outputs (patch-constant output and
  // OutputPatch), running into location mismatches between HS and DS is very
  // likely. In order to avoid location mismatches between HS and DS, use
  // alphabetical ordering.
  if (spirvOptions.stageIoOrder == "alpha" ||
      (!forInput && shaderModel.IsHS()) || (forInput && shaderModel.IsDS())) {
    // Sort stage input/output variables alphabetically
    std::sort(vars.begin(), vars.end(),
              [](const StageVar *a, const StageVar *b) {
                return a->getSemanticStr() < b->getSemanticStr();
              });
  }

  for (const auto *var : vars)
    theBuilder.decorateLocation(var->getSpirvId(), locSet.useNextLoc());

  return true;
}

namespace {
/// A class for maintaining the binding number shift requested for descriptor
/// sets.
class BindingShiftMapper {
public:
  explicit BindingShiftMapper(const llvm::SmallVectorImpl<uint32_t> &shifts)
      : masterShift(0) {
    assert(shifts.size() % 2 == 0);
    for (uint32_t i = 0; i < shifts.size(); i += 2)
      perSetShift[shifts[i + 1]] = shifts[i];
  }

  /// Returns the shift amount for the given set.
  uint32_t getShiftForSet(uint32_t set) const {
    const auto found = perSetShift.find(set);
    if (found != perSetShift.end())
      return found->second;
    return masterShift;
  }

private:
  uint32_t masterShift; /// Shift amount applies to all sets.
  llvm::DenseMap<uint32_t, uint32_t> perSetShift;
};
} // namespace

bool DeclResultIdMapper::decorateResourceBindings() {
  // For normal resource, we support 3 approaches of setting binding numbers:
  // - m1: [[vk::binding(...)]]
  // - m2: :register(...)
  // - m3: None
  //
  // For associated counters, we support 2 approaches:
  // - c1: [[vk::counter_binding(...)]
  // - c2: None
  //
  // In combination, we need to handle 9 cases:
  // - 3 cases for nomral resoures (m1, m2, m3)
  // - 6 cases for associated counters (mX * cY)
  //
  // In the following order:
  // - m1, mX * c1
  // - m2
  // - m3, mX * c2

  BindingSet bindingSet;

  // Decorates the given varId of the given category with set number
  // setNo, binding number bindingNo. Emits warning if overlap.
  const auto tryToDecorate = [this, &bindingSet](
                                 const uint32_t varId, const uint32_t setNo,
                                 const uint32_t bindingNo,
                                 const ResourceVar::Category cat,
                                 SourceLocation loc) {
    SourceLocation prevUseLoc;
    if (!bindingSet.tryToUseBinding(bindingNo, setNo, cat, loc, &prevUseLoc)) {
      emitWarning("resource binding #%0 in descriptor set #%1 already assigned",
                  loc)
          << bindingNo << setNo;
      emitNote("binding number previously assigned here", prevUseLoc);
    }
    theBuilder.decorateDSetBinding(varId, setNo, bindingNo);
  };

  for (const auto &var : resourceVars) {
    if (var.isCounter()) {
      if (const auto *vkCBinding = var.getCounterBinding()) {
        // Process mX * c1
        uint32_t set = 0;
        if (const auto *vkBinding = var.getBinding())
          set = vkBinding->getSet();
        if (const auto *reg = var.getRegister())
          set = reg->RegisterSpace;

        tryToDecorate(var.getSpirvId(), set, vkCBinding->getBinding(),
                      var.getCategory(), vkCBinding->getLocation());
      }
    } else {
      if (const auto *vkBinding = var.getBinding()) {
        // Process m1
        tryToDecorate(var.getSpirvId(), vkBinding->getSet(),
                      vkBinding->getBinding(), var.getCategory(),
                      vkBinding->getLocation());
      }
    }
  }

  BindingShiftMapper bShiftMapper(spirvOptions.bShift);
  BindingShiftMapper tShiftMapper(spirvOptions.tShift);
  BindingShiftMapper sShiftMapper(spirvOptions.sShift);
  BindingShiftMapper uShiftMapper(spirvOptions.uShift);

  // Process m2
  for (const auto &var : resourceVars)
    if (!var.isCounter() && !var.getBinding())
      if (const auto *reg = var.getRegister()) {
        const uint32_t set = reg->RegisterSpace;
        uint32_t binding = reg->RegisterNumber;
        switch (reg->RegisterType) {
        case 'b':
          binding += bShiftMapper.getShiftForSet(set);
          break;
        case 't':
          binding += tShiftMapper.getShiftForSet(set);
          break;
        case 's':
          binding += sShiftMapper.getShiftForSet(set);
          break;
        case 'u':
          binding += uShiftMapper.getShiftForSet(set);
          break;
        case 'c':
          // For setting packing offset. Does not affect binding.
          break;
        default:
          llvm_unreachable("unknown register type found");
        }

        tryToDecorate(var.getSpirvId(), set, binding, var.getCategory(),
                      reg->Loc);
      }

  for (const auto &var : resourceVars) {
    const auto cat = var.getCategory();
    if (var.isCounter()) {
      if (!var.getCounterBinding()) {
        // Process mX * c2
        uint32_t set = 0;
        if (const auto *vkBinding = var.getBinding())
          set = vkBinding->getSet();
        else if (const auto *reg = var.getRegister())
          set = reg->RegisterSpace;

        theBuilder.decorateDSetBinding(var.getSpirvId(), set,
                                       bindingSet.useNextBinding(set, cat));
      }
    } else if (!var.getBinding() && !var.getRegister()) {
      // Process m3
      theBuilder.decorateDSetBinding(var.getSpirvId(), 0,
                                     bindingSet.useNextBinding(0, cat));
    }
  }

  return true;
}

bool DeclResultIdMapper::createStageVars(
    const hlsl::SigPoint *sigPoint, const DeclaratorDecl *decl, bool asInput,
    QualType type, uint32_t arraySize, const llvm::StringRef namePrefix,
    llvm::Optional<uint32_t> invocationId, uint32_t *value, bool noWriteBack,
    SemanticInfo *inheritSemantic) {
  // invocationId should only be used for handling HS per-vertex output.
  if (invocationId.hasValue()) {
    assert(shaderModel.IsHS() && arraySize != 0 && !asInput);
  }

  assert(inheritSemantic);

  if (type->isVoidType()) {
    // No stage variables will be created for void type.
    return true;
  }

  uint32_t typeId = typeTranslator.translateType(type);

  // We have several cases regarding HLSL semantics to handle here:
  // * If the currrent decl inherits a semantic from some enclosing entity,
  //   use the inherited semantic no matter whether there is a semantic
  //   attached to the current decl.
  // * If there is no semantic to inherit,
  //   * If the current decl is a struct,
  //     * If the current decl has a semantic, all its members inhert this
  //       decl's semantic, with the index sequentially increasing;
  //     * If the current decl does not have a semantic, all its members
  //       should have semantics attached;
  //   * If the current decl is not a struct, it should have semantic attached.

  auto thisSemantic = getStageVarSemantic(decl);

  // Which semantic we should use for this decl
  auto *semanticToUse = &thisSemantic;

  // Enclosing semantics override internal ones
  if (inheritSemantic->isValid()) {
    if (thisSemantic.isValid()) {
      emitWarning(
          "internal semantic '%0' overridden by enclosing semantic '%1'",
          thisSemantic.loc)
          << thisSemantic.str << inheritSemantic->str;
    }
    semanticToUse = inheritSemantic;
  }

  if (semanticToUse->isValid() &&
      // Structs with attached semantics will be handled later.
      !type->isStructureType()) {
    // Found semantic attached directly to this Decl. This means we need to
    // map this decl to a single stage variable.

    const auto semanticKind = semanticToUse->semantic->GetKind();

    // Error out when the given semantic is invalid in this shader model
    if (hlsl::SigPoint::GetInterpretation(semanticKind, sigPoint->GetKind(),
                                          shaderModel.GetMajor(),
                                          shaderModel.GetMinor()) ==
        hlsl::DXIL::SemanticInterpretationKind::NA) {
      emitError("invalid usage of semantic '%0' in shader profile %1",
                decl->getLocation())
          << semanticToUse->str << shaderModel.GetName();
      return false;
    }

    if (!validateVKBuiltins(decl, sigPoint))
      return false;

    const auto *builtinAttr = decl->getAttr<VKBuiltInAttr>();

    // For VS/HS/DS, the PointSize builtin is handled in gl_PerVertex.
    // For GSVIn also in gl_PerVertex; for GSOut, it's a stand-alone
    // variable handled below.
    if (builtinAttr && builtinAttr->getBuiltIn() == "PointSize" &&
        glPerVertex.tryToAccessPointSize(sigPoint->GetKind(), invocationId,
                                         value, noWriteBack))
      return true;

    // Special handling of certain mappings between HLSL semantics and
    // SPIR-V builtins:
    // * SV_Position/SV_CullDistance/SV_ClipDistance should be grouped into the
    //   gl_PerVertex struct in vertex processing stages.
    // * SV_DomainLocation can refer to a float2, whereas TessCoord is a float3.
    //   To ensure SPIR-V validity, we must create a float3 and  extract a
    //   float2 from it before passing it to the main function.
    // * SV_TessFactor is an array of size 2 for isoline patch, array of size 3
    //   for tri patch, and array of size 4 for quad patch, but it must always
    //   be an array of size 4 in SPIR-V for Vulkan.
    // * SV_InsideTessFactor is a single float for tri patch, and an array of
    //   size 2 for a quad patch, but it must always be an array of size 2 in
    //   SPIR-V for Vulkan.
    // * SV_Coverage is an uint value, but the builtin it corresponds to,
    //   SampleMask, must be an array of integers.

    if (glPerVertex.tryToAccess(sigPoint->GetKind(), semanticKind,
                                semanticToUse->index, invocationId, value,
                                noWriteBack))
      return true;

    const uint32_t srcTypeId = typeId; // Variable type in source code

    switch (semanticKind) {
    case hlsl::Semantic::Kind::DomainLocation:
      typeId = theBuilder.getVecType(theBuilder.getFloat32Type(), 3);
      break;
    case hlsl::Semantic::Kind::TessFactor:
      typeId = theBuilder.getArrayType(theBuilder.getFloat32Type(),
                                       theBuilder.getConstantUint32(4));
      break;
    case hlsl::Semantic::Kind::InsideTessFactor:
      typeId = theBuilder.getArrayType(theBuilder.getFloat32Type(),
                                       theBuilder.getConstantUint32(2));
      break;
    case hlsl::Semantic::Kind::Coverage:
      typeId = theBuilder.getArrayType(typeId, theBuilder.getConstantUint32(1));
      break;
    case hlsl::Semantic::Kind::Barycentrics:
      typeId = theBuilder.getVecType(theBuilder.getFloat32Type(), 2);
      break;
    }

    // Handle the extra arrayness
    const uint32_t elementTypeId = typeId; // Array element's type
    if (arraySize != 0)
      typeId = theBuilder.getArrayType(typeId,
                                       theBuilder.getConstantUint32(arraySize));

    StageVar stageVar(sigPoint, semanticToUse->str, semanticToUse->semantic,
                      semanticToUse->name, semanticToUse->index, builtinAttr,
                      typeId);
    const auto name = namePrefix.str() + "." + stageVar.getSemanticStr();
    const uint32_t varId =
        createSpirvStageVar(&stageVar, decl, name, semanticToUse->loc);

    if (varId == 0)
      return false;

    stageVar.setSpirvId(varId);
    stageVar.setLocationAttr(decl->getAttr<VKLocationAttr>());
    stageVars.push_back(stageVar);
    stageVarIds[decl] = varId;

    // Mark that we have used one index for this semantic
    ++semanticToUse->index;

    // TODO: the following may not be correct?
    if (sigPoint->GetSignatureKind() ==
        hlsl::DXIL::SignatureKind::PatchConstant)
      theBuilder.decorate(varId, spv::Decoration::Patch);

    // Decorate with interpolation modes for pixel shader input variables
    if (shaderModel.IsPS() && sigPoint->IsInput() &&
        // BaryCoord*AMD buitins already encode the interpolation mode.
        semanticKind != hlsl::Semantic::Kind::Barycentrics)
      decoratePSInterpolationMode(decl, type, varId);

    if (asInput) {
      *value = theBuilder.createLoad(typeId, varId);

      // Fix ups for corner cases

      // Special handling of SV_TessFactor DS patch constant input.
      // TessLevelOuter is always an array of size 4 in SPIR-V, but
      // SV_TessFactor could be an array of size 2, 3, or 4 in HLSL. Only the
      // relevant indexes must be loaded.
      if (semanticKind == hlsl::Semantic::Kind::TessFactor &&
          hlsl::GetArraySize(type) != 4) {
        llvm::SmallVector<uint32_t, 4> components;
        const auto f32TypeId = theBuilder.getFloat32Type();
        const auto tessFactorSize = hlsl::GetArraySize(type);
        const auto arrType = theBuilder.getArrayType(
            f32TypeId, theBuilder.getConstantUint32(tessFactorSize));
        for (uint32_t i = 0; i < tessFactorSize; ++i)
          components.push_back(
              theBuilder.createCompositeExtract(f32TypeId, *value, {i}));
        *value = theBuilder.createCompositeConstruct(arrType, components);
      }
      // Special handling of SV_InsideTessFactor DS patch constant input.
      // TessLevelInner is always an array of size 2 in SPIR-V, but
      // SV_InsideTessFactor could be an array of size 1 (scalar) or size 2 in
      // HLSL. If SV_InsideTessFactor is a scalar, only extract index 0 of
      // TessLevelInner.
      else if (semanticKind == hlsl::Semantic::Kind::InsideTessFactor &&
               !type->isArrayType()) {
        *value = theBuilder.createCompositeExtract(theBuilder.getFloat32Type(),
                                                   *value, {0});
      }
      // SV_DomainLocation can refer to a float2 or a float3, whereas TessCoord
      // is always a float3. To ensure SPIR-V validity, a float3 stage variable
      // is created, and we must extract a float2 from it before passing it to
      // the main function.
      else if (semanticKind == hlsl::Semantic::Kind::DomainLocation &&
               hlsl::GetHLSLVecSize(type) != 3) {
        const auto domainLocSize = hlsl::GetHLSLVecSize(type);
        *value = theBuilder.createVectorShuffle(
            theBuilder.getVecType(theBuilder.getFloat32Type(), domainLocSize),
            *value, *value, {0, 1});
      }
      // Special handling of SV_Coverage, which is an uint value. We need to
      // read SampleMask and extract its first element.
      else if (semanticKind == hlsl::Semantic::Kind::Coverage) {
        *value = theBuilder.createCompositeExtract(srcTypeId, *value, {0});
      }
      // Special handling of SV_Barycentrics, which is a float3, but the
      // underlying stage input variable is a float2 (only provides the first
      // two components). Calculate the third element.
      else if (semanticKind == hlsl::Semantic::Kind::Barycentrics) {
        const auto f32Type = theBuilder.getFloat32Type();
        const auto x = theBuilder.createCompositeExtract(f32Type, *value, {0});
        const auto y = theBuilder.createCompositeExtract(f32Type, *value, {1});
        const auto xy =
            theBuilder.createBinaryOp(spv::Op::OpFAdd, f32Type, x, y);
        const auto z = theBuilder.createBinaryOp(
            spv::Op::OpFSub, f32Type, theBuilder.getConstantFloat32(1), xy);
        const auto v3f32Type = theBuilder.getVecType(f32Type, 3);

        *value = theBuilder.createCompositeConstruct(v3f32Type, {x, y, z});
      }
    } else {
      if (noWriteBack)
        return true;

      uint32_t ptr = varId;

      // Special handling of SV_TessFactor HS patch constant output.
      // TessLevelOuter is always an array of size 4 in SPIR-V, but
      // SV_TessFactor could be an array of size 2, 3, or 4 in HLSL. Only the
      // relevant indexes must be written to.
      if (semanticKind == hlsl::Semantic::Kind::TessFactor &&
          hlsl::GetArraySize(type) != 4) {
        const auto f32TypeId = theBuilder.getFloat32Type();
        const auto tessFactorSize = hlsl::GetArraySize(type);
        for (uint32_t i = 0; i < tessFactorSize; ++i) {
          const uint32_t ptrType =
              theBuilder.getPointerType(f32TypeId, spv::StorageClass::Output);
          ptr = theBuilder.createAccessChain(ptrType, varId,
                                             theBuilder.getConstantUint32(i));
          theBuilder.createStore(
              ptr, theBuilder.createCompositeExtract(f32TypeId, *value, i));
        }
      }
      // Special handling of SV_InsideTessFactor HS patch constant output.
      // TessLevelInner is always an array of size 2 in SPIR-V, but
      // SV_InsideTessFactor could be an array of size 1 (scalar) or size 2 in
      // HLSL. If SV_InsideTessFactor is a scalar, only write to index 0 of
      // TessLevelInner.
      else if (semanticKind == hlsl::Semantic::Kind::InsideTessFactor &&
               !type->isArrayType()) {
        ptr = theBuilder.createAccessChain(
            theBuilder.getPointerType(theBuilder.getFloat32Type(),
                                      spv::StorageClass::Output),
            varId, theBuilder.getConstantUint32(0));
        theBuilder.createStore(ptr, *value);
      }
      // Special handling of SV_Coverage, which is an unit value. We need to
      // write it to the first element in the SampleMask builtin.
      else if (semanticKind == hlsl::Semantic::Kind::Coverage) {
        ptr = theBuilder.createAccessChain(
            theBuilder.getPointerType(srcTypeId, spv::StorageClass::Output),
            varId, theBuilder.getConstantUint32(0));
        theBuilder.createStore(ptr, *value);
      }
      // Special handling of HS ouput, for which we write to only one
      // element in the per-vertex data array: the one indexed by
      // SV_ControlPointID.
      else if (invocationId.hasValue()) {
        const uint32_t ptrType =
            theBuilder.getPointerType(elementTypeId, spv::StorageClass::Output);
        const uint32_t index = invocationId.getValue();
        ptr = theBuilder.createAccessChain(ptrType, varId, index);
        theBuilder.createStore(ptr, *value);
      }
      // For all normal cases
      else {
        theBuilder.createStore(ptr, *value);
      }
    }

    return true;
  }

  // If the decl itself doesn't have semantic string attached and there is no
  // one to inherit, it should be a struct having all its fields with semantic
  // strings.
  if (!semanticToUse->isValid() && !type->isStructureType()) {
    emitError("semantic string missing for shader %select{output|input}0 "
              "variable '%1'",
              decl->getLocation())
        << asInput << decl->getName();
    return false;
  }

  const auto *structDecl = type->getAs<RecordType>()->getDecl();

  if (asInput) {
    // If this decl translates into multiple stage input variables, we need to
    // load their values into a composite.
    llvm::SmallVector<uint32_t, 4> subValues;

    for (const auto *field : structDecl->fields()) {
      uint32_t subValue = 0;
      if (!createStageVars(sigPoint, field, asInput, field->getType(),
                           arraySize, namePrefix, invocationId, &subValue,
                           noWriteBack, semanticToUse))
        return false;
      subValues.push_back(subValue);
    }

    if (arraySize == 0) {
      *value = theBuilder.createCompositeConstruct(typeId, subValues);
      return true;
    }

    // Handle the extra level of arrayness.

    // We need to return an array of structs. But we get arrays of fields
    // from visiting all fields. So now we need to extract all the elements
    // at the same index of each field arrays and compose a new struct out
    // of them.
    const uint32_t structType = typeTranslator.translateType(type);
    const uint32_t arrayType = theBuilder.getArrayType(
        structType, theBuilder.getConstantUint32(arraySize));
    llvm::SmallVector<uint32_t, 16> arrayElements;

    for (uint32_t arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex) {
      llvm::SmallVector<uint32_t, 8> fields;

      // Extract the element at index arrayIndex from each field
      for (const auto *field : structDecl->fields()) {
        const uint32_t fieldType =
            typeTranslator.translateType(field->getType());
        fields.push_back(theBuilder.createCompositeExtract(
            fieldType, subValues[field->getFieldIndex()], {arrayIndex}));
      }
      // Compose a new struct out of them
      arrayElements.push_back(
          theBuilder.createCompositeConstruct(structType, fields));
    }

    *value = theBuilder.createCompositeConstruct(arrayType, arrayElements);
  } else {
    // Unlike reading, which may require us to read stand-alone builtins and
    // stage input variables and compose an array of structs out of them,
    // it happens that we don't need to write an array of structs in a bunch
    // for all shader stages:
    //
    // * VS: output is a single struct, without extra arrayness
    // * HS: output is an array of structs, with extra arrayness,
    //       but we only write to the struct at the InvocationID index
    // * DS: output is a single struct, without extra arrayness
    // * GS: output is controlled by OpEmitVertex, one vertex per time
    //
    // The interesting shader stage is HS. We need the InvocationID to write
    // out the value to the correct array element.
    for (const auto *field : structDecl->fields()) {
      const uint32_t fieldType = typeTranslator.translateType(field->getType());
      uint32_t subValue = 0;
      if (!noWriteBack)
        subValue = theBuilder.createCompositeExtract(fieldType, *value,
                                                     {field->getFieldIndex()});

      if (!createStageVars(sigPoint, field, asInput, field->getType(),
                           arraySize, namePrefix, invocationId, &subValue,
                           noWriteBack, semanticToUse))
        return false;
    }
  }

  return true;
}

bool DeclResultIdMapper::writeBackOutputStream(const ValueDecl *decl,
                                               uint32_t value) {
  assert(shaderModel.IsGS()); // Only for GS use

  QualType type = decl->getType();

  if (hlsl::IsHLSLStreamOutputType(type))
    type = hlsl::GetHLSLResourceResultType(type);
  if (hasGSPrimitiveTypeQualifier(decl))
    type = astContext.getAsConstantArrayType(type)->getElementType();

  auto semanticInfo = getStageVarSemantic(decl);

  if (semanticInfo.isValid()) {
    // Found semantic attached directly to this Decl. Write the value for this
    // Decl to the corresponding stage output variable.

    const uint32_t srcTypeId = typeTranslator.translateType(type);

    // Handle SV_Position, SV_ClipDistance, and SV_CullDistance
    if (glPerVertex.tryToAccess(
            hlsl::DXIL::SigPointKind::GSOut, semanticInfo.semantic->GetKind(),
            semanticInfo.index, llvm::None, &value, /*noWriteBack=*/false))
      return true;

    // Query the <result-id> for the stage output variable generated out
    // of this decl.
    const auto found = stageVarIds.find(decl);

    // We should have recorded its stage output variable previously.
    assert(found != stageVarIds.end());

    theBuilder.createStore(found->second, value);
    return true;
  }

  // If the decl itself doesn't have semantic string attached, it should be
  // a struct having all its fields with semantic strings.
  if (!type->isStructureType()) {
    emitError("semantic string missing for shader output variable '%0'",
              decl->getLocation())
        << decl->getName();
    return false;
  }

  const auto *structDecl = type->getAs<RecordType>()->getDecl();

  // Write out each field
  for (const auto *field : structDecl->fields()) {
    const uint32_t fieldType = typeTranslator.translateType(field->getType());
    const uint32_t subValue = theBuilder.createCompositeExtract(
        fieldType, value, {field->getFieldIndex()});

    if (!writeBackOutputStream(field, subValue))
      return false;
  }

  return true;
}

void DeclResultIdMapper::decoratePSInterpolationMode(const DeclaratorDecl *decl,
                                                     QualType type,
                                                     uint32_t varId) {
  const QualType elemType = typeTranslator.getElementType(type);

  if (elemType->isBooleanType() || elemType->isIntegerType()) {
    // TODO: Probably we can call hlsl::ValidateSignatureElement() for the
    // following check.
    if (decl->getAttr<HLSLLinearAttr>() || decl->getAttr<HLSLCentroidAttr>() ||
        decl->getAttr<HLSLNoPerspectiveAttr>() ||
        decl->getAttr<HLSLSampleAttr>()) {
      emitError("only nointerpolation mode allowed for integer input "
                "parameters in pixel shader",
                decl->getLocation());
    } else {
      theBuilder.decorate(varId, spv::Decoration::Flat);
    }
  } else {
    // Do nothing for HLSLLinearAttr since its the default
    // Attributes can be used together. So cannot use else if.
    if (decl->getAttr<HLSLCentroidAttr>())
      theBuilder.decorate(varId, spv::Decoration::Centroid);
    if (decl->getAttr<HLSLNoInterpolationAttr>())
      theBuilder.decorate(varId, spv::Decoration::Flat);
    if (decl->getAttr<HLSLNoPerspectiveAttr>())
      theBuilder.decorate(varId, spv::Decoration::NoPerspective);
    if (decl->getAttr<HLSLSampleAttr>()) {
      theBuilder.requireCapability(spv::Capability::SampleRateShading);
      theBuilder.decorate(varId, spv::Decoration::Sample);
    }
  }
}

uint32_t DeclResultIdMapper::createSpirvStageVar(StageVar *stageVar,
                                                 const DeclaratorDecl *decl,
                                                 const llvm::StringRef name,
                                                 SourceLocation srcLoc) {
  using spv::BuiltIn;

  const auto sigPoint = stageVar->getSigPoint();
  const auto semanticKind = stageVar->getSemantic()->GetKind();
  const auto sigPointKind = sigPoint->GetKind();
  const uint32_t type = stageVar->getSpirvTypeId();

  spv::StorageClass sc = getStorageClassForSigPoint(sigPoint);
  if (sc == spv::StorageClass::Max)
    return 0;
  stageVar->setStorageClass(sc);

  // [[vk::builtin(...)]] takes precedence.
  if (const auto *builtinAttr = stageVar->getBuiltInAttr()) {
    const auto spvBuiltIn =
        llvm::StringSwitch<BuiltIn>(builtinAttr->getBuiltIn())
            .Case("PointSize", BuiltIn::PointSize)
            .Case("HelperInvocation", BuiltIn::HelperInvocation)
            .Default(BuiltIn::Max);

    assert(spvBuiltIn != BuiltIn::Max); // The frontend should guarantee this.

    return theBuilder.addStageBuiltinVar(type, sc, spvBuiltIn);
  }

  // The following translation assumes that semantic validity in the current
  // shader model is already checked, so it only covers valid SigPoints for
  // each semantic.
  switch (semanticKind) {
  // According to DXIL spec, the Position SV can be used by all SigPoints
  // other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the Position BuiltIn can only be used
  // by VSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::Position: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
      llvm_unreachable("should be handled in gl_PerVertex struct");
    case hlsl::SigPoint::Kind::GSOut:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Position);
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragCoord);
    default:
      llvm_unreachable("invalid usage of SV_Position sneaked in");
    }
  }
  // According to DXIL spec, the VertexID SV can only be used by VSIn.
  // According to Vulkan spec, the VertexIndex BuiltIn can only be used by
  // VSIn.
  case hlsl::Semantic::Kind::VertexID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::VertexIndex);
  }
  // According to DXIL spec, the InstanceID SV can be used by VSIn, VSOut,
  // HSCPIn, HSCPOut, DSCPIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the InstanceIndex BuitIn can only be used by
  // VSIn.
  case hlsl::Semantic::Kind::InstanceID: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InstanceIndex);
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    default:
      llvm_unreachable("invalid usage of SV_InstanceID sneaked in");
    }
  }
  // According to DXIL spec, the Depth{|GreaterEqual|LessEqual} SV can only be
  // used by PSOut.
  // According to Vulkan spec, the FragDepth BuiltIn can only be used by PSOut.
  case hlsl::Semantic::Kind::Depth:
  case hlsl::Semantic::Kind::DepthGreaterEqual:
  case hlsl::Semantic::Kind::DepthLessEqual: {
    stageVar->setIsSpirvBuiltin();
    if (semanticKind == hlsl::Semantic::Kind::DepthGreaterEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthGreater, {});
    else if (semanticKind == hlsl::Semantic::Kind::DepthLessEqual)
      theBuilder.addExecutionMode(entryFunctionId,
                                  spv::ExecutionMode::DepthLess, {});
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragDepth);
  }
  // According to DXIL spec, the ClipDistance/CullDistance SV can be used by all
  // SigPoints other than PCIn, HSIn, GSIn, PSOut, CSIn.
  // According to Vulkan spec, the ClipDistance/CullDistance BuiltIn can only
  // be
  // used by VSOut, HS/DS/GS In/Out.
  case hlsl::Semantic::Kind::ClipDistance:
  case hlsl::Semantic::Kind::CullDistance: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      llvm_unreachable("should be handled in gl_PerVertex struct");
    default:
      llvm_unreachable(
          "invalid usage of SV_ClipDistance/SV_CullDistance sneaked in");
    }
  }
  // According to DXIL spec, the IsFrontFace SV can only be used by GSOut and
  // PSIn.
  // According to Vulkan spec, the FrontFacing BuitIn can only be used in PSIn.
  case hlsl::Semantic::Kind::IsFrontFace: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::GSOut:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::PSIn:
      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FrontFacing);
    default:
      llvm_unreachable("invalid usage of SV_IsFrontFace sneaked in");
    }
  }
  // According to DXIL spec, the Target SV can only be used by PSOut.
  // There is no corresponding builtin decoration in SPIR-V. So generate normal
  // Vulkan stage input/output variables.
  case hlsl::Semantic::Kind::Target:
  // An arbitrary semantic is defined by users. Generate normal Vulkan stage
  // input/output variables.
  case hlsl::Semantic::Kind::Arbitrary: {
    return theBuilder.addStageIOVar(type, sc, name.str());
    // TODO: patch constant function in hull shader
  }
  // According to DXIL spec, the DispatchThreadID SV can only be used by CSIn.
  // According to Vulkan spec, the GlobalInvocationId can only be used in CSIn.
  case hlsl::Semantic::Kind::DispatchThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::GlobalInvocationId);
  }
  // According to DXIL spec, the GroupID SV can only be used by CSIn.
  // According to Vulkan spec, the WorkgroupId can only be used in CSIn.
  case hlsl::Semantic::Kind::GroupID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::WorkgroupId);
  }
  // According to DXIL spec, the GroupThreadID SV can only be used by CSIn.
  // According to Vulkan spec, the LocalInvocationId can only be used in CSIn.
  case hlsl::Semantic::Kind::GroupThreadID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::LocalInvocationId);
  }
  // According to DXIL spec, the GroupIndex SV can only be used by CSIn.
  // According to Vulkan spec, the LocalInvocationIndex can only be used in
  // CSIn.
  case hlsl::Semantic::Kind::GroupIndex: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc,
                                         BuiltIn::LocalInvocationIndex);
  }
  // According to DXIL spec, the OutputControlID SV can only be used by HSIn.
  // According to Vulkan spec, the InvocationId BuiltIn can only be used in
  // HS/GS In.
  case hlsl::Semantic::Kind::OutputControlPointID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InvocationId);
  }
  // According to DXIL spec, the PrimitiveID SV can only be used by PCIn, HSIn,
  // DSIn, GSIn, GSOut, and PSIn.
  // According to Vulkan spec, the PrimitiveId BuiltIn can only be used in
  // HS/DS/PS In, GS In/Out.
  case hlsl::Semantic::Kind::PrimitiveID: {
    // PrimitiveId requires either Tessellation or Geometry capability.
    // Need to require one for PSIn.
    if (sigPointKind == hlsl::SigPoint::Kind::PSIn)
      theBuilder.requireCapability(spv::Capability::Geometry);

    // Translate to PrimitiveId BuiltIn for all valid SigPoints.
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::PrimitiveId);
  }
  // According to DXIL spec, the TessFactor SV can only be used by PCOut and
  // DSIn.
  // According to Vulkan spec, the TessLevelOuter BuiltIn can only be used in
  // PCOut and DSIn.
  case hlsl::Semantic::Kind::TessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelOuter);
  }
  // According to DXIL spec, the InsideTessFactor SV can only be used by PCOut
  // and DSIn.
  // According to Vulkan spec, the TessLevelInner BuiltIn can only be used in
  // PCOut and DSIn.
  case hlsl::Semantic::Kind::InsideTessFactor: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessLevelInner);
  }
  // According to DXIL spec, the DomainLocation SV can only be used by DSIn.
  // According to Vulkan spec, the TessCoord BuiltIn can only be used in DSIn.
  case hlsl::Semantic::Kind::DomainLocation: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::TessCoord);
  }
  // According to DXIL spec, the GSInstanceID SV can only be used by GSIn.
  // According to Vulkan spec, the InvocationId BuiltIn can only be used in
  // HS/GS In.
  case hlsl::Semantic::Kind::GSInstanceID: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::InvocationId);
  }
  // According to DXIL spec, the SampleIndex SV can only be used by PSIn.
  // According to Vulkan spec, the SampleId BuiltIn can only be used in PSIn.
  case hlsl::Semantic::Kind::SampleIndex: {
    theBuilder.requireCapability(spv::Capability::SampleRateShading);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::SampleId);
  }
  // According to DXIL spec, the StencilRef SV can only be used by PSOut.
  case hlsl::Semantic::Kind::StencilRef: {
    theBuilder.addExtension("SPV_EXT_shader_stencil_export");
    theBuilder.requireCapability(spv::Capability::StencilExportEXT);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::FragStencilRefEXT);
  }
  // According to DXIL spec, the ViewID SV can only be used by PSIn.
  case hlsl::Semantic::Kind::Barycentrics: {
    theBuilder.addExtension("SPV_AMD_shader_explicit_vertex_parameter");
    stageVar->setIsSpirvBuiltin();

    // Selecting the correct builtin according to interpolation mode
    auto bi = BuiltIn::Max;
    if (decl->hasAttr<HLSLNoPerspectiveAttr>()) {
      if (decl->hasAttr<HLSLCentroidAttr>()) {
        bi = BuiltIn::BaryCoordNoPerspCentroidAMD;
      } else if (decl->hasAttr<HLSLSampleAttr>()) {
        bi = BuiltIn::BaryCoordNoPerspSampleAMD;
      } else {
        bi = BuiltIn::BaryCoordNoPerspAMD;
      }
    } else {
      if (decl->hasAttr<HLSLCentroidAttr>()) {
        bi = BuiltIn::BaryCoordSmoothCentroidAMD;
      } else if (decl->hasAttr<HLSLSampleAttr>()) {
        bi = BuiltIn::BaryCoordSmoothSampleAMD;
      } else {
        bi = BuiltIn::BaryCoordSmoothAMD;
      }
    }

    return theBuilder.addStageBuiltinVar(type, sc, bi);
  }
  // According to DXIL spec, the RenderTargetArrayIndex SV can only be used by
  // VSIn, VSOut, HSCPIn, HSCPOut, DSIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the Layer BuiltIn can only be used in GSOut and
  // PSIn.
  case hlsl::Semantic::Kind::RenderTargetArrayIndex: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      theBuilder.requireCapability(spv::Capability::Geometry);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::Layer);
    default:
      llvm_unreachable("invalid usage of SV_RenderTargetArrayIndex sneaked in");
    }
  }
  // According to DXIL spec, the ViewportArrayIndex SV can only be used by
  // VSIn, VSOut, HSCPIn, HSCPOut, DSIn, DSOut, GSVIn, GSOut, PSIn.
  // According to Vulkan spec, the ViewportIndex BuiltIn can only be used in
  // GSOut and PSIn.
  case hlsl::Semantic::Kind::ViewPortArrayIndex: {
    switch (sigPointKind) {
    case hlsl::SigPoint::Kind::VSIn:
    case hlsl::SigPoint::Kind::VSOut:
    case hlsl::SigPoint::Kind::HSCPIn:
    case hlsl::SigPoint::Kind::HSCPOut:
    case hlsl::SigPoint::Kind::PCOut:
    case hlsl::SigPoint::Kind::DSIn:
    case hlsl::SigPoint::Kind::DSCPIn:
    case hlsl::SigPoint::Kind::DSOut:
    case hlsl::SigPoint::Kind::GSVIn:
      return theBuilder.addStageIOVar(type, sc, name.str());
    case hlsl::SigPoint::Kind::GSOut:
    case hlsl::SigPoint::Kind::PSIn:
      theBuilder.requireCapability(spv::Capability::MultiViewport);

      stageVar->setIsSpirvBuiltin();
      return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::ViewportIndex);
    default:
      llvm_unreachable("invalid usage of SV_ViewportArrayIndex sneaked in");
    }
  }
  // According to DXIL spec, the Coverage SV can only be used by PSIn and PSOut.
  // According to Vulkan spec, the SampleMask BuiltIn can only be used in
  // PSIn and PSOut.
  case hlsl::Semantic::Kind::Coverage: {
    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::SampleMask);
  }
  // According to DXIL spec, the ViewID SV can only be used by VSIn, PCIn,
  // HSIn, DSIn, GSIn, PSIn.
  // According to Vulkan spec, the ViewIndex BuiltIn can only be used in
  // VS/HS/DS/GS/PS input.
  case hlsl::Semantic::Kind::ViewID: {
    theBuilder.addExtension("SPV_KHR_multiview");
    theBuilder.requireCapability(spv::Capability::MultiView);

    stageVar->setIsSpirvBuiltin();
    return theBuilder.addStageBuiltinVar(type, sc, BuiltIn::ViewIndex);
  }
  case hlsl::Semantic::Kind::InnerCoverage: {
    emitError("no equivalent for semantic SV_InnerCoverage in Vulkan", srcLoc);
    return 0;
  }
  default:
    emitError("semantic %0 unimplemented", srcLoc)
        << stageVar->getSemantic()->GetName();
    break;
  }

  return 0;
}

bool DeclResultIdMapper::validateVKBuiltins(const DeclaratorDecl *decl,
                                            const hlsl::SigPoint *sigPoint) {
  bool success = true;

  if (const auto *builtinAttr = decl->getAttr<VKBuiltInAttr>()) {
    const auto declType = getTypeOrFnRetType(decl);
    const auto loc = builtinAttr->getLocation();

    if (decl->hasAttr<VKLocationAttr>()) {
      emitError("cannot use vk::builtin and vk::location together", loc);
      success = false;
    }

    const llvm::StringRef builtin = builtinAttr->getBuiltIn();

    if (builtin == "HelperInvocation") {
      if (!declType->isBooleanType()) {
        emitError("HelperInvocation builtin must be of boolean type", loc);
        success = false;
      }

      if (sigPoint->GetKind() != hlsl::SigPoint::Kind::PSIn) {
        emitError(
            "HelperInvocation builtin can only be used as pixel shader input",
            loc);
        success = false;
      }
    } else if (builtin == "PointSize") {
      if (!declType->isFloatingType()) {
        emitError("PointSize builtin must be of float type", loc);
        success = false;
      }

      switch (sigPoint->GetKind()) {
      case hlsl::SigPoint::Kind::VSOut:
      case hlsl::SigPoint::Kind::HSCPIn:
      case hlsl::SigPoint::Kind::HSCPOut:
      case hlsl::SigPoint::Kind::DSCPIn:
      case hlsl::SigPoint::Kind::DSOut:
      case hlsl::SigPoint::Kind::GSVIn:
      case hlsl::SigPoint::Kind::GSOut:
      case hlsl::SigPoint::Kind::PSIn:
        break;
      default:
        emitError("PointSize builtin cannot be used as %0", loc)
            << sigPoint->GetName();
        success = false;
      }
    }
  }

  return success;
}

spv::StorageClass
DeclResultIdMapper::getStorageClassForSigPoint(const hlsl::SigPoint *sigPoint) {
  // This translation is done based on the HLSL reference (see docs/dxil.rst).
  const auto sigPointKind = sigPoint->GetKind();
  const auto signatureKind = sigPoint->GetSignatureKind();
  spv::StorageClass sc = spv::StorageClass::Max;
  switch (signatureKind) {
  case hlsl::DXIL::SignatureKind::Input:
    sc = spv::StorageClass::Input;
    break;
  case hlsl::DXIL::SignatureKind::Output:
    sc = spv::StorageClass::Output;
    break;
  case hlsl::DXIL::SignatureKind::Invalid: {
    // There are some special cases in HLSL (See docs/dxil.rst):
    // SignatureKind is "invalid" for PCIn, HSIn, GSIn, and CSIn.
    switch (sigPointKind) {
    case hlsl::DXIL::SigPointKind::PCIn:
    case hlsl::DXIL::SigPointKind::HSIn:
    case hlsl::DXIL::SigPointKind::GSIn:
    case hlsl::DXIL::SigPointKind::CSIn:
      sc = spv::StorageClass::Input;
      break;
    default:
      llvm_unreachable("Found invalid SigPoint kind for semantic");
    }
    break;
  }
  case hlsl::DXIL::SignatureKind::PatchConstant: {
    // There are some special cases in HLSL (See docs/dxil.rst):
    // SignatureKind is "PatchConstant" for PCOut and DSIn.
    switch (sigPointKind) {
    case hlsl::DXIL::SigPointKind::PCOut:
      // Patch Constant Output (Output of Hull which is passed to Domain).
      sc = spv::StorageClass::Output;
      break;
    case hlsl::DXIL::SigPointKind::DSIn:
      // Domain Shader regular input - Patch Constant data plus system values.
      sc = spv::StorageClass::Input;
      break;
    default:
      llvm_unreachable("Found invalid SigPoint kind for semantic");
    }
    break;
  }
  default:
    llvm_unreachable("Found invalid SigPoint kind for semantic");
  }
  return sc;
}

uint32_t DeclResultIdMapper::getTypeForPotentialAliasVar(
    const DeclaratorDecl *decl, bool *shouldBeAlias, SpirvEvalInfo *info) {
  if (const auto *varDecl = dyn_cast<VarDecl>(decl)) {
    // This method is only intended to be used to create SPIR-V variables in the
    // Function or Private storage class.
    assert(!varDecl->isExceptionVariable() || varDecl->isStaticDataMember());
  }

  const QualType type = getTypeOrFnRetType(decl);
  // Whether we should generate this decl as an alias variable.
  bool genAlias = false;
  // All texture/structured/byte buffers use GLSL std430 rules.
  LayoutRule rule = LayoutRule::GLSLStd430;

  if (const auto *buffer = dyn_cast<HLSLBufferDecl>(decl->getDeclContext())) {
    // For ConstantBuffer and TextureBuffer
    if (buffer->isConstantBufferView())
      genAlias = true;
    // ConstantBuffer uses GLSL std140 rules.
    // TODO: do we actually want to include constant/texture buffers
    // in this method?
    if (buffer->isCBuffer())
      rule = LayoutRule::GLSLStd140;
  } else if (TypeTranslator::isAKindOfStructuredOrByteBuffer(type)) {
    genAlias = true;
  }

  if (shouldBeAlias)
    *shouldBeAlias = genAlias;

  if (genAlias) {
    needsLegalization = true;

    const uint32_t valType = typeTranslator.translateType(type, rule);
    // All constant/texture/structured/byte buffers are in the Uniform
    // storage class.
    const auto ptrType =
        theBuilder.getPointerType(valType, spv::StorageClass::Uniform);

    if (info)
      info->setStorageClass(spv::StorageClass::Uniform)
          .setLayoutRule(rule)
          .setValTypeId(ptrType);

    return ptrType;
  }

  return typeTranslator.translateType(type);
}

} // end namespace spirv
} // end namespace clang
