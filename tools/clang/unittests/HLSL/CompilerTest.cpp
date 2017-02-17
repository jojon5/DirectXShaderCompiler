///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// CompilerTest.cpp                                                          //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests for the compiler API.                                      //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <cassert>
#include <sstream>
#include <algorithm>
#include "dxc/HLSL/DxilContainer.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include <atlfile.h>

#include "HLSLTestData.h"
#include "WexTestClass.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"
#include "dia2.h"

#include <fstream>

using namespace std;
using namespace hlsl_test;

std::wstring BlobToUtf16(_In_ IDxcBlob *pBlob) {
  CComPtr<IDxcBlobEncoding> pBlobEncoding;
  const UINT CP_UTF16 = 1200;
  IFT(pBlob->QueryInterface(&pBlobEncoding));
  BOOL known;
  UINT32 codePage;
  IFT(pBlobEncoding->GetEncoding(&known, &codePage));
  std::wstring result;
  if (codePage == CP_UTF16) {
    result.resize(pBlob->GetBufferSize() + 1);
    memcpy((void *)result.data(), pBlob->GetBufferPointer(),
           pBlob->GetBufferSize());
    return result;
  } else if (codePage == CP_UTF8) {
    Unicode::UTF8ToUTF16String((char *)pBlob->GetBufferPointer(),
                               pBlob->GetBufferSize(), &result);
    return result;
  } else {
    throw std::exception("Unsupported codepage.");
  }
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const char *pVal, _Outptr_ IDxcBlobEncoding **ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(pVal, strlen(pVal), CP_UTF8, ppBlob));
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val, _Outptr_ IDxcBlobEncoding **ppBlob) {
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(val.data(), val.size(), CP_UTF8, ppBlob));
}

void Utf8ToBlob(dxc::DxcDllSupport &dllSupport, const std::string &val, _Outptr_ IDxcBlob **ppBlob) {
  Utf8ToBlob(dllSupport, val, (IDxcBlobEncoding**)ppBlob);
}

void Utf16ToBlob(dxc::DxcDllSupport &dllSupport, const std::wstring &val, _Outptr_ IDxcBlobEncoding **ppBlob) {
  const UINT32 CP_UTF16 = 1200;
  CComPtr<IDxcLibrary> library;
  IFT(dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
  IFT(library->CreateBlobWithEncodingOnHeapCopy(val.data(), val.size() * sizeof(wchar_t), CP_UTF16, ppBlob));
}

void Utf16ToBlob(dxc::DxcDllSupport &dllSupport, const std::wstring &val, _Outptr_ IDxcBlob **ppBlob) {
  Utf16ToBlob(dllSupport, val, (IDxcBlobEncoding**)ppBlob);
}

// Aligned to SymTagEnum.
const char *SymTagEnumText[] =
{
  "Null", // SymTagNull
  "Exe", // SymTagExe
  "Compiland", // SymTagCompiland
  "CompilandDetails", // SymTagCompilandDetails
  "CompilandEnv", // SymTagCompilandEnv
  "Function", // SymTagFunction
  "Block", // SymTagBlock
  "Data", // SymTagData
  "Annotation", // SymTagAnnotation
  "Label", // SymTagLabel
  "PublicSymbol", // SymTagPublicSymbol
  "UDT", // SymTagUDT
  "Enum", // SymTagEnum
  "FunctionType", // SymTagFunctionType
  "PointerType", // SymTagPointerType
  "ArrayType", // SymTagArrayType
  "BaseType", // SymTagBaseType
  "Typedef", // SymTagTypedef
  "BaseClass", // SymTagBaseClass
  "Friend", // SymTagFriend
  "FunctionArgType", // SymTagFunctionArgType
  "FuncDebugStart", // SymTagFuncDebugStart
  "FuncDebugEnd", // SymTagFuncDebugEnd
  "UsingNamespace", // SymTagUsingNamespace
  "VTableShape", // SymTagVTableShape
  "VTable", // SymTagVTable
  "Custom", // SymTagCustom
  "Thunk", // SymTagThunk
  "CustomType", // SymTagCustomType
  "ManagedType", // SymTagManagedType
  "Dimension", // SymTagDimension
  "CallSite", // SymTagCallSite
  "InlineSite", // SymTagInlineSite
  "BaseInterface", // SymTagBaseInterface
  "VectorType", // SymTagVectorType
  "MatrixType", // SymTagMatrixType
  "HLSLType", // SymTagHLSLType
  "Caller", // SymTagCaller
  "Callee", // SymTagCallee
  "Export", // SymTagExport
  "HeapAllocationSite", // SymTagHeapAllocationSite
  "CoffGroup", // SymTagCoffGroup
};

// Aligned to LocationType.
const char *LocationTypeText[] =
{
  "Null",
  "Static",
  "TLS",
  "RegRel",
  "ThisRel",
  "Enregistered",
  "BitField",
  "Slot",
  "IlRel",
  "MetaData",
  "Constant",
};

// Aligned to DataKind.
const char *DataKindText[] =
{
  "Unknown",
  "Local",
  "StaticLocal",
  "Param",
  "ObjectPtr",
  "FileStatic",
  "Global",
  "Member",
  "StaticMember",
  "Constant",
};

// Aligned to UdtKind.
const char *UdtKindText[] =
{
  "Struct",
  "Class",
  "Union",
  "Interface",
};

// BasicType is not contiguous.
const char *GetBasicTypeText(enum BasicType value) {
  switch (value) {
  case btNoType: return "NoType";
  case btVoid: return "Void";
  case btChar: return "Char";
  case btWChar: return "WChar";
  case btInt: return "Int";
  case btUInt: return "UInt";
  case btFloat: return "Float";
  case btBCD: return "BCD";
  case btBool: return "Bool";
  case btLong: return "Long";
  case btULong: return "ULong";
  case btCurrency: return "Currency";
  case btDate: return "Date";
  case btVariant: return "Variant";
  case btComplex: return "Complex";
  case btBit: return "Bit";
  case btBSTR: return "BSTR";
  case btHresult: return "Hresult";
  // The following may not be present in cvconst.h
  //case btChar16: return "Char16";
  //case btChar32: return "Char32";
  }
  return "?";
}


class TestIncludeHandler : public IDxcIncludeHandler {
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  dxc::DxcDllSupport &m_dllSupport;
  HRESULT m_defaultErrorCode = E_FAIL;
  TestIncludeHandler(dxc::DxcDllSupport &dllSupport) : m_dwRef(0), callIndex(0), m_dllSupport(dllSupport) { }
  __override HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this,  iid, ppvObject);
  }

  struct LoadSourceCallInfo {
    std::wstring Filename;     // Filename as written in #include statement
    LoadSourceCallInfo(LPCWSTR pFilename) :
      Filename(pFilename) { }
  };
  std::vector<LoadSourceCallInfo> CallInfos;
  std::wstring GetAllFileNames() const {
    std::wstringstream s;
    for (size_t i = 0; i < CallInfos.size(); ++i) {
      s << CallInfos[i].Filename << ';';
    }
    return s.str();
  }
  struct LoadSourceCallResult {
    HRESULT hr;
    std::string source;
    LoadSourceCallResult() : hr(E_FAIL) { }
    LoadSourceCallResult(const char *pSource) : hr(S_OK), source(pSource) { }
  };
  std::vector<LoadSourceCallResult> CallResults;
  size_t callIndex;

  __override HRESULT STDMETHODCALLTYPE LoadSource(
    _In_ LPCWSTR pFilename,                   // Filename as written in #include statement
    _COM_Outptr_ IDxcBlob **ppIncludeSource   // Resultant source object for included file
    ) {
    CallInfos.push_back(LoadSourceCallInfo(pFilename));

    *ppIncludeSource = nullptr;
    if (callIndex >= CallResults.size()) {
      return m_defaultErrorCode;
    }
    if (FAILED(CallResults[callIndex].hr)) {
      return CallResults[callIndex++].hr;
    }
    Utf8ToBlob(m_dllSupport, CallResults[callIndex].source, ppIncludeSource);
    return CallResults[callIndex++].hr;
  }
};

