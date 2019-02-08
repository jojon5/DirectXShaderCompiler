//===--- TypeProbe.cpp - Static functions for probing QualType ---*- C++ -*-==//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include "clang/SPIRV/AstTypeProbe.h"
#include "clang/AST/Decl.h"
#include "clang/AST/HlslTypes.h"

namespace clang {
namespace spirv {

std::string getAstTypeName(QualType type) {
  {
    QualType ty = {};
    if (isScalarType(type, &ty))
      if (const auto *builtinType = ty->getAs<BuiltinType>())
        switch (builtinType->getKind()) {
        case BuiltinType::Void:
          return "void";
        case BuiltinType::Bool:
          return "bool";
        case BuiltinType::Int:
          return "int";
        case BuiltinType::UInt:
          return "uint";
        case BuiltinType::Float:
          return "float";
        case BuiltinType::Double:
          return "double";
        case BuiltinType::LongLong:
          return "int64";
        case BuiltinType::ULongLong:
          return "uint64";
        case BuiltinType::Short:
          return "short";
        case BuiltinType::UShort:
          return "ushort";
        case BuiltinType::Half:
        case BuiltinType::HalfFloat:
          return "half";
        case BuiltinType::Min12Int:
          return "min12int";
        case BuiltinType::Min16Int:
          return "min16int";
        case BuiltinType::Min16UInt:
          return "min16uint";
        case BuiltinType::Min16Float:
          return "min16float";
        case BuiltinType::Min10Float:
          return "min10float";
        default:
          return "";
        }
  }

  {
    QualType elemType = {};
    uint32_t elemCount = {};
    if (isVectorType(type, &elemType, &elemCount))
      return "v" + std::to_string(elemCount) + getAstTypeName(elemType);
  }

  {
    QualType elemType = {};
    uint32_t rowCount = 0, colCount = 0;
    if (isMxNMatrix(type, &elemType, &rowCount, &colCount))
      return "mat" + std::to_string(rowCount) + "v" + std::to_string(colCount) +
             getAstTypeName(elemType);
  }

  if (const auto *structType = type->getAs<RecordType>())
    return structType->getDecl()->getName();

  return "";
}

bool isScalarType(QualType type, QualType *scalarType) {
  bool isScalar = false;
  QualType ty = {};

  if (type->isBuiltinType()) {
    isScalar = true;
    ty = type;
  } else if (hlsl::IsHLSLVecType(type) && hlsl::GetHLSLVecSize(type) == 1) {
    isScalar = true;
    ty = hlsl::GetHLSLVecElementType(type);
  } else if (const auto *extVecType =
                 dyn_cast<ExtVectorType>(type.getTypePtr())) {
    if (extVecType->getNumElements() == 1) {
      isScalar = true;
      ty = extVecType->getElementType();
    }
  } else if (is1x1Matrix(type)) {
    isScalar = true;
    ty = hlsl::GetHLSLMatElementType(type);
  }

  if (isScalar && scalarType)
    *scalarType = ty;

  return isScalar;
}

bool isVectorType(QualType type, QualType *elemType, uint32_t *elemCount) {
  bool isVec = false;
  QualType ty = {};
  uint32_t count = 0;

  if (hlsl::IsHLSLVecType(type)) {
    ty = hlsl::GetHLSLVecElementType(type);
    count = hlsl::GetHLSLVecSize(type);
    isVec = count > 1;
  } else if (const auto *extVecType =
                 dyn_cast<ExtVectorType>(type.getTypePtr())) {
    ty = extVecType->getElementType();
    count = extVecType->getNumElements();
    isVec = count > 1;
  } else if (hlsl::IsHLSLMatType(type)) {
    uint32_t rowCount = 0, colCount = 0;
    hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

    ty = hlsl::GetHLSLMatElementType(type);
    count = rowCount == 1 ? colCount : rowCount;
    isVec = (rowCount == 1) != (colCount == 1);
  }

  if (isVec) {
    if (elemType)
      *elemType = ty;
    if (elemCount)
      *elemCount = count;
  }
  return isVec;
}

bool is1x1Matrix(QualType type, QualType *elemType) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount == 1 && colCount == 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    return true;
  }

