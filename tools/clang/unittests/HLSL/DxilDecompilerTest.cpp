///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// DecompilerTest.cpp                                                          //
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

#include "dxc/DxilContainer/DxilContainer.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#include "dxc/dxcpix.h"
#include <algorithm>
#include <cassert>
#include <cfloat>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#ifdef _WIN32
#include "dia2.h"
#include <atlfile.h>
#endif

#include "dxc/Test/DxcTestUtils.h"
#include "dxc/Test/HLSLTestData.h"
#include "dxc/Test/HlslTestUtils.h"

#include "dxc/Support/Global.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/microcom.h"
#include "llvm/Support/raw_os_ostream.h"

#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/MSFileSystem.h"
#include "llvm/Support/Path.h"
#include <fstream>

using namespace std;
using namespace hlsl_test;

class TestIncludeHandler : public IDxcIncludeHandler {
  DXC_MICROCOM_REF_FIELD(m_dwRef)
public:
  DXC_MICROCOM_ADDREF_RELEASE_IMPL(m_dwRef)
  dxc::DxcDllSupport &m_dllSupport;
  HRESULT m_defaultErrorCode = E_FAIL;
  TestIncludeHandler(dxc::DxcDllSupport &dllSupport)
      : m_dwRef(0), m_dllSupport(dllSupport), callIndex(0) {}
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid,
                                           void **ppvObject) override {
    return DoBasicQueryInterface<IDxcIncludeHandler>(this, iid, ppvObject);
  }

  struct LoadSourceCallInfo {
    std::wstring Filename; // Filename as written in #include statement
    LoadSourceCallInfo(LPCWSTR pFilename) : Filename(pFilename) {}
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
    UINT32 codePage;
    LoadSourceCallResult() : hr(E_FAIL), codePage(0) {}
    LoadSourceCallResult(const char *pSource, UINT32 codePage = CP_UTF8)
        : hr(S_OK), source(pSource), codePage(codePage) {}
  };
  std::vector<LoadSourceCallResult> CallResults;
  size_t callIndex;

  HRESULT STDMETHODCALLTYPE LoadSource(
      _In_ LPCWSTR pFilename, // Filename as written in #include statement
      _COM_Outptr_ IDxcBlob *
          *ppIncludeSource // Resultant source object for included file
      ) override {
    CallInfos.push_back(LoadSourceCallInfo(pFilename));

    *ppIncludeSource = nullptr;
    if (callIndex >= CallResults.size()) {
      return m_defaultErrorCode;
    }
    if (FAILED(CallResults[callIndex].hr)) {
      return CallResults[callIndex++].hr;
    }
    MultiByteStringToBlob(m_dllSupport, CallResults[callIndex].source,
                          CallResults[callIndex].codePage, ppIncludeSource);
    return CallResults[callIndex++].hr;
  }
};

#ifdef _WIN32
class DecompilerTest {
#else
class DecompilerTest : public ::testing::Test {
#endif
public:
  BEGIN_TEST_CLASS(DecompilerTest)
  TEST_CLASS_PROPERTY(L"Parallel", L"true")
  TEST_METHOD_PROPERTY(L"Priority", L"0")
  END_TEST_CLASS()

  TEST_CLASS_SETUP(InitSupport);

  TEST_METHOD(CompileWhenDefinesThenApplied)

  dxc::DxcDllSupport m_dllSupport;
  VersionSupportInfo m_ver;

