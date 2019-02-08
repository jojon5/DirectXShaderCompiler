//===-- EmitVisitor.h - Emit Visitor ----------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_EMITVISITOR_H
#define LLVM_CLANG_SPIRV_EMITVISITOR_H

#include "clang/SPIRV/SPIRVContext.h"
#include "clang/SPIRV/SpirvVisitor.h"
#include "llvm/ADT/DenseMap.h"

#include <functional>

namespace clang {
namespace spirv {

class SpirvFunction;
class SpirvBasicBlock;
class SpirvType;

class EmitTypeHandler {
public:
  struct DecorationInfo {
    DecorationInfo(spv::Decoration decor, llvm::ArrayRef<uint32_t> params = {},
                   llvm::Optional<uint32_t> index = llvm::None)
        : decoration(decor), decorationParams(params.begin(), params.end()),
          memberIndex(index) {}

    bool operator==(const DecorationInfo &other) const {
      return decoration == other.decoration &&
             decorationParams == other.decorationParams &&
             memberIndex.hasValue() == other.memberIndex.hasValue() &&
             (!memberIndex.hasValue() ||
              memberIndex.getValue() == other.memberIndex.getValue());
    }

    spv::Decoration decoration;
    llvm::SmallVector<uint32_t, 4> decorationParams;
    llvm::Optional<uint32_t> memberIndex;
  };

  using DecorationList = llvm::SmallVector<DecorationInfo, 4>;

  // Provides DenseMapInfo for SpirvLayoutRule so that we can use it as key to
  // DenseMap.
  struct DecorationSetDenseMapInfo {
    static inline DecorationList getEmptyKey() {
      return {DecorationInfo(spv::Decoration::Max)};
    }
    static inline DecorationList getTombstoneKey() {
      return {DecorationInfo(spv::Decoration::Max)};
    }
    static unsigned getHashValue(const DecorationList &Val) {
      unsigned hashValue = Val.size();
      for (auto &decorationInfo : Val)
        hashValue += static_cast<unsigned>(decorationInfo.decoration);

      return hashValue;
    }

    static bool isEqual(const DecorationList &LHS, const DecorationList &RHS) {
      // Must have the same number of decorations.
      if (LHS.size() != RHS.size())
        return false;

      // Order of decorations does not matter.
      for (auto &dec : LHS) {
        auto found = std::find_if(
            RHS.begin(), RHS.end(),
            [&dec](const DecorationInfo &otherDec) { return dec == otherDec; });

        if (found == RHS.end())
          return false;
      }

      return true;
    }
  };

public:
  EmitTypeHandler(ASTContext &astCtx, SpirvContext &spvContext,
                  std::vector<uint32_t> *debugVec,
                  std::vector<uint32_t> *decVec,
                  std::vector<uint32_t> *typesVec,
                  const std::function<uint32_t()> &takeNextIdFn)
      : astContext(astCtx), context(spvContext), debugBinary(debugVec),
        annotationsBinary(decVec), typeConstantBinary(typesVec),
        takeNextIdFunction(takeNextIdFn) {
    assert(decVec);
    assert(typesVec);
  }

  // Disable copy constructor/assignment.
  EmitTypeHandler(const EmitTypeHandler &) = delete;
  EmitTypeHandler &operator=(const EmitTypeHandler &) = delete;

  // Emits the instruction for the given type into the typeConstantBinary and
  // returns the result-id for the type. If the type has already been emitted,
  // it only returns its result-id.
  //
  // If any names are associated with the type (or its members in case of
  // structs), the OpName/OpMemberNames will also be emitted.
  //
  // If any decorations apply to the type, it also emits the decoration
  // instructions into the annotationsBinary.
  uint32_t emitType(const SpirvType *, SpirvLayoutRule,
                    llvm::Optional<bool> isRowMajor = llvm::None);

  // Emits an OpConstant instruction with uint32 type and returns its result-id.
  // If such constant has already been emitted, just returns its resutl-id.
  // Modifies the curTypeInst. Do not call in the middle of construction of
  // another instruction.
  uint32_t getOrCreateConstantUint32(uint32_t value);

private:
  void initTypeInstruction(spv::Op op);
  void finalizeTypeInstruction();

  // Figures out the decorations that apply to the given type with the given
  // layout rule, and populates the given decoration set.
  void getDecorationsForType(const SpirvType *type, SpirvLayoutRule rule,
                             llvm::Optional<bool> isRowMajor,
                             DecorationList *decorations);

  // Returns the result-id for the given type and decorations. If a type with
  // the same decorations have already been used, it returns the existing
  // result-id. If not, creates a new result-id for such type and returns it.
  uint32_t getResultIdForType(const SpirvType *, const DecorationList &,
                              bool *alreadyExists);

