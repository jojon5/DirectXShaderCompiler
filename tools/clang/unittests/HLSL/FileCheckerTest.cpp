///////////////////////////////////////////////////////////////////////////////
//                                                                           //
// FileCheckerTest.cpp                                                       //
// Copyright (C) Microsoft Corporation. All rights reserved.                 //
// This file is distributed under the University of Illinois Open Source     //
// License. See LICENSE.TXT for details.                                     //
//                                                                           //
// Provides tests that are based on FileChecker.                             //
//                                                                           //
///////////////////////////////////////////////////////////////////////////////

#ifndef UNICODE
#define UNICODE
#endif

#include <memory>
#include <vector>
#include <string>
#include <cctype>
#include <cassert>
#include <algorithm>
#include "dxc/Support/WinIncludes.h"
#include "dxc/dxcapi.h"
#ifdef _WIN32
#include <atlfile.h>
#endif

#include "HLSLTestData.h"
#include "HlslTestUtils.h"
#include "DxcTestUtils.h"

#include "llvm/Support/raw_os_ostream.h"
#include "llvm/Support/MD5.h"
#include "dxc/Support/Global.h"
#include "dxc/Support/dxcapi.use.h"
#include "dxc/Support/HLSLOptions.h"
#include "dxc/Support/Unicode.h"
#include "dxc/DxilContainer/DxilContainer.h"
#include "D3DReflectionDumper.h"

#include "d3d12shader.h"

using namespace std;
using namespace hlsl_test;

FileRunCommandPart::FileRunCommandPart(const std::string &command, const std::string &arguments, LPCWSTR commandFileName) :
  Command(command), Arguments(arguments), CommandFileName(commandFileName) { }

FileRunCommandResult FileRunCommandPart::RunHashTests(dxc::DxcDllSupport &DllSupport) {
  if (0 == _stricmp(Command.c_str(), "%dxc")) {
    return RunDxcHashTest(DllSupport);
  }
  else {
    return FileRunCommandResult::Success();
  }
}

FileRunCommandResult FileRunCommandPart::Run(dxc::DxcDllSupport &DllSupport, const FileRunCommandResult *Prior) {
  bool isFileCheck =
    0 == _stricmp(Command.c_str(), "FileCheck") ||
    0 == _stricmp(Command.c_str(), "%FileCheck");
  bool isXFail = 0 == _stricmp(Command.c_str(), "xfail");
  bool consumeErrors = isFileCheck || isXFail;

  // Stop the pipeline if on errors unless the command can consume them.
  if (Prior != nullptr && Prior->ExitCode && !consumeErrors) {
    FileRunCommandResult result = *Prior;
    result.AbortPipeline = true;
    return result;
  }

  // We would add support for 'not' and 'llc' here.
  if (isFileCheck) {
    return RunFileChecker(Prior);
  }
  else if (isXFail) {
    return RunXFail(Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "tee")) {
    return RunTee(Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "%dxilver")) {
    return RunDxilVer(DllSupport, Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "%dxc")) {
    return RunDxc(DllSupport, Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "%dxv")) {
    return RunDxv(DllSupport, Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "%opt")) {
    return RunOpt(DllSupport, Prior);
  }
  else if (0 == _stricmp(Command.c_str(), "%D3DReflect")) {
    return RunD3DReflect(DllSupport, Prior);
  }
  else {
    FileRunCommandResult result {};
    result.ExitCode = 1;
    result.StdErr = "Unrecognized command ";
    result.StdErr += Command;
    return result;
  }
}

