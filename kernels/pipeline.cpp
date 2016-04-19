/*
 *  Copyright (c) 2016 International Characters.
 *  This software is licensed to the public under the Open Software License 3.0.
 */

#include "pipeline.h"
#include "utf_encoding.h"

#include <kernels/scanmatchgen.h>
#include <kernels/s2p_kernel.h>
#include <kernels/instance.h>

#include <pablo/function.h>
#include <pablo/pablo_compiler.h>
#include <pablo/pablo_toolchain.h>

#include <llvm/Support/CommandLine.h>

static cl::opt<unsigned> SegmentSize("segment-size", cl::desc("Segment Size"), cl::value_desc("positive integer"), cl::init(1));

using namespace pablo;
using namespace kernel;

PipelineBuilder::PipelineBuilder(Module * m, IDISA::IDISA_Builder * b)
: mMod(m)
, iBuilder(b)
, mBitBlockType(b->getBitBlockType())
, mBlockSize(b->getBitBlockWidth()) {

}

PipelineBuilder::~PipelineBuilder() {
    delete mS2PKernel;
    delete mICgrepKernel;
    delete mScanMatchKernel;
}

void PipelineBuilder::CreateKernels(PabloFunction * function, bool isNameExpression){
    mS2PKernel = new KernelBuilder(iBuilder, "s2p", SegmentSize);
    mICgrepKernel = new KernelBuilder(iBuilder,"icgrep", SegmentSize * 2);
    mScanMatchKernel = new KernelBuilder(iBuilder, "scanMatch", SegmentSize);
    generateS2PKernel(mMod, iBuilder, mS2PKernel);
    generateScanMatch(mMod, iBuilder, 64, mScanMatchKernel, isNameExpression);
    pablo_function_passes(function);
    PabloCompiler pablo_compiler(mMod, iBuilder);
    try {
        pablo_compiler.setKernel(mICgrepKernel);
        pablo_compiler.compile(function);
        delete function;
        releaseSlabAllocatorMemory();
    } catch (std::runtime_error e) {
        delete function;
        releaseSlabAllocatorMemory();
        std::cerr << "Runtime error: " << e.what() << std::endl;
        exit(1);
    }
}

