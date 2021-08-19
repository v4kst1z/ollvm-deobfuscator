/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:00:58
 * @LastEditors: V4kst1z
 * @Description: BogusFlow 解混淆实现
 * @FilePath: /ollvm-deobfuscator/lib/DBogusFlow.cpp
 */

#include "../include/DBogusFlow.h"

#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
using namespace llvm;

llvm::PreservedAnalyses DBogusFlow::run(llvm::Module &M,
                                        llvm::ModuleAnalysisManager &MAM) {
  bool Changed = runOnModule(M);

  // 其实这些优化没必要加，因为在把改动过的 ll
  // 文件编译成二进制文件时，编译期会自动进行一些必要的优化
  auto MPM = std::make_unique<ModulePassManager>();

  MPM->addPass(createModuleToFunctionPassAdaptor(ADCEPass()));
  MPM->addPass(createModuleToFunctionPassAdaptor(InstCombinePass()));
  MPM->addPass(createModuleToFunctionPassAdaptor(SimplifyCFGPass()));

  MPM->run(M, MAM);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

bool DBogusFlow::runOnModule(llvm::Module &M) {
  bool Changed = false;

  for (auto &F : M) {
    if (F.isDeclaration())
      continue;
    Changed |= runOnFunction(F);
  }

  return Changed;
}

bool DBogusFlow::runOnFunction(llvm::Function &Func) {
  auto &CTX = Func.getContext();
  bool Changed = false;

  for (auto &BB : Func) {

    SmallVector<Value *, 32> bogusCondVec;
    for (BasicBlock::iterator instIter = BB.begin(), end = BB.end();
         instIter != end; ++instIter) {
      Instruction *inst = &(*instIter);
      // 匹配 x * (x - 1) % 2 == 0
      //%142 = add i32 %140, -1, !insn.addr !71
      //%143 = mul i32 %142, %140, !insn.addr !72
      //%144 = and i32 %143, 1
      //%145 = icmp eq i32 %144, 0, !insn.addr !73

      // %20 = add i32 %18, -1, !insn.addr !16
      if (inst->getOpcode() != Instruction::Add ||
          inst->getOperand(1) !=
              ConstantInt::getSigned(Type::getInt32Ty(CTX), -1))
        continue;

      // %21 = mul i32 %20, %18, !insn.addr !17
      inst = &(*(++instIter));
      if (inst == &(*end) || inst->getOpcode() != Instruction::Mul)
        continue;

      // 下面两种都可以求余
      // %22 = urem i32 %21, 2, !insn.addr !18
      //  %13 = and i32 %12, 1
      inst = &(*(++instIter));
      if (inst == &(*end))
        continue;

      if (inst->getOpcode() == Instruction::URem &&
          (inst->getOperand(1) ==
           ConstantInt::getSigned(Type::getInt32Ty(CTX), 2))) {
      } else if (inst->getOpcode() == Instruction::And &&
                 (inst->getOperand(1) ==
                  ConstantInt::getSigned(Type::getInt32Ty(CTX), 1))) {
      } else {
        continue;
      }

      // %23 = icmp eq i32 %22, 0, !insn.addr !19
      inst = &(*(++instIter));
      if (inst == &(*end) || inst->getOpcode() != Instruction::ICmp ||
          inst->getOperand(1) !=
              ConstantInt::getSigned(Type::getInt32Ty(CTX), 0))
        continue;

      inst->replaceAllUsesWith(
          ConstantInt::get(Type::getInt1Ty(Func.getContext()), APInt(1, 1)));

      auto *firstExpr = dyn_cast<Value>(inst);
      // y < 10 这部分 retdec 反汇编后如下，为了简单只匹配部分指令
      // %24 = add i32 %19, -10, !insn.addr !20
      // %25 = sub i32 9, %19
      // %26 = and i32 %25, %19, !insn.addr !20
      // %27 = icmp slt i32 %26, 0, !insn.addr !20
      // %28 = icmp slt i32 %24, 0, !insn.addr !20
      // %29 = icmp ne i1 %28, %27, !insn.addr !21

      //%146 = add i32 %141, -10, !insn.addr !74
      //%147 = sub i32 9, %141
      //%148 = and i32 %147, %141, !insn.addr !74
      //%149 = xor i32 %146, %148
      //%150 = icmp slt i32 %149, 0

      inst = &(*(++instIter));
      if (inst == &(*end) || inst->getOpcode() != Instruction::Add ||
          inst->getOperand(1) !=
              ConstantInt::getSigned(Type::getInt32Ty(CTX), -10))
        continue;

      inst = &(*(++instIter));
      if (inst == &(*end) || inst->getOpcode() != Instruction::Sub ||
          inst->getOperand(0) !=
              ConstantInt::getSigned(Type::getInt32Ty(CTX), 9))
        continue;

      for (auto userIter = firstExpr->user_begin();
           userIter != firstExpr->user_end(); userIter++) {
        auto *userInst = dyn_cast<Instruction>(*userIter);
        if (userInst == nullptr) {
          errs() << "error in find firstExpr user\n";
        }

        if (userInst->getOpcode() == Instruction::Or)
          bogusCondVec.push_back(userInst);
      }

      Changed = true;
      break;
    }

    if (!bogusCondVec.empty()) {
      SmallVector<BasicBlock *, 32> remBB;
      for (auto &bogusCond : bogusCondVec) {
        for (auto userIter = bogusCond->user_begin();
             userIter != bogusCond->user_end(); userIter++) {

          auto *brInst = dyn_cast<BranchInst>(*userIter);
          if (brInst == nullptr) {
            errs() << "error in find bogusCond\n";
            continue;
          }

          IRBuilder<> bInstBuilder(brInst);
          bInstBuilder.CreateBr(dyn_cast<BasicBlock>(brInst->getOperand(2)));
          remBB.push_back(brInst->getParent());
        }
      }
      for (auto &BB : remBB) {
        auto *brInst = BB->getTerminator();
        brInst->eraseFromParent();
      }
    }
  }
  return Changed;
}

llvm::PassPluginLibraryInfo getDBogusFlowPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "dbcf", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "dbcf") {
                    MPM.addPass(DBogusFlow());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDBogusFlowPluginInfo();
}