class CompilerTest {
public:
  BEGIN_TEST_CLASS(CompilerTest)
    TEST_CLASS_PROPERTY(L"Parallel", L"true")
    TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDebugThenDIPresent)

  TEST_METHOD(CompileWhenDefinesThenApplied)
  TEST_METHOD(CompileWhenDefinesManyThenApplied)
  TEST_METHOD(CompileWhenEmptyThenFails)
  TEST_METHOD(CompileWhenIncorrectThenFails)
  TEST_METHOD(CompileWhenWorksThenDisassembleWorks)
  TEST_METHOD(CompileWhenDebugWorksThenStripDebug)
  TEST_METHOD(CompileWhenWorksThenAddRemovePrivate)
  TEST_METHOD(CompileWithRootSignatureThenStripRootSignature)

  TEST_METHOD(CompileWhenIncludeThenLoadInvoked)
  TEST_METHOD(CompileWhenIncludeThenLoadUsed)
  TEST_METHOD(CompileWhenIncludeAbsoluteThenLoadAbsolute)
  TEST_METHOD(CompileWhenIncludeLocalThenLoadRelative)
  TEST_METHOD(CompileWhenIncludeSystemThenLoadNotRelative)
  TEST_METHOD(CompileWhenIncludeSystemMissingThenLoadAttempt)
  TEST_METHOD(CompileWhenIncludeFlagsThenIncludeUsed)
  TEST_METHOD(CompileWhenIncludeMissingThenFail)

  TEST_METHOD(CompileWhenODumpThenPassConfig)
  TEST_METHOD(CompileWhenODumpThenOptimizerMatch)
  TEST_METHOD(CompileWhenVdThenProducesDxilContainer)

  TEST_METHOD(CompileWhenShaderModelMismatchAttributeThenFail)
  TEST_METHOD(CompileBadHlslThenFail)
  TEST_METHOD(CompileLegacyShaderModelThenFail)

  TEST_METHOD(CodeGenAbs1)
  TEST_METHOD(CodeGenAbs2)
  TEST_METHOD(CodeGenAddUint64)
  TEST_METHOD(CodeGenArrayArg)
  TEST_METHOD(CodeGenArrayOfStruct)
  TEST_METHOD(CodeGenAsUint)
  TEST_METHOD(CodeGenAsUint2)
  TEST_METHOD(CodeGenAtomic)
  TEST_METHOD(CodeGenBinary1)
  TEST_METHOD(CodeGenBoolComb)
  TEST_METHOD(CodeGenBoolSvTarget)
  TEST_METHOD(CodeGenCalcLod2DArray)
  TEST_METHOD(CodeGenCall1)
  TEST_METHOD(CodeGenCall3)
  TEST_METHOD(CodeGenCast1)
  TEST_METHOD(CodeGenCast2)
  TEST_METHOD(CodeGenCast3)
  TEST_METHOD(CodeGenCast4)
  TEST_METHOD(CodeGenCast5)
  TEST_METHOD(CodeGenCast6)
  TEST_METHOD(CodeGenCbuffer_unused)
  TEST_METHOD(CodeGenCbuffer1_50)
  TEST_METHOD(CodeGenCbuffer1_51)
  TEST_METHOD(CodeGenCbuffer2_50)
  TEST_METHOD(CodeGenCbuffer2_51)
  TEST_METHOD(CodeGenCbuffer3_50)
  TEST_METHOD(CodeGenCbuffer3_51)
  TEST_METHOD(CodeGenCbuffer5_51)
  TEST_METHOD(CodeGenCbuffer6_51)
  TEST_METHOD(CodeGenCbufferAlloc)
  TEST_METHOD(CodeGenCbufferAllocLegacy)
  TEST_METHOD(CodeGenCbufferInLoop)
  TEST_METHOD(CodeGenClipPlanes)
  TEST_METHOD(CodeGenConstoperand1)
  TEST_METHOD(CodeGenDiscard)
  TEST_METHOD(CodeGenDivZero)
  TEST_METHOD(CodeGenDot1)
  TEST_METHOD(CodeGenDynamic_Resources)
  TEST_METHOD(CodeGenEffectSkip)
  TEST_METHOD(CodeGenEmpty)
  TEST_METHOD(CodeGenEmptyStruct)
  TEST_METHOD(CodeGenEarlyDepthStencil)
  TEST_METHOD(CodeGenEval)
  TEST_METHOD(CodeGenEvalPos)
  TEST_METHOD(CodeGenFirstbitHi)
  TEST_METHOD(CodeGenFirstbitLo)
  TEST_METHOD(CodeGenFloatMaxtessfactor)
  TEST_METHOD(CodeGenFModPS)
  TEST_METHOD(CodeGenGather)
  TEST_METHOD(CodeGenGatherCmp)
  TEST_METHOD(CodeGenGatherCubeOffset)
  TEST_METHOD(CodeGenGatherOffset)
  TEST_METHOD(CodeGenIcb1)
  TEST_METHOD(CodeGenIf1)
  TEST_METHOD(CodeGenIf2)
  TEST_METHOD(CodeGenIf3)
  TEST_METHOD(CodeGenIf4)
  TEST_METHOD(CodeGenIf5)
  TEST_METHOD(CodeGenIf6)
  TEST_METHOD(CodeGenIf7)
  TEST_METHOD(CodeGenIf8)
  TEST_METHOD(CodeGenIf9)
  TEST_METHOD(CodeGenImm0)
  TEST_METHOD(CodeGenInclude)
  TEST_METHOD(CodeGenIncompletePos)
  TEST_METHOD(CodeGenIndexableinput1)
  TEST_METHOD(CodeGenIndexableinput2)
  TEST_METHOD(CodeGenIndexableinput3)
  TEST_METHOD(CodeGenIndexableinput4)
  TEST_METHOD(CodeGenIndexableoutput1)
  TEST_METHOD(CodeGenIndexabletemp1)
  TEST_METHOD(CodeGenIndexabletemp2)
  TEST_METHOD(CodeGenIndexabletemp3)
  TEST_METHOD(CodeGenInoutSE)
  TEST_METHOD(CodeGenInout1)
  TEST_METHOD(CodeGenInout2)
  TEST_METHOD(CodeGenInout3)
  TEST_METHOD(CodeGenInput1)
  TEST_METHOD(CodeGenInput2)
  TEST_METHOD(CodeGenInput3)
  TEST_METHOD(CodeGenIntrinsic1)
  TEST_METHOD(CodeGenIntrinsic2)
  TEST_METHOD(CodeGenIntrinsic3_even)
  TEST_METHOD(CodeGenIntrinsic3_integer)
  TEST_METHOD(CodeGenIntrinsic3_odd)
  TEST_METHOD(CodeGenIntrinsic3_pow2)
  TEST_METHOD(CodeGenIntrinsic4)
  TEST_METHOD(CodeGenIntrinsic4_dbg)
  TEST_METHOD(CodeGenIntrinsic5)
  TEST_METHOD(CodeGenLegacyStruct)
  TEST_METHOD(CodeGenLitInParen)
  TEST_METHOD(CodeGenLiteralShift)
  TEST_METHOD(CodeGenLiveness1)
  TEST_METHOD(CodeGenLocalRes1)
  TEST_METHOD(CodeGenLocalRes4)
  TEST_METHOD(CodeGenLocalRes7)
  TEST_METHOD(CodeGenLocalRes7Dbg)
  TEST_METHOD(CodeGenLoop1)
  TEST_METHOD(CodeGenLoop2)
  TEST_METHOD(CodeGenLoop3)
  TEST_METHOD(CodeGenLoop4)
  TEST_METHOD(CodeGenLoop5)
  TEST_METHOD(CodeGenMatInit)
  TEST_METHOD(CodeGenMatMulMat)
  TEST_METHOD(CodeGenMatOps)
  TEST_METHOD(CodeGenMatInStruct)
  TEST_METHOD(CodeGenMatInStructRet)
  TEST_METHOD(CodeGenMatIn)
  TEST_METHOD(CodeGenMatOut)
  TEST_METHOD(CodeGenMatSubscript)
  TEST_METHOD(CodeGenMatSubscript2)
  TEST_METHOD(CodeGenMatSubscript3)
  TEST_METHOD(CodeGenMatSubscript4)
  TEST_METHOD(CodeGenMatSubscript5)
  TEST_METHOD(CodeGenMatSubscript6)
  TEST_METHOD(CodeGenMaxMin)
  TEST_METHOD(CodeGenMinprec1)
  TEST_METHOD(CodeGenMinprec2)
  TEST_METHOD(CodeGenMinprec3)
  TEST_METHOD(CodeGenMinprec4)
  TEST_METHOD(CodeGenMinprec5)
  TEST_METHOD(CodeGenMinprec6)
  TEST_METHOD(CodeGenMinprec7)
  TEST_METHOD(CodeGenMultiStream)
  TEST_METHOD(CodeGenMultiStream2)
  TEST_METHOD(CodeGenNeg1)
  TEST_METHOD(CodeGenNeg2)
  TEST_METHOD(CodeGenNegabs1)
  TEST_METHOD(CodeGenNonUniform)
  TEST_METHOD(CodeGenOptionGis)
  TEST_METHOD(CodeGenOptionWX)
  TEST_METHOD(CodeGenOutput1)
  TEST_METHOD(CodeGenOutput2)
  TEST_METHOD(CodeGenOutput3)
  TEST_METHOD(CodeGenOutput4)
  TEST_METHOD(CodeGenOutput5)
  TEST_METHOD(CodeGenOutput6)
  TEST_METHOD(CodeGenOutputArray)
  TEST_METHOD(CodeGenPassthrough1)
  TEST_METHOD(CodeGenPassthrough2)
  TEST_METHOD(CodeGenPrecise1)
  TEST_METHOD(CodeGenPrecise2)
  TEST_METHOD(CodeGenPrecise3)
  TEST_METHOD(CodeGenPrecise4)
  TEST_METHOD(CodeGenPreciseOnCall)
  TEST_METHOD(CodeGenPreciseOnCallNot)
  TEST_METHOD(CodeGenRaceCond2)
  TEST_METHOD(CodeGenRaw_Buf1)
  TEST_METHOD(CodeGenRcp1)
  TEST_METHOD(CodeGenReadFromOutput)
  TEST_METHOD(CodeGenReadFromOutput2)
  TEST_METHOD(CodeGenReadFromOutput3)
  TEST_METHOD(CodeGenRedundantinput1)
  TEST_METHOD(CodeGenRes64bit)
  TEST_METHOD(CodeGenRovs)
  TEST_METHOD(CodeGenRValSubscript)
  TEST_METHOD(CodeGenSample1)
  TEST_METHOD(CodeGenSample2)
  TEST_METHOD(CodeGenSample3)
  TEST_METHOD(CodeGenSample4)
  TEST_METHOD(CodeGenSample5)
  TEST_METHOD(CodeGenSampleBias)
  TEST_METHOD(CodeGenSampleCmp)
  TEST_METHOD(CodeGenSampleCmpLZ)
  TEST_METHOD(CodeGenSampleCmpLZ2)
  TEST_METHOD(CodeGenSampleGrad)
  TEST_METHOD(CodeGenSampleL)
  TEST_METHOD(CodeGenSaturate1)
  TEST_METHOD(CodeGenScalarOnVecIntrinsic)
  TEST_METHOD(CodeGenSelectObj)
  TEST_METHOD(CodeGenSelectObj2)
  TEST_METHOD(CodeGenSelectObj3)
  TEST_METHOD(CodeGenSelMat)
  TEST_METHOD(CodeGenShare_Mem_Dbg)
  TEST_METHOD(CodeGenShare_Mem1)
  TEST_METHOD(CodeGenShare_Mem2)
  TEST_METHOD(CodeGenShare_Mem2Dim)
  TEST_METHOD(CodeGenShift)
  TEST_METHOD(CodeGenSimpleDS1)
  TEST_METHOD(CodeGenSimpleGS1)
  TEST_METHOD(CodeGenSimpleGS2)
  TEST_METHOD(CodeGenSimpleGS3)
  TEST_METHOD(CodeGenSimpleGS4)
  TEST_METHOD(CodeGenSimpleGS5)
  TEST_METHOD(CodeGenSimpleGS6)
  TEST_METHOD(CodeGenSimpleGS7)
  TEST_METHOD(CodeGenSimpleGS11)
  TEST_METHOD(CodeGenSimpleGS12)
  TEST_METHOD(CodeGenSimpleHS1)
  TEST_METHOD(CodeGenSimpleHS2)
  TEST_METHOD(CodeGenSimpleHS3)
  TEST_METHOD(CodeGenSimpleHS4)
  TEST_METHOD(CodeGenSimpleHS5)
  TEST_METHOD(CodeGenSimpleHS6)
  TEST_METHOD(CodeGenSimpleHS7)
  TEST_METHOD(CodeGenSimpleHS8)
  TEST_METHOD(CodeGenSMFail)
  TEST_METHOD(CodeGenSrv_Ms_Load1)
  TEST_METHOD(CodeGenSrv_Ms_Load2)
  TEST_METHOD(CodeGenSrv_Typed_Load1)
  TEST_METHOD(CodeGenSrv_Typed_Load2)
  TEST_METHOD(CodeGenStaticGlobals)
  TEST_METHOD(CodeGenStaticGlobals2)
  TEST_METHOD(CodeGenStruct_Buf1)
  TEST_METHOD(CodeGenStruct_BufHasCounter)
  TEST_METHOD(CodeGenStruct_BufHasCounter2)
  TEST_METHOD(CodeGenStructCast)
  TEST_METHOD(CodeGenStructCast2)
  TEST_METHOD(CodeGenStructInBuffer)
  TEST_METHOD(CodeGenStructInBuffer2)
  TEST_METHOD(CodeGenStructInBuffer3)
  TEST_METHOD(CodeGenSwitchFloat)
  TEST_METHOD(CodeGenSwitch1)
  TEST_METHOD(CodeGenSwitch2)
  TEST_METHOD(CodeGenSwitch3)
  TEST_METHOD(CodeGenSwizzle1)
  TEST_METHOD(CodeGenSwizzle2)
  TEST_METHOD(CodeGenSwizzleAtomic)
  TEST_METHOD(CodeGenSwizzleAtomic2)
  TEST_METHOD(CodeGenTemp1)
  TEST_METHOD(CodeGenTemp2)
  TEST_METHOD(CodeGenTexSubscript)
  TEST_METHOD(CodeGenUav_Raw1)
  TEST_METHOD(CodeGenUav_Typed_Load_Store1)
  TEST_METHOD(CodeGenUav_Typed_Load_Store2)
  TEST_METHOD(CodeGenUint64_1)
  TEST_METHOD(CodeGenUint64_2)
  TEST_METHOD(CodeGenUintSample)
  TEST_METHOD(CodeGenUmaxObjectAtomic)
  TEST_METHOD(CodeGenUpdateCounter)
  TEST_METHOD(CodeGenUpperCaseRegister1);
  TEST_METHOD(CodeGenVcmp)
  TEST_METHOD(CodeGenVec_Comp_Arg)
  TEST_METHOD(CodeGenWave)
  TEST_METHOD(CodeGenWriteToInput)
  TEST_METHOD(CodeGenWriteToInput2)
  TEST_METHOD(CodeGenWriteToInput3)

  TEST_METHOD(CodeGenAttributes_Mod)
  TEST_METHOD(CodeGenConst_Exprb_Mod)
  TEST_METHOD(CodeGenConst_Expr_Mod)
  TEST_METHOD(CodeGenFunctions_Mod)
  TEST_METHOD(CodeGenImplicit_Casts_Mod)
  TEST_METHOD(CodeGenIndexing_Operator_Mod)
  TEST_METHOD(CodeGenIntrinsic_Examples_Mod)
  TEST_METHOD(CodeGenLiterals_Mod)
  TEST_METHOD(CodeGenMatrix_Assignments_Mod)
  TEST_METHOD(CodeGenMatrix_Syntax_Mod)
  //TEST_METHOD(CodeGenMore_Operators_Mod)
  //TEST_METHOD(CodeGenObject_Operators_Mod)
  TEST_METHOD(CodeGenPackreg_Mod)
  TEST_METHOD(CodeGenParameter_Types)
  TEST_METHOD(CodeGenScalar_Assignments_Mod)
  TEST_METHOD(CodeGenScalar_Operators_Assign_Mod)
  TEST_METHOD(CodeGenScalar_Operators_Mod)
  TEST_METHOD(CodeGenSemantics_Mod)
  //TEST_METHOD(CodeGenSpec_Mod)
  TEST_METHOD(CodeGenString_Mod)
  TEST_METHOD(CodeGenStruct_Assignments_Mod)
  TEST_METHOD(CodeGenStruct_AssignmentsFull_Mod)
  TEST_METHOD(CodeGenTemplate_Checks_Mod)
  TEST_METHOD(CodeGenToinclude2_Mod)
  TEST_METHOD(CodeGenTypemods_Syntax_Mod)
  TEST_METHOD(CodeGenVarmods_Syntax_Mod)
  TEST_METHOD(CodeGenVector_Assignments_Mod)
  TEST_METHOD(CodeGenVector_Syntax_Mix_Mod)
  TEST_METHOD(CodeGenVector_Syntax_Mod)
  TEST_METHOD(CodeGenBasicHLSL11_PS)
  TEST_METHOD(CodeGenBasicHLSL11_PS2)
  TEST_METHOD(CodeGenBasicHLSL11_PS3)
  TEST_METHOD(CodeGenBasicHLSL11_VS)
  TEST_METHOD(CodeGenBasicHLSL11_VS2)
  TEST_METHOD(CodeGenVecIndexingInput)
  TEST_METHOD(CodeGenVecMulMat)
  TEST_METHOD(CodeGenBindings1)
  TEST_METHOD(CodeGenBindings2)
  TEST_METHOD(CodeGenBindings3)
  TEST_METHOD(CodeGenResCopy)
  TEST_METHOD(CodeGenResourceInStruct)
  TEST_METHOD(CodeGenResourceInCB)
  TEST_METHOD(CodeGenResourceInCBV)
  TEST_METHOD(CodeGenResourceInTB)
  TEST_METHOD(CodeGenResourceInTBV)
  TEST_METHOD(CodeGenResourceInStruct2)
  TEST_METHOD(CodeGenResourceInCB2)
  TEST_METHOD(CodeGenResourceInCBV2)
  TEST_METHOD(CodeGenResourceInTB2)
  TEST_METHOD(CodeGenResourceInTBV2)
  TEST_METHOD(CodeGenRootSigEntry)
  TEST_METHOD(CodeGenCBufferStructArray)
  TEST_METHOD(PreprocessWhenValidThenOK)
  TEST_METHOD(WhenSigMismatchPCFunctionThenFail)

  // Dx11 Sample
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Blurx_Ps)
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Blury_Ps)
  TEST_METHOD(CodeGenDX11Sample_2Dquadshaders_Vs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hdecode)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Encodeblockcs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Trymodeg10Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc6Hencode_Trymodele10Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Decode)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Encodeblockcs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode02Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode137Cs)
  TEST_METHOD(CodeGenDX11Sample_Bc7Encode_Trymode456Cs)
  TEST_METHOD(CodeGenDX11Sample_Brightpassandhorizfiltercs)
  TEST_METHOD(CodeGenDX11Sample_Computeshadersort11)
  TEST_METHOD(CodeGenDX11Sample_Computeshadersort11_Matrixtranspose)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Sm_Vs)
  TEST_METHOD(CodeGenDX11Sample_Contacthardeningshadows11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Decaltessellation11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Ps)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Detailtessellation11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Dumptotexture)
  TEST_METHOD(CodeGenDX11Sample_Filtercs_Horz)
  TEST_METHOD(CodeGenDX11Sample_Filtercs_Vertical)
  TEST_METHOD(CodeGenDX11Sample_Finalpass_Cpu_Ps)
  TEST_METHOD(CodeGenDX11Sample_Finalpass_Ps)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Buildgridcs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Buildgridindicescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Cleargridindicescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Grid)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Shared)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Densitycs_Simple)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Grid)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Shared)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Forcecs_Simple)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Integratecs)
  TEST_METHOD(CodeGenDX11Sample_Fluidcs11_Rearrangeparticlescs)
  TEST_METHOD(CodeGenDX11Sample_Fluidrender_Gs)
  TEST_METHOD(CodeGenDX11Sample_Fluidrender_Vs)
  TEST_METHOD(CodeGenDX11Sample_Nbodygravitycs11)
  TEST_METHOD(CodeGenDX11Sample_Oit_Createprefixsum_Pass0_Cs)
  TEST_METHOD(CodeGenDX11Sample_Oit_Createprefixsum_Pass1_Cs)
  TEST_METHOD(CodeGenDX11Sample_Oit_Fragmentcountps)
  TEST_METHOD(CodeGenDX11Sample_Oit_Ps)
  TEST_METHOD(CodeGenDX11Sample_Oit_Sortandrendercs)
  TEST_METHOD(CodeGenDX11Sample_Particledraw_Gs)
  TEST_METHOD(CodeGenDX11Sample_Particledraw_Vs)
  TEST_METHOD(CodeGenDX11Sample_Particle_Gs)
  TEST_METHOD(CodeGenDX11Sample_Particle_Ps)
  TEST_METHOD(CodeGenDX11Sample_Particle_Vs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Ds)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Hs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Tessvs)
  TEST_METHOD(CodeGenDX11Sample_Pntriangles11_Vs)
  TEST_METHOD(CodeGenDX11Sample_Pom_Ps)
  TEST_METHOD(CodeGenDX11Sample_Pom_Vs)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Bloomps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale2X2_Lumps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale3X3Ps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Downscale3X3_Brightpassps)
  TEST_METHOD(CodeGenDX11Sample_Psapproach_Finalpassps)
  TEST_METHOD(CodeGenDX11Sample_Reduceto1Dcs)
  TEST_METHOD(CodeGenDX11Sample_Reducetosinglecs)
  TEST_METHOD(CodeGenDX11Sample_Rendervariancesceneps)
  TEST_METHOD(CodeGenDX11Sample_Rendervs)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Ds)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Hs)
  TEST_METHOD(CodeGenDX11Sample_Simplebezier11Ps)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Bezierevalds)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Meshskinningvs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Patchskinningvs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Smoothps)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Subdtobezierhs)
  TEST_METHOD(CodeGenDX11Sample_Subd11_Subdtobezierhs4444)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Edgefactorcs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Numverticesindicescs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Scatteridcs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Tessellateindicescs)
  TEST_METHOD(CodeGenDX11Sample_Tessellatorcs40_Tessellateverticescs)

  // Dx12 Sample
  TEST_METHOD(CodeGenSamplesD12_DynamicIndexing_PS)
  TEST_METHOD(CodeGenSamplesD12_ExecuteIndirect_CS)
  TEST_METHOD(CodeGenSamplesD12_MultiThreading_VS)
  TEST_METHOD(CodeGenSamplesD12_MultiThreading_PS)
  TEST_METHOD(CodeGenSamplesD12_NBodyGravity_CS)

  // Dx12 samples/MiniEngine.
  TEST_METHOD(CodeGenDx12MiniEngineAdaptexposurecs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsampleblendoutcs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplecs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplepreminblendoutcs)
  TEST_METHOD(CodeGenDx12MiniEngineAoblurupsamplepremincs)
  TEST_METHOD(CodeGenDx12MiniEngineAopreparedepthbuffers1Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAopreparedepthbuffers2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAorender1Cs)
  TEST_METHOD(CodeGenDx12MiniEngineAorender2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineApplybloomcs)
  TEST_METHOD(CodeGenDx12MiniEngineAveragelumacs)
  TEST_METHOD(CodeGenDx12MiniEngineBicubichorizontalupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicupsamplegammaps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBicubicverticalupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBilinearupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineBloomextractanddownsamplehdrcs)
  TEST_METHOD(CodeGenDx12MiniEngineBloomextractanddownsampleldrcs)
  TEST_METHOD(CodeGenDx12MiniEngineBlurcs)
  TEST_METHOD(CodeGenDx12MiniEngineBuffercopyps)
  TEST_METHOD(CodeGenDx12MiniEngineCameramotionblurprepasscs)
  TEST_METHOD(CodeGenDx12MiniEngineCameramotionblurprepasslinearzcs)
  TEST_METHOD(CodeGenDx12MiniEngineCameravelocitycs)
  TEST_METHOD(CodeGenDx12MiniEngineConvertldrtodisplayaltps)
  TEST_METHOD(CodeGenDx12MiniEngineConvertldrtodisplayps)
  TEST_METHOD(CodeGenDx12MiniEngineDebugdrawhistogramcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugluminancehdrcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugluminanceldrcs)
  TEST_METHOD(CodeGenDx12MiniEngineDebugssaocs)
  TEST_METHOD(CodeGenDx12MiniEngineDepthviewerps)
  TEST_METHOD(CodeGenDx12MiniEngineDepthviewervs)
  TEST_METHOD(CodeGenDx12MiniEngineDownsamplebloomallcs)
  TEST_METHOD(CodeGenDx12MiniEngineDownsamplebloomcs)
  TEST_METHOD(CodeGenDx12MiniEngineExtractlumacs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass1_Luma_Cs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass1_Rgb_Cs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Hcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Hdebugcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Vcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaapass2Vdebugcs)
  TEST_METHOD(CodeGenDx12MiniEngineFxaaresolveworkqueuecs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratehistogramcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammacs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddxcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipsgammaoddycs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddxcs)
  TEST_METHOD(CodeGenDx12MiniEngineGeneratemipslinearoddycs)
  TEST_METHOD(CodeGenDx12MiniEngineLinearizedepthcs)
  TEST_METHOD(CodeGenDx12MiniEngineMagnifypixelsps)
  TEST_METHOD(CodeGenDx12MiniEngineModelviewerps)
  TEST_METHOD(CodeGenDx12MiniEngineModelviewervs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurfinalpasscs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurfinalpasstemporalcs)
  TEST_METHOD(CodeGenDx12MiniEngineMotionblurprepasscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlebincullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticledepthboundscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticledispatchindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlefinaldispatchindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleinnersortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlelargebincullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleoutersortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlepresortcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleps)
  TEST_METHOD(CodeGenDx12MiniEngineParticlesortindirectargscs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlespawncs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilecullingcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerendercs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastcs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastdynamiccs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderfastlowrescs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderslowdynamiccs)
  TEST_METHOD(CodeGenDx12MiniEngineParticletilerenderslowlowrescs)
  TEST_METHOD(CodeGenDx12MiniEngineParticleupdatecs)
  TEST_METHOD(CodeGenDx12MiniEngineParticlevs)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphbackgroundvs)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphps)
  TEST_METHOD(CodeGenDx12MiniEnginePerfgraphvs)
  TEST_METHOD(CodeGenDx12MiniEngineScreenquadvs)
  TEST_METHOD(CodeGenDx12MiniEngineSharpeningupsamplegammaps)
  TEST_METHOD(CodeGenDx12MiniEngineSharpeningupsampleps)
  TEST_METHOD(CodeGenDx12MiniEngineTemporalblendcs)
  TEST_METHOD(CodeGenDx12MiniEngineTextantialiasps)
  TEST_METHOD(CodeGenDx12MiniEngineTextshadowps)
  TEST_METHOD(CodeGenDx12MiniEngineTextvs)
  TEST_METHOD(CodeGenDx12MiniEngineTonemap2Cs)
  TEST_METHOD(CodeGenDx12MiniEngineTonemapcs)
  TEST_METHOD(CodeGenDx12MiniEngineUpsampleandblurcs)
  TEST_METHOD(DxilGen_StoreOutput)

  dxc::DxcDllSupport m_dllSupport;
  bool m_CompilerPreservesBBNames;

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned((LPBYTE)data, size, codePage,
                                                  ppBlob));
  }

  void CreateBlobFromFile(LPCWSTR name, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    const std::wstring path = hlsl_test::GetPathToHlslDataFile(name);
    IFT(library->CreateBlobFromFile(path.c_str(), nullptr, ppBlob));
  }

  void CreateBlobFromText(_In_z_ const char *pText,
                          _Outptr_ IDxcBlobEncoding **ppBlob) {
    CreateBlobPinned(pText, strlen(pText), CP_UTF8, ppBlob);
  }

  HRESULT CreateCompiler(IDxcCompiler **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }

  template <typename T, typename TDefault, typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
                    TDefault defaultValue, LPCWSTR valueLabel,
                    HRESULT (__stdcall TIface::*pFn)(T *)) {
    T value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value != defaultValue) {
      o << L", " << valueLabel << L": " << value;
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(BSTR *)) {
    CComBSTR value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.Length()) {
      o << L", " << valueLabel << L": " << (LPCWSTR)value;
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(VARIANT *)) {
    CComVariant value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.vt != VT_NULL && value.vt != VT_EMPTY) {
      if (SUCCEEDED(value.ChangeType(VT_BSTR))) {
        o << L", " << valueLabel << L": " << (LPCWSTR)value.bstrVal;
      }
    }
  }
  template <typename TIface>
  void WriteIfValue(TIface *pSymbol, std::wstringstream &o,
    LPCWSTR valueLabel, HRESULT(__stdcall TIface::*pFn)(IDiaSymbol **)) {
    CComPtr<IDiaSymbol> value;
    HRESULT hr = (pSymbol->*(pFn))(&value);
    if (SUCCEEDED(hr) && value.p != nullptr) {
      DWORD symId;
      value->get_symIndexId(&symId);
      o << L", " << valueLabel << L": id=" << symId;
    }
  }

  std::wstring GetDebugInfoAsText(_In_ IDiaDataSource* pDataSource) {
    CComPtr<IDiaSession> pSession;
    CComPtr<IDiaTable> pTable;
    CComPtr<IDiaEnumTables> pEnumTables;
    std::wstringstream o;

    VERIFY_SUCCEEDED(pDataSource->openSession(&pSession));
    VERIFY_SUCCEEDED(pSession->getEnumTables(&pEnumTables));
    LONG count;
    VERIFY_SUCCEEDED(pEnumTables->get_Count(&count));
    for (LONG i = 0; i < count; ++i) {
      pTable.Release();
      ULONG fetched;
      VERIFY_SUCCEEDED(pEnumTables->Next(1, &pTable, &fetched));
      VERIFY_ARE_EQUAL(fetched, 1);
      CComBSTR tableName;
      VERIFY_SUCCEEDED(pTable->get_name(&tableName));
      o << L"Table: " << (LPWSTR)tableName << std::endl;
      LONG rowCount;
      IFT(pTable->get_Count(&rowCount));
      o << L" Row count: " << rowCount << std::endl;

      for (LONG rowIndex = 0; rowIndex < rowCount; ++rowIndex) {
        CComPtr<IUnknown> item;
        o << L'#' << rowIndex;
        IFT(pTable->Item(rowIndex, &item));
        CComPtr<IDiaSymbol> pSymbol;
        if (SUCCEEDED(item.QueryInterface(&pSymbol))) {
          DWORD symTag;
          DWORD dataKind;
          DWORD locationType;
          DWORD registerId;
          pSymbol->get_symTag(&symTag);
          pSymbol->get_dataKind(&dataKind);
          pSymbol->get_locationType(&locationType);
          pSymbol->get_registerId(&registerId);
          //pSymbol->get_value(&value);

          WriteIfValue(pSymbol.p, o, 0, L"symIndexId", &IDiaSymbol::get_symIndexId);
          o << L", " << SymTagEnumText[symTag];
          if (dataKind != 0) o << L", " << DataKindText[dataKind];
          WriteIfValue(pSymbol.p, o, L"name", &IDiaSymbol::get_name);
          WriteIfValue(pSymbol.p, o, L"lexicalParent", &IDiaSymbol::get_lexicalParent);
          WriteIfValue(pSymbol.p, o, L"type", &IDiaSymbol::get_type);
          WriteIfValue(pSymbol.p, o, 0, L"slot", &IDiaSymbol::get_slot);
          WriteIfValue(pSymbol.p, o, 0, L"platform", &IDiaSymbol::get_platform);
          WriteIfValue(pSymbol.p, o, 0, L"language", &IDiaSymbol::get_language);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMajor", &IDiaSymbol::get_frontEndMajor);
          WriteIfValue(pSymbol.p, o, 0, L"frontEndMinor", &IDiaSymbol::get_frontEndMinor);
          WriteIfValue(pSymbol.p, o, 0, L"token", &IDiaSymbol::get_token);
          WriteIfValue(pSymbol.p, o,    L"value", &IDiaSymbol::get_value);
          WriteIfValue(pSymbol.p, o, 0, L"code", &IDiaSymbol::get_code);
          WriteIfValue(pSymbol.p, o, 0, L"function", &IDiaSymbol::get_function);
          WriteIfValue(pSymbol.p, o, 0, L"udtKind", &IDiaSymbol::get_udtKind);
          WriteIfValue(pSymbol.p, o, 0, L"hasDebugInfo", &IDiaSymbol::get_hasDebugInfo);
          WriteIfValue(pSymbol.p, o,    L"compilerName", &IDiaSymbol::get_compilerName);
          WriteIfValue(pSymbol.p, o, 0, L"isLocationControlFlowDependent", &IDiaSymbol::get_isLocationControlFlowDependent);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfRows", &IDiaSymbol::get_numberOfRows);
          WriteIfValue(pSymbol.p, o, 0, L"numberOfColumns", &IDiaSymbol::get_numberOfColumns);
          WriteIfValue(pSymbol.p, o, 0, L"length", &IDiaSymbol::get_length);
          WriteIfValue(pSymbol.p, o, 0, L"isMatrixRowMajor", &IDiaSymbol::get_isMatrixRowMajor);
          WriteIfValue(pSymbol.p, o, 0, L"builtInKind", &IDiaSymbol::get_builtInKind);
          WriteIfValue(pSymbol.p, o, 0, L"textureSlot", &IDiaSymbol::get_textureSlot);
          WriteIfValue(pSymbol.p, o, 0, L"memorySpaceKind", &IDiaSymbol::get_memorySpaceKind);
          WriteIfValue(pSymbol.p, o, 0, L"isHLSLData", &IDiaSymbol::get_isHLSLData);
        }

        CComPtr<IDiaSourceFile> pSourceFile;
        if (SUCCEEDED(item.QueryInterface(&pSourceFile))) {
          WriteIfValue(pSourceFile.p, o, 0, L"uniqueId", &IDiaSourceFile::get_uniqueId);
          WriteIfValue(pSourceFile.p, o, L"fileName", &IDiaSourceFile::get_fileName);
        }

        CComPtr<IDiaLineNumber> pLineNumber;
        if (SUCCEEDED(item.QueryInterface(&pLineNumber))) {
          WriteIfValue(pLineNumber.p, o, L"compiland", &IDiaLineNumber::get_compiland);
          //WriteIfValue(pLineNumber.p, o, L"sourceFile", &IDiaLineNumber::get_sourceFile);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumber", &IDiaLineNumber::get_lineNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"lineNumberEnd", &IDiaLineNumber::get_lineNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumber", &IDiaLineNumber::get_columnNumber);
          WriteIfValue(pLineNumber.p, o, 0, L"columnNumberEnd", &IDiaLineNumber::get_columnNumberEnd);
          WriteIfValue(pLineNumber.p, o, 0, L"addressSection", &IDiaLineNumber::get_addressSection);
          WriteIfValue(pLineNumber.p, o, 0, L"addressOffset", &IDiaLineNumber::get_addressOffset);
          WriteIfValue(pLineNumber.p, o, 0, L"relativeVirtualAddress", &IDiaLineNumber::get_relativeVirtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"virtualAddress", &IDiaLineNumber::get_virtualAddress);
          WriteIfValue(pLineNumber.p, o, 0, L"length", &IDiaLineNumber::get_length);
          WriteIfValue(pLineNumber.p, o, 0, L"sourceFileId", &IDiaLineNumber::get_sourceFileId);
          WriteIfValue(pLineNumber.p, o, 0, L"statement", &IDiaLineNumber::get_statement);
          WriteIfValue(pLineNumber.p, o, 0, L"compilandId", &IDiaLineNumber::get_compilandId);
        }

        CComPtr<IDiaSectionContrib> pSectionContrib;
        if (SUCCEEDED(item.QueryInterface(&pSectionContrib))) {
          WriteIfValue(pSectionContrib.p, o, L"compiland", &IDiaSectionContrib::get_compiland);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressSection", &IDiaSectionContrib::get_addressSection);
          WriteIfValue(pSectionContrib.p, o, 0, L"addressOffset", &IDiaSectionContrib::get_addressOffset);
          WriteIfValue(pSectionContrib.p, o, 0, L"relativeVirtualAddress", &IDiaSectionContrib::get_relativeVirtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"virtualAddress", &IDiaSectionContrib::get_virtualAddress);
          WriteIfValue(pSectionContrib.p, o, 0, L"length", &IDiaSectionContrib::get_length);
          WriteIfValue(pSectionContrib.p, o, 0, L"notPaged", &IDiaSectionContrib::get_notPaged);
          WriteIfValue(pSectionContrib.p, o, 0, L"code", &IDiaSectionContrib::get_code);
          WriteIfValue(pSectionContrib.p, o, 0, L"initializedData", &IDiaSectionContrib::get_initializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"uninitializedData", &IDiaSectionContrib::get_uninitializedData);
          WriteIfValue(pSectionContrib.p, o, 0, L"remove", &IDiaSectionContrib::get_remove);
          WriteIfValue(pSectionContrib.p, o, 0, L"comdat", &IDiaSectionContrib::get_comdat);
          WriteIfValue(pSectionContrib.p, o, 0, L"discardable", &IDiaSectionContrib::get_discardable);
          WriteIfValue(pSectionContrib.p, o, 0, L"notCached", &IDiaSectionContrib::get_notCached);
          WriteIfValue(pSectionContrib.p, o, 0, L"share", &IDiaSectionContrib::get_share);
          WriteIfValue(pSectionContrib.p, o, 0, L"execute", &IDiaSectionContrib::get_execute);
          WriteIfValue(pSectionContrib.p, o, 0, L"read", &IDiaSectionContrib::get_read);
          WriteIfValue(pSectionContrib.p, o, 0, L"write", &IDiaSectionContrib::get_write);
          WriteIfValue(pSectionContrib.p, o, 0, L"dataCrc", &IDiaSectionContrib::get_dataCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"relocationsCrc", &IDiaSectionContrib::get_relocationsCrc);
          WriteIfValue(pSectionContrib.p, o, 0, L"compilandId", &IDiaSectionContrib::get_compilandId);
        }

        CComPtr<IDiaSegment> pSegment;
        if (SUCCEEDED(item.QueryInterface(&pSegment))) {
          WriteIfValue(pSegment.p, o, 0, L"frame", &IDiaSegment::get_frame);
          WriteIfValue(pSegment.p, o, 0, L"offset", &IDiaSegment::get_offset);
          WriteIfValue(pSegment.p, o, 0, L"length", &IDiaSegment::get_length);
          WriteIfValue(pSegment.p, o, 0, L"read", &IDiaSegment::get_read);
          WriteIfValue(pSegment.p, o, 0, L"write", &IDiaSegment::get_write);
          WriteIfValue(pSegment.p, o, 0, L"execute", &IDiaSegment::get_execute);
          WriteIfValue(pSegment.p, o, 0, L"addressSection", &IDiaSegment::get_addressSection);
          WriteIfValue(pSegment.p, o, 0, L"relativeVirtualAddress", &IDiaSegment::get_relativeVirtualAddress);
          WriteIfValue(pSegment.p, o, 0, L"virtualAddress", &IDiaSegment::get_virtualAddress);
        }

        CComPtr<IDiaInjectedSource> pInjectedSource;
        if (SUCCEEDED(item.QueryInterface(&pInjectedSource))) {
          WriteIfValue(pInjectedSource.p, o, 0, L"crc", &IDiaInjectedSource::get_crc);
          WriteIfValue(pInjectedSource.p, o, 0, L"length", &IDiaInjectedSource::get_length);
          WriteIfValue(pInjectedSource.p, o, L"filename", &IDiaInjectedSource::get_filename);
          WriteIfValue(pInjectedSource.p, o, L"objectFilename", &IDiaInjectedSource::get_objectFilename);
          WriteIfValue(pInjectedSource.p, o, L"virtualFilename", &IDiaInjectedSource::get_virtualFilename);
          WriteIfValue(pInjectedSource.p, o, 0, L"sourceCompression", &IDiaInjectedSource::get_sourceCompression);
          // get_source is also available
        }

        CComPtr<IDiaFrameData> pFrameData;
        if (SUCCEEDED(item.QueryInterface(&pFrameData))) {
        }

        o << std::endl;
      }
    }

    return o.str();
  }

  std::string GetOption(std::string &cmd, char *opt) {
    std::string option = cmd.substr(cmd.find(opt));
    option = option.substr(option.find_first_of(' '));
    option = option.substr(option.find_first_not_of(' '));
    return option.substr(0, option.find_first_of(' '));
  }

  void CodeGenTest(LPCWSTR name) {
    CComPtr<IDxcCompiler> pCompiler;
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
    CreateBlobFromFile(name, &pSource);

    std::string cmdLine = GetFirstLine(name);

    llvm::StringRef argsRef = cmdLine;
    llvm::SmallVector<llvm::StringRef, 8> splitArgs;
    argsRef.split(splitArgs, " ");
    hlsl::options::MainArgs argStrings(splitArgs);
    std::string errorString;
    llvm::raw_string_ostream errorStream(errorString);
    hlsl::options::DxcOpts opts;
    IFT(ReadDxcOpts(hlsl::options::getHlslOptTable(), /*flagsToInclude*/ 0,
                    argStrings, opts, errorStream));
    std::wstring entry =
        Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
    std::wstring profile =
        Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());

    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, name, entry.c_str(),
                                        profile.c_str(), nullptr, 0, nullptr, 0,
                                        nullptr, &pResult));
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErr;
      IFT(pResult->GetErrorBuffer(&pErr));
      std::string errString(BlobToUtf8(pErr));
      CA2W errStringW(errString.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(L"Failed to compile - errors follow");
      WEX::Logging::Log::Comment(errStringW);
    }
    VERIFY_SUCCEEDED(result);

    CComPtr<IDxcBlob> pProgram;
    VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

    CComPtr<IDxcBlobEncoding> pDisassembleBlob;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

    std::string disassembleString(BlobToUtf8(pDisassembleBlob));
    VERIFY_ARE_NOT_EQUAL(0, disassembleString.size());
  }

  void CodeGenTestCheck(LPCWSTR name) {
    std::wstring fullPath = hlsl_test::GetPathToHlslDataFile(name);
    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(fullPath.c_str());
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void VerifyOperationSucceeded(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    if (FAILED(result)) {
      CComPtr<IDxcBlobEncoding> pErrors;
      VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
      CA2W errorsWide(BlobToUtf8(pErrors).c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(errorsWide);
    }
    VERIFY_SUCCEEDED(result);
  }

  std::string VerifyOperationFailed(IDxcOperationResult *pResult) {
    HRESULT result;
    VERIFY_SUCCEEDED(pResult->GetStatus(&result));
    VERIFY_FAILED(result);
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    return BlobToUtf8(pErrors);
  }
};

// Useful for debugging.
#if SUPPORT_FXC_PDB
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
HRESULT GetBlobPdb(IDxcBlob *pBlob, IDxcBlob **ppDebugInfo) {
  return D3DGetBlobPart(pBlob->GetBufferPointer(), pBlob->GetBufferSize(),
    D3D_BLOB_PDB, 0, (ID3DBlob **)ppDebugInfo);
}

std::string FourCCStr(uint32_t val) {
  std::stringstream o;
  char c[5];
  c[0] = val & 0xFF;
  c[1] = (val & 0xFF00) >> 8;
  c[2] = (val & 0xFF0000) >> 16;
  c[3] = (val & 0xFF000000) >> 24;
  c[4] = '\0';
  o << c << " (" << std::hex << val << std::dec << ")";
  return o.str();
}
std::string DumpParts(IDxcBlob *pBlob) {
  std::stringstream o;

  hlsl::DxilContainerHeader *pContainer = (hlsl::DxilContainerHeader *)pBlob->GetBufferPointer();
  o << "Container:" << std::endl
    << " Size: " << pContainer->ContainerSizeInBytes << std::endl
    << " FourCC: " << FourCCStr(pContainer->HeaderFourCC) << std::endl
    << " Part count: " << pContainer->PartCount << std::endl;
  for (uint32_t i = 0; i < pContainer->PartCount; ++i) {
    hlsl::DxilPartHeader *pPart = hlsl::GetDxilContainerPart(pContainer, i);
    o << "Part " << i << std::endl
      << " FourCC: " << FourCCStr(pPart->PartFourCC) << std::endl
      << " Size: " << pPart->PartSize << std::endl;
  }
  return o.str();
}

HRESULT CreateDiaSourceFromDxbcBlob(IDxcLibrary *pLib, IDxcBlob *pDxbcBlob,
                                    IDiaDataSource **ppDiaSource) {
  HRESULT hr = S_OK;
  CComPtr<IDxcBlob> pdbBlob;
  CComPtr<IStream> pPdbStream;
  CComPtr<IDiaDataSource> pDiaSource;
  IFR(GetBlobPdb(pDxbcBlob, &pdbBlob));
  IFR(pLib->CreateStreamFromBlobReadOnly(pdbBlob, &pPdbStream));
  IFR(CoCreateInstance(CLSID_DiaSource, NULL, CLSCTX_INPROC_SERVER,
                       __uuidof(IDiaDataSource), (void **)&pDiaSource));
  IFR(pDiaSource->loadDataFromIStream(pPdbStream));
  *ppDiaSource = pDiaSource.Detach();
  return hr;
}
#endif

bool CompilerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());

    // This is a very indirect way of testing this. Consider improving support.
    CComPtr<IDxcValidator> pValidator;
    CComPtr<IDxcVersionInfo> pVersionInfo;
    UINT32 VersionFlags = 0;
    VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
    VERIFY_SUCCEEDED(pValidator.QueryInterface(&pVersionInfo));
    VERIFY_SUCCEEDED(pVersionInfo->GetFlags(&VersionFlags));
    m_CompilerPreservesBBNames = VersionFlags & DxcVersionInfoFlags_Debug;
  }
  return true;
}