  // Emits OpDecorate (or OpMemberDecorate if memberIndex is non-zero)
  // targetting the given type. Uses the given decoration kind and its
  // parameters.
  void emitDecoration(uint32_t typeResultId, spv::Decoration,
                      llvm::ArrayRef<uint32_t> decorationParams,
                      llvm::Optional<uint32_t> memberIndex = llvm::None);

  // Emits an OpName (if memberIndex is not provided) or OpMemberName (if
  // memberIndex is provided) for the given target result-id.
  void emitNameForType(llvm::StringRef name, uint32_t targetTypeId,
                       llvm::Optional<uint32_t> memberIndex = llvm::None);

  // There is no guarantee that an instruction or a function or a basic block
  // has been assigned result-id. This method returns the result-id for the
  // given object. If a result-id has not been assigned yet, it'll assign
  // one and return it.
  template <class T> uint32_t getOrAssignResultId(T *obj) {
    if (!obj->getResultId()) {
      obj->setResultId(takeNextIdFunction());
    }
    return obj->getResultId();
  }

  // ---- Methods associated with layout calculations ----

  // If the given type is a matrix type inside a struct, its majorness should
  // also be passed to this method in order to determine the correct alignment.
  std::pair<uint32_t, uint32_t>
  getAlignmentAndSize(const SpirvType *type, SpirvLayoutRule rule,
                      uint32_t *stride,
                      llvm::Optional<bool> isRowMajorStructMember = llvm::None);

  void alignUsingHLSLRelaxedLayout(const SpirvType *fieldType,
                                   uint32_t fieldSize, uint32_t fieldAlignment,
                                   uint32_t *currentOffset);

  // Adds the layout decorations for the given type and layout rule to the given
  // vector of decorations.
  void getLayoutDecorations(const StructType *, SpirvLayoutRule,
                            DecorationList *);

private:
  /// Emits error to the diagnostic engine associated with this visitor.
  template <unsigned N>
  DiagnosticBuilder emitError(const char (&message)[N],
                              SourceLocation loc = {}) {
    const auto diagId = astContext.getDiagnostics().getCustomDiagID(
        clang::DiagnosticsEngine::Error, message);
    return astContext.getDiagnostics().Report(loc, diagId);
  }

private:
  ASTContext &astContext;
  SpirvContext &context;
  std::vector<uint32_t> curTypeInst;
  std::vector<uint32_t> curDecorationInst;
  std::vector<uint32_t> *debugBinary;
  std::vector<uint32_t> *annotationsBinary;
  std::vector<uint32_t> *typeConstantBinary;
  std::function<uint32_t()> takeNextIdFunction;

  // The array type requires the result-id of an OpConstant for its length. In
  // order to avoid duplicate OpConstant instructions, we keep a map of constant
  // uint value to the result-id of the OpConstant for that value.
  llvm::DenseMap<uint32_t, uint32_t> UintConstantValueToResultIdMap;

  // emittedTypes is a map that caches the result-id of types with a given list
  // of decorations in order to avoid emitting an identical type multiple times.
  using DecorationSetToTypeIdMap =
      llvm::DenseMap<DecorationList, uint32_t, DecorationSetDenseMapInfo>;
  llvm::DenseMap<const SpirvType *, DecorationSetToTypeIdMap> emittedTypes;
};

/// \breif The visitor class that emits the SPIR-V words from the in-memory
/// representation.
class EmitVisitor : public Visitor {
public:
  /// \brief The struct representing a SPIR-V module header.
  struct Header {
    /// \brief Default constructs a SPIR-V module header with id bound 0.
    Header(uint32_t bound, uint32_t version);

    /// \brief Feeds the consumer with all the SPIR-V words for this header.
    std::vector<uint32_t> takeBinary();

    const uint32_t magicNumber;
    uint32_t version;
    const uint32_t generator;
    uint32_t bound;
    const uint32_t reserved;
  };

public:
  EmitVisitor(ASTContext &astCtx, SpirvContext &spvCtx,
              const SpirvCodeGenOptions &opts)
      : Visitor(opts, spvCtx), id(0),
        typeHandler(astCtx, spvCtx, &debugBinary, &annotationsBinary,
                    &typeConstantBinary,
                    [this]() -> uint32_t { return takeNextId(); }) {}

  // Visit different SPIR-V constructs for emitting.
  bool visit(SpirvModule *, Phase phase);
  bool visit(SpirvFunction *, Phase phase);
  bool visit(SpirvBasicBlock *, Phase phase);