Function * PipelineBuilder::ExecuteKernels() {
    Type * const int64ty = iBuilder->getInt64Ty();
    Type * const int8PtrTy = iBuilder->getInt8PtrTy();
    Type * const inputType = PointerType::get(ArrayType::get(StructType::get(mMod->getContext(), std::vector<Type *>({ArrayType::get(mBitBlockType, 8)})), 1), 0);

    Function * const main = cast<Function>(mMod->getOrInsertFunction("Main", Type::getVoidTy(mMod->getContext()), inputType, inputType, int64ty, int64ty, int8PtrTy, int8PtrTy, int64ty, int64ty, nullptr));
    main->setCallingConv(CallingConv::C);
    Function::arg_iterator args = main->arg_begin();

    Value * const inputStream1 = args++;
    inputStream1->setName("input1");
    Value * const inputStream2 = args++;
    inputStream2->setName("input2");
    Value * bufferSize1 = args++;
    bufferSize1->setName("bufferSize1");
    Value * bufferSize2 = args++;
    bufferSize2->setName("bufferSize2");
    Value * fileName1 = args++;
    fileName1->setName("fileName1");
    Value * fileName2 = args++;
    fileName2->setName("filename2");
    Value * finalLineUnterminated1 = args++;
    finalLineUnterminated1->setName("finalLineUnterminated1");
    Value * finalLineUnterminated2 = args++;
    finalLineUnterminated2->setName("finalLineUnterminated2");

    iBuilder->SetInsertPoint(BasicBlock::Create(mMod->getContext(), "entry", main,0));


    BasicBlock * entryBlock = iBuilder->GetInsertBlock();
    BasicBlock * segmentCondBlock = nullptr;
    BasicBlock * segmentBodyBlock = nullptr;
    const unsigned segmentSize = SegmentSize;

    if (segmentSize > 1) {
        segmentCondBlock = BasicBlock::Create(mMod->getContext(), "segmentCond", main, 0);
        segmentBodyBlock = BasicBlock::Create(mMod->getContext(), "segmentBody", main, 0);
    }
    BasicBlock * fullCondBlock = BasicBlock::Create(mMod->getContext(), "fullCond", main, 0);
    BasicBlock * fullBodyBlock = BasicBlock::Create(mMod->getContext(), "fullBody", main, 0);
    BasicBlock * finalBlock = BasicBlock::Create(mMod->getContext(), "final", main, 0);
    BasicBlock * finalPartialBlock = BasicBlock::Create(mMod->getContext(), "partial", main, 0);
    BasicBlock * finalEmptyBlock = BasicBlock::Create(mMod->getContext(), "empty", main, 0);
    BasicBlock * endBlock = BasicBlock::Create(mMod->getContext(), "end", main, 0);
    BasicBlock * unterminatedBlock = BasicBlock::Create(mMod->getContext(), "unterminated", main, 0);
    BasicBlock * exitBlock = BasicBlock::Create(mMod->getContext(), "exit", main, 0);

    Value * inputMerge1 = iBuilder->CreateAlloca(ArrayType::get(mBitBlockType, 8));
    Value * inputMerge2 = iBuilder->CreateAlloca(ArrayType::get(mBitBlockType, 8));
    Instance * s2pInstance1 = mS2PKernel->instantiate(std::make_pair(inputMerge1, 1));
    Instance * s2pInstance2 = mS2PKernel->instantiate(std::make_pair(inputMerge2, 1));

    Value * outMerge = iBuilder->CreateAlloca(ArrayType::get(mBitBlockType, 8), iBuilder->getInt32(2));
    Instance * icGrepInstance = mICgrepKernel->instantiate(std::make_pair(outMerge, 2));

    Value * mem1 = iBuilder->CreateAlloca(ArrayType::get(mBitBlockType, 2));
    Value * mem2 = iBuilder->CreateAlloca(ArrayType::get(mBitBlockType, 2));
    Instance * scanMatchInstance1 = mScanMatchKernel->instantiate(std::make_pair(mem1, 1));
    Instance * scanMatchInstance2 = mScanMatchKernel->instantiate(std::make_pair(mem2, 1));


    scanMatchInstance1->setInternalState("FileBuf", iBuilder->CreateBitCast(inputStream1, int8PtrTy));
    scanMatchInstance1->setInternalState("FileSize", bufferSize1);
    scanMatchInstance1->setInternalState("FileName", fileName1);

    scanMatchInstance2->setInternalState("FileBuf", iBuilder->CreateBitCast(inputStream2, int8PtrTy));
    scanMatchInstance2->setInternalState("FileSize", bufferSize2);
    scanMatchInstance2->setInternalState("FileName", fileName2);

    Value * initialBufferSize = nullptr;
    BasicBlock * initialBlock = nullptr;

    if (segmentSize > 1) {
        iBuilder->CreateBr(segmentCondBlock);
        iBuilder->SetInsertPoint(segmentCondBlock);
        PHINode * remainingBytes = iBuilder->CreatePHI(int64ty, 2, "remainingBytes");
        remainingBytes->addIncoming(bufferSize1, entryBlock);
        Constant * const step = ConstantInt::get(int64ty, mBlockSize * segmentSize);
        Value * segmentCondTest = iBuilder->CreateICmpULT(remainingBytes, step);
        iBuilder->CreateCondBr(segmentCondTest, fullCondBlock, segmentBodyBlock);
        iBuilder->SetInsertPoint(segmentBodyBlock);

            for (unsigned i=0; i<8; i++) {
                Value * ptr1 = iBuilder->CreateGEP(inputStream1, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
                Value * s1 = iBuilder->CreateBlockAlignedLoad(ptr1);
                Value * ptr2 = iBuilder->CreateGEP(inputStream2, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
                Value * s2 = iBuilder->CreateBlockAlignedLoad(ptr2);
                if (i <4){
                    Value * merged1 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
                    Value * merged2 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i+4)});
                    iBuilder->CreateBlockAlignedStore(s1, merged1);
                    iBuilder->CreateBlockAlignedStore(s2, merged2);
                }else{
                    Value * merged1 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i-4)});
                    Value * merged2 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
                    iBuilder->CreateBlockAlignedStore(s1, merged1);
                    iBuilder->CreateBlockAlignedStore(s2, merged2);
                }
            }

        Value * outputStream0[segmentSize];
        Value * outputStream1[segmentSize];

        for (unsigned i = 0; i < segmentSize; ++i) {
            Value * pntr1 = s2pInstance1->getOutputStreamSet();
            Value * pntr2 = s2pInstance2->getOutputStreamSet();
            s2pInstance1->CreateDoBlockCall();
            s2pInstance2->CreateDoBlockCall();
            iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr1), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(0), "storeMerged1"));
            iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr2), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(1), "storeMerged2"));
            outputStream0[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
            outputStream0[1] = icGrepInstance->getOutputStream(1);
            icGrepInstance->CreateDoBlockCall();
            outputStream1[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
            outputStream1[1] = icGrepInstance->getOutputStream(1);
            icGrepInstance->CreateDoBlockCall();
        }

        for (unsigned i = 0; i < segmentSize; ++i) {
            for (unsigned j = 0; j < 2; ++j) {
                Value * o1 = iBuilder->CreateBlockAlignedLoad(outputStream0[j]);
                Value * o2 = iBuilder->CreateBlockAlignedLoad(outputStream1[j]);
                Value * merged1 = iBuilder->esimd_mergel(64, o1, o2);
                Value * merged2 = iBuilder->esimd_mergeh(64, o1, o2);
                iBuilder->CreateBlockAlignedStore(merged1, iBuilder->CreateGEP(mem1, {iBuilder->getInt32(0), iBuilder->getInt32(j)}));
                iBuilder->CreateBlockAlignedStore(merged2, iBuilder->CreateGEP(mem2, {iBuilder->getInt32(0), iBuilder->getInt32(j)}));
            }
            scanMatchInstance1->CreateDoBlockCall();
            scanMatchInstance2->CreateDoBlockCall();
        }
        remainingBytes->addIncoming(iBuilder->CreateSub(remainingBytes, step), segmentBodyBlock);
        iBuilder->CreateBr(segmentCondBlock);
        initialBufferSize = remainingBytes;
        initialBlock = segmentCondBlock;
    } else {
        initialBufferSize = bufferSize1;
        initialBlock = entryBlock;
        iBuilder->CreateBr(fullCondBlock);
    }

    iBuilder->SetInsertPoint(fullCondBlock);
    PHINode * remainingBytes = iBuilder->CreatePHI(int64ty, 2, "remainingBytes");
    remainingBytes->addIncoming(initialBufferSize, initialBlock);

    Constant * const step = ConstantInt::get(int64ty, mBlockSize);
    Value * fullCondTest = iBuilder->CreateICmpULT(remainingBytes, step);
    iBuilder->CreateCondBr(fullCondTest, finalBlock, fullBodyBlock);
    Value * outputStream0[2];
    Value * outputStream1[2];

    iBuilder->SetInsertPoint(fullBodyBlock);
    for (unsigned i=0; i<8; i++) {
        Value * ptr1 = iBuilder->CreateGEP(inputStream1, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
        Value * s1 = iBuilder->CreateBlockAlignedLoad(ptr1);
        Value * ptr2 = iBuilder->CreateGEP(inputStream2, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
        Value * s2 = iBuilder->CreateBlockAlignedLoad(ptr2);
        if (i <4){
            Value * merged1 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
            Value * merged2 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i+4)});
            iBuilder->CreateBlockAlignedStore(s1, merged1);
            iBuilder->CreateBlockAlignedStore(s2, merged2);
        }else{
            Value * merged1 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i-4)});
            Value * merged2 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
            iBuilder->CreateBlockAlignedStore(s1, merged1);
            iBuilder->CreateBlockAlignedStore(s2, merged2);
        }
    }
    Value * pntr1 = s2pInstance1->getOutputStreamSet();
    Value * pntr2 = s2pInstance2->getOutputStreamSet();
    s2pInstance1->CreateDoBlockCall();
    s2pInstance2->CreateDoBlockCall();
    iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr1), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(0), "storeMerged1"));
    iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr2), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(1), "storeMerged1"));

    outputStream0[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
    outputStream0[1] = icGrepInstance->getOutputStream(1);
    icGrepInstance->CreateDoBlockCall();

    outputStream1[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
    outputStream1[1] = icGrepInstance->getOutputStream(1);
    icGrepInstance->CreateDoBlockCall();

for (unsigned j = 0; j < 2; ++j) {

        Value * o1 = iBuilder->CreateBlockAlignedLoad(outputStream0[j]);
        Value * o2 = iBuilder->CreateBlockAlignedLoad(outputStream1[j]);
        Value * merged1 = iBuilder->esimd_mergel(64, o1, o2);
        Value * merged2 = iBuilder->esimd_mergeh(64, o1, o2);

        iBuilder->CreateBlockAlignedStore(merged1, iBuilder->CreateGEP(mem1, {iBuilder->getInt32(0), iBuilder->getInt32(j)}, "storeMerged1"));
        iBuilder->CreateBlockAlignedStore(merged2, iBuilder->CreateGEP(mem2, {iBuilder->getInt32(0), iBuilder->getInt32(j)}, "storeMerged2"));
    }

    scanMatchInstance1->CreateDoBlockCall();
    scanMatchInstance2->CreateDoBlockCall();


    remainingBytes->addIncoming(iBuilder->CreateSub(remainingBytes, step), fullBodyBlock);
    iBuilder->CreateBr(fullCondBlock);

    iBuilder->SetInsertPoint(finalBlock);
    for (unsigned i=0; i<8; i++) {
        Value * ptr1 = iBuilder->CreateGEP(inputStream1, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
        Value * s1 = iBuilder->CreateBlockAlignedLoad(ptr1);
        Value * ptr2 = iBuilder->CreateGEP(inputStream2, {iBuilder->getInt32(0), iBuilder->CreateLoad(s2pInstance1->getBlockNo()), iBuilder->getInt32(0), iBuilder->getInt32(i)});
        Value * s2 = iBuilder->CreateBlockAlignedLoad(ptr2);
        if (i <4){
            Value * merged1 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
            Value * merged2 = iBuilder->CreateGEP(inputMerge1, {iBuilder->getInt32(0), iBuilder->getInt32(i+4)});
            iBuilder->CreateBlockAlignedStore(s1, merged1);
            iBuilder->CreateBlockAlignedStore(s2, merged2);
        }else{
            Value * merged1 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i-4)});
            Value * merged2 = iBuilder->CreateGEP(inputMerge2, {iBuilder->getInt32(0), iBuilder->getInt32(i)});
            iBuilder->CreateBlockAlignedStore(s1, merged1);
            iBuilder->CreateBlockAlignedStore(s2, merged2);
        }
    }

    Value * const b41 = s2pInstance1->getOutputStream(4);
    Value * const b61 = s2pInstance1->getOutputStream(6);
    Value * const b42 = s2pInstance2->getOutputStream(4);
    Value * const b62 = s2pInstance2->getOutputStream(6);
    Value * emptyBlockCond = iBuilder->CreateICmpEQ(remainingBytes, ConstantInt::get(int64ty, 0));
    iBuilder->CreateCondBr(emptyBlockCond, finalEmptyBlock, finalPartialBlock);


    iBuilder->SetInsertPoint(finalPartialBlock);
    pntr1 = s2pInstance1->getOutputStreamSet();
    pntr2 = s2pInstance2->getOutputStreamSet();
    s2pInstance1->CreateDoBlockCall();
    s2pInstance2->CreateDoBlockCall();
    iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr1), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(0), "storeMerged1"));
    iBuilder->CreateBlockAlignedStore(iBuilder->CreateBlockAlignedLoad(pntr2), iBuilder->CreateGEP(outMerge, iBuilder->getInt32(1), "storeMerged1"));

    iBuilder->CreateBr(endBlock);

    iBuilder->SetInsertPoint(finalEmptyBlock);
    s2pInstance1->clearOutputStreamSet();
    s2pInstance2->clearOutputStreamSet();
    iBuilder->CreateBr(endBlock);

    iBuilder->SetInsertPoint(endBlock);
    Value * isFinalLineUnterminated1 = iBuilder->CreateICmpEQ(finalLineUnterminated1, ConstantInt::get(int64ty, 0));
    iBuilder->CreateCondBr(isFinalLineUnterminated1, exitBlock, unterminatedBlock);

    iBuilder->SetInsertPoint(unterminatedBlock);

    Value * remaining = iBuilder->CreateZExt(remainingBytes, iBuilder->getIntNTy(mBlockSize));
    Value * EOF_pos = iBuilder->CreateShl(ConstantInt::get(iBuilder->getIntNTy(mBlockSize), 1), remaining);
    EOF_pos = iBuilder->CreateBitCast(EOF_pos, mBitBlockType);


    Value * b4val1 = iBuilder->CreateBlockAlignedLoad(b41);
    b4val1 = iBuilder->CreateOr(b4val1, EOF_pos);
    iBuilder->CreateBlockAlignedStore(b4val1, b41);

    Value * b6val1 = iBuilder->CreateBlockAlignedLoad(b61);
    b6val1 = iBuilder->CreateOr(b6val1, EOF_pos);
    iBuilder->CreateBlockAlignedStore(b6val1, b61);

    Value * b4val2 = iBuilder->CreateBlockAlignedLoad(b42);
    b4val2 = iBuilder->CreateOr(b4val2, EOF_pos);
    iBuilder->CreateBlockAlignedStore(b4val2, b42);

    Value * b6val2 = iBuilder->CreateBlockAlignedLoad(b62);
    b6val2 = iBuilder->CreateOr(b6val2, EOF_pos);

    iBuilder->CreateBlockAlignedStore(b6val2, b62);

    iBuilder->CreateBr(exitBlock);

    iBuilder->SetInsertPoint(exitBlock);
    outputStream0[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
    outputStream0[1] = icGrepInstance->getOutputStream(1);
    icGrepInstance->CreateDoBlockCall();
    outputStream1[0] = icGrepInstance->getOutputStream(static_cast<unsigned>(0));
    outputStream1[1] = icGrepInstance->getOutputStream(1);
    icGrepInstance->CreateDoBlockCall();

    for (unsigned j = 0; j < 2; ++j) {
        Value * o1 = iBuilder->CreateBlockAlignedLoad(outputStream0[j]);
        Value * o2 = iBuilder->CreateBlockAlignedLoad(outputStream1[j]);
        Value * merged1 = iBuilder->esimd_mergel(64, o1, o2);
        Value * merged2 = iBuilder->esimd_mergeh(64, o1, o2);

        iBuilder->CreateBlockAlignedStore(merged1, iBuilder->CreateGEP(mem1, {iBuilder->getInt32(0), iBuilder->getInt32(j)}));
        iBuilder->CreateBlockAlignedStore(merged2, iBuilder->CreateGEP(mem2, {iBuilder->getInt32(0), iBuilder->getInt32(j)}));
    }
    scanMatchInstance1->CreateDoBlockCall();
    scanMatchInstance2->CreateDoBlockCall();

    iBuilder->CreateRetVoid();

    return main;
}