TEST_F(CompilerTest, CompileWhenDebugThenDIPresent) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  // BUG: the first test written was of this form:
  // float4 local = 0; return local;
  //
  // However we get no numbers because of the _wrapper form
  // that exports the zero initialization from main into
  // a global can't be attributed to any particular location
  // within main, and everything in main is eventually folded away.
  //
  // Making the function do a bit more work by calling an intrinsic
  // helps this case.
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
    "  float4 local = abs(pos);\r\n"
    "  return local;\r\n"
    "}", &pSource);
  LPCWSTR args[] = { L"/Zi" };
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  // Disassemble the compiled (stripped) program.
  {
    CComPtr<IDxcBlobEncoding> pDisassembly;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembly));
    std::string disText = BlobToUtf8(pDisassembly);
    CA2W disTextW(disText.c_str(), CP_UTF8);
    //WEX::Logging::Log::Comment(disTextW);
  }

  // CONSIDER: have the dia data source look for the part if passed a whole container.
  CComPtr<IDiaDataSource> pDiaSource;
  CComPtr<IStream> pProgramStream;
  CComPtr<IDxcLibrary> pLib;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &pLib));
  const hlsl::DxilContainerHeader *pContainer = hlsl::IsDxilContainerLike(
      pProgram->GetBufferPointer(), pProgram->GetBufferSize());
  VERIFY_IS_NOT_NULL(pContainer);
  hlsl::DxilPartIterator partIter =
      std::find_if(hlsl::begin(pContainer), hlsl::end(pContainer),
                   hlsl::DxilPartIsType(hlsl::DFCC_ShaderDebugInfoDXIL));
  const hlsl::DxilProgramHeader *pProgramHeader =
      (const hlsl::DxilProgramHeader *)hlsl::GetDxilPartData(*partIter);
  uint32_t bitcodeLength;
  const char *pBitcode;
  CComPtr<IDxcBlob> pProgramPdb;
  hlsl::GetDxilProgramBitcode(pProgramHeader, &pBitcode, &bitcodeLength);
  VERIFY_SUCCEEDED(pLib->CreateBlobFromBlob(
      pProgram, pBitcode - (char *)pProgram->GetBufferPointer(), bitcodeLength,
      &pProgramPdb));

  // Disassemble the program with debug information.
  {
    CComPtr<IDxcBlobEncoding> pDbgDisassembly;
    VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgramPdb, &pDbgDisassembly));
    std::string disText = BlobToUtf8(pDbgDisassembly);
    CA2W disTextW(disText.c_str(), CP_UTF8);
    //WEX::Logging::Log::Comment(disTextW);
  }

  // Create a short text dump of debug information.
  VERIFY_SUCCEEDED(pLib->CreateStreamFromBlobReadOnly(pProgramPdb, &pProgramStream));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcDiaDataSource, &pDiaSource));
  VERIFY_SUCCEEDED(pDiaSource->loadDataFromIStream(pProgramStream));
  std::wstring diaDump = GetDebugInfoAsText(pDiaSource).c_str();
  //WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());

  // Very basic tests - we have basic symbols, line numbers, and files with sources.
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"symIndexId: 5, CompilandEnv, name: hlslTarget, value: ps_6_0"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"lineNumber: 2"));
  VERIFY_IS_NOT_NULL(wcsstr(diaDump.c_str(), L"length: 99, filename: source.hlsl"));