FileRunCommandResult FileRunCommandPart::RunFileChecker(const FileRunCommandResult *Prior) {
  if (!Prior) return FileRunCommandResult::Error("Prior command required to generate stdin");

  FileCheckForTest t;
  t.CheckFilename = CW2A(CommandFileName, CP_UTF8);
  t.InputForStdin = Prior->ExitCode ? Prior->StdErr : Prior->StdOut;

  // Parse command arguments
  static constexpr char checkPrefixStr[] = "-check-prefix=";
  static constexpr char checkPrefixesStr[] = "-check-prefixes=";
  bool hasInputFilename = false;
  for (const std::string& arg : strtok(Arguments)) {
    if (arg == "%s") hasInputFilename = true;
    else if (arg == "-input=stderr") t.InputForStdin = Prior->StdErr;
    else if (strstartswith(arg, checkPrefixStr))
      t.CheckPrefixes.emplace_back(arg.substr(sizeof(checkPrefixStr) - 1));
    else if (strstartswith(arg, checkPrefixesStr)) {
      auto prefixes = strtok(arg.substr(sizeof(checkPrefixesStr) - 1), ", ");
      for (auto &prefix : prefixes)
        t.CheckPrefixes.emplace_back(prefix);
    }
    else return FileRunCommandResult::Error("Invalid argument");
  }
  if (!hasInputFilename) return FileRunCommandResult::Error("Missing input filename");

  FileRunCommandResult result {};
  // Run
  result.ExitCode = t.Run();
  result.StdOut = t.test_outs;
  result.StdErr = t.test_errs;
  // Capture the input as well.
  if (result.ExitCode != 0 && Prior != nullptr) {
    result.StdErr += "\n<full input to FileCheck>\n";
    result.StdErr += t.InputForStdin;
  }

  return result;
}

FileRunCommandResult FileRunCommandPart::ReadOptsForDxc(
    hlsl::options::MainArgs &argStrings, hlsl::options::DxcOpts &Opts) {
  std::string args(strtrim(Arguments));
  const char *inputPos = strstr(args.c_str(), "%s");
  if (inputPos == nullptr)
    return FileRunCommandResult::Error("Only supported pattern includes input file as argument");
  args.erase(inputPos - args.c_str(), strlen("%s"));

  llvm::StringRef argsRef = args;
  llvm::SmallVector<llvm::StringRef, 8> splitArgs;
  argsRef.split(splitArgs, " ");
  argStrings = hlsl::options::MainArgs(splitArgs);
  std::string errorString;
  llvm::raw_string_ostream errorStream(errorString);
  int RunResult = ReadDxcOpts(hlsl::options::getHlslOptTable(), /*flagsToInclude*/ 0,
                          argStrings, Opts, errorStream);
  errorStream.flush();
  if (RunResult)
    return FileRunCommandResult::Error(RunResult, errorString);

  return FileRunCommandResult::Success("");
}

static HRESULT ReAssembleTo(dxc::DxcDllSupport &DllSupport, void *bitcode, UINT32 size, IDxcBlob **pBlob) {
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcLibrary> pLibrary;
  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(DllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));

  CComPtr<IDxcBlobEncoding> pInBlob;

  IFT(pLibrary->CreateBlobWithEncodingFromPinned(bitcode, size, 0, &pInBlob));
  
  CComPtr<IDxcOperationResult> pResult;
  pAssembler->AssembleToContainer(pInBlob, &pResult);

  HRESULT Result = 0;
  IFT(pResult->GetStatus(&Result));
  IFT(Result);

  IFT(pResult->GetResult(pBlob));

  return S_OK;
}

static HRESULT GetDxilBitcode(dxc::DxcDllSupport &DllSupport, IDxcBlob *pCompiledBlob, IDxcBlob **pBitcodeBlob) {
  CComPtr<IDxcContainerReflection> pReflection;
  CComPtr<IDxcLibrary> pLibrary;
  IFT(DllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));
  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));

  IFT(pReflection->Load(pCompiledBlob));

  UINT32 uIndex = 0;
  IFT(pReflection->FindFirstPartKind(hlsl::DFCC_DXIL, &uIndex));
  CComPtr<IDxcBlob> pPart;
  IFT(pReflection->GetPartContent(uIndex, &pPart));

  auto header = (hlsl::DxilProgramHeader*)pPart->GetBufferPointer();
  void *bitcode = (char *)&header->BitcodeHeader + header->BitcodeHeader.BitcodeOffset;
  UINT32 bitcode_size = header->BitcodeHeader.BitcodeSize;

  CComPtr<IDxcBlobEncoding> pBlob;
  IFT(pLibrary->CreateBlobWithEncodingFromPinned(bitcode, bitcode_size, 0, &pBlob));
  *pBitcodeBlob = pBlob.Detach();

  return S_OK;
}

