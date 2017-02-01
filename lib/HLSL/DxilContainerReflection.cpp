///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DxilContainerReflection.cpp                                               //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides support for reading DXIL container structures.                   //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/InstIterator.h"
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/HLSL/DxilModule.h"
#include "dxc/HLSL/DxilShaderModel.h"
#include "dxc/HLSL/DxilOperations.h"
#include "dxc/HLSL/DxilInstructions.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/FileIOHelper.h"
#include "dxc/Support/dxcapi.impl.h"

#include <unordered_set>

#include "dxc/dxcapi.h"

#include "d3d12shader.h" // for compatibility
#include "d3d11shader.h" // for compatibility
const GUID IID_ID3D11ShaderReflection_43 = {
    0x0a233719,
    0x3960,
    0x4578,
    {0x9d, 0x7c, 0x20, 0x3b, 0x8b, 0x1d, 0x9c, 0xc1}};
const GUID IID_ID3D11ShaderReflection_47 = {
    0x8d536ca1,
    0x0cca,
    0x4956,
    {0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84}};

using namespace llvm;
using namespace hlsl;

class DxilContainerReflection : public IDxcContainerReflection {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CComPtr<IDxcBlob> m_container;
  const DxilContainerHeader *m_pHeader;
  uint32_t m_headerLen;
  bool IsLoaded() const { return m_pHeader != nullptr; }
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    return DoBasicQueryInterface<IDxcContainerReflection>(this, iid, ppvObject);
  }

  DxilContainerReflection() : m_dwRef(0), m_pHeader(nullptr), m_headerLen(0) { }
  __override HRESULT STDMETHODCALLTYPE Load(_In_ IDxcBlob *pContainer);
  __override HRESULT STDMETHODCALLTYPE GetPartCount(_Out_ UINT32 *pResult);
  __override HRESULT STDMETHODCALLTYPE GetPartKind(UINT32 idx, _Out_ UINT32 *pResult);
  __override HRESULT STDMETHODCALLTYPE GetPartContent(UINT32 idx, _COM_Outptr_ IDxcBlob **ppResult);
  __override HRESULT STDMETHODCALLTYPE FindFirstPartKind(UINT32 kind, _Out_ UINT32 *pResult);
  __override HRESULT STDMETHODCALLTYPE GetPartReflection(UINT32 idx, REFIID iid, _COM_Outptr_ void **ppvObject);
};

class CShaderReflectionConstantBuffer;
class DxilShaderReflection : public ID3D12ShaderReflection {
private:
  DXC_MICROCOM_REF_FIELD(m_dwRef)
  CComPtr<IDxcBlob> m_pContainer;
  LLVMContext Context;
  std::unique_ptr<Module> m_pModule; // Must come after LLVMContext, otherwise unique_ptr will over-delete.
  DxilModule *m_pDxilModule;
  std::vector<CShaderReflectionConstantBuffer>    m_CBs;
  std::vector<D3D12_SHADER_INPUT_BIND_DESC>       m_Resources;
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_InputSignature;
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_OutputSignature;
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC>     m_PatchConstantSignature;
  std::vector<std::unique_ptr<char[]>>            m_UpperCaseNames;
  void CreateReflectionObjects();
  void SetCBufferUsage();
  void CreateReflectionObjectForResource(DxilResourceBase *R);
  void CreateReflectionObjectsForSignature(
      const DxilSignature &Sig,
      std::vector<D3D12_SIGNATURE_PARAMETER_DESC> &Descs);
  LPCSTR CreateUpperCase(LPCSTR pValue);
  void MarkUsedSignatureElements();
public:
  enum class PublicAPI { D3D12 = 0, D3D11_47 = 1, D3D11_43 = 2 };
  PublicAPI m_PublicAPI;
  void SetPublicAPI(PublicAPI value) { m_PublicAPI = value; }
  static PublicAPI IIDToAPI(REFIID iid) {
    DxilShaderReflection::PublicAPI api =
        DxilShaderReflection::PublicAPI::D3D12;
    if (IsEqualIID(IID_ID3D11ShaderReflection_43, iid))
      api = DxilShaderReflection::PublicAPI::D3D11_43;
    else if (IsEqualIID(IID_ID3D11ShaderReflection_47, iid))
      api = DxilShaderReflection::PublicAPI::D3D11_47;
    return api;
  }
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **ppvObject) {
    HRESULT hr = DoBasicQueryInterface<ID3D12ShaderReflection>(this, iid, ppvObject);
    if (hr == E_NOINTERFACE) {
      // ID3D11ShaderReflection is identical to ID3D12ShaderReflection, except
      // for some shorter data structures in some out parameters.
      PublicAPI api = IIDToAPI(iid);
      if (api == m_PublicAPI) {
        *ppvObject = (ID3D12ShaderReflection *)this;
        this->AddRef();
        hr = S_OK;
      }
    }
    return hr;
  }

  DxilShaderReflection() : m_dwRef(0), m_pDxilModule(nullptr) { }
  HRESULT Load(IDxcBlob *pBlob, const DxilPartHeader *pPart);

  // ID3D12ShaderReflection
  STDMETHODIMP GetDesc(THIS_ _Out_ D3D12_SHADER_DESC *pDesc);

  STDMETHODIMP_(ID3D12ShaderReflectionConstantBuffer*) GetConstantBufferByIndex(THIS_ _In_ UINT Index);
  STDMETHODIMP_(ID3D12ShaderReflectionConstantBuffer*) GetConstantBufferByName(THIS_ _In_ LPCSTR Name);

  STDMETHODIMP GetResourceBindingDesc(THIS_ _In_ UINT ResourceIndex,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc);

  STDMETHODIMP GetInputParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);
  STDMETHODIMP GetOutputParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);
  STDMETHODIMP GetPatchConstantParameterDesc(THIS_ _In_ UINT ParameterIndex,
    _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc);

  STDMETHODIMP_(ID3D12ShaderReflectionVariable*) GetVariableByName(THIS_ _In_ LPCSTR Name);

  STDMETHODIMP GetResourceBindingDescByName(THIS_ _In_ LPCSTR Name,
    _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc);

  STDMETHODIMP_(UINT) GetMovInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetMovcInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetConversionInstructionCount(THIS);
  STDMETHODIMP_(UINT) GetBitwiseInstructionCount(THIS);

  STDMETHODIMP_(D3D_PRIMITIVE) GetGSInputPrimitive(THIS);
  STDMETHODIMP_(BOOL) IsSampleFrequencyShader(THIS);

  STDMETHODIMP_(UINT) GetNumInterfaceSlots(THIS);
  STDMETHODIMP GetMinFeatureLevel(THIS_ _Out_ enum D3D_FEATURE_LEVEL* pLevel);

  STDMETHODIMP_(UINT) GetThreadGroupSize(THIS_
    _Out_opt_ UINT* pSizeX,
    _Out_opt_ UINT* pSizeY,
    _Out_opt_ UINT* pSizeZ);

  STDMETHODIMP_(UINT64) GetRequiresFlags(THIS);
};