#if SUPPORT_FXC_PDB
  // Now, fake it by loading from a .pdb!
  VERIFY_SUCCEEDED(CoInitializeEx(0, COINITBASE_MULTITHREADED));
  const wchar_t path[] = L"path-to-fxc-blob.bin";
  pDiaSource.Release();
  pProgramStream.Release();
  CComPtr<IDxcBlobEncoding> fxcBlob;
  CComPtr<IDxcBlob> pdbBlob;
  VERIFY_SUCCEEDED(pLib->CreateBlobFromFile(path, nullptr, &fxcBlob));
  std::string s = DumpParts(fxcBlob);
  CA2W sW(s.c_str(), CP_UTF8);
  WEX::Logging::Log::Comment(sW);
  VERIFY_SUCCEEDED(CreateDiaSourceFromDxbcBlob(pLib, fxcBlob, &pDiaSource));
  WEX::Logging::Log::Comment(GetDebugInfoAsText(pDiaSource).c_str());
#endif
}

TEST_F(CompilerTest, CompileWhenDefinesThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[] = {{L"F4", L"float4"}};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("F4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, defines,
                                      _countof(defines), nullptr, &pResult));
}

TEST_F(CompilerTest, CompileWhenDefinesManyThenApplied) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  LPCWSTR args[] = {L"/DVAL1=1",  L"/DVAL2=2",  L"/DVAL3=3",  L"/DVAL4=2",
                    L"/DVAL5=4",  L"/DNVAL1",   L"/DNVAL2",   L"/DNVAL3",
                    L"/DNVAL4",   L"/DNVAL5",   L"/DCVAL1=1", L"/DCVAL2=2",
                    L"/DCVAL3=3", L"/DCVAL4=2", L"/DCVAL5=4", L"/DCVALNONE="};

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
                     "#ifndef VAL1\r\n"
                     "#error VAL1 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef NVAL5\r\n"
                     "#error NVAL5 not defined\r\n"
                     "#endif\r\n"
                     "#ifndef CVALNONE\r\n"
                     "#error CVALNONE not defined\r\n"
                     "#endif\r\n"
                     "return 0; }",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  HRESULT compileStatus;
  VERIFY_SUCCEEDED(pResult->GetStatus(&compileStatus));
  if (FAILED(compileStatus)) {
    CComPtr<IDxcBlobEncoding> pErrors;
    VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrors));
    OutputDebugStringA((LPCSTR)pErrors->GetBufferPointer());
  }
  VERIFY_SUCCEEDED(compileStatus);
}