static HRESULT CompileForHash(hlsl::options::DxcOpts &opts, LPCWSTR CommandFileName, dxc::DxcDllSupport &DllSupport, std::vector<LPCWSTR> &flags, IDxcBlob **ppHashBlob, std::string &output) {
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcCompiler2> pCompiler2;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlob> pCompiledBlob;
  CComPtr<IDxcBlob> pCompiledName;
  CComPtr<IDxcIncludeHandler> pIncludeHandler;
  WCHAR *pDebugName = nullptr;
  CComPtr<IDxcBlob> pPDBBlob;

  std::wstring entry =
      Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
  std::wstring profile =
      Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());

  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
  IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
  IFT(DllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  IFT(pCompiler.QueryInterface(&pCompiler2));
  IFT(pCompiler2->CompileWithDebug(pSource, CommandFileName, entry.c_str(), profile.c_str(),
    flags.data(), flags.size(), nullptr, 0, pIncludeHandler, &pResult, &pDebugName, &pPDBBlob));

  HRESULT resultStatus = 0;
  IFT(pResult->GetStatus(&resultStatus));
  if (SUCCEEDED(resultStatus)) {

    IFT(pResult->GetResult(&pCompiledBlob));

    CComPtr<IDxcContainerReflection> pReflection;
    IFT(DllSupport.CreateInstance(CLSID_DxcContainerReflection, &pReflection));

    // If failed to load here, it's likely some non-compile operation thing. Just fail the hash generation.
    if (FAILED(pReflection->Load(pCompiledBlob)))
      return E_FAIL;

    *ppHashBlob = nullptr;
    UINT32 uHashIdx = 0;
    if (SUCCEEDED(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderHash, &uHashIdx))) {
      CComPtr<IDxcBlob> pHashBlob;
      IFT(pReflection->GetPartContent(uHashIdx, &pHashBlob));
      *ppHashBlob = pHashBlob.Detach();
    }

    // Test that PDB is generated correctly.
    // This test needs to be done elsewhere later, ideally a fully
    // customizable test on all our test set with different compile options.
    if (pPDBBlob) {
      IFT(pReflection->Load(pPDBBlob));
      UINT32 uDebugInfoIndex = 0;
      IFT(pReflection->FindFirstPartKind(hlsl::DFCC_ShaderDebugInfoDXIL, &uDebugInfoIndex));
    }

    return S_OK;
  }
  else {
    CComPtr<IDxcBlobEncoding> pErrors;
    IFT(pResult->GetErrorBuffer(&pErrors));
    const char *errors = (char *)pErrors->GetBufferPointer();
    output = errors;
    return resultStatus;
  }
}

FileRunCommandResult FileRunCommandPart::RunDxcHashTest(dxc::DxcDllSupport &DllSupport) {
  hlsl::options::MainArgs args;
  hlsl::options::DxcOpts opts;
  ReadOptsForDxc(args, opts);

  std::vector<std::wstring> argWStrings;
  CopyArgsToWStrings(opts.Args, hlsl::options::CoreOption, argWStrings);

  // Extract the vanilla flags for the test (i.e. no debug or ast-dump)
  std::vector<LPCWSTR> original_flags;
  for (const std::wstring &a : argWStrings) {
    if (a.find(L"ast-dump") != std::wstring::npos) continue;
    if (a.find(L"Zi") != std::wstring::npos) continue;
    original_flags.push_back(a.data());
  }

  std::string originalOutput;
  CComPtr<IDxcBlob> pOriginalHash;
  // If failed the original compilation, just pass the test. The original test was likely
  // testing for failure.
  if (FAILED(CompileForHash(opts, CommandFileName, DllSupport, original_flags, &pOriginalHash, originalOutput)))
    return FileRunCommandResult::Success();

  // Results of our compilations
  CComPtr<IDxcBlob> pHash1;
  std::string Output0;
  CComPtr<IDxcBlob> pHash0;
  std::string Output1;

  // Fail if -Qstrip_reflect failed the compilation
  std::vector<LPCWSTR> normal_flags = original_flags;
  normal_flags.push_back(L"-Qstrip_reflect");
  normal_flags.push_back(L"-Zsb");
  std::string StdErr;
  if (FAILED(CompileForHash(opts, CommandFileName, DllSupport, normal_flags, &pHash0, Output0))) {
    StdErr += "Adding Qstrip_reflect failed compilation.";
    StdErr += originalOutput;
    StdErr += Output0;
    return FileRunCommandResult::Error(StdErr);
  }

  // Fail if -Qstrip_reflect failed the compilation
  std::vector<LPCWSTR> dbg_flags = original_flags;
  dbg_flags.push_back(L"/Zi");
  dbg_flags.push_back(L"-Qstrip_reflect");
  dbg_flags.push_back(L"-Zsb");
  if (FAILED(CompileForHash(opts, CommandFileName, DllSupport, dbg_flags, &pHash1, Output1))) {
    return FileRunCommandResult::Error("Adding Qstrip_reflect and Zi failed compilation.");
  }

  if (pHash0->GetBufferSize() != pHash1->GetBufferSize() || 0 != memcmp(pHash0->GetBufferPointer(), pHash0->GetBufferPointer(), pHash1->GetBufferSize())) {
    StdErr = "Hashes do not match between normal and debug!!!\n";
    StdErr += Output0;
    StdErr += Output1;
    return FileRunCommandResult::Error(StdErr);
  }

  return FileRunCommandResult::Success();
}

