
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------

std::string friendly_name_block(llvm::StringRef unfriendly) {
  llvm::StringRef tail = unfriendly;
  llvm::StringRef head;
  std::string result;
  while (tail != "") {
    auto spl = tail.split('.');
    head = spl.first;
    tail = spl.second;
    result.append(llvm::demangle(head));
    if (tail != "") {
      result.push_back('.');
    }
  }

  return result;
}

struct Block {
  std::string name;
  std::string llvm_name;
};

struct SCCLoopPassResult {
  std::set<std::string> blocks_with_loops;
};

struct SCCLoopPass : public llvm::AnalysisInfoMixin<SCCLoopPass> {
  using Result = SCCLoopPassResult;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<SCCLoopPass>;
};

//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class SCCLoopPrinter : public llvm::PassInfoMixin<SCCLoopPrinter> {
public:
  explicit SCCLoopPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

llvm::AnalysisKey SCCLoopPass::Key;

SCCLoopPass::Result SCCLoopPass::run(llvm::Function &F,
                                     llvm::FunctionAnalysisManager &) {
  SCCLoopPass::Result result;
  for (llvm::scc_iterator<llvm::Function *> it = llvm::scc_begin(&F),
                                            it_end = llvm::scc_end(&F);
       it != it_end; ++it) {
    if (it.hasCycle()) {
      // Arbitrarily choosing size of 10 as our size for an SCC
      llvm::SmallPtrSet<llvm::BasicBlock *, 10> basic_blocks(it->begin(),
                                                             it->end());
      for (auto block : basic_blocks) {
        for (const llvm::BasicBlock *pred : llvm::predecessors(block)) {
          // Check whether pred is not in our basic blocks set
          if (!basic_blocks.contains(pred)) {
            result.blocks_with_loops.insert(std::string(block->getName()));
          }
        }
      }
    }
  }

  return result;
}

llvm::PreservedAnalyses
SCCLoopPrinter::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  SCCLoopPass::Result &result = FAM.getResult<SCCLoopPass>(F);
  auto demangled_fn = demangle(F.getName());
  OS << demangled_fn << " blocks with loops:\n";
  OS << "\t"
     << "[\n";
  for (const auto &block_name : result.blocks_with_loops) {
    OS << "\t" << friendly_name_block(block_name) << ",\n";
  }
  OS << "\t"
     << "]\n";

  return llvm::PreservedAnalyses::all();
}

llvm::PassPluginLibraryInfo getSCCLoopPassPluginInfo() {
  using namespace ::llvm;
  return {LLVM_PLUGIN_API_VERSION, "scc-loop-pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=print<static-cc>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<scc-loop-pass>") {
                    FPM.addPass(SCCLoopPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            // #2 REGISTRATION FOR "MAM.getResult<StaticCallCounter>(Module)"
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return SCCLoopPass(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getSCCLoopPassPluginInfo();
}