TEST_F(CompilerTest, CompileWhenEmptyThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

  // null source
  VERIFY_FAILED(pCompiler->Compile(nullptr, L"source.hlsl", nullptr, nullptr,
                                   nullptr, 0, nullptr, 0, nullptr, &pResult));
  // null result
  VERIFY_FAILED(pCompiler->Compile(pSource, L"source.hlsl", nullptr, nullptr,
                                   nullptr, 0, nullptr, 0, nullptr, nullptr));
}

TEST_F(CompilerTest, CompileWhenIncorrectThenFails) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4_undefined main() : SV_Target { return 0; }",
                     &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main", L"ps_6_0",
                                      nullptr, 0, nullptr, 0, nullptr,
                                      &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_FAILED(result);

  CComPtr<IDxcBlobEncoding> pErrorBuffer;
  VERIFY_SUCCEEDED(pResult->GetErrorBuffer(&pErrorBuffer));
  std::string errorString(BlobToUtf8(pErrorBuffer));
  VERIFY_ARE_NOT_EQUAL(0, errorString.size());
  // Useful for examining actual error message:
  // CA2W errorStringW(errorString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(errorStringW.m_psz);
}

TEST_F(CompilerTest, CompileWhenWorksThenDisassembleWorks) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  HRESULT result;
  VERIFY_SUCCEEDED(pResult->GetStatus(&result));
  VERIFY_SUCCEEDED(result);

  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));

  CComPtr<IDxcBlobEncoding> pDisassembleBlob;
  VERIFY_SUCCEEDED(pCompiler->Disassemble(pProgram, &pDisassembleBlob));

  std::string disassembleString(BlobToUtf8(pDisassembleBlob));
  VERIFY_ARE_NOT_EQUAL(0, disassembleString.size());
  // Useful for examining disassembly:
  // CA2W disassembleStringW(disassembleString.c_str(), CP_UTF8);
  // WEX::Logging::Log::Comment(disassembleStringW.m_psz);
}

TEST_F(CompilerTest, CompileWhenDebugWorksThenStripDebug) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main(float4 pos : SV_Position) : SV_Target {\r\n"
                     "  float4 local = abs(pos);\r\n"
                     "  return local;\r\n"
                     "}",
                     &pSource);
  LPCWSTR args[] = {L"/Zi"};

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", args, _countof(args), nullptr,
                                      0, nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Check if it contains debug blob
  hlsl::DxilContainerHeader *pHeader =
      (hlsl::DxilContainerHeader *)(pProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // Check debug info part does not exist after strip debug info

  CComPtr<IDxcBlob> pNewProgram;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pHeader = (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(
      pHeader, hlsl::DxilFourCC::DFCC_ShaderDebugInfoDXIL);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWhenWorksThenAddRemovePrivate) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("float4 main() : SV_Target {\r\n"
                     "  return 0;\r\n"
                     "}",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr, 0,
                                      nullptr, &pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  // Append private data blob
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));

  std::string privateTxt("private data");
  CComPtr<IDxcBlobEncoding> pPrivate;
  CreateBlobFromText(privateTxt.c_str(), &pPrivate);
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->AddPart(hlsl::DxilFourCC::DFCC_PrivateData, pPrivate));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  CComPtr<IDxcBlob> pNewProgram;
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  hlsl::DxilContainerHeader *pContainerHeader =
      (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NOT_NULL(pPartHeader);
  // compare data
  std::string privatePart((const char *)(pPartHeader + 1), privateTxt.size());
  VERIFY_IS_TRUE(strcmp(privatePart.c_str(), privateTxt.c_str()) == 0);

  // Remove private data blob
  pBuilder.Release();
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pNewProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_PrivateData));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));

  pNewProgram.Release();
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader =
    (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(
    pContainerHeader, hlsl::DxilFourCC::DFCC_PrivateData);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWithRootSignatureThenStripRootSignature) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pProgram;
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText("[RootSignature(\"\")] \r\n"
                     "float4 main(float a : A) : SV_Target {\r\n"
                     "  return a;\r\n"
                     "}",
                     &pSource);
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
                                      L"ps_6_0", nullptr, 0, nullptr,
                                      0, nullptr, &pResult));
  VERIFY_IS_NOT_NULL(pResult);
  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_SUCCEEDED(status);
  VERIFY_SUCCEEDED(pResult->GetResult(&pProgram));
  VERIFY_IS_NOT_NULL(pProgram);
  hlsl::DxilContainerHeader *pContainerHeader =
      (hlsl::DxilContainerHeader *)(pProgram->GetBufferPointer());
  hlsl::DxilPartHeader *pPartHeader = hlsl::GetDxilPartByType(
      pContainerHeader, hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NOT_NULL(pPartHeader);
  
  // Remove root signature
  CComPtr<IDxcBlob> pNewProgram;
  CComPtr<IDxcContainerBuilder> pBuilder;
  VERIFY_SUCCEEDED(CreateContainerBuilder(&pBuilder));
  VERIFY_SUCCEEDED(pBuilder->Load(pProgram));
  VERIFY_SUCCEEDED(pBuilder->RemovePart(hlsl::DxilFourCC::DFCC_RootSignature));
  pResult.Release();
  VERIFY_SUCCEEDED(pBuilder->SerializeContainer(&pResult));
  VERIFY_SUCCEEDED(pResult->GetResult(&pNewProgram));
  pContainerHeader = (hlsl::DxilContainerHeader *)(pNewProgram->GetBufferPointer());
  pPartHeader = hlsl::GetDxilPartByType(pContainerHeader,
                                        hlsl::DxilFourCC::DFCC_RootSignature);
  VERIFY_IS_NULL(pPartHeader);
}