static FileRunCommandResult CheckDxilVer(dxc::DxcDllSupport& DllSupport,
                                         unsigned RequiredDxilMajor,
                                         unsigned RequiredDxilMinor,
                                         bool bCheckValidator = true) {
  bool Supported = true;

  // If the following fails, we have Dxil 1.0 compiler
  unsigned DxilMajor = 1, DxilMinor = 0;
  GetVersion(DllSupport, CLSID_DxcCompiler, DxilMajor, DxilMinor);
  Supported &= hlsl::DXIL::CompareVersions(DxilMajor, DxilMinor, RequiredDxilMajor, RequiredDxilMinor) >= 0;

  if (bCheckValidator) {
    // If the following fails, we have validator 1.0
    unsigned ValMajor = 1, ValMinor = 0;
    GetVersion(DllSupport, CLSID_DxcValidator, ValMajor, ValMinor);
    Supported &= hlsl::DXIL::CompareVersions(ValMajor, ValMinor, RequiredDxilMajor, RequiredDxilMinor) >= 0;
  }

  if (!Supported) {
    FileRunCommandResult result {};
    result.StdErr = "Skipping test due to unsupported dxil version";
    result.ExitCode = 0; // Succeed the test
    result.AbortPipeline = true;
    return result;
  }

  return FileRunCommandResult::Success();
}