  void CreateBlobPinned(_In_bytecount_(size) LPCVOID data, SIZE_T size,
                        UINT32 codePage, _Outptr_ IDxcBlobEncoding **ppBlob) {
    CComPtr<IDxcLibrary> library;
    IFT(m_dllSupport.CreateInstance(CLSID_DxcLibrary, &library));
    IFT(library->CreateBlobWithEncodingFromPinned(data, size, codePage,
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
    CreateBlobPinned(pText, strlen(pText) + 1, CP_UTF8, ppBlob);
  }

  HRESULT CreateCompiler(IDxcCompiler4 **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcCompiler, ppResult);
  }

#ifdef _WIN32 // No ContainerBuilder support yet
  HRESULT CreateContainerBuilder(IDxcContainerBuilder **ppResult) {
    return m_dllSupport.CreateInstance(CLSID_DxcContainerBuilder, ppResult);
  }
#endif

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

   void CodeGenTest(std::wstring name) {
    CComPtr<IDxcCompiler4> pCompiler;
     CComPtr<IDxcResult> pResult;
    CComPtr<IDxcBlobEncoding> pSource;

    const std::wstring sourcePath   = L"..\\CodeGenDXIL\\" + name;
    const std::wstring sourceFile   = sourcePath + L".dxil";
    const std::wstring destFile     = sourcePath + L".hlsl";
    CreateBlobFromFile(sourceFile.c_str(), &pSource);

    VERIFY_SUCCEEDED(CreateCompiler(&pCompiler));

   DxcBuffer buffer = {pSource->GetBufferPointer(), pSource->GetBufferSize(), 0};

   VERIFY_SUCCEEDED(pCompiler->Decompile(&buffer, IID_PPV_ARGS(&pResult)));
   // VERIFY_SUCCEEDED(pCompiler->Disassemble(&buffer, IID_PPV_ARGS(&pResult)));

    HRESULT hr = S_OK;
    VERIFY_SUCCEEDED(pResult->GetStatus(&hr));
    VERIFY_SUCCEEDED(hr);

    
  CComPtr<IDxcBlobEncoding> pDecompileResult;
   VERIFY_SUCCEEDED(pResult->GetOutput(DXC_OUT_HLSL,
                                      IID_PPV_ARGS(&pDecompileResult), nullptr));

    dxc::WriteBlobToConsole(pDecompileResult);
  // dxc::WriteBlobToFile(pDecompileResult, destFile.c_str(), DXC_CP_UTF8);
  }

  std::string GetOption(std::string &cmd, char *opt) {
    std::string option = cmd.substr(cmd.find(opt));
    option = option.substr(option.find_first_of(' '));
    option = option.substr(option.find_first_not_of(' '));
    return option.substr(0, option.find_first_of(' '));
  }

  void CodeGenTestHashFullPath(LPCWSTR fullPath) {
    FileRunTestResult t =
        FileRunTestResult::RunHashTestFromFileCommands(fullPath);
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CodeGenTestHash(LPCWSTR name, bool implicitDir) {
    std::wstring path = name;
    if (implicitDir) {
      path.insert(0, L"..\\CodeGenHLSL\\");
      path = hlsl_test::GetPathToHlslDataFile(path.c_str());
    }
    CodeGenTestHashFullPath(path.c_str());
  }

  void CodeGenTestCheckBatchHash(std::wstring suitePath,
                                 bool implicitDir = true) {
    using namespace llvm;
    using namespace WEX::TestExecution;

    if (implicitDir)
      suitePath.insert(0, L"..\\HLSLFileCheck\\");

    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    CW2A pUtf8Filename(suitePath.c_str());
    if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
      suitePath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str());
    }

    CW2A utf8SuitePath(suitePath.c_str());

    unsigned numTestsRun = 0;

    std::error_code EC;
    llvm::SmallString<128> DirNative;
    llvm::sys::path::native(utf8SuitePath.m_psz, DirNative);
    for (llvm::sys::fs::recursive_directory_iterator Dir(DirNative, EC), DirEnd;
         Dir != DirEnd && !EC; Dir.increment(EC)) {
      // Check whether this entry has an extension typically associated with
      // headers.
      if (!llvm::StringSwitch<bool>(llvm::sys::path::extension(Dir->path()))
               .Cases(".hlsl", ".ll", true)
               .Default(false))
        continue;
      StringRef filename = Dir->path();
      std::string filetag = Dir->path();
      filetag += "<HASH>";

      CA2W wRelTag(filetag.data());
      CA2W wRelPath(filename.data());

      WEX::Logging::Log::StartGroup(wRelTag);
      CodeGenTestHash(wRelPath, /*implicitDir*/ false);
      WEX::Logging::Log::EndGroup(wRelTag);

      numTestsRun++;
    }

    VERIFY_IS_GREATER_THAN(numTestsRun, (unsigned)0,
                           L"No test files found in batch directory.");
  }