TEST_F(CompilerTest, CompileWhenIncludeThenLoadInvoked) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeThenLoadUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeAbsoluteThenLoadAbsolute) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"C:\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"C:\\helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeLocalThenLoadRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"..\\helper.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./..\\helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeSystemThenLoadNotRelative) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  LPCWSTR args[] = {
    L"-Ifoo"
  };
  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./foo\\helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeSystemMissingThenLoadAttempt) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"subdir/other/file.h\"\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#include <helper.h>");
  pInclude->CallResults.emplace_back("#define ZERO 0");

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(std::string::npos, failLog.find("<angled>")); // error message should prompt to use <angled> rather than "quotes"
  VERIFY_ARE_EQUAL_WSTR(L"./subdir/other/file.h;./subdir/other/helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeFlagsThenIncludeUsed) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include <helper.h>\r\n"
    "float4 main() : SV_Target { return ZERO; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);
  pInclude->CallResults.emplace_back("#define ZERO 0");

  LPCWSTR args[] = {
    L"-I\\\\server\\share"
  };
  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", args, _countof(args), nullptr, 0, pInclude, &pResult));
  VerifyOperationSucceeded(pResult);
  VERIFY_ARE_EQUAL_WSTR(L"\\\\server\\share\\helper.h;", pInclude->GetAllFileNames().c_str());
}

TEST_F(CompilerTest, CompileWhenIncludeMissingThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<TestIncludeHandler> pInclude;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "#include \"file.h\"\r\n"
    "float4 main() : SV_Target { return 0; }", &pSource);

  pInclude = new TestIncludeHandler(m_dllSupport);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, pInclude, &pResult));
  HRESULT hr;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
  VERIFY_FAILED(hr);
}

static const char EmptyCompute[] = "[numthreads(8,8,1)] void main() { }";

TEST_F(CompilerTest, CompileWhenODumpThenPassConfig) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Odump" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  string passes((char *)pResultBlob->GetBufferPointer(), pResultBlob->GetBufferSize());
  VERIFY_ARE_NOT_EQUAL(string::npos, passes.find("inline"));
}

TEST_F(CompilerTest, CompileWhenVdThenProducesDxilContainer) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  LPCWSTR Args[] = { L"/Vd" };

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"cs_6_0", Args, _countof(Args), nullptr, 0, nullptr, &pResult));
  VerifyOperationSucceeded(pResult);
  CComPtr<IDxcBlob> pResultBlob;
  VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
  VERIFY_IS_TRUE(hlsl::IsValidDxilContainer(reinterpret_cast<hlsl::DxilContainerHeader *>(pResultBlob->GetBufferPointer()), pResultBlob->GetBufferSize()));
}

TEST_F(CompilerTest, CompileWhenODumpThenOptimizerMatch) {
  LPCWSTR OptLevels[] = { L"/Od", L"/O1", L"/O2" };
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOptimizer> pOptimizer;
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcValidator> pValidator;
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));
  VERIFY_SUCCEEDED(m_dllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
  for (LPCWSTR OptLevel : OptLevels) {
    CComPtr<IDxcOperationResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;
    CComPtr<IDxcBlob> pHighLevelBlob;
    CComPtr<IDxcBlob> pOptimizedModule;
    CComPtr<IDxcBlob> pAssembledBlob;

    // Could use EmptyCompute and cs_6_0, but there is an issue where properties
    // don't round-trip properly at high-level, so validation fails because
    // dimensions are set to zero. Workaround by using pixel shader instead.
    LPCWSTR Target = L"ps_6_0";
    CreateBlobFromText("float4 main() : SV_Target { return 0; }", &pSource);

    LPCWSTR Args[2] = { OptLevel, L"/Odump" };

    // Get the passes for this optimization level.
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    CComPtr<IDxcBlob> pResultBlob;
    VERIFY_SUCCEEDED(pResult->GetResult(&pResultBlob));
    string passes((char *)pResultBlob->GetBufferPointer(), pResultBlob->GetBufferSize());

    // Get wchar_t version and prepend hlsl-hlensure, to do a split high-level/opt compilation pass.
    CA2W passesW(passes.c_str(), CP_UTF8);
    std::vector<LPCWSTR> Options;
    Options.push_back(L"-hlsl-hlensure");
    wchar_t *pPassesBuffer = passesW.m_psz;
    while (*pPassesBuffer) {
      // Skip comment lines.
      if (*pPassesBuffer == L'#') {
        while (*pPassesBuffer && *pPassesBuffer != '\n' && *pPassesBuffer != '\r') {
          ++pPassesBuffer;
        }
        while (*pPassesBuffer == '\n' || *pPassesBuffer == '\r') {
          ++pPassesBuffer;
        }
        continue;
      }
      // Every other line is an option. Find the end of the line/buffer and terminate it.
      Options.push_back(pPassesBuffer);
      while (*pPassesBuffer && *pPassesBuffer != '\n' && *pPassesBuffer != '\r') {
        ++pPassesBuffer;
      }
      while (*pPassesBuffer == '\n' || *pPassesBuffer == '\r') {
        *pPassesBuffer = L'\0';
        ++pPassesBuffer;
      }
    }

    // Now compile directly.
    pResult.Release();
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, 1, nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);

    // Now compile via a high-level compile followed by the optimization passes.
    pResult.Release();
    Args[_countof(Args)-1] = L"/fcgl";
    VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
      Target, Args, _countof(Args), nullptr, 0, nullptr, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pHighLevelBlob));
    VERIFY_SUCCEEDED(pOptimizer->RunOptimizer(pHighLevelBlob, Options.data(),
                                              Options.size(), &pOptimizedModule,
                                              nullptr));

    // At the very least, the module should be valid.
    pResult.Release();
    VERIFY_SUCCEEDED(pAssembler->AssembleToContainer(pOptimizedModule, &pResult));
    VerifyOperationSucceeded(pResult);
    VERIFY_SUCCEEDED(pResult->GetResult(&pAssembledBlob));
    pResult.Release();
    VERIFY_SUCCEEDED(pValidator->Validate(pAssembledBlob, DxcValidatorFlags_Default, &pResult));
    VerifyOperationSucceeded(pResult);
  }
}

TEST_F(CompilerTest, CompileWhenShaderModelMismatchAttributeThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(EmptyCompute, &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find("attribute numthreads only valid for CS"));
}

TEST_F(CompilerTest, CompileBadHlslThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "bad hlsl", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_FAILED(status);
}

TEST_F(CompilerTest, CompileLegacyShaderModelThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "float4 main(float4 pos : SV_Position) : SV_Target { return pos; }", &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"ps_5_1", nullptr, 0, nullptr, 0, nullptr, &pResult));

  HRESULT status;
  VERIFY_SUCCEEDED(pResult->GetStatus(&status));
  VERIFY_FAILED(status);
}


TEST_F(CompilerTest, CodeGenAbs1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\abs1.hlsl");
}

TEST_F(CompilerTest, CodeGenAbs2) {
  CodeGenTest(L"..\\CodeGenHLSL\\abs2.hlsl");
}

TEST_F(CompilerTest, CodeGenAddUint64) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\AddUint64.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayArg){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayArg.hlsl");
}

TEST_F(CompilerTest, CodeGenArrayOfStruct){
  CodeGenTest(L"..\\CodeGenHLSL\\arrayOfStruct.hlsl");
}

TEST_F(CompilerTest, CodeGenAsUint) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\asuint.hlsl");
}

TEST_F(CompilerTest, CodeGenAsUint2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\asuint2.hlsl");
}

TEST_F(CompilerTest, CodeGenAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\atomic.hlsl");
}

TEST_F(CompilerTest, CodeGenBinary1) {
  CodeGenTest(L"..\\CodeGenHLSL\\binary1.hlsl");
}

TEST_F(CompilerTest, CodeGenBoolComb) {
  CodeGenTest(L"..\\CodeGenHLSL\\boolComb.hlsl");
}

TEST_F(CompilerTest, CodeGenBoolSvTarget) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\boolSvTarget.hlsl");
}

TEST_F(CompilerTest, CodeGenCalcLod2DArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\calcLod2DArray.hlsl");
}

TEST_F(CompilerTest, CodeGenCall1) {
  CodeGenTest(L"..\\CodeGenHLSL\\call1.hlsl");
}

TEST_F(CompilerTest, CodeGenCall3) {
  CodeGenTest(L"..\\CodeGenHLSL\\call3.hlsl");
}

TEST_F(CompilerTest, CodeGenCast1) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast1.hlsl");
}

TEST_F(CompilerTest, CodeGenCast2) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast2.hlsl");
}

TEST_F(CompilerTest, CodeGenCast3) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast3.hlsl");
}

TEST_F(CompilerTest, CodeGenCast4) {
  CodeGenTest(L"..\\CodeGenHLSL\\cast4.hlsl");
}

TEST_F(CompilerTest, CodeGenCast5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cast5.hlsl");
}

TEST_F(CompilerTest, CodeGenCast6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cast6.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer_unused) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer_unused.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer1_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer1.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer1_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer1.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer2_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer2.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer2_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer2.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer3_50) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer3.50.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer3_51) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbuffer3.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer5_51) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer5.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbuffer6_51) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer6.51.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferAlloc) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferAlloc.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferAllocLegacy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbufferAlloc_legacy.hlsl");
}

TEST_F(CompilerTest, CodeGenCbufferInLoop) {
  CodeGenTest(L"..\\CodeGenHLSL\\cbufferInLoop.hlsl");
}

TEST_F(CompilerTest, CodeGenClipPlanes) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\clip_planes.hlsl");
}

TEST_F(CompilerTest, CodeGenConstoperand1) {
  CodeGenTest(L"..\\CodeGenHLSL\\constoperand1.hlsl");
}

TEST_F(CompilerTest, CodeGenDiscard) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\discard.hlsl");
}

TEST_F(CompilerTest, CodeGenDivZero) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\divZero.hlsl");
}

TEST_F(CompilerTest, CodeGenDot1) {
  CodeGenTest(L"..\\CodeGenHLSL\\dot1.hlsl");
}

TEST_F(CompilerTest, CodeGenDynamic_Resources) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dynamic-resources.hlsl");
}

TEST_F(CompilerTest, CodeGenEffectSkip) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\effect_skip.hlsl");
}

TEST_F(CompilerTest, CodeGenEmpty) {
  CodeGenTest(L"..\\CodeGenHLSL\\empty.hlsl");
}

TEST_F(CompilerTest, CodeGenEmptyStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\emptyStruct.hlsl");
}

TEST_F(CompilerTest, CodeGenEarlyDepthStencil) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\earlyDepthStencil.hlsl");
}

TEST_F(CompilerTest, CodeGenEval) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\eval.hlsl");
}

TEST_F(CompilerTest, CodeGenEvalPos) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\evalPos.hlsl");
}

TEST_F(CompilerTest, CodeGenFirstbitHi) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\firstbitHi.hlsl");
}

TEST_F(CompilerTest, CodeGenFirstbitLo) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\firstbitLo.hlsl");
}

TEST_F(CompilerTest, CodeGenFloatMaxtessfactor) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\FloatMaxtessfactorHs.hlsl");
}

TEST_F(CompilerTest, CodeGenFModPS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\fmodPS.hlsl");
}

TEST_F(CompilerTest, CodeGenGather) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gather.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherCmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherCmp.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherCubeOffset) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherCubeOffset.hlsl");
}

TEST_F(CompilerTest, CodeGenGatherOffset) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\gatherOffset.hlsl");
}

TEST_F(CompilerTest, CodeGenIcb1) {
  CodeGenTest(L"..\\CodeGenHLSL\\icb1.hlsl");
}

TEST_F(CompilerTest, CodeGenIf1) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if1.hlsl"); }

TEST_F(CompilerTest, CodeGenIf2) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if2.hlsl"); }

TEST_F(CompilerTest, CodeGenIf3) { CodeGenTest(L"..\\CodeGenHLSL\\if3.hlsl"); }

TEST_F(CompilerTest, CodeGenIf4) { CodeGenTest(L"..\\CodeGenHLSL\\if4.hlsl"); }

TEST_F(CompilerTest, CodeGenIf5) { CodeGenTest(L"..\\CodeGenHLSL\\if5.hlsl"); }

TEST_F(CompilerTest, CodeGenIf6) { CodeGenTest(L"..\\CodeGenHLSL\\if6.hlsl"); }

TEST_F(CompilerTest, CodeGenIf7) { CodeGenTest(L"..\\CodeGenHLSL\\if7.hlsl"); }

TEST_F(CompilerTest, CodeGenIf8) { CodeGenTest(L"..\\CodeGenHLSL\\if8.hlsl"); }

TEST_F(CompilerTest, CodeGenIf9) { CodeGenTestCheck(L"..\\CodeGenHLSL\\if9.hlsl"); }

TEST_F(CompilerTest, CodeGenImm0) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\imm0.hlsl");
}