FileRunCommandResult FileRunCommandPart::RunDxc(dxc::DxcDllSupport &DllSupport, const FileRunCommandResult *Prior) {
  // Support piping stdin from prior if needed.
  UNREFERENCED_PARAMETER(Prior);
  hlsl::options::MainArgs args;
  hlsl::options::DxcOpts opts;
  FileRunCommandResult readOptsResult = ReadOptsForDxc(args, opts);
  if (readOptsResult.ExitCode) return readOptsResult;

  std::wstring entry =
      Unicode::UTF8ToUTF16StringOrThrow(opts.EntryPoint.str().c_str());
  std::wstring profile =
      Unicode::UTF8ToUTF16StringOrThrow(opts.TargetProfile.str().c_str());
  std::vector<LPCWSTR> flags;
  if (opts.CodeGenHighLevel) {
    flags.push_back(L"-fcgl");
  }

  // Skip targets that require a newer compiler or validator.
  // Some features may require newer compiler/validator than indicated by the
  // shader model, but these should use %dxilver explicitly.
  {
    unsigned RequiredDxilMajor = 1, RequiredDxilMinor = 0;
    llvm::StringRef stage;
    IFTBOOL(ParseTargetProfile(opts.TargetProfile, stage, RequiredDxilMajor, RequiredDxilMinor), E_INVALIDARG);
    if (RequiredDxilMinor != 0xF && stage.compare("rootsig") != 0) {
      // Convert stage to minimum dxil/validator version:
      RequiredDxilMajor = std::max(RequiredDxilMajor, (unsigned)6) - 5;
      FileRunCommandResult result = CheckDxilVer(DllSupport, RequiredDxilMajor, RequiredDxilMinor, !opts.DisableValidation);
      if (result.AbortPipeline) {
        return result;
      }
    }
  }

  // For now, too many tests are sensitive to stripping the refleciton info
  // from the main module, so use this flag to prevent this until tests
  // can be updated.
  // That is, unless the test explicitly requests -Qstrip_reflect_from_dxil or -Qstrip_reflect
  if (!opts.StripReflectionFromDxil && !opts.StripReflection) {
    flags.push_back(L"-Qkeep_reflect_in_dxil");
  }

  std::vector<std::wstring> argWStrings;
  CopyArgsToWStrings(opts.Args, hlsl::options::CoreOption, argWStrings);
  for (const std::wstring &a : argWStrings)
    flags.push_back(a.data());

  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcCompiler> pCompiler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pDisassembly;
  CComPtr<IDxcBlob> pCompiledBlob;
  CComPtr<IDxcIncludeHandler> pIncludeHandler;

  HRESULT resultStatus;

  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
  IFT(pLibrary->CreateIncludeHandler(&pIncludeHandler));
  IFT(DllSupport.CreateInstance(CLSID_DxcCompiler, &pCompiler));
  IFT(pCompiler->Compile(pSource, CommandFileName, entry.c_str(), profile.c_str(),
                          flags.data(), flags.size(), nullptr, 0, pIncludeHandler, &pResult));
  IFT(pResult->GetStatus(&resultStatus));

  FileRunCommandResult result = {};
  if (SUCCEEDED(resultStatus)) {
    IFT(pResult->GetResult(&pCompiledBlob));
    if (!opts.AstDump) {
      IFT(pCompiler->Disassemble(pCompiledBlob, &pDisassembly));
      result.StdOut = BlobToUtf8(pDisassembly);
    } else {
      result.StdOut = BlobToUtf8(pCompiledBlob);
    }
    CComPtr<IDxcBlobEncoding> pStdErr;
    IFT(pResult->GetErrorBuffer(&pStdErr));
    result.StdErr = BlobToUtf8(pStdErr);
    result.ExitCode = 0;
  }
  else {
    IFT(pResult->GetErrorBuffer(&pDisassembly));
    result.StdErr = BlobToUtf8(pDisassembly);
    result.ExitCode = resultStatus;
  }

  result.OpResult = pResult;
  return result;
}

