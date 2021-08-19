/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:02:54
 * @LastEditors: V4kst1z
 * @Description: Flattening 解混淆
 * @FilePath: /ollvm-deobfuscator/include/DFlattening.h
 */

#ifndef OLLVM_DEOBFUSCATOR_INCLUDE_DFLATTENING_H_
#define OLLVM_DEOBFUSCATOR_INCLUDE_DFLATTENING_H_

#include "llvm/IR/PassManager.h"

struct DFlattening : public llvm::PassInfoMixin<DFlattening> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &Func);

private:
  void eliminationCmp(llvm::Function &Func);
};

#endif // OLLVM_DEOBFUSCATOR_INCLUDE_DFLATTENING_H_