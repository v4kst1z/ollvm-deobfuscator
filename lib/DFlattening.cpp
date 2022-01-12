/*
 * @Author: V4kst1z (dcydane@gmail.com)
 * @Date: 2021-08-19 18:03:25
 * @LastEditors: V4kst1z
 * @Description: Flattening 解混淆实现
 * @FilePath: /ollvm-deobfuscator/lib/DFlattening.cpp
 */

#include "../include/DFlattening.h"

#include <unordered_set>

#include "llvm/ADT/MapVector.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/IPO/SCCP.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar/ADCE.h"
#include "llvm/Transforms/Scalar/SCCP.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Mem2Reg.h"
#include "llvm/Transforms/Utils/ValueMapper.h"

using namespace llvm;

llvm::PreservedAnalyses DFlattening::run(llvm::Module &M,
                                         llvm::ModuleAnalysisManager &MAM) {
  auto MPM = std::make_unique<ModulePassManager>();
  MPM->addPass(createModuleToFunctionPassAdaptor(SimplifyCFGPass()));
  MPM->run(M, MAM);

  bool Changed = runOnModule(M);

  MPM->addPass(createModuleToFunctionPassAdaptor(SCCPPass()));
  MPM->addPass(createModuleToFunctionPassAdaptor(PromotePass()));
  MPM->addPass(IPSCCPPass());
  MPM->addPass(createModuleToFunctionPassAdaptor(ADCEPass()));
  MPM->addPass(createModuleToFunctionPassAdaptor(SimplifyCFGPass()));

  MPM->run(M, MAM);

  return (Changed ? llvm::PreservedAnalyses::none()
                  : llvm::PreservedAnalyses::all());
}

bool DFlattening::runOnModule(llvm::Module &M) {
  bool Changed = false;

  for (auto &F : M) {
    if (F.isDeclaration()) continue;
    Changed |= runOnFunction(F);
    // Changed |= EliminateUnreachableBlocks(F);
  }

  return Changed;
}