_Use_decl_annotations_
HRESULT DxilContainerReflection::Load(IDxcBlob *pContainer) {
  if (pContainer == nullptr) {
    m_container.Release();
    m_pHeader = nullptr;
    m_headerLen = 0;
    return S_OK;
  }

  uint32_t bufLen = pContainer->GetBufferSize();
  const DxilContainerHeader *pHeader =
      IsDxilContainerLike(pContainer->GetBufferPointer(), bufLen);
  if (pHeader == nullptr) {
    return E_INVALIDARG;
  }
  if (!IsValidDxilContainer(pHeader, bufLen)) {
    return E_INVALIDARG;
  }

  m_container = pContainer;
  m_headerLen = bufLen;
  m_pHeader = pHeader;

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartCount(UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  *pResult = m_pHeader->PartCount;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartKind(UINT32 idx, _Out_ UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  *pResult = pPart->PartFourCC;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartContent(UINT32 idx, _COM_Outptr_ IDxcBlob **ppResult) {
  if (ppResult == nullptr) return E_POINTER;
  *ppResult = nullptr;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  const char *pData = GetDxilPartData(pPart);
  uint32_t offset = (uint32_t)(pData - (char*)m_container->GetBufferPointer()); // Offset from the beginning.
  uint32_t length = pPart->PartSize;
  return DxcCreateBlobFromBlob(m_container, offset, length, ppResult);
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::FindFirstPartKind(UINT32 kind, _Out_ UINT32 *pResult) {
  if (pResult == nullptr) return E_POINTER;
  *pResult = 0;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  DxilPartIterator it = std::find_if(begin(m_pHeader), end(m_pHeader), DxilPartIsType(kind));
  if (it == end(m_pHeader)) return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
  *pResult = it.index;
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilContainerReflection::GetPartReflection(UINT32 idx, REFIID iid, void **ppvObject) {
  if (ppvObject == nullptr) return E_POINTER;
  *ppvObject = nullptr;
  if (!IsLoaded()) return E_NOT_VALID_STATE;
  if (idx >= m_pHeader->PartCount) return E_BOUNDS;
  const DxilPartHeader *pPart = GetDxilContainerPart(m_pHeader, idx);
  if (pPart->PartFourCC != DFCC_DXIL && pPart->PartFourCC != DFCC_ShaderDebugInfoDXIL) {
    return E_NOTIMPL;
  }
  
  HRESULT hr = S_OK;
  CComPtr<DxilShaderReflection> pReflection = new (std::nothrow)DxilShaderReflection();
  IFCOOM(pReflection.p);
  DxilShaderReflection::PublicAPI api = DxilShaderReflection::IIDToAPI(iid);
  pReflection->SetPublicAPI(api);

  IFC(pReflection->Load(m_container, pPart));
  IFC(pReflection.p->QueryInterface(iid, ppvObject));
Cleanup:
  return hr;
}

void hlsl::CreateDxcContainerReflection(IDxcContainerReflection **ppResult) {
  CComPtr<DxilContainerReflection> pReflection = new DxilContainerReflection();
  *ppResult = pReflection.Detach();
}

///////////////////////////////////////////////////////////////////////////////
// DxilShaderReflection implementation - helper objects.                     //

class CShaderReflectionType;
class CShaderReflectionVariable;
class CShaderReflectionConstantBuffer;
class CShaderReflection;
struct D3D11_INTERNALSHADER_RESOURCE_DEF;
class CShaderReflectionType : public ID3D12ShaderReflectionType
{
protected:
  D3D12_SHADER_TYPE_DESC              m_Desc;
  std::vector<StringRef>              m_MemberNames;
  std::vector<CShaderReflectionType>  m_MemberTypes;
  CShaderReflectionType*              m_pSubType;
  CShaderReflectionType*              m_pBaseClass;
  std::vector<CShaderReflectionType>  m_Interfaces;
  ULONG_PTR                           m_Identity;

public:
  // Internal
  CShaderReflectionType();
  ~CShaderReflectionType();

  HRESULT Initialize(D3D11_INTERNALSHADER_RESOURCE_DEF *pResourceDef,
                     BYTE *pBase, BYTE *pMax, BYTE *pRawTypeDef);

  // ID3D12ShaderReflectionType
  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByName)(LPCSTR Name);
  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index);

  STDMETHOD(IsEqual)(THIS_ ID3D12ShaderReflectionType* pType);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetSubType)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetBaseClass)(THIS);
  STDMETHOD_(UINT, GetNumInterfaces)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetInterfaceByIndex)(THIS_ UINT uIndex);
  STDMETHOD(IsOfType)(THIS_ ID3D12ShaderReflectionType* pType);
  STDMETHOD(ImplementsInterface)(THIS_ ID3D12ShaderReflectionType* pBase);

  bool CheckEqual(__in CShaderReflectionType *pOther) {
    return m_Identity == pOther->m_Identity;
  }
};

class CShaderReflectionVariable : public ID3D12ShaderReflectionVariable
{
protected:
  D3D12_SHADER_VARIABLE_DESC          m_Desc;
  CShaderReflectionType              *m_pType;
  CShaderReflectionConstantBuffer    *m_pBuffer;
  BYTE                               *m_pDefaultValue;

public:
  void Initialize(CShaderReflectionConstantBuffer *pBuffer,
                  D3D12_SHADER_VARIABLE_DESC *pDesc,
                  CShaderReflectionType *pType, BYTE *pDefaultValue);

  LPCSTR GetName() { return m_Desc.Name; }

  // ID3D12ShaderReflectionVariable
  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionType*, GetType)();
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer*, GetBuffer)();

  STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT uArrayIndex);
};

class CShaderReflectionConstantBuffer : public ID3D12ShaderReflectionConstantBuffer
{
protected:
  D3D12_SHADER_BUFFER_DESC                m_Desc;
  std::vector<CShaderReflectionVariable>  m_Variables;

public:
  CShaderReflectionConstantBuffer() = default;
  CShaderReflectionConstantBuffer(CShaderReflectionConstantBuffer &&other) {
    m_Desc = other.m_Desc;
    std::swap(m_Variables, other.m_Variables);
  }

  void Initialize(DxilModule &M, DxilCBuffer &CB);
  void InitializeStructuredBuffer(DxilModule &M, DxilResource &R);
  LPCSTR GetName() { return m_Desc.Name; }

  // ID3D12ShaderReflectionConstantBuffer
  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc);

  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByName)(LPCSTR Name);
};

// Invalid type sentinel definitions
class CInvalidSRType;
class CInvalidSRVariable;
class CInvalidSRConstantBuffer;
class CInvalidSRLibraryFunction;
class CInvalidSRFunctionParameter;

class CInvalidSRType : public ID3D12ShaderReflectionType {
  STDMETHOD(GetDesc)(D3D12_SHADER_TYPE_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByIndex)(UINT Index);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetMemberTypeByName)(LPCSTR Name);
  STDMETHOD_(LPCSTR, GetMemberTypeName)(UINT Index) { return "$Invalid"; }

  STDMETHOD(IsEqual)(THIS_ ID3D12ShaderReflectionType* pType) { return E_FAIL; }
  STDMETHOD_(ID3D12ShaderReflectionType*, GetSubType)(THIS);
  STDMETHOD_(ID3D12ShaderReflectionType*, GetBaseClass)(THIS);
  STDMETHOD_(UINT, GetNumInterfaces)(THIS) { return 0; }
  STDMETHOD_(ID3D12ShaderReflectionType*, GetInterfaceByIndex)(THIS_ UINT uIndex);
  STDMETHOD(IsOfType)(THIS_ ID3D12ShaderReflectionType* pType) { return E_FAIL; }
  STDMETHOD(ImplementsInterface)(THIS_ ID3D12ShaderReflectionType* pBase) { return E_FAIL; }
};
static CInvalidSRType g_InvalidSRType;