  return false;
}

bool is1xNMatrix(QualType type, QualType *elemType, uint32_t *elemCount) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount == 1 && colCount > 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (elemCount)
      *elemCount = colCount;
    return true;
  }

  return false;
}

bool isMx1Matrix(QualType type, QualType *elemType, uint32_t *elemCount) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount > 1 && colCount == 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (elemCount)
      *elemCount = rowCount;
    return true;
  }

  return false;
}

bool isMxNMatrix(QualType type, QualType *elemType, uint32_t *numRows,
                 uint32_t *numCols) {
  if (!hlsl::IsHLSLMatType(type))
    return false;

  uint32_t rowCount = 0, colCount = 0;
  hlsl::GetHLSLMatRowColCount(type, rowCount, colCount);

  if (rowCount > 1 && colCount > 1) {
    if (elemType)
      *elemType = hlsl::GetHLSLMatElementType(type);
    if (numRows)
      *numRows = rowCount;
    if (numCols)
      *numCols = colCount;
    return true;
  }

  return false;
}

bool isOrContainsAKindOfStructuredOrByteBuffer(QualType type) {
  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    if (name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
        name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
        name == "AppendStructuredBuffer" || name == "ConsumeStructuredBuffer")
      return true;

    for (const auto *field : recordType->getDecl()->fields()) {
      if (isOrContainsAKindOfStructuredOrByteBuffer(field->getType()))
        return true;
    }
  }
  return false;
}

bool isSubpassInput(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInput";

  return false;
}

bool isSubpassInputMS(QualType type) {
  if (const auto *rt = type->getAs<RecordType>())
    return rt->getDecl()->getName() == "SubpassInputMS";

  return false;
}

bool isConstantTextureBuffer(const Decl *decl) {
  if (const auto *bufferDecl = dyn_cast<HLSLBufferDecl>(decl->getDeclContext()))
    // Make sure we are not returning true for VarDecls inside cbuffer/tbuffer.
    return bufferDecl->isConstantBufferView();

  return false;
}

bool isResourceType(const ValueDecl *decl) {
  if (isConstantTextureBuffer(decl))
    return true;

  QualType declType = decl->getType();

  // Deprive the arrayness to see the element type
  while (declType->isArrayType()) {
    declType = declType->getAsArrayTypeUnsafe()->getElementType();
  }

  if (isSubpassInput(declType) || isSubpassInputMS(declType))
    return true;

  return hlsl::IsHLSLResourceType(declType);
}

bool isAKindOfStructuredOrByteBuffer(QualType type) {
  // Strip outer arrayness first
  while (type->isArrayType())
    type = type->getAsArrayTypeUnsafe()->getElementType();

  if (const RecordType *recordType = type->getAs<RecordType>()) {
    StringRef name = recordType->getDecl()->getName();
    return name == "StructuredBuffer" || name == "RWStructuredBuffer" ||
           name == "ByteAddressBuffer" || name == "RWByteAddressBuffer" ||
           name == "AppendStructuredBuffer" ||
           name == "ConsumeStructuredBuffer";
  }
  return false;
}