bool DFlattening::runOnFunction(llvm::Function &Func) {
  // 第一个基本快
  auto &entryBlock = Func.getEntryBlock();
  auto *brInst = entryBlock.getTerminator();

  // 第一个基本块的最后一条指令为跳转指令
  if (brInst->getOpcode() != Instruction::Br) return false;

  // 第二个基本块
  auto *switchBlock = dyn_cast<BasicBlock>(brInst->getOperand(0));
  if (switchBlock == nullptr) return false;

  // 第二个基本快的最后一条指令必须为 switch
  auto *switchInst = dyn_cast<SwitchInst>(switchBlock->getTerminator());

  if (switchInst == nullptr) return false;

  std::unordered_set<Value *> bbTerValSet;
  for (auto caseIter = switchInst->case_begin();
       caseIter != switchInst->case_end(); caseIter++) {
    bbTerValSet.insert(
        caseIter->getCaseSuccessor()->getTerminator()->getOperand(0));
  }
  if (bbTerValSet.size() != 1 && bbTerValSet.size() != 2) return false;

  //现在基本上可以判断这个函数有控制流平坦化混淆

  //找到所有的 switch 基本块，默认情况下只有一个主 switch
  //分支，其他的基本块都属于对应的上一个基本块的 default
  //分支，在分支较多时会有一部分当道 default 分支继续 switch 处理
  SmallVector<BasicBlock *, 32> switchBBs;
  std::set<BasicBlock *> switchBBSet;
  errs() << "switch 基本块 :\n";
  for (auto &BB : Func) {
    auto *switchInst = dyn_cast<SwitchInst>(BB.getTerminator());
    if (switchInst == nullptr) continue;
    switchBBs.push_back(&BB);
    switchBBSet.insert(&BB);
    errs() << BB.getName().str().c_str() << "\n";
  }
  errs() << "\n";

  //返回基本块数组
  SmallVector<BasicBlock *, 8> retBBs;

  // loop end 基本块应该是默认只有一个
  BasicBlock *loopEndBB = nullptr;

  // switch 分支值及对应的 BB
  MapVector<int64_t, BasicBlock *> numToBB;

  // 计算所有返回基本块、loopEndBB、numToBB
  for (auto &BB : switchBBs) {
    auto *bbSwitchInst = dyn_cast<SwitchInst>(BB->getTerminator());
    for (auto caseIter = bbSwitchInst->case_begin();
         caseIter != bbSwitchInst->case_end(); caseIter++) {
      numToBB.insert({caseIter->getCaseValue()->getSExtValue(),
                      caseIter->getCaseSuccessor()});

      auto *lastInst = caseIter->getCaseSuccessor()->getTerminator();
      if (loopEndBB == nullptr) {
        if (lastInst->getOpcode() == Instruction::Br)
          loopEndBB = dyn_cast<BasicBlock>(lastInst->getOperand(0));
        continue;
      }
      if (lastInst->getOpcode() == Instruction::Ret) retBBs.push_back(BB);
    }
  }

  // 找到影响 switch var 变量的内存
  std::set<Value *> switchVarAddr;

  //在第一个 switch 一般会从一个内存获取 switchvar
  //的值，类似于下面的指令，所以先获取 %stack_var_-932.0.reg2mem
  //指向的地址，之后寻找会对该地址有影响的指令
  // %stack_var_-932.0.reload = load i32, i32* %stack_var_-932.0.reg2mem
  //......
  // switch i32 %stack_var_-932.0.reload, label %dec_label_pc_4016d2 []
  Value *switchVar =
      dyn_cast<Instruction>(switchInst->getOperand(0))->getOperand(0);

  switchVarAddr.insert(switchVar);

  // 在 switchVar 的 user 中找到会影响 %stack_var_-932.0.reg2mem
  // 执行地址内容的指令，并且其第一个操作数必须为寄存器值，不能为常量（第一个基本块会
  // store 常量，对分析无影响）
  // store i32 %stack_var_-932.1.reload, i32* %stack_var_-932.0.reg2mem
  for (auto userIter : switchVar->users()) {
    if (auto *Inst = dyn_cast<Instruction>(userIter)) {
      if (Inst->getOpcode() == Instruction::Store &&
          nullptr == dyn_cast<ConstantInt>(Inst->getOperand(0))) {
        auto *defInst = dyn_cast<Instruction>(Inst->getOperand(0));
        if (defInst->getOpcode() == Instruction::Load) {
          // TODO
          switchVarAddr.insert(defInst->getOperand(0));
        }
      }
    }
  }

  // 在第一个基本块寻找影响 switchVar 的 store 指令
  auto *prevInst = brInst;
  int64_t firstBlockNum = 0;
  while (true) {
    prevInst = prevInst->getPrevNonDebugInstruction();
    if (prevInst->getOpcode() == Instruction::Store &&
        switchVarAddr.count(prevInst->getOperand(1))) {
      firstBlockNum =
          dyn_cast<ConstantInt>(prevInst->getOperand(0))->getSExtValue();
      // 删除 store 指令
      // prevInst->eraseFromParent();
      break;
    }
  }
  if (!firstBlockNum) {
    errs() << "error in find firstBlockNum\n";
    return false;
  }

  // 修改第一个基本块的 br 参数
  brInst->setOperand(0, numToBB[firstBlockNum]);

  // 迭代修改 switch 分支指向基本块的跳转指令
  for (size_t idx = 0; idx < switchBBs.size(); idx++) {
    BasicBlock *iterBB = switchBBs[idx];
    auto *bbSwitchInst = dyn_cast<SwitchInst>(iterBB->getTerminator());
    // 遍历 switch 指令的每一个分支
    for (auto caseIter = bbSwitchInst->case_begin();
         caseIter != bbSwitchInst->case_end(); caseIter++) {
      // 分支指向的基本块
      auto *sBB = caseIter->getCaseSuccessor();
      auto *brSInst = sBB->getTerminator();
      auto *prevInst = brSInst;

      // 如果基本块最后一条指令不是 Br， 则跳过
      if (brSInst->getOpcode() != Instruction::Br) continue;

      // loopEndBB 也含有符合指令，但是 loopEndBB 不需要修改
      if (sBB == loopEndBB) continue;

      while (true) {
        // 从后往前迭代寻找基本块里符合条件的决定后继基本块的 store 指令
        prevInst = prevInst->getPrevNonDebugInstruction();
        if (prevInst->getOpcode() == Instruction::Store &&
            switchVarAddr.count(prevInst->getOperand(1))) {
          // store 指令第一个参数为寄存器
          if (nullptr == dyn_cast<ConstantInt>(prevInst->getOperand(0))) {
            // %33 = select i1 %32, i32 -2054979789, i32 1857292562,
            // !insn.addr store i32 %33, i32* %stack_var_-932.1.reg2mem,
            // !insn.addr !19
            auto *selectInst = dyn_cast<Instruction>(prevInst->getOperand(0));
            if (selectInst == nullptr) {
              errs() << "error in find select instruction\n";
              continue;
            }

            std::string DummyStr;
            raw_string_ostream InstrStr(DummyStr);
            selectInst->print(InstrStr);
            errs() << selectInst->getParent()->getName().str().c_str();
            errs() << InstrStr.str().c_str() << "\n";

            // select 第一个参数为 true 时的后继基本块
            auto *trueBlock =
                numToBB[dyn_cast<ConstantInt>(selectInst->getOperand(1))
                            ->getSExtValue()];

            // select 第一个参数为 false 时的后继基本块
            auto *falseBlock =
                numToBB[dyn_cast<ConstantInt>(selectInst->getOperand(2))
                            ->getSExtValue()];

            IRBuilder<> bInstBuilder(brSInst);
            bInstBuilder.CreateCondBr(selectInst->getOperand(0), trueBlock,
                                      falseBlock);

            selectInst->replaceAllUsesWith(ConstantInt::get(
                cast<Type>(Type::getInt32Ty(Func.getContext())), APInt(32, 0)));

            // 删除 select 指令
            selectInst->eraseFromParent();

            // 删除 store 指令
            prevInst->eraseFromParent();

            // 删除跳转指令
            brSInst->eraseFromParent();
          } else /* store 指令第一个参数为常数 */ {
            brSInst->setOperand(
                0, numToBB[dyn_cast<ConstantInt>(prevInst->getOperand(0))
                               ->getSExtValue()]);
          }
          break;
        }

        if (prevInst == sBB->getFirstNonPHIOrDbgOrLifetime()) {
          errs() << "没有找到合适的决定后继基本块的 store 指令\n";
          break;
        }
      }
    }
  }

  errs() << "switch var user inst delete " << switchVarAddr.size() << "\n";
  for (auto sVar : switchVarAddr) {
    for (auto userIter : sVar->users()) {
      if (auto *Inst = dyn_cast<Instruction>(userIter)) {
        if (Inst->getOpcode() == Instruction::Store)
          Inst->eraseFromParent();
        else if (Inst->getOpcode() == Instruction::Load)
          Inst->replaceAllUsesWith(ConstantInt::get(
              cast<Type>(Type::getInt32Ty(Func.getContext())), APInt(32, 0)));
      }
    }
  }
  errs() << "copy start\n";

  // 为所有 switch 分支指向的基本块加上前驱指令
  // 如下，在经过处理后 switch 基本块除去 switch 指令外都被拷贝到了
  // dec_label_pc_4012a6 基本块前面，但是当 switch 的 default 分支不等于 loopend
  // 时（此时为分支较多，可能是 retdec 把多余的分支都放到了 default里），后面的
  // switch 在拷贝指令时需要加上前面出现过的 switch 基本块的指令
  // switch:
  //   %stack_var_-41.0.reload = load i8, i8* %stack_var_-41.0.reg2mem
  //   %stack_var_-64.0.reload = load i32, i32* %stack_var_-64.0.reg2mem
  //   %stack_var_-60.0.reload = load i32, i32* %stack_var_-60.0.reg2mem
  //   store i32 %stack_var_-60.0.reload, i32* %stack_var_-60.1.reg2mem
  //   store i32 %stack_var_-64.0.reload, i32* %stack_var_-64.1.reg2mem
  //   store i8 %stack_var_-41.0.reload, i8* %stack_var_-41.1.reg2mem
  //   switch i32 %stack_var_-64.0.reload, label %dec_label_pc_401610 [
  //     i32 -1970668751, label %dec_label_pc_4014f1
  //     ......
  //     i32 1538300203, label %dec_label_pc_4012a6
  //  ]

  // dec_label_pc_4012a6:
  //   store i32 %stack_var_-60.0.reload, i32* %stack_var_-60.1.reg2mem
  //   store i32 %2, i32* %stack_var_-64.1.reg2mem, !insn.addr !3
  //   store i8 %stack_var_-41.0.reload, i8* %stack_var_-41.1.reg2mem
  //   br label %dec_label_pc_401610, !insn.addr !3
  auto *iterBB = switchBBs[0];
  SmallVector<BasicBlock *, 16> prevBBs;

  while (true) {
    // for (size_t idx = 1; idx < switchBBs.size(); idx++) {
    if (iterBB == loopEndBB) break;
    prevBBs.push_back(iterBB);
    errs() << "prevBBs size is " << prevBBs.size() << "\n";

    auto *bbSwitchInst = dyn_cast<SwitchInst>(iterBB->getTerminator());
    // 遍历 switch 指令的每一个分支
    for (auto caseIter = bbSwitchInst->case_begin();
         caseIter != bbSwitchInst->case_end(); caseIter++) {
      // 分支指向的基本块
      auto *sBB = caseIter->getCaseSuccessor();
      if (switchBBSet.count(sBB) || sBB == loopEndBB) continue;

      ValueToValueMapTy bbValMap;

      // 在 sBB 第一条有效指令前插入前驱指令
      IRBuilder<> bInstBuilder(sBB->getFirstNonPHIOrDbgOrLifetime());
      for (auto editBB : prevBBs) {
        for (auto &Inst : *editBB) {
          if (Inst.getOpcode() == Instruction::Switch) break;

          Instruction *instClone = Inst.clone();
          RemapInstruction(instClone, bbValMap, RF_IgnoreMissingLocals);
          bInstBuilder.Insert(instClone);
          bbValMap[&Inst] = instClone;
        }
      }

      // 更新原来基本块的变量名字
      for (auto &Inst : *sBB) {
        RemapInstruction(&Inst, bbValMap, RF_IgnoreMissingLocals);
      }

      //如果基本块为 ret 块，不需要插入后驱指令，跳过
      auto *lastInst = sBB->getTerminator();
      if (lastInst->getOpcode() == Instruction::Ret) continue;

      // 在每个基本块前插入后驱指令，其实就是 loopend 里的指令
      bInstBuilder.SetInsertPoint(sBB->getTerminator());
      for (auto &Inst : *loopEndBB) {
        if (Inst.getOpcode() == Instruction::Br) break;

        Instruction *instClone = Inst.clone();
        RemapInstruction(instClone, bbValMap, RF_IgnoreMissingLocals);
        bInstBuilder.Insert(instClone);
        bbValMap[&Inst] = instClone;
      }
    }
    // 处理 default 基本块
    iterBB = bbSwitchInst->getDefaultDest();
  }

  return true;
}

void DFlattening::eliminationCmp(llvm::Function &Func) {
  // 匹配类似于下面的指令
  //%1 = add i32 %x, -5, !insn.addr !1
  //%2 = sub i32 4, %x
  //%3 = and i32 %2, %x, !insn.addr !1
  //%4 = icmp eq i32 %1, 0, !insn.addr !1
  //%5 = xor i32 %1, %3
  //%6 = icmp slt i32 %5, 0
  //%7 = or i1 %4, %6, !insn.addr !2
}

llvm::PassPluginLibraryInfo getDFlatteningPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "dfla", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager &MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "dfla") {
                    MPM.addPass(DFlattening());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getDFlatteningPluginInfo();
}