ID3D12ShaderReflectionType* CInvalidSRType::GetMemberTypeByIndex(UINT) { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetMemberTypeByName(LPCSTR) { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetSubType() { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetBaseClass() { return &g_InvalidSRType; }
ID3D12ShaderReflectionType* CInvalidSRType::GetInterfaceByIndex(UINT) { return &g_InvalidSRType; }

class CInvalidSRVariable : public ID3D12ShaderReflectionVariable {
  STDMETHOD(GetDesc)(D3D12_SHADER_VARIABLE_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionType*, GetType)() { return &g_InvalidSRType; }
  STDMETHOD_(ID3D12ShaderReflectionConstantBuffer*, GetBuffer)();

  STDMETHOD_(UINT, GetInterfaceSlot)(THIS_ UINT uIndex) { return UINT_MAX; }
};
static CInvalidSRVariable g_InvalidSRVariable;

class CInvalidSRConstantBuffer : public ID3D12ShaderReflectionConstantBuffer {
  STDMETHOD(GetDesc)(D3D12_SHADER_BUFFER_DESC *pDesc) { return E_FAIL; }

  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByIndex)(UINT Index) { return &g_InvalidSRVariable; }
  STDMETHOD_(ID3D12ShaderReflectionVariable*, GetVariableByName)(LPCSTR Name) { return &g_InvalidSRVariable; }
};
static CInvalidSRConstantBuffer g_InvalidSRConstantBuffer;

void CShaderReflectionVariable::Initialize(
    CShaderReflectionConstantBuffer *pBuffer, D3D12_SHADER_VARIABLE_DESC *pDesc,
    CShaderReflectionType *pType, BYTE *pDefaultValue) {
  m_pBuffer = pBuffer;
  memcpy(&m_Desc, pDesc, sizeof(m_Desc));
  m_pType = pType;
  m_pDefaultValue = pDefaultValue;
}

HRESULT CShaderReflectionVariable::GetDesc(D3D12_SHADER_VARIABLE_DESC *pDesc) {
  if (!pDesc) return E_POINTER;
  memcpy(pDesc, &m_Desc, sizeof(m_Desc));
  return S_OK;
}

ID3D12ShaderReflectionType *CShaderReflectionVariable::GetType() {
  return m_pType;
}

ID3D12ShaderReflectionConstantBuffer *CShaderReflectionVariable::GetBuffer() {
  return m_pBuffer;
}

UINT CShaderReflectionVariable::GetInterfaceSlot(UINT uArrayIndex) {
  return UINT_MAX;
}

ID3D12ShaderReflectionConstantBuffer *CInvalidSRVariable::GetBuffer() {
  return &g_InvalidSRConstantBuffer;
}

void CShaderReflectionConstantBuffer::Initialize(DxilModule &M, DxilCBuffer &CB) {
  ZeroMemory(&m_Desc, sizeof(m_Desc));
  m_Desc.Name = CB.GetGlobalName().c_str();
  m_Desc.Size = CB.GetSize() / CB.GetRangeSize();
  m_Desc.Size = (m_Desc.Size + 0x0f) & ~(0x0f); // Round up to 16 bytes for reflection.
  m_Desc.Type = D3D_CT_CBUFFER;
  m_Desc.uFlags = 0;
  Type *Ty = CB.GetGlobalSymbol()->getType()->getPointerElementType();
  // For ConstantBuffer<> buf[2], the array size is in Resource binding count
  // part.
  if (Ty->isArrayTy())
    Ty = Ty->getArrayElementType();

  DxilTypeSystem &typeSys = M.GetTypeSystem();
  StructType *ST = cast<StructType>(Ty);
  DxilStructAnnotation *annotation =
      typeSys.GetStructAnnotation(cast<StructType>(ST));
  // Dxil from dxbc doesn't have annotation.
  if (!annotation)
    return;

  m_Desc.Variables = ST->getNumContainedTypes();
  unsigned lastIndex = ST->getNumContainedTypes() - 1;

  for (unsigned i = 0; i < ST->getNumContainedTypes(); ++i) {
    DxilFieldAnnotation &fieldAnnotation = annotation->GetFieldAnnotation(i);

    D3D12_SHADER_VARIABLE_DESC VarDesc;
    ZeroMemory(&VarDesc, sizeof(VarDesc));
    VarDesc.uFlags |= D3D_SVF_USED; // Will update in SetCBufferUsage.
    CShaderReflectionVariable Var;
    // TODO: create reflection type.
    CShaderReflectionType *pVarType = nullptr;
    BYTE *pDefaultValue = nullptr;

    VarDesc.Name = fieldAnnotation.GetFieldName().c_str();
    VarDesc.StartOffset = fieldAnnotation.GetCBufferOffset();
    if (i < lastIndex) {
      DxilFieldAnnotation &nextFieldAnnotation =
          annotation->GetFieldAnnotation(i + 1);
      VarDesc.Size = nextFieldAnnotation.GetCBufferOffset() - fieldAnnotation.GetCBufferOffset();
    }
    else {
      VarDesc.Size = CB.GetSize() - fieldAnnotation.GetCBufferOffset();
    }
    Var.Initialize(this, &VarDesc, pVarType, pDefaultValue);
    m_Variables.push_back(Var);
  }
}

static unsigned CalcTypeSize(Type *Ty) {
  // Assume aligned values.
  if (Ty->isIntegerTy() || Ty->isFloatTy()) {
    return Ty->getPrimitiveSizeInBits() / 8;
  }
  else if (Ty->isArrayTy()) {
    ArrayType *AT = dyn_cast<ArrayType>(Ty);
    return AT->getNumElements() * CalcTypeSize(AT->getArrayElementType());
  }
  else if (Ty->isStructTy()) {
    StructType *ST = dyn_cast<StructType>(Ty);
    unsigned i = 0, c = ST->getStructNumElements();
    unsigned result = 0;
    for (; i < c; ++i) {
      result += CalcTypeSize(ST->getStructElementType(i));
      // TODO: align!
    }
    return result;
  }
  else if (Ty->isVectorTy()) {
    VectorType *VT = dyn_cast<VectorType>(Ty);
    return VT->getVectorNumElements() * CalcTypeSize(VT->getVectorElementType());
  }
  else {
    DXASSERT_NOMSG(false);
    return 0;
  }
}

static unsigned CalcResTypeSize(DxilModule &M, DxilResource &R) {
  UNREFERENCED_PARAMETER(M);
  Type *Ty = R.GetGlobalSymbol()->getType()->getPointerElementType();
  return CalcTypeSize(Ty);
}

void CShaderReflectionConstantBuffer::InitializeStructuredBuffer(DxilModule &M, DxilResource &R) {
  ZeroMemory(&m_Desc, sizeof(m_Desc));
  m_Desc.Name = R.GetGlobalName().c_str();
  //m_Desc.Size = R.GetSize();
  m_Desc.Type = D3D11_CT_RESOURCE_BIND_INFO;
  m_Desc.uFlags = 0;
  m_Desc.Variables = 1;

  D3D12_SHADER_VARIABLE_DESC VarDesc;
  ZeroMemory(&VarDesc, sizeof(VarDesc));
  VarDesc.Name = "$Element";
  VarDesc.Size = CalcResTypeSize(M, R); // aligned bytes
  VarDesc.StartTexture = UINT_MAX;
  VarDesc.StartSampler = UINT_MAX;
  VarDesc.uFlags |= D3D_SVF_USED; // TODO: not necessarily true
  CShaderReflectionVariable Var;
  CShaderReflectionType *pVarType = nullptr;
  BYTE *pDefaultValue = nullptr;
  Var.Initialize(this, &VarDesc, pVarType, pDefaultValue);
  m_Variables.push_back(Var);

  m_Desc.Size = VarDesc.Size;
}

HRESULT CShaderReflectionConstantBuffer::GetDesc(D3D12_SHADER_BUFFER_DESC *pDesc) {
  if (!pDesc)
    return E_POINTER;
  memcpy(pDesc, &m_Desc, sizeof(m_Desc));
  return S_OK;
}

ID3D12ShaderReflectionVariable *
CShaderReflectionConstantBuffer::GetVariableByIndex(UINT Index) {
  if (Index >= m_Variables.size()) {
    return &g_InvalidSRVariable;
  }

  return &m_Variables[Index];
}

ID3D12ShaderReflectionVariable *
CShaderReflectionConstantBuffer::GetVariableByName(LPCSTR Name) {
  UINT index;

  if (NULL == Name) {
    return &g_InvalidSRVariable;
  }

  for (index = 0; index < m_Variables.size(); ++index) {
    if (0 == strcmp(m_Variables[index].GetName(), Name)) {
      return &m_Variables[index];
    }
  }

  return &g_InvalidSRVariable;
}

///////////////////////////////////////////////////////////////////////////////
// DxilShaderReflection implementation.                                      //

static DxilResource *DxilResourceFromBase(DxilResourceBase *RB) {
  DxilResourceBase::Class C = RB->GetClass();
  if (C == DXIL::ResourceClass::UAV || C == DXIL::ResourceClass::SRV)
    return (DxilResource *)RB;
  return nullptr;
}

static D3D_SHADER_INPUT_TYPE ResourceToShaderInputType(DxilResourceBase *RB) {
  DxilResource *R = DxilResourceFromBase(RB);
  bool isUAV = RB->GetClass() == DxilResourceBase::Class::UAV;
  switch (RB->GetKind()) {
  case DxilResource::Kind::CBuffer:
    return D3D_SIT_CBUFFER;
  case DxilResource::Kind::Sampler:
    return D3D_SIT_SAMPLER;
  case DxilResource::Kind::RawBuffer:
    return isUAV ? D3D_SIT_UAV_RWBYTEADDRESS : D3D_SIT_BYTEADDRESS;
  case DxilResource::Kind::StructuredBuffer: {
    if (!isUAV) return D3D_SIT_STRUCTURED;
    // TODO: D3D_SIT_UAV_CONSUME_STRUCTURED, D3D_SIT_UAV_APPEND_STRUCTURED?
    if (R->HasCounter()) return D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER;
    return D3D_SIT_UAV_RWSTRUCTURED;
  }
  case DxilResource::Kind::TypedBuffer:
    return isUAV ? D3D_SIT_UAV_RWTYPED : D3D_SIT_STRUCTURED;
  case DxilResource::Kind::Texture1D:
  case DxilResource::Kind::Texture1DArray:
  case DxilResource::Kind::Texture2D:
  case DxilResource::Kind::Texture2DArray:
  case DxilResource::Kind::Texture2DMS:
  case DxilResource::Kind::Texture2DMSArray:
  case DxilResource::Kind::Texture3D:
  case DxilResource::Kind::TextureCube:
  case DxilResource::Kind::TextureCubeArray:
    return R->IsRW() ? D3D_SIT_UAV_RWTYPED : D3D_SIT_TEXTURE;
  default:
    return (D3D_SHADER_INPUT_TYPE)0;
  }
}

static D3D_RESOURCE_RETURN_TYPE ResourceToReturnType(DxilResourceBase *RB) {
  DxilResource *R = DxilResourceFromBase(RB);
  if (R != nullptr) {
    CompType CT = R->GetCompType();
    if (CT.GetKind() == CompType::Kind::F64) return D3D_RETURN_TYPE_DOUBLE;
    if (CT.IsUNorm()) return D3D_RETURN_TYPE_UNORM;
    if (CT.IsSNorm()) return D3D_RETURN_TYPE_SNORM;
    if (CT.IsSIntTy()) return D3D_RETURN_TYPE_SINT;
    if (CT.IsUIntTy()) return D3D_RETURN_TYPE_UINT;
    if (CT.IsFloatTy()) return D3D_RETURN_TYPE_FLOAT;

    // D3D_RETURN_TYPE_CONTINUED: Return type is a multiple-dword type, such as a
    // double or uint64, and the component is continued from the previous
    // component that was declared. The first component represents the lower bits.
    return D3D_RETURN_TYPE_MIXED;
  }

  return (D3D_RESOURCE_RETURN_TYPE)0;
}

static D3D_SRV_DIMENSION ResourceToDimension(DxilResourceBase *RB) {
  switch (RB->GetKind()) {
  case DxilResource::Kind::StructuredBuffer:
  case DxilResource::Kind::TypedBuffer:
    return D3D_SRV_DIMENSION_BUFFER;
  case DxilResource::Kind::Texture1D:
    return D3D_SRV_DIMENSION_TEXTURE1D;
  case DxilResource::Kind::Texture1DArray:
    return D3D_SRV_DIMENSION_TEXTURE1DARRAY;
  case DxilResource::Kind::Texture2D:
    return D3D_SRV_DIMENSION_TEXTURE2D;
  case DxilResource::Kind::Texture2DArray:
    return D3D_SRV_DIMENSION_TEXTURE2DARRAY;
  case DxilResource::Kind::Texture2DMS:
    return D3D_SRV_DIMENSION_TEXTURE2DMS;
  case DxilResource::Kind::Texture2DMSArray:
    return D3D_SRV_DIMENSION_TEXTURE2DMSARRAY;
  case DxilResource::Kind::Texture3D:
    return D3D_SRV_DIMENSION_TEXTURE3D;
  case DxilResource::Kind::TextureCube:
    return D3D_SRV_DIMENSION_TEXTURECUBE;
  case DxilResource::Kind::TextureCubeArray:
    return D3D_SRV_DIMENSION_TEXTURECUBEARRAY;
  case DxilResource::Kind::RawBuffer:
    return D3D11_SRV_DIMENSION_BUFFER; // D3D11_SRV_DIMENSION_BUFFEREX?
  default:
    return D3D_SRV_DIMENSION_UNKNOWN;
  }
}

static UINT ResourceToFlags(DxilResourceBase *RB) {
  UINT result = 0;
  DxilResource *R = DxilResourceFromBase(RB);
  if (R != nullptr &&
      (R->IsAnyTexture() || R->GetKind() == DXIL::ResourceKind::TypedBuffer)) {
    llvm::Type *RetTy = R->GetRetType();
    if (VectorType *VT = dyn_cast<VectorType>(RetTy)) {
      unsigned vecSize = VT->getNumElements();
      switch (vecSize) {
      case 4:
        result |= D3D_SIF_TEXTURE_COMPONENTS;
        break;
      case 3:
        result |= D3D_SIF_TEXTURE_COMPONENT_1;
        break;
      case 2:
        result |= D3D_SIF_TEXTURE_COMPONENT_0;
        break;
      }
    }
  }
  // D3D_SIF_USERPACKED
  if (RB->GetClass() == DXIL::ResourceClass::Sampler) {
    DxilSampler *S = static_cast<DxilSampler *>(RB);
    if (S->GetSamplerKind() == DXIL::SamplerKind::Comparison)
      result |= D3D_SIF_COMPARISON_SAMPLER;
  }
  return result;
}

void DxilShaderReflection::CreateReflectionObjectForResource(DxilResourceBase *RB) {
  DxilResourceBase::Class C = RB->GetClass();
  DxilResource *R =
      (C == DXIL::ResourceClass::UAV || C == DXIL::ResourceClass::SRV)
          ? (DxilResource *)RB
          : nullptr;
  D3D12_SHADER_INPUT_BIND_DESC inputBind;
  ZeroMemory(&inputBind, sizeof(inputBind));
  inputBind.BindCount = RB->GetRangeSize();
  if (RB->GetRangeSize() == UINT_MAX)
    inputBind.BindCount = 0;
  inputBind.BindPoint = RB->GetLowerBound();
  inputBind.Dimension = ResourceToDimension(RB);
  inputBind.Name = RB->GetGlobalName().c_str();
  inputBind.Type = ResourceToShaderInputType(RB);
  if (R == nullptr) {
    inputBind.NumSamples = 0;
  }
  else {
    inputBind.NumSamples = R->GetSampleCount();
    if (inputBind.NumSamples == 0) {
      if (R->IsStructuredBuffer()) {
        inputBind.NumSamples = CalcResTypeSize(*m_pDxilModule, *R);
      }
      else if (!R->IsRawBuffer()) {
        inputBind.NumSamples = 0xFFFFFFFF;
      }
    }
  }
  inputBind.ReturnType = ResourceToReturnType(RB);
  inputBind.Space = RB->GetSpaceID();
  inputBind.uFlags = ResourceToFlags(RB);
  inputBind.uID = RB->GetID();
  m_Resources.push_back(inputBind);
}

// Find the imm offset part from a value.
// It must exist unless offset is 0.
static unsigned GetCBOffset(Value *V) {
  if (ConstantInt *Imm = dyn_cast<ConstantInt>(V))
    return Imm->getLimitedValue();
  else if (UnaryInstruction *UI = dyn_cast<UnaryInstruction>(V)) {
    return 0;
  } else if (BinaryOperator *BO = dyn_cast<BinaryOperator>(V)) {
    switch (BO->getOpcode()) {
    case Instruction::Add: {
      unsigned left = GetCBOffset(BO->getOperand(0));
      unsigned right = GetCBOffset(BO->getOperand(1));
      return left + right;
    } break;
    case Instruction::Or: {
      unsigned left = GetCBOffset(BO->getOperand(0));
      unsigned right = GetCBOffset(BO->getOperand(1));
      return left | right;
    } break;
    default:
      return 0;
    }
  } else {
    return 0;
  }
}

void CollectInPhiChain(PHINode *cbUser, std::vector<unsigned> &cbufUsage,
                  unsigned offset, std::unordered_set<Value *> &userSet) {
  if (userSet.count(cbUser) > 0)
    return;

  userSet.insert(cbUser);
  for (User *cbU : cbUser->users()) {
    if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(cbU)) {
      for (unsigned idx : EV->getIndices()) {
        cbufUsage.emplace_back(offset + idx * 4);
      }
    } else {
      PHINode *phi = cast<PHINode>(cbU);
      CollectInPhiChain(phi, cbufUsage, offset, userSet);
    }
  }
}

static void CollectCBufUsage(Value *cbHandle,
                             std::vector<unsigned> &cbufUsage) {
  for (User *U : cbHandle->users()) {
    CallInst *CI = cast<CallInst>(U);
    ConstantInt *opcodeV =
        cast<ConstantInt>(CI->getArgOperand(DXIL::OperandIndex::kOpcodeIdx));
    DXIL::OpCode opcode = static_cast<DXIL::OpCode>(opcodeV->getLimitedValue());
    if (opcode == DXIL::OpCode::CBufferLoadLegacy) {
      DxilInst_CBufferLoadLegacy cbload(CI);
      Value *resIndex = cbload.get_regIndex();
      unsigned offset = GetCBOffset(resIndex);
      // 16 bytes align.
      offset <<= 4;
      for (User *cbU : U->users()) {
        if (ExtractValueInst *EV = dyn_cast<ExtractValueInst>(cbU)) {
          for (unsigned idx : EV->getIndices()) {
            cbufUsage.emplace_back(offset + idx * 4);
          }
        } else {
          PHINode *phi = cast<PHINode>(cbU);
          std::unordered_set<Value *> userSet;
          CollectInPhiChain(phi, cbufUsage, offset, userSet);
        }
      }
    } else if (opcode == DXIL::OpCode::CBufferLoad) {
      DxilInst_CBufferLoad cbload(CI);
      Value *byteOffset = cbload.get_byteOffset();
      unsigned offset = GetCBOffset(byteOffset);
      cbufUsage.emplace_back(offset);
    } else {
      //
      DXASSERT(0, "invalid opcode");
    }
  }
}

static void SetCBufVarUsage(CShaderReflectionConstantBuffer &cb,
                            std::vector<unsigned> usage) {
  D3D12_SHADER_BUFFER_DESC Desc;
  if (FAILED(cb.GetDesc(&Desc)))
    return;

  unsigned size = Desc.Variables;

  std::sort(usage.begin(), usage.end());
  for (unsigned i = 0; i < size; i++) {
    ID3D12ShaderReflectionVariable *pVar = cb.GetVariableByIndex(i);
    D3D12_SHADER_VARIABLE_DESC VarDesc;
    if (FAILED(pVar->GetDesc(&VarDesc)))
      continue;
    if (!pVar)
      continue;

    unsigned begin = VarDesc.StartOffset;
    unsigned end = begin + VarDesc.Size;
    auto beginIt = std::find_if(usage.begin(), usage.end(),
                                [&](unsigned v) { return v >= begin; });
    auto endIt = std::find_if(usage.begin(), usage.end(),
                              [&](unsigned v) { return v >= end; });

    bool used = beginIt != endIt;
    // Clear used.
    if (!used) {
      CShaderReflectionType *pVarType = (CShaderReflectionType *)pVar->GetType();
      BYTE *pDefaultValue = nullptr;

      VarDesc.uFlags &= ~D3D_SVF_USED;
      CShaderReflectionVariable *pCVarDesc = (CShaderReflectionVariable*)pVar;
      pCVarDesc->Initialize(&cb, &VarDesc, pVarType, pDefaultValue);
    }
  }
}

void DxilShaderReflection::SetCBufferUsage() {
  hlsl::OP *hlslOP = m_pDxilModule->GetOP();
  LLVMContext &Ctx = m_pDxilModule->GetCtx();
  unsigned cbSize = m_CBs.size();
  std::vector< std::vector<unsigned> > cbufUsage(cbSize);

  Function *createHandle = hlslOP->GetOpFunc(DXIL::OpCode::CreateHandle, Type::getVoidTy(Ctx));

  if (createHandle->user_empty()) {
    createHandle->eraseFromParent();
    return;
  }

  // Find all cb handles.
  for (User *U : createHandle->users()) {
    DxilInst_CreateHandle handle(cast<CallInst>(U));
    Value *resClass = handle.get_resourceClass();
    ConstantInt *immResClass = cast<ConstantInt>(resClass);
    if (immResClass->getLimitedValue() == (unsigned)DXIL::ResourceClass::CBuffer) {
      ConstantInt *cbID = cast<ConstantInt>(handle.get_rangeId());
      CollectCBufUsage(U, cbufUsage[cbID->getLimitedValue()]);
    }
  }

  for (unsigned i=0;i<cbSize;i++) {
    SetCBufVarUsage(m_CBs[i], cbufUsage[i]);
  }
}

void DxilShaderReflection::CreateReflectionObjects() {
  DXASSERT_NOMSG(m_pDxilModule != nullptr);

  // Create constant buffers, resources and signatures.
  for (auto && cb : m_pDxilModule->GetCBuffers()) {
    CShaderReflectionConstantBuffer rcb;
    rcb.Initialize(*m_pDxilModule, *(cb.get()));
    m_CBs.push_back(std::move(rcb));
  }
  // Set cbuf usage.
  SetCBufferUsage();

  // TODO: add tbuffers into m_CBs
  for (auto && uav : m_pDxilModule->GetUAVs()) {
    if (uav->GetKind() != DxilResource::Kind::StructuredBuffer) {
      continue;
    }
    CShaderReflectionConstantBuffer rcb;
    rcb.InitializeStructuredBuffer(*m_pDxilModule, *(uav.get()));
    m_CBs.push_back(std::move(rcb));
  }
  for (auto && srv : m_pDxilModule->GetSRVs()) {
    if (srv->GetKind() != DxilResource::Kind::StructuredBuffer) {
      continue;
    }
    CShaderReflectionConstantBuffer rcb;
    rcb.InitializeStructuredBuffer(*m_pDxilModule, *(srv.get()));
    m_CBs.push_back(std::move(rcb));
  }

  // Populate all resources.
  for (auto && cbRes : m_pDxilModule->GetCBuffers()) {
    CreateReflectionObjectForResource(cbRes.get());
  }
  for (auto && samplerRes : m_pDxilModule->GetSamplers()) {
    CreateReflectionObjectForResource(samplerRes.get());
  }
  for (auto && srvRes : m_pDxilModule->GetSRVs()) {
    CreateReflectionObjectForResource(srvRes.get());
  }
  for (auto && uavRes : m_pDxilModule->GetUAVs()) {
    CreateReflectionObjectForResource(uavRes.get());
  }

  // Populate input/output/patch constant signatures.
  CreateReflectionObjectsForSignature(m_pDxilModule->GetInputSignature(), m_InputSignature);
  CreateReflectionObjectsForSignature(m_pDxilModule->GetOutputSignature(), m_OutputSignature);
  CreateReflectionObjectsForSignature(m_pDxilModule->GetPatchConstantSignature(), m_PatchConstantSignature);
  MarkUsedSignatureElements();
}

static D3D_REGISTER_COMPONENT_TYPE CompTypeToRegisterComponentType(CompType CT) {
  switch (CT.GetKind()) {
  case DXIL::ComponentType::F16:
  case DXIL::ComponentType::F32:
    return D3D_REGISTER_COMPONENT_FLOAT32;
  case DXIL::ComponentType::I1:
  case DXIL::ComponentType::U16:
  case DXIL::ComponentType::U32:
    return D3D_REGISTER_COMPONENT_UINT32;
  case DXIL::ComponentType::I16:
  case DXIL::ComponentType::I32:
    return D3D_REGISTER_COMPONENT_SINT32;
  default:
    return D3D_REGISTER_COMPONENT_UNKNOWN;
  }
}

static D3D_MIN_PRECISION CompTypeToMinPrecision(CompType CT) {
  switch (CT.GetKind()) {
  case DXIL::ComponentType::F16:
    return D3D_MIN_PRECISION_FLOAT_16;
  case DXIL::ComponentType::I16:
    return D3D_MIN_PRECISION_SINT_16;
  case DXIL::ComponentType::U16:
    return D3D_MIN_PRECISION_UINT_16;
  default:
    return D3D_MIN_PRECISION_DEFAULT;
  }
}

D3D_NAME SemanticToSystemValueType(const Semantic *S, DXIL::TessellatorDomain domain) {
  switch (S->GetKind()) {
  case Semantic::Kind::ClipDistance:
    return D3D_NAME_CLIP_DISTANCE;
  case Semantic::Kind::Arbitrary:
    return D3D_NAME_UNDEFINED;
  case Semantic::Kind::VertexID:
    return D3D_NAME_VERTEX_ID;
  case Semantic::Kind::InstanceID:
    return D3D_NAME_INSTANCE_ID;
  case Semantic::Kind::Position:
    return D3D_NAME_POSITION;
  case Semantic::Kind::Coverage:
    return D3D_NAME_COVERAGE;
  case Semantic::Kind::InnerCoverage:
    return D3D_NAME_INNER_COVERAGE;
  case Semantic::Kind::PrimitiveID:
    return D3D_NAME_PRIMITIVE_ID;
  case Semantic::Kind::SampleIndex:
    return D3D_NAME_SAMPLE_INDEX;
  case Semantic::Kind::IsFrontFace:
    return D3D_NAME_IS_FRONT_FACE;
  case Semantic::Kind::RenderTargetArrayIndex:
    return D3D_NAME_RENDER_TARGET_ARRAY_INDEX;
  case Semantic::Kind::ViewPortArrayIndex:
    return D3D_NAME_VIEWPORT_ARRAY_INDEX;
  case Semantic::Kind::CullDistance:
    return D3D_NAME_CULL_DISTANCE;
  case Semantic::Kind::Target:
    return D3D_NAME_TARGET;
  case Semantic::Kind::Depth:
    return D3D_NAME_DEPTH;
  case Semantic::Kind::DepthLessEqual:
    return D3D_NAME_DEPTH_LESS_EQUAL;
  case Semantic::Kind::DepthGreaterEqual:
    return D3D_NAME_DEPTH_GREATER_EQUAL;
  case Semantic::Kind::StencilRef:
    return D3D_NAME_STENCIL_REF;
  case Semantic::Kind::TessFactor: {
    switch (domain) {
    case DXIL::TessellatorDomain::IsoLine:
        return D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR;
    case DXIL::TessellatorDomain::Tri:
        return D3D_NAME_FINAL_TRI_EDGE_TESSFACTOR;
    case DXIL::TessellatorDomain::Quad:
        return D3D_NAME_FINAL_QUAD_EDGE_TESSFACTOR;
    default:
    return D3D_NAME_UNDEFINED;
    }
  }
  case Semantic::Kind::InsideTessFactor:
    switch (domain) {
    case DXIL::TessellatorDomain::Tri:
        return D3D_NAME_FINAL_TRI_INSIDE_TESSFACTOR;
    case DXIL::TessellatorDomain::Quad:
        return D3D_NAME_FINAL_QUAD_INSIDE_TESSFACTOR;
    default:
    return D3D_NAME_UNDEFINED;
    }
  case Semantic::Kind::DispatchThreadID:
  case Semantic::Kind::GroupID:
  case Semantic::Kind::GroupIndex:
  case Semantic::Kind::GroupThreadID:
  case Semantic::Kind::DomainLocation:
  case Semantic::Kind::OutputControlPointID:
  case Semantic::Kind::GSInstanceID:
  case Semantic::Kind::Invalid:
  default:
    return D3D_NAME_UNDEFINED;
  }
}

static uint8_t NegMask(uint8_t V) {
  V ^= 0xF;
  return V & 0xF;
}

void DxilShaderReflection::CreateReflectionObjectsForSignature(
  const DxilSignature &Sig,
  std::vector<D3D12_SIGNATURE_PARAMETER_DESC> &Descs) {
  bool clipDistanceSeen = false;
  for (auto && SigElem : Sig.GetElements()) {
    D3D12_SIGNATURE_PARAMETER_DESC Desc;

    // TODO: why do we have multiple SV_ClipDistance elements?
    if (SigElem->GetSemantic()->GetKind() == DXIL::SemanticKind::ClipDistance) {
      if (clipDistanceSeen) continue;
      clipDistanceSeen = true;
    }

    Desc.ComponentType = CompTypeToRegisterComponentType(SigElem->GetCompType());
    Desc.Mask = SigElem->GetColsAsMask();
    // D3D11_43 does not have MinPrecison.
    if (m_PublicAPI != PublicAPI::D3D11_43)
      Desc.MinPrecision = CompTypeToMinPrecision(SigElem->GetCompType());
    Desc.ReadWriteMask = Sig.IsInput() ? 0 : Desc.Mask; // Start with output-never-written/input-never-read.
    Desc.Register = SigElem->GetStartRow();
    Desc.Stream = SigElem->GetOutputStream();
    Desc.SystemValueType = SemanticToSystemValueType(SigElem->GetSemantic(), m_pDxilModule->GetTessellatorDomain());
    Desc.SemanticName = SigElem->GetName();
    if (!SigElem->GetSemantic()->IsArbitrary())
      Desc.SemanticName = CreateUpperCase(Desc.SemanticName);

    const std::vector<unsigned> &indexVec = SigElem->GetSemanticIndexVec();
    for (unsigned semIdx = 0; semIdx < indexVec.size(); ++semIdx) {
      Desc.SemanticIndex = indexVec[semIdx];
      if (Desc.SystemValueType == D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR &&
          Desc.SemanticIndex == 1)
        Desc.SystemValueType = D3D_NAME_FINAL_LINE_DETAIL_TESSFACTOR;
      Descs.push_back(Desc);
    }
  }
}

LPCSTR DxilShaderReflection::CreateUpperCase(LPCSTR pValue) {
  // Restricted only to [a-z] ASCII.
  LPCSTR pCursor = pValue;
  while (*pCursor != '\0') {
    if ('a' <= *pCursor && *pCursor <= 'z') {
      break;
    }
    ++pCursor;
  }
  if (*pCursor == '\0')
    return pValue;

  std::unique_ptr<char[]> pUpperStr = std::make_unique<char[]>(strlen(pValue) + 1);
  char *pWrite = pUpperStr.get();
  pCursor = pValue;
  for (;;) {
    *pWrite = *pCursor;
    if ('a' <= *pWrite && *pWrite <= 'z') {
      *pWrite += ('A' - 'a');
    }
    if (*pWrite == '\0') break;
    ++pWrite;
    ++pCursor;
  }
  m_UpperCaseNames.push_back(std::move(pUpperStr));
  return m_UpperCaseNames.back().get();
}

HRESULT DxilShaderReflection::Load(IDxcBlob *pBlob,
                                   const DxilPartHeader *pPart) {
  DXASSERT_NOMSG(pBlob != nullptr);
  DXASSERT_NOMSG(pPart != nullptr);
  m_pContainer = pBlob;
  const char *pData = GetDxilPartData(pPart);
  try {
    const char *pBitcode;
    uint32_t bitcodeLength;
    GetDxilProgramBitcode((DxilProgramHeader *)pData, &pBitcode, &bitcodeLength);
    std::unique_ptr<MemoryBuffer> pMemBuffer =
        MemoryBuffer::getMemBufferCopy(StringRef(pBitcode, bitcodeLength));
#if 0 // We materialize eagerly, because we'll need to walk instructions to look for usage information.
    ErrorOr<std::unique_ptr<Module>> module =
        getLazyBitcodeModule(std::move(pMemBuffer), Context);
#else
    ErrorOr<std::unique_ptr<Module>> module =
      parseBitcodeFile(pMemBuffer->getMemBufferRef(), Context, nullptr);
#endif
    if (!module) {
      return E_INVALIDARG;
    }
    std::swap(m_pModule, module.get());
    m_pDxilModule = &m_pModule->GetOrCreateDxilModule();
    CreateReflectionObjects();
    return S_OK;
  }
  CATCH_CPP_RETURN_HRESULT();
};

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetDesc(D3D12_SHADER_DESC *pDesc) {
  IFR(ZeroMemoryToOut(pDesc));
  const DxilModule &M = *m_pDxilModule;
  const ShaderModel *pSM = M.GetShaderModel();

  pDesc->Version = EncodeVersion(pSM->GetKind(), pSM->GetMajor(), pSM->GetMinor());
  // Unset:  LPCSTR                  Creator;                     // Creator string
  // Unset:  UINT                    Flags;                       // Shader compilation/parse flags

  pDesc->ConstantBuffers = m_CBs.size();
  pDesc->BoundResources = m_Resources.size();
  pDesc->InputParameters = m_InputSignature.size();
  pDesc->OutputParameters = m_OutputSignature.size();
  pDesc->PatchConstantParameters = m_PatchConstantSignature.size();

  // Unset:  UINT                    InstructionCount;            // Number of emitted instructions
  // Unset:  UINT                    TempRegisterCount;           // Number of temporary registers used 
  // Unset:  UINT                    TempArrayCount;              // Number of temporary arrays used
  // Unset:  UINT                    DefCount;                    // Number of constant defines 
  // Unset:  UINT                    DclCount;                    // Number of declarations (input + output)
  // Unset:  UINT                    TextureNormalInstructions;   // Number of non-categorized texture instructions
  // Unset:  UINT                    TextureLoadInstructions;     // Number of texture load instructions
  // Unset:  UINT                    TextureCompInstructions;     // Number of texture comparison instructions
  // Unset:  UINT                    TextureBiasInstructions;     // Number of texture bias instructions
  // Unset:  UINT                    TextureGradientInstructions; // Number of texture gradient instructions
  // Unset:  UINT                    FloatInstructionCount;       // Number of floating point arithmetic instructions used
  // Unset:  UINT                    IntInstructionCount;         // Number of signed integer arithmetic instructions used
  // Unset:  UINT                    UintInstructionCount;        // Number of unsigned integer arithmetic instructions used
  // Unset:  UINT                    StaticFlowControlCount;      // Number of static flow control instructions used
  // Unset:  UINT                    DynamicFlowControlCount;     // Number of dynamic flow control instructions used
  // Unset:  UINT                    MacroInstructionCount;       // Number of macro instructions used
  // Unset:  UINT                    ArrayInstructionCount;       // Number of array instructions used
  // Unset:  UINT                    CutInstructionCount;         // Number of cut instructions used
  // Unset:  UINT                    EmitInstructionCount;        // Number of emit instructions used
  // Unset:  D3D_PRIMITIVE_TOPOLOGY  GSOutputTopology;            // Geometry shader output topology
  // Unset:  UINT                    GSMaxOutputVertexCount;      // Geometry shader maximum output vertex count
  // Unset:  D3D_PRIMITIVE           InputPrimitive;              // GS/HS input primitive
  // Unset:  UINT                    cGSInstanceCount;            // Number of Geometry shader instances
  // Unset:  UINT                    cControlPoints;              // Number of control points in the HS->DS stage
  // Unset:  D3D_TESSELLATOR_OUTPUT_PRIMITIVE HSOutputPrimitive;  // Primitive output by the tessellator
  // Unset:  D3D_TESSELLATOR_PARTITIONING HSPartitioning;         // Partitioning mode of the tessellator
  // Unset:  D3D_TESSELLATOR_DOMAIN  TessellatorDomain;           // Domain of the tessellator (quad, tri, isoline)
  // instruction counts
  // Unset:  UINT cBarrierInstructions;                           // Number of barrier instructions in a compute shader
  // Unset:  UINT cInterlockedInstructions;                       // Number of interlocked instructions
  // Unset:  UINT cTextureStoreInstructions;                      // Number of texture writes
  return S_OK;
}

static bool GetUnsignedVal(Value *V, uint32_t *pValue) {
  ConstantInt *CI = dyn_cast<ConstantInt>(V);
  if (!CI) return false;
  uint64_t u = CI->getZExtValue();
  if (u > UINT32_MAX) return false;
  *pValue = (uint32_t)u;
  return true;
}

void DxilShaderReflection::MarkUsedSignatureElements() {
  Function *F = m_pDxilModule->GetEntryFunction();
  DXASSERT(F != nullptr, "else module load should have failed");
  // For every loadInput/storeOutput, update the corresponding ReadWriteMask.
  // F is a pointer to a Function instance
  unsigned elementCount = m_InputSignature.size() + m_OutputSignature.size() +
                          m_PatchConstantSignature.size();
  unsigned markedElementCount = 0;
  for (inst_iterator I = inst_begin(F), E = inst_end(F); I != E; ++I) {
    DxilInst_LoadInput LI(&*I);
    DxilInst_StoreOutput SO(&*I);
    DxilInst_LoadPatchConstant LPC(&*I);
    DxilInst_StorePatchConstant SPC(&*I);
    std::vector<D3D12_SIGNATURE_PARAMETER_DESC> *pDescs;
    const DxilSignature *pSig;
    uint32_t col, row, sigId;
    if (LI) {
      if (!GetUnsignedVal(LI.get_inputSigId(), &sigId)) continue;
      if (!GetUnsignedVal(LI.get_colIndex(), &col)) continue;
      if (!GetUnsignedVal(LI.get_rowIndex(), &row)) continue;
      pDescs = &m_InputSignature;
      pSig = &m_pDxilModule->GetInputSignature();
    }
    else if (SO) {
      if (!GetUnsignedVal(SO.get_outputtSigId(), &sigId)) continue;
      if (!GetUnsignedVal(SO.get_colIndex(), &col)) continue;
      if (!GetUnsignedVal(SO.get_rowIndex(), &row)) continue;
      pDescs = &m_OutputSignature;
      pSig = &m_pDxilModule->GetOutputSignature();
    }
    else if (SPC) {
      if (!GetUnsignedVal(SPC.get_outputSigID(), &sigId)) continue;
      if (!GetUnsignedVal(SPC.get_col(), &col)) continue;
      if (!GetUnsignedVal(SPC.get_row(), &row)) continue;
      pDescs = &m_PatchConstantSignature;
      pSig = &m_pDxilModule->GetPatchConstantSignature();
    }
    else if (LPC) {
      if (!GetUnsignedVal(LPC.get_inputSigId(), &sigId)) continue;
      if (!GetUnsignedVal(LPC.get_col(), &col)) continue;
      if (!GetUnsignedVal(LPC.get_row(), &row)) continue;
      pDescs = &m_PatchConstantSignature;
      pSig = &m_pDxilModule->GetPatchConstantSignature();
    }
    else {
      continue;
    }

    if (sigId >= pDescs->size()) continue;

    D3D12_SIGNATURE_PARAMETER_DESC *pDesc = &(*pDescs)[sigId];
    // Consider being more fine-grained about masks.
    // We report sometimes-read on input as always-read.
    unsigned UsedMask = pSig->IsInput() ? pDesc->Mask : NegMask(pDesc->Mask);
    if (pDesc->ReadWriteMask == UsedMask)
      continue;
    pDesc->ReadWriteMask = UsedMask;
    ++markedElementCount;
    if (markedElementCount == elementCount)
      return;
  }
}

_Use_decl_annotations_
ID3D12ShaderReflectionConstantBuffer* DxilShaderReflection::GetConstantBufferByIndex(UINT Index) {
  if (Index >= m_CBs.size()) {
    return &g_InvalidSRConstantBuffer;
  }
  return &m_CBs[Index];
}

_Use_decl_annotations_
ID3D12ShaderReflectionConstantBuffer* DxilShaderReflection::GetConstantBufferByName(LPCSTR Name) {
  if (!Name) {
    return &g_InvalidSRConstantBuffer;
  }
  for (UINT index = 0; index < m_CBs.size(); ++index) {
    if (0 == strcmp(m_CBs[index].GetName(), Name)) {
      return &m_CBs[index];
    }
  }
  return &g_InvalidSRConstantBuffer;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetResourceBindingDesc(UINT ResourceIndex,
  _Out_ D3D12_SHADER_INPUT_BIND_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ResourceIndex < m_Resources.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D12) {
    memcpy(pDesc, &m_Resources[ResourceIndex], sizeof(D3D11_SHADER_INPUT_BIND_DESC));
  }
  else {
    *pDesc = m_Resources[ResourceIndex];
  }
  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetInputParameterDesc(UINT ParameterIndex,
  _Out_ D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_InputSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_InputSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_InputSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetOutputParameterDesc(UINT ParameterIndex,
  D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_OutputSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_OutputSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_OutputSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetPatchConstantParameterDesc(UINT ParameterIndex,
  D3D12_SIGNATURE_PARAMETER_DESC *pDesc) {
  IFRBOOL(pDesc != nullptr, E_INVALIDARG);
  IFRBOOL(ParameterIndex < m_PatchConstantSignature.size(), E_INVALIDARG);
  if (m_PublicAPI != PublicAPI::D3D11_43)
    *pDesc = m_PatchConstantSignature[ParameterIndex];
  else
    memcpy(pDesc, &m_PatchConstantSignature[ParameterIndex],
           // D3D11_43 does not have MinPrecison.
           sizeof(D3D12_SIGNATURE_PARAMETER_DESC) - sizeof(D3D_MIN_PRECISION));

  return S_OK;
}

_Use_decl_annotations_
ID3D12ShaderReflectionVariable* DxilShaderReflection::GetVariableByName(LPCSTR Name) {
  if (Name != nullptr) {
    // Iterate through all cbuffers to find the variable.
    for (UINT i = 0; i < m_CBs.size(); i++) {
      ID3D12ShaderReflectionVariable *pVar = m_CBs[i].GetVariableByName(Name);
      if (pVar != &g_InvalidSRVariable) {
        return pVar;
      }
    }
  }

  return &g_InvalidSRVariable;
}

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetResourceBindingDescByName(LPCSTR Name,
  D3D12_SHADER_INPUT_BIND_DESC *pDesc) {
  IFRBOOL(Name != nullptr, E_INVALIDARG);
  IFR(ZeroMemoryToOut(pDesc));

  for (UINT i = 0; i < m_Resources.size(); i++) {
    if (strcmp(m_Resources[i].Name, Name) == 0) {
      if (m_PublicAPI != PublicAPI::D3D12) {
        memcpy(pDesc, &m_Resources[i], sizeof(D3D11_SHADER_INPUT_BIND_DESC));
      }
      else {
        *pDesc = m_Resources[i];
      }
      return S_OK;
    }
  }

  return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
}

UINT DxilShaderReflection::GetMovInstructionCount() { return 0; }
UINT DxilShaderReflection::GetMovcInstructionCount() { return 0; }
UINT DxilShaderReflection::GetConversionInstructionCount() { return 0; }
UINT DxilShaderReflection::GetBitwiseInstructionCount() { return 0; }

D3D_PRIMITIVE DxilShaderReflection::GetGSInputPrimitive() {
  return (D3D_PRIMITIVE)m_pDxilModule->GetInputPrimitive();
}

BOOL DxilShaderReflection::IsSampleFrequencyShader() {
  // TODO: determine correct value
  return FALSE;
}

UINT DxilShaderReflection::GetNumInterfaceSlots() { return 0; }

_Use_decl_annotations_
HRESULT DxilShaderReflection::GetMinFeatureLevel(enum D3D_FEATURE_LEVEL* pLevel) {
  IFR(AssignToOut(D3D_FEATURE_LEVEL_12_0, pLevel));
  return S_OK;
}

_Use_decl_annotations_
UINT DxilShaderReflection::GetThreadGroupSize(UINT *pSizeX, UINT *pSizeY, UINT *pSizeZ) {
  UINT *pNumThreads = m_pDxilModule->m_NumThreads;
  AssignToOutOpt(pNumThreads[0], pSizeX);
  AssignToOutOpt(pNumThreads[1], pSizeY);
  AssignToOutOpt(pNumThreads[2], pSizeZ);
  return pNumThreads[0] * pNumThreads[1] * pNumThreads[2];
}

UINT64 DxilShaderReflection::GetRequiresFlags() {
  UINT64 result = 0;
  uint64_t features = m_pDxilModule->m_ShaderFlags.GetFeatureInfo();
  if (features & ShaderFeatureInfo_Doubles) result |= D3D_SHADER_REQUIRES_DOUBLES;
  if (features & ShaderFeatureInfo_UAVsAtEveryStage) result |= D3D_SHADER_REQUIRES_UAVS_AT_EVERY_STAGE;
  if (features & ShaderFeatureInfo_64UAVs) result |= D3D_SHADER_REQUIRES_64_UAVS;
  if (features & ShaderFeatureInfo_MininumPrecision) result |= D3D_SHADER_REQUIRES_MINIMUM_PRECISION;
  if (features & ShaderFeatureInfo_11_1_DoubleExtensions) result |= D3D_SHADER_REQUIRES_11_1_DOUBLE_EXTENSIONS;
  if (features & ShaderFeatureInfo_11_1_ShaderExtensions) result |= D3D_SHADER_REQUIRES_11_1_SHADER_EXTENSIONS;
  if (features & ShaderFeatureInfo_LEVEL9ComparisonFiltering) result |= D3D_SHADER_REQUIRES_LEVEL_9_COMPARISON_FILTERING;
  if (features & ShaderFeatureInfo_TiledResources) result |= D3D_SHADER_REQUIRES_TILED_RESOURCES;
  if (features & ShaderFeatureInfo_StencilRef) result |= D3D_SHADER_REQUIRES_STENCIL_REF;
  if (features & ShaderFeatureInfo_InnerCoverage) result |= D3D_SHADER_REQUIRES_INNER_COVERAGE;
  if (features & ShaderFeatureInfo_TypedUAVLoadAdditionalFormats) result |= D3D_SHADER_REQUIRES_TYPED_UAV_LOAD_ADDITIONAL_FORMATS;
  if (features & ShaderFeatureInfo_ROVs) result |= D3D_SHADER_REQUIRES_ROVS;
  if (features & ShaderFeatureInfo_ViewportAndRTArrayIndexFromAnyShaderFeedingRasterizer) result |= D3D_SHADER_REQUIRES_VIEWPORT_AND_RT_ARRAY_INDEX_FROM_ANY_SHADER_FEEDING_RASTERIZER;
  return result;
}