TEST_F(CompilerTest, CodeGenInclude) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Include.hlsl");
}

TEST_F(CompilerTest, CodeGenIncompletePos) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\incompletePos.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput2) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput2.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput3) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput3.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableinput4) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableinput4.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexableoutput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexableoutput1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp1.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp2) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp2.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexabletemp3) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexabletemp3.hlsl");
}

TEST_F(CompilerTest, CodeGenInoutSE) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout_se.hlsl");
}

TEST_F(CompilerTest, CodeGenInout1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout1.hlsl");
}

TEST_F(CompilerTest, CodeGenInout2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout2.hlsl");
}

TEST_F(CompilerTest, CodeGenInout3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\inout3.hlsl");
}

TEST_F(CompilerTest, CodeGenInput1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\input1.hlsl");
}

TEST_F(CompilerTest, CodeGenInput2) {
  CodeGenTest(L"..\\CodeGenHLSL\\input2.hlsl");
}

TEST_F(CompilerTest, CodeGenInput3) {
  CodeGenTest(L"..\\CodeGenHLSL\\input3.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic1.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic2.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_even) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_even.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_integer) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_integer.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_odd) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_odd.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic3_pow2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic3_pow2.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic4.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic4_dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic4_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic5.hlsl");
}

TEST_F(CompilerTest, CodeGenLegacyStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\legacy_struct.hlsl");
}

TEST_F(CompilerTest, CodeGenLitInParen) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\lit_in_paren.hlsl");
}

TEST_F(CompilerTest, CodeGenLiteralShift) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\literalShift.hlsl");
}

TEST_F(CompilerTest, CodeGenLiveness1) {
  CodeGenTest(L"..\\CodeGenHLSL\\liveness1.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop1.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource1.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource4.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource7.hlsl");
}

TEST_F(CompilerTest, CodeGenLocalRes7Dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\local_resource7_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop2.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\loop3.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop4) {
  CodeGenTest(L"..\\CodeGenHLSL\\loop4.hlsl");
}

TEST_F(CompilerTest, CodeGenLoop5) {
  CodeGenTest(L"..\\CodeGenHLSL\\loop5.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInit) {
  CodeGenTest(L"..\\CodeGenHLSL\\matInit.hlsl");
}

TEST_F(CompilerTest, CodeGenMatMulMat) {
  CodeGenTest(L"..\\CodeGenHLSL\\matMulMat.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOps) {
  // TODO: change to CodeGenTestCheck
  CodeGenTest(L"..\\CodeGenHLSL\\matOps.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrix_in_struct.hlsl");
}

TEST_F(CompilerTest, CodeGenMatInStructRet) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matrix_in_struct_ret.hlsl");
}

TEST_F(CompilerTest, CodeGenMatIn) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrixIn.hlsl");
}

TEST_F(CompilerTest, CodeGenMatOut) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrixOut.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript2.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript3.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript4.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript5.hlsl");
}

TEST_F(CompilerTest, CodeGenMatSubscript6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\matSubscript6.hlsl");
}

TEST_F(CompilerTest, CodeGenMaxMin) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\max_min.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec1.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec2.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec3.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec4.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec5.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec6.hlsl");
}

TEST_F(CompilerTest, CodeGenMinprec7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\minprec7.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiStream) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiStreamGS.hlsl");
}

TEST_F(CompilerTest, CodeGenMultiStream2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\multiStreamGS2.hlsl");
}

TEST_F(CompilerTest, CodeGenNeg1) {
  CodeGenTest(L"..\\CodeGenHLSL\\neg1.hlsl");
}

TEST_F(CompilerTest, CodeGenNeg2) {
  CodeGenTest(L"..\\CodeGenHLSL\\neg2.hlsl");
}

TEST_F(CompilerTest, CodeGenNegabs1) {
  CodeGenTest(L"..\\CodeGenHLSL\\negabs1.hlsl");
}

TEST_F(CompilerTest, CodeGenNonUniform) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\NonUniform.hlsl");
}

TEST_F(CompilerTest, CodeGenOptionGis) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\option_gis.hlsl");
}

TEST_F(CompilerTest, CodeGenOptionWX) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\option_WX.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output1.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output2.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output3.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output4.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output5.hlsl");
}

TEST_F(CompilerTest, CodeGenOutput6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\output6.hlsl");
}

TEST_F(CompilerTest, CodeGenOutputArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\outputArray.hlsl");
}

TEST_F(CompilerTest, CodeGenPassthrough1) {
  CodeGenTest(L"..\\CodeGenHLSL\\passthrough1.hlsl");
}

TEST_F(CompilerTest, CodeGenPassthrough2) {
  CodeGenTest(L"..\\CodeGenHLSL\\passthrough2.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise1.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise2.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise3.hlsl");
}

TEST_F(CompilerTest, CodeGenPrecise4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise4.hlsl");
}

TEST_F(CompilerTest, CodeGenPreciseOnCall) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise_call.hlsl");
}

TEST_F(CompilerTest, CodeGenPreciseOnCallNot) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\precise_call_not.hlsl");
}

TEST_F(CompilerTest, CodeGenRaceCond2) {
  CodeGenTest(L"..\\CodeGenHLSL\\RaceCond2.hlsl");
}

TEST_F(CompilerTest, CodeGenRaw_Buf1) {
  CodeGenTest(L"..\\CodeGenHLSL\\raw_buf1.hlsl");
}

TEST_F(CompilerTest, CodeGenRcp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\rcp1.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput2.hlsl");
}

TEST_F(CompilerTest, CodeGenReadFromOutput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\readFromOutput3.hlsl");
}

TEST_F(CompilerTest, CodeGenRedundantinput1) {
  CodeGenTest(L"..\\CodeGenHLSL\\redundantinput1.hlsl");
}

TEST_F(CompilerTest, CodeGenRes64bit) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\res64bit.hlsl");
}

TEST_F(CompilerTest, CodeGenRovs) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\rovs.hlsl");
}

TEST_F(CompilerTest, CodeGenRValSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\RValSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenSample1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sample1.hlsl");
}

TEST_F(CompilerTest, CodeGenSample2) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample2.hlsl");
}

TEST_F(CompilerTest, CodeGenSample3) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample3.hlsl");
}

TEST_F(CompilerTest, CodeGenSample4) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample4.hlsl");
}

TEST_F(CompilerTest, CodeGenSample5) {
  CodeGenTest(L"..\\CodeGenHLSL\\sample5.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleBias) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleBias.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmp.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmpLZ) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmpLZ.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleCmpLZ2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleCmpLZ2.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleGrad) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleGrad.hlsl");
}

TEST_F(CompilerTest, CodeGenSampleL) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\sampleL.hlsl");
}

TEST_F(CompilerTest, CodeGenSaturate1) {
  CodeGenTest(L"..\\CodeGenHLSL\\saturate1.hlsl");
}

TEST_F(CompilerTest, CodeGenScalarOnVecIntrinsic) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalarOnVecIntrisic.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj2.hlsl");
}

TEST_F(CompilerTest, CodeGenSelectObj3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selectObj3.hlsl");
}

TEST_F(CompilerTest, CodeGenSelMat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\selMat.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem_Dbg) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem_dbg.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem1.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem2.hlsl");
}

TEST_F(CompilerTest, CodeGenShare_Mem2Dim) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\share_mem2Dim.hlsl");
}

TEST_F(CompilerTest, CodeGenShift) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\shift.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleDS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleDS1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS2.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS3.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS4.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS5.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS6.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS7.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS11) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS11.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleGS12) {
	CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleGS12.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS1.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS2.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS3.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS4) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS4.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS5) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS5.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS6) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS6.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS7) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS7.hlsl");
}

TEST_F(CompilerTest, CodeGenSimpleHS8) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\SimpleHS8.hlsl");
}

TEST_F(CompilerTest, CodeGenSMFail) {
  CodeGenTestCheck(L"sm-fail.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Ms_Load1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\srv_ms_load1.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Ms_Load2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\srv_ms_load2.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Typed_Load1) {
  CodeGenTest(L"..\\CodeGenHLSL\\srv_typed_load1.hlsl");
}

TEST_F(CompilerTest, CodeGenSrv_Typed_Load2) {
  CodeGenTest(L"..\\CodeGenHLSL\\srv_typed_load2.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals) {
  CodeGenTest(L"..\\CodeGenHLSL\\staticGlobals.hlsl");
}

TEST_F(CompilerTest, CodeGenStaticGlobals2) {
  CodeGenTest(L"..\\CodeGenHLSL\\staticGlobals2.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Buf1) {
  CodeGenTest(L"..\\CodeGenHLSL\\struct_buf1.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_BufHasCounter) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_bufHasCounter.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_BufHasCounter2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\struct_bufHasCounter2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructCast) {
  CodeGenTest(L"..\\CodeGenHLSL\\StructCast.hlsl");
}

TEST_F(CompilerTest, CodeGenStructCast2) {
  CodeGenTest(L"..\\CodeGenHLSL\\StructCast2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer) {
  CodeGenTest(L"..\\CodeGenHLSL\\structInBuffer.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer2) {
  CodeGenTest(L"..\\CodeGenHLSL\\structInBuffer2.hlsl");
}

TEST_F(CompilerTest, CodeGenStructInBuffer3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\structInBuffer3.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitchFloat) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\switch_float.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\switch1.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch2) {
  CodeGenTest(L"..\\CodeGenHLSL\\switch2.hlsl");
}

TEST_F(CompilerTest, CodeGenSwitch3) {
  CodeGenTest(L"..\\CodeGenHLSL\\switch3.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzle1) {
  CodeGenTest(L"..\\CodeGenHLSL\\swizzle1.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzle2) {
  CodeGenTest(L"..\\CodeGenHLSL\\swizzle2.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzleAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\swizzleAtomic.hlsl");
}

TEST_F(CompilerTest, CodeGenSwizzleAtomic2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\swizzleAtomic2.hlsl");
}

TEST_F(CompilerTest, CodeGenTemp1) {
  CodeGenTest(L"..\\CodeGenHLSL\\temp1.hlsl");
}

TEST_F(CompilerTest, CodeGenTemp2) {
  CodeGenTest(L"..\\CodeGenHLSL\\temp2.hlsl");
}

TEST_F(CompilerTest, CodeGenTexSubscript) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\TexSubscript.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Raw1){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uav_raw1.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Typed_Load_Store1) {
  CodeGenTest(L"..\\CodeGenHLSL\\uav_typed_load_store1.hlsl");
}

TEST_F(CompilerTest, CodeGenUav_Typed_Load_Store2) {
  CodeGenTest(L"..\\CodeGenHLSL\\uav_typed_load_store2.hlsl");
}

TEST_F(CompilerTest, CodeGenUint64_1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uint64_1.hlsl");
}

TEST_F(CompilerTest, CodeGenUint64_2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uint64_2.hlsl");
}

TEST_F(CompilerTest, CodeGenUintSample) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uintSample.hlsl");
}

TEST_F(CompilerTest, CodeGenUmaxObjectAtomic) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\umaxObjectAtomic.hlsl");
}

TEST_F(CompilerTest, CodeGenUpdateCounter) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\updateCounter.hlsl");
}

TEST_F(CompilerTest, CodeGenUpperCaseRegister1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\uppercase-register1.hlsl");
}

TEST_F(CompilerTest, CodeGenVcmp) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vcmp.hlsl");
}

TEST_F(CompilerTest, CodeGenVec_Comp_Arg){
  CodeGenTest(L"..\\CodeGenHLSL\\vec_comp_arg.hlsl");
}

TEST_F(CompilerTest, CodeGenWave) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\wave.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput2.hlsl");
}

TEST_F(CompilerTest, CodeGenWriteToInput3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\writeToInput3.hlsl");
}

TEST_F(CompilerTest, CodeGenAttributes_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\attributes_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenConst_Exprb_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\const-exprB_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenConst_Expr_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\const-expr_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenFunctions_Mod){
  CodeGenTest(L"..\\CodeGenHLSL\\functions_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenImplicit_Casts_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\implicit-casts_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenIndexing_Operator_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\indexing-operator_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenIntrinsic_Examples_Mod) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\intrinsic-examples_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenLiterals_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\literals_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenMatrix_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrix-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenMatrix_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\matrix-syntax_Mod.hlsl");
}

//TEST_F(CompilerTest, CodeGenMore_Operators_Mod){
//  CodeGenTest(L"..\\CodeGenHLSL\\more-operators_Mod.hlsl");
//}

// TODO: enable this after support local/parameter resource.
//TEST_F(CompilerTest, CodeGenObject_Operators_Mod) {
//  CodeGenTest(L"..\\CodeGenHLSL\\object-operators_Mod.hlsl");
//}

TEST_F(CompilerTest, CodeGenPackreg_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\packreg_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenParameter_Types) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\parameter_types.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Operators_Assign_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-operators-assign_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenScalar_Operators_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\scalar-operators_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenSemantics_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\semantics_Mod.hlsl");
}

// TEST_F(CompilerTest, CodeGenSpec_Mod){
//  CodeGenTest(L"..\\CodeGenHLSL\\spec_Mod.hlsl");
//}