FileRunCommandResult FileRunCommandPart::RunDxv(dxc::DxcDllSupport &DllSupport, const FileRunCommandResult *Prior) {
  std::string args(strtrim(Arguments));
  const char *inputPos = strstr(args.c_str(), "%s");
  if (inputPos == nullptr) {
    return FileRunCommandResult::Error("Only supported pattern includes input file as argument");
  }
  args.erase(inputPos - args.c_str(), strlen("%s"));

  llvm::StringRef argsRef = args;
  llvm::SmallVector<llvm::StringRef, 8> splitArgs;
  argsRef.split(splitArgs, " ");
  IFTMSG(splitArgs.size()==1, "wrong arg num for dxv");
      
  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcValidator> pValidator;
  CComPtr<IDxcOperationResult> pResult;

  CComPtr<IDxcBlobEncoding> pSource;

  CComPtr<IDxcBlob> pContainerBlob;
  HRESULT resultStatus;

  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
  IFT(DllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
  IFT(pAssembler->AssembleToContainer(pSource, &pResult));
  IFT(pResult->GetStatus(&resultStatus));
  if (FAILED(resultStatus)) {
    CComPtr<IDxcBlobEncoding> pAssembleBlob;
    IFT(pResult->GetErrorBuffer(&pAssembleBlob));
    return FileRunCommandResult::Error(resultStatus, BlobToUtf8(pAssembleBlob));
  }
  IFT(pResult->GetResult(&pContainerBlob));

  IFT(DllSupport.CreateInstance(CLSID_DxcValidator, &pValidator));
  CComPtr<IDxcOperationResult> pValidationResult;
  IFT(pValidator->Validate(pContainerBlob, DxcValidatorFlags_InPlaceEdit,
                            &pValidationResult));
  IFT(pValidationResult->GetStatus(&resultStatus));

  if (FAILED(resultStatus)) {
    CComPtr<IDxcBlobEncoding> pValidateBlob;
    IFT(pValidationResult->GetErrorBuffer(&pValidateBlob));
    return FileRunCommandResult::Success(BlobToUtf8(pValidateBlob));
  }

  return FileRunCommandResult::Success("");
}

FileRunCommandResult FileRunCommandPart::RunOpt(dxc::DxcDllSupport &DllSupport, const FileRunCommandResult *Prior) {
  std::string args(strtrim(Arguments));
  const char *inputPos = strstr(args.c_str(), "%s");
  if (inputPos == nullptr && Prior == nullptr) {
    return FileRunCommandResult::Error("Only supported patterns are input file as argument or prior "
      "command with disassembly");
  }

  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcOptimizer> pOptimizer;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcBlobEncoding> pOutputText;
  CComPtr<IDxcBlob> pOutputModule;

  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(DllSupport.CreateInstance(CLSID_DxcOptimizer, &pOptimizer));

  if (inputPos != nullptr) {
    args.erase(inputPos - args.c_str(), strlen("%s"));
    IFT(pLibrary->CreateBlobFromFile(CommandFileName, nullptr, &pSource));
  }
  else {
    assert(Prior != nullptr && "else early check should have returned");
    CComPtr<IDxcAssembler> pAssembler;
    IFT(DllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));
    IFT(pLibrary->CreateBlobWithEncodingFromPinned(
        Prior->StdOut.c_str(), Prior->StdOut.size(), CP_UTF8,
        &pSource));
  }

  args = strtrim(args);
  llvm::StringRef argsRef = args;
  llvm::SmallVector<llvm::StringRef, 8> splitArgs;
  argsRef.split(splitArgs, " ");
  std::vector<LPCWSTR> options;
  std::vector<std::wstring> optionStrings;
  for (llvm::StringRef S : splitArgs) {
    optionStrings.push_back(
        Unicode::UTF8ToUTF16StringOrThrow(strtrim(S.str()).c_str()));
  }

  // Add the options outside the above loop in case the vector is resized.
  for (const std::wstring& str : optionStrings)
    options.push_back(str.c_str());

  IFT(pOptimizer->RunOptimizer(pSource, options.data(), options.size(),
                                &pOutputModule, &pOutputText));
  return FileRunCommandResult::Success(BlobToUtf8(pOutputText));
}