  bool visit(SpirvCapability *);
  bool visit(SpirvExtension *);
  bool visit(SpirvExtInstImport *);
  bool visit(SpirvMemoryModel *);
  bool visit(SpirvEmitVertex *);
  bool visit(SpirvEndPrimitive *);
  bool visit(SpirvEntryPoint *);
  bool visit(SpirvExecutionMode *);
  bool visit(SpirvString *);
  bool visit(SpirvSource *);
  bool visit(SpirvModuleProcessed *);
  bool visit(SpirvDecoration *);
  bool visit(SpirvVariable *);
  bool visit(SpirvFunctionParameter *);
  bool visit(SpirvLoopMerge *);
  bool visit(SpirvSelectionMerge *);
  bool visit(SpirvBranch *);
  bool visit(SpirvBranchConditional *);
  bool visit(SpirvKill *);
  bool visit(SpirvReturn *);
  bool visit(SpirvSwitch *);
  bool visit(SpirvUnreachable *);
  bool visit(SpirvAccessChain *);
  bool visit(SpirvAtomic *);
  bool visit(SpirvBarrier *);
  bool visit(SpirvBinaryOp *);
  bool visit(SpirvBitFieldExtract *);
  bool visit(SpirvBitFieldInsert *);
  bool visit(SpirvConstantBoolean *);
  bool visit(SpirvConstantInteger *);
  bool visit(SpirvConstantFloat *);
  bool visit(SpirvConstantComposite *);
  bool visit(SpirvConstantNull *);
  bool visit(SpirvComposite *);
  bool visit(SpirvCompositeExtract *);
  bool visit(SpirvCompositeInsert *);
  bool visit(SpirvExtInst *);
  bool visit(SpirvFunctionCall *);
  bool visit(SpirvNonUniformBinaryOp *);
  bool visit(SpirvNonUniformElect *);
  bool visit(SpirvNonUniformUnaryOp *);
  bool visit(SpirvImageOp *);
  bool visit(SpirvImageQuery *);
  bool visit(SpirvImageSparseTexelsResident *);
  bool visit(SpirvImageTexelPointer *);
  bool visit(SpirvLoad *);
  bool visit(SpirvSampledImage *);
  bool visit(SpirvSelect *);
  bool visit(SpirvSpecConstantBinaryOp *);
  bool visit(SpirvSpecConstantUnaryOp *);
  bool visit(SpirvStore *);
  bool visit(SpirvUnaryOp *);
  bool visit(SpirvVectorShuffle *);
  bool visit(SpirvArrayLength *);

  // Returns the assembled binary built up in this visitor.
  std::vector<uint32_t> takeBinary();

private:
  // Returns the next available result-id.
  uint32_t takeNextId() { return ++id; }

  // There is no guarantee that an instruction or a function or a basic block
  // has been assigned result-id. This method returns the result-id for the
  // given object. If a result-id has not been assigned yet, it'll assign
  // one and return it.
  template <class T> uint32_t getOrAssignResultId(T *obj) {
    if (!obj->getResultId()) {
      obj->setResultId(takeNextId());
    }
    return obj->getResultId();
  }

  // Initiates the creation of a new instruction with the given Opcode.
  void initInstruction(spv::Op);
  // Initiates the creation of the given SPIR-V instruction.
  // If the given instruction has a return type, it will also trigger emitting
  // the necessary type (and its associated decorations) and uses its result-id
  // in the instruction.
  void initInstruction(SpirvInstruction *);

  // Finalizes the current instruction by encoding the instruction size into the
  // first word, and then appends the current instruction to the SPIR-V binary.
  void finalizeInstruction();

  // Encodes the given string into the current instruction that is being built.
  void encodeString(llvm::StringRef value);

  // Emits an OpName instruction into the debugBinary for the given target.
  void emitDebugNameForInstruction(uint32_t resultId, llvm::StringRef name);

  // TODO: Add a method for adding OpMemberName instructions for struct members
  // using the type information.

private:
  // The last result-id that's been used so far.
  uint32_t id;
  // Handler for emitting types and their related instructions.
  EmitTypeHandler typeHandler;
  // Current instruction being built
  SmallVector<uint32_t, 16> curInst;
  // All preamble instructions in the following order:
  // OpCapability, OpExtension, OpExtInstImport, OpMemoryModel, OpEntryPoint,
  // OpExecutionMode(Id)
  std::vector<uint32_t> preambleBinary;
  // All debug instructions *except* OpLine. Includes:
  // OpString, OpSourceExtension, OpSource, OpSourceContinued, OpName,
  // OpMemberName, OpModuleProcessed
  std::vector<uint32_t> debugBinary;
  // All annotation instructions: OpDecorate, OpMemberDecorate, OpGroupDecorate,
  // OpGroupMemberDecorate, and OpDecorationGroup.
  std::vector<uint32_t> annotationsBinary;
  // All type and constant instructions
  std::vector<uint32_t> typeConstantBinary;
  // All other instructions
  std::vector<uint32_t> mainBinary;
};

} // namespace spirv
} // namespace clang

#endif // LLVM_CLANG_SPIRV_EMITVISITOR_H
