/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:01:29
 * @LastEditors: V4kst1z
 * @Description: BogusFlow 解混淆
 * @FilePath: /ollvm-deobfuscator/include/DBogusFlow.h
 */

#ifndef OLLVM_DEOBFUSCATOR_INCLUDE_DBOGUSFLOW_H_
#define OLLVM_DEOBFUSCATOR_INCLUDE_DBOGUSFLOW_H_

#include "llvm/IR/PassManager.h"

struct DBogusFlow : public llvm::PassInfoMixin<DBogusFlow> {
  llvm::PreservedAnalyses run(llvm::Module &M, llvm::ModuleAnalysisManager &);

  bool runOnModule(llvm::Module &M);
  bool runOnFunction(llvm::Function &Func);
};

#endif // OLLVM_DEOBFUSCATOR_INCLUDE_DBOGUSFLOW_H_