FileRunCommandResult FileRunCommandPart::RunD3DReflect(dxc::DxcDllSupport &DllSupport, const FileRunCommandResult *Prior) {
  std::string args(strtrim(Arguments));
  if (args != "%s")
    return FileRunCommandResult::Error("Only supported pattern is a plain input file");
  if (!Prior)
    return FileRunCommandResult::Error("Prior command required to generate stdin");

  CComPtr<IDxcLibrary> pLibrary;
  CComPtr<IDxcBlobEncoding> pSource;
  CComPtr<IDxcAssembler> pAssembler;
  CComPtr<IDxcOperationResult> pResult;
  CComPtr<ID3D12ShaderReflection> pShaderReflection;
  CComPtr<ID3D12LibraryReflection> pLibraryReflection;
  CComPtr<IDxcContainerReflection> containerReflection;
  uint32_t partCount;
  CComPtr<IDxcBlob> pContainerBlob;
  HRESULT resultStatus;
  bool blobFound = false;
  std::ostringstream ss;
  D3DReflectionDumper dumper(ss);

  IFT(DllSupport.CreateInstance(CLSID_DxcLibrary, &pLibrary));
  IFT(DllSupport.CreateInstance(CLSID_DxcAssembler, &pAssembler));

  IFT(pLibrary->CreateBlobWithEncodingFromPinned(
      (LPBYTE)Prior->StdOut.c_str(), Prior->StdOut.size(), CP_UTF8,
      &pSource));

  IFT(pAssembler->AssembleToContainer(pSource, &pResult));
  IFT(pResult->GetStatus(&resultStatus));
  if (FAILED(resultStatus)) {
    CComPtr<IDxcBlobEncoding> pAssembleBlob;
    IFT(pResult->GetErrorBuffer(&pAssembleBlob));
    return FileRunCommandResult::Error(resultStatus, BlobToUtf8(pAssembleBlob));
  }
  IFT(pResult->GetResult(&pContainerBlob));

  VERIFY_SUCCEEDED(DllSupport.CreateInstance(CLSID_DxcContainerReflection, &containerReflection));
  VERIFY_SUCCEEDED(containerReflection->Load(pContainerBlob));
  VERIFY_SUCCEEDED(containerReflection->GetPartCount(&partCount));

  for (uint32_t i = 0; i < partCount; ++i) {
    uint32_t kind;
    VERIFY_SUCCEEDED(containerReflection->GetPartKind(i, &kind));
    if (kind == (uint32_t)hlsl::DxilFourCC::DFCC_DXIL) {
      blobFound = true;
      CComPtr<IDxcBlob> pPart;
      IFT(containerReflection->GetPartContent(i, &pPart));
      const hlsl::DxilProgramHeader *pProgramHeader =
        reinterpret_cast<const hlsl::DxilProgramHeader*>(pPart->GetBufferPointer());
      VERIFY_IS_TRUE(IsValidDxilProgramHeader(pProgramHeader, (uint32_t)pPart->GetBufferSize()));
      hlsl::DXIL::ShaderKind SK = hlsl::GetVersionShaderType(pProgramHeader->ProgramVersion);
      if (SK == hlsl::DXIL::ShaderKind::Library)
        VERIFY_SUCCEEDED(containerReflection->GetPartReflection(i, IID_PPV_ARGS(&pLibraryReflection)));
      else
        VERIFY_SUCCEEDED(containerReflection->GetPartReflection(i, IID_PPV_ARGS(&pShaderReflection)));
      break;
    }
  }

  if (!blobFound) {
    return FileRunCommandResult::Error("Unable to find DXIL part");
  } else if (pShaderReflection) {
    dumper.Dump(pShaderReflection);
  } else if (pLibraryReflection) {
    dumper.Dump(pLibraryReflection);
  }

  ss.flush();

  return FileRunCommandResult::Success(ss.str());
}

FileRunCommandResult FileRunCommandPart::RunTee(const FileRunCommandResult *Prior) {
  if (Prior == nullptr) {
    return FileRunCommandResult::Error("tee requires a prior command");
  }

  // Ignore commands for now - simply log out through test framework.
  {
    CA2W outWide(Prior->StdOut.c_str(), CP_UTF8);
    WEX::Logging::Log::Comment(outWide.m_psz);
  }
  if (!Prior->StdErr.empty()) {
    CA2W errWide(Prior->StdErr.c_str(), CP_UTF8);
    WEX::Logging::Log::Comment(L"<stderr>");
    WEX::Logging::Log::Comment(errWide.m_psz);
  }

  return *Prior;
}

FileRunCommandResult FileRunCommandPart::RunXFail(const FileRunCommandResult *Prior) {
  if (Prior == nullptr)
    return FileRunCommandResult::Error("XFail requires a prior command");

  if (Prior->ExitCode == 0) {
    return FileRunCommandResult::Error("XFail expected a failure from previous command");
  } else {
    return FileRunCommandResult::Success("");
  }
}

FileRunCommandResult FileRunCommandPart::RunDxilVer(dxc::DxcDllSupport& DllSupport, const FileRunCommandResult* Prior) {
  Arguments = strtrim(Arguments);
  if (Arguments.size() != 3 || !std::isdigit(Arguments[0]) || Arguments[1] != '.' || !std::isdigit(Arguments[2])) {
    return FileRunCommandResult::Error("Invalid dxil version format");
  }

  unsigned RequiredDxilMajor = Arguments[0] - '0';
  unsigned RequiredDxilMinor = Arguments[2] - '0';

  return CheckDxilVer(DllSupport, RequiredDxilMajor, RequiredDxilMinor);
}

class FileRunTestResultImpl : public FileRunTestResult {
  dxc::DxcDllSupport &m_support;

