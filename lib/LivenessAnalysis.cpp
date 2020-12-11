//
// Created by anshil on 2020-11-26.
//

#include "LivenessAnalysis.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"

#define PASS_NAME "liveness"
#define DEBUG_TYPE "liveness"

// #define debug_def_sets
// #define debug_use_sets

PreservedAnalyses
LivenessAnalysis::run(llvm::Function& fn, llvm::FunctionAnalysisManager&){

  computeGenKillVariables(&fn);

  // compute liveIn and liveOut sets for each basic block
  bool changed = true;
  int iter = 0;
  while (changed) {
    // errs() << "iteration " << iter++ << "...\n";

    // track whether this iteration changed the liveIn set or not
    changed = false;

    // iterate over each basic block
    for (BasicBlock& bb : reverse(fn)){

      VarSet prevOut = liveOut[&bb];
      VarSet prevIn = liveIn[&bb];

      // compute liveOut set for the current basic block
      liveOut[&bb].clear();
      for (BasicBlock* nextBB : successors(&bb)){
        for (auto& liveVar : liveIn[nextBB]){
          liveOut[&bb].insert(liveVar);
        }
      }
      if (prevOut != liveOut[&bb]) changed = true;

      // compute liveIn set for the current basic block
      liveIn[&bb].clear();
      liveIn[&bb] = use[&bb];
      for (auto& var : liveOut[&bb]){
        if (def[&bb].count(var) == 0){
          liveIn[&bb].insert(var);
        }
      }

      // check if the liveIn set has reached a fixed point
      if (liveIn[&bb] != prevIn)  changed = true;
    }
  }

  for (auto& bb : fn){
    errs() << "Live in set for basic block " << bb.getName() << " with #instructions = " << bb.getInstList().size() << '\n';
    debugPrintVarSet(liveIn[&bb]);
    errs() << '\n';

    errs() << "\nLive out set for basic block " << bb.getName() << '\n';
    debugPrintVarSet(liveOut[&bb]);
    errs() << "\n---------------------------------\n";
  }
  return llvm::PreservedAnalyses::all();
}

void LivenessAnalysis::computeGenKillVariables(Function *fn) {
  for (auto& bb : *fn) {
    for (auto& inst : bb) {
      const Instruction *cInst = &inst;
      if (cInst->isTerminator())  continue;

      // insert each operand into the use set of this basic block
      for (auto &op : cInst->operands()) {
        if (!isa<Constant>(op))
          use[&bb].insert(op);
      }

      /// a value was killed by 
      if (cInst->getType() != Type::getVoidTy(fn->getContext())){
        if (!isa<Constant>(cInst))
          def[&bb].insert(cInst);
      }
    }

#ifdef debug_def_sets
    errs() << "Def set for basic block " << bb.getName() << '\n';
    debugPrintVarSet(def[&bb]);
    errs() << "\n";
#endif

#ifdef debug_use_sets
    errs() << "Use set for basic block " << bb.getName() << '\n';
    debugPrintVarSet(use[&bb]);
    errs() << "--------------------------------------\n";
    errs() << "// ======================================== //\n";
#endif
  }
}

void LivenessAnalysis::debugPrintVarSet(LivenessAnalysis::VarSet& s){
  for (auto& var : s){
    errs() << *var << "\n";
  }
}

// --------------------------------------------------------------------------------
// Pass Manager registration
// --------------------------------------------------------------------------------

llvm::PassPluginLibraryInfo getLivenessPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, PASS_NAME, LLVM_VERSION_STRING,
          [](llvm::PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](llvm::StringRef Name, llvm::FunctionPassManager &FPM,
                   llvm::ArrayRef<llvm::PassBuilder::PipelineElement>) {
                  if (Name == "liveness") {
                    FPM.addPass(LivenessAnalysis());
                    return true;
                  }
                  return false;
                });
          }};
}

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getLivenessPluginInfo();
}