bool isOrContains16BitType(QualType type, bool enable16BitTypesOption) {
  // Primitive types
  {
    QualType ty = {};
    if (isScalarType(type, &ty)) {
      if (const auto *builtinType = ty->getAs<BuiltinType>()) {
        switch (builtinType->getKind()) {
        case BuiltinType::Min12Int:
        case BuiltinType::Min16Int:
        case BuiltinType::Min16UInt:
        case BuiltinType::Min10Float:
        case BuiltinType::Min16Float:
          return enable16BitTypesOption;
        // the 'Half' enum always represents 16-bit and 'HalfFloat' always
        // represents 32-bit floats.
        // int16_t and uint16_t map to Short and UShort
        case BuiltinType::Short:
        case BuiltinType::UShort:
        case BuiltinType::Half:
          return true;
        default:
          return false;
        }
      }
    }
  }

  // Vector types
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType))
      return isOrContains16BitType(elemType, enable16BitTypesOption);
  }

  // Matrix types
  {
    QualType elemType = {};
    if (isMxNMatrix(type, &elemType)) {
      return isOrContains16BitType(elemType, enable16BitTypesOption);
    }
  }

  // Struct type
  if (const auto *structType = type->getAs<RecordType>()) {
    const auto *decl = structType->getDecl();

    for (const auto *field : decl->fields()) {
      if (isOrContains16BitType(field->getType(), enable16BitTypesOption))
        return true;
    }

    return false;
  }

  // Array type
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return isOrContains16BitType(arrayType->getElementType(),
                                 enable16BitTypesOption);
  }

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>()) {
    return isOrContains16BitType(refType->getPointeeType(),
                                 enable16BitTypesOption);
  }

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>()) {
    return isOrContains16BitType(ptrType->getPointeeType(),
                                 enable16BitTypesOption);
  }

  if (const auto *typedefType = type->getAs<TypedefType>()) {
    return isOrContains16BitType(typedefType->desugar(),
                                 enable16BitTypesOption);
  }

  llvm_unreachable("checking 16-bit type unimplemented");
  return 0;
}

uint32_t getElementSpirvBitwidth(const ASTContext &astContext, QualType type,
                                 bool is16BitTypeEnabled) {
  const auto canonicalType = type.getCanonicalType();
  if (canonicalType != type)
    return getElementSpirvBitwidth(astContext, canonicalType,
                                   is16BitTypeEnabled);

  // Vector types
  {
    QualType elemType = {};
    if (isVectorType(type, &elemType))
      return getElementSpirvBitwidth(astContext, elemType, is16BitTypeEnabled);
  }

  // Matrix types
  if (hlsl::IsHLSLMatType(type))
    return getElementSpirvBitwidth(
        astContext, hlsl::GetHLSLMatElementType(type), is16BitTypeEnabled);

  // Array types
  if (const auto *arrayType = type->getAsArrayTypeUnsafe()) {
    return getElementSpirvBitwidth(astContext, arrayType->getElementType(),
                                   is16BitTypeEnabled);
  }

  // Typedefs
  if (const auto *typedefType = type->getAs<TypedefType>())
    return getElementSpirvBitwidth(astContext, typedefType->desugar(),
                                   is16BitTypeEnabled);

  // Reference types
  if (const auto *refType = type->getAs<ReferenceType>())
    return getElementSpirvBitwidth(astContext, refType->getPointeeType(),
                                   is16BitTypeEnabled);

  // Pointer types
  if (const auto *ptrType = type->getAs<PointerType>())
    return getElementSpirvBitwidth(astContext, ptrType->getPointeeType(),
                                   is16BitTypeEnabled);

  // Scalar types
  QualType ty = {};
  const bool isScalar = isScalarType(type, &ty);
  assert(isScalar);
  (void)isScalar;
  if (const auto *builtinType = ty->getAs<BuiltinType>()) {
    switch (builtinType->getKind()) {
    case BuiltinType::Bool:
    case BuiltinType::Int:
    case BuiltinType::UInt:
    case BuiltinType::Float:
      return 32;
    case BuiltinType::Double:
    case BuiltinType::LongLong:
    case BuiltinType::ULongLong:
      return 64;
    // Half builtin type is always 16-bit. The HLSL 'half' keyword is translated
    // to 'Half' enum if -enable-16bit-types is true.
    // int16_t and uint16_t map to Short and UShort
    case BuiltinType::Half:
    case BuiltinType::Short:
    case BuiltinType::UShort:
      return 16;
    // HalfFloat builtin type is just an alias for Float builtin type and is
    // always 32-bit. The HLSL 'half' keyword is translated to 'HalfFloat' enum
    // if -enable-16bit-types is false.
    case BuiltinType::HalfFloat:
      return 32;
    // The following types are treated as 16-bit if '-enable-16bit-types' option
    // is enabled. They are treated as 32-bit otherwise.
    case BuiltinType::Min12Int:
    case BuiltinType::Min16Int:
    case BuiltinType::Min16UInt:
    case BuiltinType::Min16Float:
    case BuiltinType::Min10Float: {
      return is16BitTypeEnabled ? 16 : 32;
    }
    case BuiltinType::LitFloat: {
      // TODO(ehsan): Literal types not handled properly.
      return 64;
    }
    case BuiltinType::LitInt: {
      // TODO(ehsan): Literal types not handled properly.
      return 64;
    }
    default:
      // Other builtin types are either not relevant to bitcount or not in HLSL.
      break;
    }
  }
  llvm_unreachable("invalid type passed to getElementSpirvBitwidth");
}