TEST_F(CompilerTest, CodeGenString_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\string_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\struct-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenStruct_AssignmentsFull_Mod){
  CodeGenTest(L"..\\CodeGenHLSL\\struct-assignmentsFull_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenTemplate_Checks_Mod) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\template-checks_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenToinclude2_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\toinclude2_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenTypemods_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\typemods-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVarmods_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\varmods-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Assignments_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-assignments_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Syntax_Mix_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-syntax-mix_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenVector_Syntax_Mod) {
  CodeGenTest(L"..\\CodeGenHLSL\\vector-syntax_Mod.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS2.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_PS3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_PS3.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_VS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenBasicHLSL11_VS2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\BasicHLSL11_VS2.hlsl");
}

TEST_F(CompilerTest, CodeGenVecIndexingInput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\vecIndexingInput.hlsl");
}

TEST_F(CompilerTest, CodeGenVecMulMat) {
  CodeGenTest(L"..\\CodeGenHLSL\\vecMulMat.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings1) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings1.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings2.hlsl");
}

TEST_F(CompilerTest, CodeGenBindings3) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\bindings2.hlsl");
}

TEST_F(CompilerTest, CodeGenResCopy) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resCopy.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInStruct) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-struct.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCBV) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cbv.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTB) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tb.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTBV) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tbv.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInStruct2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-struct2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCB2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cb2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInCBV2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-cbv2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTB2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tb2.hlsl");
}

TEST_F(CompilerTest, CodeGenResourceInTBV2) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\resource-in-tbv2.hlsl");
}

TEST_F(CompilerTest, CodeGenRootSigEntry) {
  CodeGenTest(L"..\\CodeGenHLSL\\rootSigEntry.hlsl");
}

TEST_F(CompilerTest, CodeGenCBufferStructArray) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\cbuffer-structarray.hlsl");
}


// Dx11 Sample

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Blurx_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_BlurX_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Blury_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_BlurY_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_2Dquadshaders_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\2DQuadShaders_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hdecode){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HDecode.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Encodeblockcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_EncodeBlockCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Trymodeg10Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_TryModeG10CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc6Hencode_Trymodele10Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC6HEncode_TryModeLE10CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Decode){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Decode.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Encodeblockcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_EncodeBlockCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode02Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode02CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode137Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode137CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Bc7Encode_Trymode456Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BC7Encode_TryMode456CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Brightpassandhorizfiltercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\BrightPassAndHorizFilterCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Computeshadersort11){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ComputeShaderSort11.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Computeshadersort11_Matrixtranspose){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ComputeShaderSort11_MatrixTranspose.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Sm_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_SM_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Contacthardeningshadows11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ContactHardeningShadows11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Decaltessellation11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DecalTessellation11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Detailtessellation11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DetailTessellation11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Dumptotexture){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\DumpToTexture.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Filtercs_Horz){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FilterCS_Horz.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Filtercs_Vertical){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FilterCS_Vertical.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Finalpass_Cpu_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FinalPass_CPU_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Finalpass_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FinalPass_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Buildgridcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_BuildGridCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Buildgridindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_BuildGridIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Cleargridindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ClearGridIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Grid){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Grid.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Shared){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Shared.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Densitycs_Simple){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_DensityCS_Simple.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Grid){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Grid.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Shared){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Shared.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Forcecs_Simple){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_ForceCS_Simple.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Integratecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_IntegrateCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidcs11_Rearrangeparticlescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidCS11_RearrangeParticlesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidrender_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidRender_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Fluidrender_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\FluidRender_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Nbodygravitycs11){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\NBodyGravityCS11.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Createprefixsum_Pass0_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_CreatePrefixSum_Pass0_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Createprefixsum_Pass1_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_CreatePrefixSum_Pass1_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Fragmentcountps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_FragmentCountPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Oit_Sortandrendercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\OIT_SortAndRenderCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particledraw_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ParticleDraw_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particledraw_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ParticleDraw_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Gs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_GS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Particle_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\Particle_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Tessvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_TessVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pntriangles11_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PNTriangles11_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pom_Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\POM_PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Pom_Vs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\POM_VS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Bloomps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_BloomPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale2X2_Lumps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale2x2_LumPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale3X3Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale3x3PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Downscale3X3_Brightpassps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_DownScale3x3_BrightPassPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Psapproach_Finalpassps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\PSApproach_FinalPassPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Reduceto1Dcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ReduceTo1DCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Reducetosinglecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\ReduceToSingleCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Rendervariancesceneps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\RenderVarianceScenePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Rendervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\RenderVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Ds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11DS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Hs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11HS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Simplebezier11Ps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SimpleBezier11PS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Bezierevalds){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_BezierEvalDS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Meshskinningvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_MeshSkinningVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Patchskinningvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_PatchSkinningVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Smoothps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SmoothPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Subdtobezierhs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SubDToBezierHS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Subd11_Subdtobezierhs4444){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\SubD11_SubDToBezierHS4444.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Edgefactorcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_EdgeFactorCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Numverticesindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_NumVerticesIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Scatteridcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_ScatterIDCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Tessellateindicescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_TessellateIndicesCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDX11Sample_Tessellatorcs40_Tessellateverticescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\DX11\\TessellatorCS40_TessellateVerticesCS.hlsl");
}

// Dx12 Sample

TEST_F(CompilerTest, CodeGenSamplesD12_DynamicIndexing_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_dynamic_indexing_pixel.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_ExecuteIndirect_CS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_execute_indirect_cs.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_MultiThreading_VS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_multithreading_vs.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_MultiThreading_PS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_multithreading_ps.hlsl");
}

TEST_F(CompilerTest, CodeGenSamplesD12_NBodyGravity_CS) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\d12_nBodyGravityCS.hlsl");
}

// Dx12 sample/MiniEngine
TEST_F(CompilerTest, CodeGenDx12MiniEngineAdaptexposurecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AdaptExposureCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsampleblendoutcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsampleBlendOutCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsampleCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplepreminblendoutcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsamplePreMinBlendOutCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAoblurupsamplepremincs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoBlurUpsamplePreMinCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAopreparedepthbuffers1Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoPrepareDepthBuffers1CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAopreparedepthbuffers2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoPrepareDepthBuffers2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAorender1Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoRender1CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAorender2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AoRender2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineApplybloomcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ApplyBloomCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineAveragelumacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\AverageLumaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubichorizontalupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicHorizontalUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicupsamplegammaps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicUpsampleGammaPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBicubicverticalupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BicubicVerticalUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBilinearupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BilinearUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBloomextractanddownsamplehdrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BloomExtractAndDownsampleHdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBloomextractanddownsampleldrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BloomExtractAndDownsampleLdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBlurcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BlurCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineBuffercopyps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\BufferCopyPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameramotionblurprepasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraMotionBlurPrePassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameramotionblurprepasslinearzcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraMotionBlurPrePassLinearZCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineCameravelocitycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\CameraVelocityCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineConvertldrtodisplayaltps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ConvertLDRToDisplayAltPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineConvertldrtodisplayps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ConvertLDRToDisplayPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugdrawhistogramcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugDrawHistogramCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugluminancehdrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugLuminanceHdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugluminanceldrcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugLuminanceLdrCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDebugssaocs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DebugSSAOCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDepthviewerps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DepthViewerPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDepthviewervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DepthViewerVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDownsamplebloomallcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DownsampleBloomAllCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineDownsamplebloomcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\DownsampleBloomCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineExtractlumacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ExtractLumaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass1_Luma_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass1_Luma_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass1_Rgb_Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass1_RGB_CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Hcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2HCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Hdebugcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2HDebugCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Vcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2VCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaapass2Vdebugcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAPass2VDebugCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineFxaaresolveworkqueuecs){
  if (!m_CompilerPreservesBBNames) return;
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\FXAAResolveWorkQueueCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratehistogramcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateHistogramCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammacs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddxcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddXCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipsgammaoddycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsGammaOddYCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddxcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddXCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineGeneratemipslinearoddycs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\GenerateMipsLinearOddYCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineLinearizedepthcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\LinearizeDepthCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMagnifypixelsps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MagnifyPixelsPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineModelviewerps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ModelViewerPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineModelviewervs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ModelViewerVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurfinalpasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurFinalPassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurfinalpasstemporalcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurFinalPassTemporalCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineMotionblurprepasscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\MotionBlurPrePassCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlebincullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleBinCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticledepthboundscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleDepthBoundsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticledispatchindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleDispatchIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlefinaldispatchindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleFinalDispatchIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleinnersortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleInnerSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlelargebincullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleLargeBinCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleoutersortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleOuterSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlepresortcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticlePreSortCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticlePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlesortindirectargscs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleSortIndirectArgsCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlespawncs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleSpawnCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilecullingcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileCullingCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerendercs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastdynamiccs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastDynamicCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderfastlowrescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderFastLowResCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderslowdynamiccs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderSlowDynamicCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticletilerenderslowlowrescs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleTileRenderSlowLowResCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticleupdatecs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleUpdateCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineParticlevs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ParticleVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphbackgroundvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphBackgroundVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEnginePerfgraphvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\PerfGraphVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineScreenquadvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ScreenQuadVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineSharpeningupsamplegammaps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\SharpeningUpsampleGammaPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineSharpeningupsampleps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\SharpeningUpsamplePS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTemporalblendcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TemporalBlendCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextantialiasps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextAntialiasPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextshadowps){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextShadowPS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTextvs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\TextVS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTonemap2Cs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ToneMap2CS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineTonemapcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\ToneMapCS.hlsl");
}

TEST_F(CompilerTest, CodeGenDx12MiniEngineUpsampleandblurcs){
  CodeGenTestCheck(L"..\\CodeGenHLSL\\Samples\\MiniEngine\\UpsampleAndBlurCS.hlsl");
}

TEST_F(CompilerTest, DxilGen_StoreOutput) {
  CodeGenTestCheck(L"..\\CodeGenHLSL\\dxilgen_storeoutput.hlsl");
}

TEST_F(CompilerTest, PreprocessWhenValidThenOK) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  DxcDefine defines[2];
  defines[0].Name = L"MYDEF";
  defines[0].Value = L"int";
  defines[1].Name = L"MYOTHERDEF";
  defines[1].Value = L"123";
  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "// First line\r\n"
    "MYDEF g_int = MYOTHERDEF;\r\n"
    "#define FOO BAR\r\n"
    "int FOO;", &pSource);
  VERIFY_SUCCEEDED(pCompiler->Preprocess(pSource, L"file.hlsl", nullptr, 0,
                                         defines, _countof(defines), nullptr,
                                         &pResult));
  HRESULT hrOp;
  VERIFY_SUCCEEDED(pResult->GetStatus(&hrOp));
  VERIFY_SUCCEEDED(hrOp);

  CComPtr<IDxcBlob> pOutText;
  VERIFY_SUCCEEDED(pResult->GetResult(&pOutText));
  std::string text(BlobToUtf8(pOutText));
  VERIFY_ARE_EQUAL_STR(
    "#line 1 \"file.hlsl\"\n"
    "\n"
    "int g_int = 123;\n"
    "\n"
    "int BAR;\n", text.c_str());
}

TEST_F(CompilerTest, WhenSigMismatchPCFunctionThenFail) {
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;

  VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));
  CreateBlobFromText(
    "struct PSSceneIn \n\
    { \n\
      float4 pos  : SV_Position; \n\
      float2 tex  : TEXCOORD0; \n\
      float3 norm : NORMAL; \n\
    }; \n"
    "struct HSPerPatchData {  \n\
      float edges[ 3 ] : SV_TessFactor; \n\
      float inside : SV_InsideTessFactor; \n\
      float foo : FOO; \n\
    }; \n"
    "HSPerPatchData HSPerPatchFunc( InputPatch< PSSceneIn, 3 > points, \n\
      OutputPatch<PSSceneIn, 3> outpoints) { \n\
      HSPerPatchData d = (HSPerPatchData)0; \n\
      d.edges[ 0 ] = points[0].tex.x + outpoints[0].tex.x; \n\
      d.edges[ 1 ] = 1; \n\
      d.edges[ 2 ] = 1; \n\
      d.inside = 1; \n\
      return d; \n\
    } \n"
    "[domain(\"tri\")] \n\
    [partitioning(\"fractional_odd\")] \n\
    [outputtopology(\"triangle_cw\")] \n\
    [patchconstantfunc(\"HSPerPatchFunc\")] \n\
    [outputcontrolpoints(3)] \n"
    "void main(const uint id : SV_OutputControlPointID, \n\
               const InputPatch< PSSceneIn, 3 > points ) { \n\
    } \n"
    , &pSource);

  VERIFY_SUCCEEDED(pCompiler->Compile(pSource, L"source.hlsl", L"main",
    L"hs_6_0", nullptr, 0, nullptr, 0, nullptr, &pResult));
  std::string failLog(VerifyOperationFailed(pResult));
  VERIFY_ARE_NOT_EQUAL(string::npos, failLog.find(
    "Signature element SV_Position, referred to by patch constant function, is not found in corresponding hull shader output."));
}