  void CodeGenTestCheckFullPath(LPCWSTR fullPath, LPCWSTR dumpPath = nullptr) {
    // Create file system if needed
    llvm::sys::fs::MSFileSystem *msfPtr =
        llvm::sys::fs::GetCurrentThreadFileSystem();
    std::unique_ptr<llvm::sys::fs::MSFileSystem> msf;
    if (!msfPtr) {
      VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
      msf.reset(msfPtr);
    }
    llvm::sys::fs::AutoPerThreadSystem pts(msfPtr);
    IFTLLVM(pts.error_code());

    FileRunTestResult t = FileRunTestResult::RunFromFileCommands(
        fullPath,
        /*pPluginToolsPaths*/ nullptr, dumpPath);
    if (t.RunResult != 0) {
      CA2W commentWide(t.ErrorMessage.c_str(), CP_UTF8);
      WEX::Logging::Log::Comment(commentWide);
      WEX::Logging::Log::Error(L"Run result is not zero");
    }
  }

  void CodeGenTestCheck(LPCWSTR name, bool implicitDir = true,
                        LPCWSTR dumpPath = nullptr) {
    std::wstring path = name;
    std::wstring dumpStr;
    if (implicitDir) {
      path.insert(0, L"..\\CodeGenHLSL\\");
      path = hlsl_test::GetPathToHlslDataFile(path.c_str());
      if (!dumpPath) {
        dumpStr = hlsl_test::GetPathToHlslDataFile(path.c_str(),
                                                   FILECHECKDUMPDIRPARAM);
        dumpPath = dumpStr.empty() ? nullptr : dumpStr.c_str();
      }
    }
    CodeGenTestCheckFullPath(path.c_str(), dumpPath);
  }

  void CodeGenTestCheckBatchDir(std::wstring suitePath,
                                bool implicitDir = true) {
    using namespace llvm;
    using namespace WEX::TestExecution;

    if (implicitDir)
      suitePath.insert(0, L"..\\HLSLFileCheck\\");

    ::llvm::sys::fs::MSFileSystem *msfPtr;
    VERIFY_SUCCEEDED(CreateMSFileSystemForDisk(&msfPtr));
    std::unique_ptr<::llvm::sys::fs::MSFileSystem> msf(msfPtr);
    ::llvm::sys::fs::AutoPerThreadSystem pts(msf.get());
    IFTLLVM(pts.error_code());

    std::wstring dumpPath;
    CW2A pUtf8Filename(suitePath.c_str());
    if (!llvm::sys::path::is_absolute(pUtf8Filename.m_psz)) {
      dumpPath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str(),
                                                  FILECHECKDUMPDIRPARAM);
      suitePath = hlsl_test::GetPathToHlslDataFile(suitePath.c_str());
    }

    CW2A utf8SuitePath(suitePath.c_str());

    unsigned numTestsRun = 0;

    std::error_code EC;
    llvm::SmallString<128> DirNative;
    llvm::sys::path::native(utf8SuitePath.m_psz, DirNative);
    for (llvm::sys::fs::recursive_directory_iterator Dir(DirNative, EC), DirEnd;
         Dir != DirEnd && !EC; Dir.increment(EC)) {
      // Check whether this entry has an extension typically associated with
      // headers.
      if (!llvm::StringSwitch<bool>(llvm::sys::path::extension(Dir->path()))
               .Cases(".hlsl", ".ll", true)
               .Default(false))
        continue;
      StringRef filename = Dir->path();
      CA2W wRelPath(filename.data());
      std::wstring dumpStr;
      if (!dumpPath.empty() &&
          suitePath.compare(0, suitePath.size(), wRelPath.m_psz,
                            suitePath.size()) == 0) {
        dumpStr = dumpPath + (wRelPath.m_psz + suitePath.size());
      }

      WEX::Logging::Log::StartGroup(wRelPath);
      CodeGenTestCheck(wRelPath, /*implicitDir*/ false,
                       dumpStr.empty() ? nullptr : dumpStr.c_str());
      WEX::Logging::Log::EndGroup(wRelPath);

      numTestsRun++;
    }

    VERIFY_IS_GREATER_THAN(numTestsRun, (unsigned)0,
                           L"No test files found in batch directory.");
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

bool DecompilerTest::InitSupport() {
  if (!m_dllSupport.IsEnabled()) {
    VERIFY_SUCCEEDED(m_dllSupport.Initialize());
    m_ver.Initialize(m_dllSupport);
  }
  return true;
}

TEST_F(DecompilerTest, CompileWhenDefinesThenApplied) { 
  CodeGenTest(L"cs1ac132b695bff3a4"); // surfelGridClearCs
  // CodeGenTest(L"cs2b69ce61d4fb649b"); // surfelGridBinCs
}