bool canTreatAsSameScalarType(QualType type1, QualType type2) {
  // Treat const int/float the same as const int/float
  type1.removeLocalConst();
  type2.removeLocalConst();

  return (type1.getCanonicalType() == type2.getCanonicalType()) ||
         // Treat 'literal float' and 'float' as the same
         (type1->isSpecificBuiltinType(BuiltinType::LitFloat) &&
          type2->isFloatingType()) ||
         (type2->isSpecificBuiltinType(BuiltinType::LitFloat) &&
          type1->isFloatingType()) ||
         // Treat 'literal int' and 'int'/'uint' as the same
         (type1->isSpecificBuiltinType(BuiltinType::LitInt) &&
          type2->isIntegerType() &&
          // Disallow boolean types
          !type2->isSpecificBuiltinType(BuiltinType::Bool)) ||
         (type2->isSpecificBuiltinType(BuiltinType::LitInt) &&
          type1->isIntegerType() &&
          // Disallow boolean types
          !type1->isSpecificBuiltinType(BuiltinType::Bool));
}

bool canFitIntoOneRegister(QualType structType, QualType *elemType,
                           uint32_t *elemCount) {
  if (structType->getAsStructureType() == nullptr)
    return false;

  const auto *structDecl = structType->getAsStructureType()->getDecl();
  QualType firstElemType;
  uint32_t totalCount = 0;

  for (const auto *field : structDecl->fields()) {
    QualType type;
    uint32_t count = 1;

    if (isScalarType(field->getType(), &type) ||
        isVectorType(field->getType(), &type, &count)) {
      if (firstElemType.isNull()) {
        firstElemType = type;
      } else {
        if (!canTreatAsSameScalarType(firstElemType, type)) {
          assert(false && "all struct members should have the same element "
                          "type for resource template instantiation");
          return false;
        }
      }
      totalCount += count;
    } else {
      assert(false && "unsupported struct element type for resource template "
                      "instantiation");
      return false;
    }
  }

  if (totalCount > 4) {
    assert(
        false &&
        "resource template element type cannot fit into four 32-bit scalars");
    return false;
  }

  if (elemType)
    *elemType = firstElemType;
  if (elemCount)
    *elemCount = totalCount;
  return true;
}

QualType getElementType(QualType type) {
  QualType elemType = {};
  if (isScalarType(type, &elemType) || isVectorType(type, &elemType) ||
      isMxNMatrix(type, &elemType) || canFitIntoOneRegister(type, &elemType)) {
    return elemType;
  }

  if (const auto *arrType = dyn_cast<ConstantArrayType>(type)) {
    return arrType->getElementType();
  }

  assert(false && "unsupported resource type parameter");
  return type;
}

} // namespace spirv
} // namespace clang
