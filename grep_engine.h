#ifndef DO_GREP_H
#define DO_GREP_H
/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 *  icgrep is a trademark of International Characters.
 */

#include <string>
#include <stdint.h>
#include <re/re_re.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>


namespace llvm { class raw_ostream; }

class GrepEngine {
    typedef void (*GrepFunctionType)(char * byte_data, size_t filesize, const char* filename, uint64_t finalLineUnterminated);
    typedef void (*GrepFunctionTypeX)(char * byte_data1, char * byte_data2, size_t filesize1, size_t filesize2, const char* filename1, const char* filename2, uint64_t finalLineUnterminated1, uint64_t finalLineUnterminated2);
public:

    GrepEngine() {}

    ~GrepEngine();
  
    void grepCodeGen(std::string moduleName, re::RE * re_ast, bool isNameExpression = false);
    
    void doGrep(const std::string & fileName);

    void doGrep(const std::string & fileName1, const std::string & fileName2);
    
    re::CC *  grepCodepoints();
    
private:
   
    static bool finalLineIsUnterminated(const char * const fileBuffer, const size_t fileSize);

    GrepFunctionType mGrepFunction;
    GrepFunctionTypeX mGrepFunctionX;
    
    bool mIsNameExpression;
    llvm::ExecutionEngine * mEngine;
};


#endif
