//===- unittests/SPIRV/WholeFileCheck.h ---- WholeFileCheck Test Fixture --===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#include <algorithm>
#include <fstream>

#include "dxc/Support/Global.h"
#include "dxc/Support/WinIncludes.h"
#include "dxc/Support/dxcapi.use.h"
#include "spirv-tools/libspirv.hpp"
#include "gtest/gtest.h"

#include "SpirvTestOptions.h"

namespace {
const char hlslStartLabel[] = "// Run:";
const char spirvStartLabel[] = "// CHECK-WHOLE-SPIR-V:";
}

/// \brief The purpose of the this test class is to take in an input file with
/// the following format:
///
///    // Comments...
///    // More comments...
///    // Run: %dxc -T ps_6_0 -E main
///    ...
///    <HLSL code goes here>
///    ...
///    // CHECK-WHOLE-SPIR-V:
///    // ...
///    // <SPIR-V code goes here>
///    // ...
///
/// This file is fully read in as the HLSL source (therefore any non-HLSL must
/// be commented out). It is fed to the DXC compiler with the SPIR-V Generation
/// option. The resulting SPIR-V binary is then fed to the SPIR-V disassembler
/// (via SPIR-V Tools) to get a SPIR-V assembly text. The resulting SPIR-V
/// assembly text is compared to the second part of the input file (after the
/// <CHECK-WHOLE-SPIR-V:> directive). If these match, the test is marked as a
/// PASS, and marked as a FAILED otherwise.
class WholeFileTest : public ::testing::Test {
public:
  WholeFileTest();

  /// \brief Runs a WHOLE-FILE-TEST! (See class description for more info)
  /// Returns true if the test passes; false otherwise.
  /// Since SPIR-V headers may change, a test is more robust if the
  /// disassembler does not include the header.
  /// It is also important that all generated SPIR-V code is valid. Users of
  /// WholeFileTest may choose not to run the SPIR-V Validator (for cases where
  /// a certain feature has not been added to the Validator yet).
  bool runWholeFileTest(std::string path, bool generateHeader = false,
                        bool runSpirvValidation = true);

private:
  /// \brief Reads in the given input file.
  /// Stores the SPIR-V portion of the file into the <expectedSpirvAsm>
  /// member variable. All "//" are also removed from the SPIR-V assembly.
  /// Returns true on success, and false on failure.
  bool parseInputFile();

  /// \brief Passes the HLSL input to the DXC compiler with SPIR-V CodeGen.
  /// Writes the SPIR-V Binary to the output file.
  /// Returns true on success, and false on failure.
  bool runCompilerWithSpirvGeneration();

  /// \brief Passes the SPIR-V Binary to the disassembler.
  bool disassembleSpirvBinary(bool generatedHeader = false);

  /// \brief Runs the SPIR-V tools validation on the SPIR-V Binary.
  /// Returns true if validation is successful; false otherwise.
  bool validateSpirvBinary();

  /// \brief Compares the expected and the generated SPIR-V code.
  /// Returns true if they match, and false otherwise.
  bool compareExpectedSpirvAndGeneratedSpirv();

  /// \brief Parses the Target Profile and Entry Point from the Run command
  bool processRunCommandArgs(const std::string &runCommandLine);

  /// \brief Converts an IDxcBlob that is the output of "%DXC -spirv" into a
  /// vector of 32-bit unsigned integers that can be passed into the
  /// disassembler. Stores the results in <generatedBinary>.
  void convertIDxcBlobToUint32(const CComPtr<IDxcBlob> &blob);

  /// \brief Returns the absolute path to the input file of the test.
  std::string getAbsPathOfInputDataFile(const std::string &filename);

  std::string targetProfile;             ///< Target profile (argument of -T)
  std::string entryPoint;                ///< Entry point name (argument of -E)
  std::string inputFilePath;             ///< Path to the input test file
  std::vector<uint32_t> generatedBinary; ///< The generated SPIR-V Binary
  std::string expectedSpirvAsm;          ///< Expected SPIR-V parsed from input
  std::string generatedSpirvAsm;         ///< Disassembled binary (SPIR-V code)
  spvtools::SpirvTools spirvTools;       ///< SPIR-V Tools used by the test
};
