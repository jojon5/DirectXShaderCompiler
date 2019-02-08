//===-- SPIRVContext.h - Context holding SPIR-V codegen data ----*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
#ifndef LLVM_CLANG_SPIRV_SPIRVCONTEXT_H
#define LLVM_CLANG_SPIRV_SPIRVCONTEXT_H

#include <unordered_map>

#include "clang/Frontend/FrontendAction.h"
#include "clang/SPIRV/Constant.h"
#include "clang/SPIRV/Decoration.h"
#include "clang/SPIRV/Type.h"
#include "llvm/Support/Allocator.h"

namespace clang {
namespace spirv {

struct TypeHash {
  std::size_t operator()(const Type &t) const {
    // TODO: We could improve this hash function if necessary.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(t.getOpcode()));
  }
};
struct DecorationHash {
  std::size_t operator()(const Decoration &d) const {
    // TODO: We could probably improve this hash function if needed.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(d.getValue()));
  }
};
struct ConstantHash {
  std::size_t operator()(const Constant &c) const {
    // TODO: We could improve this hash function if necessary.
    return std::hash<uint32_t>{}(static_cast<uint32_t>(c.getTypeId()));
  }
};

/// \brief A class for holding various data needed in SPIR-V codegen.
/// It should outlive all SPIR-V codegen components that requires/allocates
/// data.
class SPIRVContext {
public:
  /// \brief Constructs a default SPIR-V context.
  inline SPIRVContext();

  // Disable copy/move constructors/assignments.
  SPIRVContext(const SPIRVContext &) = delete;
  SPIRVContext(SPIRVContext &&) = delete;
  SPIRVContext &operator=(const SPIRVContext &) = delete;
  SPIRVContext &operator=(SPIRVContext &&) = delete;

  /// \brief Returns the next unused <result-id>.
  inline uint32_t getNextId() const;
  /// \brief Consumes the next unused <result-id>.
  inline uint32_t takeNextId();

  /// \brief Returns the <result-id> that defines the given Type. If the type
  /// has not been defined, it will define and store its instruction.
  /// If isRegistered is not nullptr, *isRegistered will contain whether the
  /// type was previously seen.
  uint32_t getResultIdForType(const Type *type, bool *isRegistered = nullptr);

  /// \brief Returns the <result-id> that defines the given Constant. If the
  /// constant has not been defined, it will define and return its result-id.
  uint32_t getResultIdForConstant(const Constant *);

  /// \brief Registers the existence of the given type in the current context,
  /// and returns the unique Type pointer.
  const Type *registerType(const Type &);

  /// \brief Registers the existence of the given constant in the current
  /// context, and returns the unique pointer to it.
  const Constant *registerConstant(const Constant &);

  /// \brief Registers the existence of the given decoration in the current
  /// context, and returns the unique Decoration pointer.
  const Decoration *registerDecoration(const Decoration &);

private:
  using TypeSet = std::unordered_set<Type, TypeHash>;
  using ConstantSet = std::unordered_set<Constant, ConstantHash>;
  using DecorationSet = std::unordered_set<Decoration, DecorationHash>;

  uint32_t nextId;

  /// \brief All the unique Decorations defined in the current context.
  DecorationSet existingDecorations;

  /// \brief All the unique types defined in the current context.
  TypeSet existingTypes;

  /// \brief All constants defined in the current context.
  /// These can be boolean, integer, float, or composite constants.
  ConstantSet existingConstants;

  /// \brief Maps a given type to the <result-id> that is defined for
  /// that type. If a Type* does not exist in the map, the type
  /// is not yet defined and is not associated with a <result-id>.
  std::unordered_map<const Type *, uint32_t> typeResultIdMap;

  /// \brief Maps a given constant to the <result-id> that is defined for
  /// that constant. If a Constant* does not exist in the map, the constant
  /// is not yet defined and is not associated with a <result-id>.
  std::unordered_map<const Constant *, uint32_t> constantResultIdMap;
};

SPIRVContext::SPIRVContext() : nextId(1) {}
uint32_t SPIRVContext::getNextId() const { return nextId; }
uint32_t SPIRVContext::takeNextId() { return nextId++; }

/// The class owning various SPIR-V entities allocated in memory during CodeGen.
///
/// All entities should be allocated from an object of this class using
/// placement new. This way other components of the CodeGen do not need to worry
/// about lifetime of those SPIR-V entities. They will be deleted when such a
/// context is deleted. Therefore, this context should outlive the usages of the
/// the SPIR-V entities allocated in memory.
class SpirvContext {
public:
  SpirvContext() = default;
  ~SpirvContext() = default;

  // Forbid copy construction and assignment
  SpirvContext(const SpirvContext &) = delete;
  SpirvContext &operator=(const SpirvContext &) = delete;

  // Forbid move construction and assignment
  SpirvContext(SpirvContext &&) = delete;
  SpirvContext &operator=(SpirvContext &&) = delete;

  /// Allocates memory of the given size and alignment.
  void *allocate(size_t size, unsigned align = 8) const {
    return allocator.Allocate(size, align);
  }

  /// Deallocates the memory pointed by the given pointer.
  void deallocate(void *ptr) const {}

private:
  /// \brief The allocator used to create SPIR-V entity objects.
  ///
  /// SPIR-V entity objects are never destructed; rather, all memory associated
  /// with the SPIR-V entity objects will be released when the SpirvContext
  /// itself is destroyed.
  mutable llvm::BumpPtrAllocator allocator;
};

} // end namespace spirv
} // end namespace clang

// operator new and delete aren't allowed inside namespaces.

/// Placement new for using the SpirvContext's allocator.
inline void *operator new(size_t bytes, const clang::spirv::SpirvContext &c,
                          size_t align = 8) {
  return c.allocate(bytes, align);
}

/// Placement delete companion to the new above.
inline void operator delete(void *ptr, const clang::spirv::SpirvContext &c,
                            size_t) {
  c.deallocate(ptr);
}

#endif
