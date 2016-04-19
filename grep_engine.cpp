/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <grep_engine.h>
#include <IDISA/idisa_builder.h>
#include <IDISA/idisa_target.h>
#include <re/re_toolchain.h>
#include <pablo/pablo_toolchain.h>
#include <toolchain.h>
#include <utf_encoding.h>
#include <pablo/pablo_compiler.h>
#include <kernels/pipeline.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Module.h>
#include <llvm/ExecutionEngine/MCJIT.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/Debug.h>
#include <llvm/IR/Verifier.h>
#include <UCD/UnicodeNameData.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>
#include <stdint.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdexcept>
#include <cctype>


#include <llvm/Support/raw_os_ostream.h>

// mmap system
#include <boost/filesystem.hpp>
#include <boost/iostreams/device/mapped_file.hpp>
using namespace boost::iostreams;
using namespace boost::filesystem;

#include <fcntl.h>

#include <kernels/kernel.h>



bool GrepEngine::finalLineIsUnterminated(const char * const fileBuffer, const size_t fileSize) {
    if (fileSize == 0) return false;
    unsigned char end_byte = static_cast<unsigned char>(fileBuffer[fileSize-1]);
    // LF through CR are line break characters
    if ((end_byte >= 0xA) && (end_byte <= 0xD)) return false;
    // Other line breaks require at least two bytes.
    if (fileSize == 1) return true;
    // NEL
    unsigned char penult_byte = static_cast<unsigned char>(fileBuffer[fileSize-2]);
    if ((end_byte == 0x85) && (penult_byte == 0xC2)) return false;
    if (fileSize == 2) return true;
    // LS and PS
    if ((end_byte < 0xA8) || (end_byte > 0xA9)) return true;
    return (static_cast<unsigned char>(fileBuffer[fileSize-3]) != 0xE2) || (penult_byte != 0x80);
}

void GrepEngine::doGrep(const std::string & fileName) {
    const path file(fileName);
    if (exists(file)) {
        if (is_directory(file)) {
            return;
        }
    } else {
        std::cerr << "Error: cannot open " << fileName << " for processing. Skipped.\n";
        return;
    }

    const size_t fileSize = file_size(file);
    if (fileSize > 0) {
        mapped_file file;
        try {
            file.open(fileName, mapped_file::priv, fileSize, 0);
        } catch (std::ios_base::failure e) {
            throw std::runtime_error("Boost mmap error: " + fileName + ": " + e.what());
        }
        char * const fileBuffer = file.data();
        mGrepFunction(fileBuffer, fileSize, fileName.c_str(), finalLineIsUnterminated(fileBuffer, fileSize));
        file.close();
    }
}

void GrepEngine::doGrep(const std::string & fileName1, const std::string & fileName2) {


    const path file1(fileName1);
    if (exists(file1)) {
        if (is_directory(file1)) {
            return;
        }
    } else {
        std::cerr << "Error: cannot open " << fileName1 << " for processing. Skipped.\n";
        return;
    }

    const path file2(fileName2);
    if (exists(file2)) {
        if (is_directory(file2)) {
            return;
        }
    } else {
        std::cerr << "Error: cannot open " << fileName2 << " for processing. Skipped.\n";
        return;
    }

    const size_t fileSize1 = file_size(file1);
    const size_t fileSize2 = file_size(file2);
    if (fileSize1 > 0 && fileSize2 >0) {
        mapped_file file1;
        mapped_file file2;

        try {
            file1.open(fileName1, mapped_file::priv, fileSize1, 0);
            file2.open(fileName2, mapped_file::priv, fileSize2, 0);

        } catch (std::ios_base::failure e) {
            throw std::runtime_error("Boost mmap error: " + fileName1 + " or " + fileName2 + ": " + e.what());
        }
        char * const fileBuffer1 = file1.data();
        char * const fileBuffer2 = file2.data();
        mGrepFunctionX(fileBuffer1, fileBuffer2, fileSize1, fileSize2, fileName1.c_str(), fileName2.c_str(), finalLineIsUnterminated(fileBuffer1, fileSize1), finalLineIsUnterminated(fileBuffer1, fileSize2));

        file1.close();
        file2.close();
    }
}


void GrepEngine::grepCodeGen(std::string moduleName, re::RE * re_ast, bool isNameExpression) {
                            
    Module * M = new Module(moduleName, getGlobalContext());
    
    IDISA::IDISA_Builder * idb = GetIDISA_Builder(M);

    kernel::PipelineBuilder pipelineBuilder(M, idb);
    Encoding encoding(Encoding::Type::UTF_8, 8);
    mIsNameExpression = isNameExpression;
    re_ast = regular_expression_passes(encoding, re_ast);   
    pablo::PabloFunction * function = re2pablo_compiler(encoding, re_ast);


    pipelineBuilder.CreateKernels(function, isNameExpression);

    llvm::Function * grepIR = pipelineBuilder.ExecuteKernels();

    mEngine = JIT_to_ExecutionEngine(M);

    icgrep_Linking(M, mEngine);
    //#ifndef NDEBUG
    verifyModule(*M, &dbgs());
    //#endif
    mEngine->finalizeObject();
    delete idb;

    mGrepFunctionX = reinterpret_cast<GrepFunctionTypeX>(mEngine->getPointerToFunction(grepIR));
}

re::CC *  GrepEngine::grepCodepoints() {

    setParsedCodePointSet();
    char * mFileBuffer = getUnicodeNameDataPtr();
    size_t mFileSize = getUnicodeNameDataSize();
    std::string mFileName = "Uname.txt";

    uint64_t finalLineUnterminated = 0;
    if(finalLineIsUnterminated(mFileBuffer, mFileSize))
        finalLineUnterminated = 1;    
    mGrepFunction(mFileBuffer, mFileSize, mFileName.c_str(), finalLineUnterminated);

    return getParsedCodePointSet();
}

GrepEngine::~GrepEngine() {
    delete mEngine;
}