  void RunHashTestFromCommands(LPCSTR commands, LPCWSTR fileName) {
    std::vector<FileRunCommandPart> parts;
    ParseCommandParts(commands, fileName, parts);
    FileRunCommandResult result;
    bool ran = false;
    for (FileRunCommandPart & part : parts) {
      result = part.RunHashTests(m_support);
      ran = true;
      break;
    }
    if (ran) {
      this->RunResult = result.ExitCode;
      this->ErrorMessage = result.StdErr;
    }
    else {
      this->RunResult = 0;
    }
  }

  void RunFileCheckFromCommands(LPCSTR commands, LPCWSTR fileName) {
    std::vector<FileRunCommandPart> parts;
    ParseCommandParts(commands, fileName, parts);

    if (parts.empty()) {
      this->RunResult = 1;
      this->ErrorMessage = "FileCheck found no commands to run";
      return;
    }
    
    FileRunCommandResult result;
    FileRunCommandResult* previousResult = nullptr;
    for (FileRunCommandPart & part : parts) {
      result = part.Run(m_support, previousResult);
      previousResult = &result;
      if (result.AbortPipeline) break;
    }

    this->RunResult = result.ExitCode;
    this->ErrorMessage = result.StdErr;
  }

public:
  FileRunTestResultImpl(dxc::DxcDllSupport &support) : m_support(support) {}
  void RunFileCheckFromFileCommands(LPCWSTR fileName) {
    // Assume UTF-8 files.
    auto cmds = GetRunLines(fileName);
    // Iterate over all RUN lines
    for (auto &cmd : cmds) {
      RunFileCheckFromCommands(cmd.c_str(), fileName);
      // If any of the RUN cmd fails then skip executing remaining cmds
      // and report the error
      if (this->RunResult != 0) break;
    }
  }

  void RunHashTestFromFileCommands(LPCWSTR fileName) {
    // Assume UTF-8 files.
    std::string commands(GetFirstLine(fileName));
    return RunHashTestFromCommands(commands.c_str(), fileName);
  }
};

FileRunTestResult FileRunTestResult::RunHashTestFromFileCommands(LPCWSTR fileName) {
  dxc::DxcDllSupport dllSupport;
  IFT(dllSupport.Initialize());
  FileRunTestResultImpl result(dllSupport);
  result.RunHashTestFromFileCommands(fileName);
  return result;
}

FileRunTestResult FileRunTestResult::RunFromFileCommands(LPCWSTR fileName) {
  dxc::DxcDllSupport dllSupport;
  IFT(dllSupport.Initialize());
  FileRunTestResultImpl result(dllSupport);
  result.RunFileCheckFromFileCommands(fileName);
  return result;
}

FileRunTestResult FileRunTestResult::RunFromFileCommands(LPCWSTR fileName, dxc::DxcDllSupport &dllSupport) {
  FileRunTestResultImpl result(dllSupport);
  result.RunFileCheckFromFileCommands(fileName);
  return result;
}

void ParseCommandParts(LPCSTR commands, LPCWSTR fileName,
                       std::vector<FileRunCommandPart> &parts) {
  // Barely enough parsing here.
  commands = strstr(commands, "RUN: ");
  if (!commands) {
    return;
  }
  commands += strlen("RUN: ");

  LPCSTR endCommands = strchr(commands, '\0');
  while (commands != endCommands) {
    LPCSTR nextStart;
    LPCSTR thisEnd = strchr(commands, '|');
    if (!thisEnd) {
      nextStart = thisEnd = endCommands;
    } else {
      nextStart = thisEnd + 2;
    }
    LPCSTR commandEnd = strchr(commands, ' ');
    if (!commandEnd)
      commandEnd = endCommands;
    parts.emplace_back(std::string(commands, commandEnd),
                       std::string(commandEnd, thisEnd), fileName);
    commands = nextStart;
  }
}

void ParseCommandPartsFromFile(LPCWSTR fileName,
                               std::vector<FileRunCommandPart> &parts) {
  // Assume UTF-8 files.
  std::string commands(GetFirstLine(fileName));
  ParseCommandParts(commands.c_str(), fileName, parts);
}
