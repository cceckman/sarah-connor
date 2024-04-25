
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------

// Key result for the bounded termination pass:
// Does this X terminate / do we know?
enum DoesThisTerminate {
  // We haven't evaluated this X yet.
  // Bottom of the lattice.
  Unevaluated,

  // Definitely terminates in a statically-bounded amount of time.
  // We assume:
  // - All memory operations (load/store) complete;
  //   though this may not be true in all systems (embedded),
  //   we consider using such transactions in an unbounded context undefined.
  Terminates,

  // The analyzer believes this will not terminate in bounded time:
  // It diverges, or may extend indefinitely.
  // For instance, it may read from stdin and wait for a newline-
  // which may come arbitrarily far in the future, or may never appear.
  // Or it may attempt to acquire a lock, which may never be released,
  // or may take arbitrarily long.
  //
  // In the current implementation, we assume these things have unbounded
  // latency:
  // - System calls
  // - Some loops (see "unknown")
  Unbounded,

  // The analyzer cannot reason about this X.
  // This may be because the path is data-flow dependent,
  // or because the analyzer does not have the reasoning to
  // bound the flow.
  //
  //   TODO: Use annotations to say "assume yes/no" for a
  //   function/block/call/etc
  Unknown,
};

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

struct BoundedTerminationPassResult {
  DoesThisTerminate elt;
  std::string explanation;
};

struct BoundedTerminationPass
    : public llvm::AnalysisInfoMixin<BoundedTerminationPass> {
  using Result = BoundedTerminationPassResult;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<BoundedTerminationPass>;
};

//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class BoundedTerminationPrinter
    : public llvm::PassInfoMixin<BoundedTerminationPrinter> {
public:
  explicit BoundedTerminationPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

BoundedTerminationPassResult
BasicBlockClassified(const llvm::BasicBlock &block) {
  for (const auto &I : block) {
    // Classify instructions based on whether we need to look at their metadata
    // In particular: call, invoke, callbr (these might have unbounded behavior)
    if (const auto *CI = llvm::dyn_cast<const llvm::CallBase>(&I)) {
      std::string callee_name;

      // TODO: Use the function attributes to improve this analysis
      if (auto called_function = CI->getCalledFunction();
          called_function != nullptr) {
        callee_name = llvm::demangle(called_function->getName());
      } else {
        callee_name = "Indirect";
      }
      return BoundedTerminationPassResult{
          .elt = DoesThisTerminate::Unknown,
          .explanation = "Calls function with unknown properties."};
    }
  }

  return BoundedTerminationPassResult{
      .elt = DoesThisTerminate::Terminates,
      .explanation = "Calls function with unknown properties."};
}

llvm::AnalysisKey BoundedTerminationPass::Key;

BoundedTerminationPass::Result
BoundedTerminationPass::run(llvm::Function &F,
                            llvm::FunctionAnalysisManager &) {
  BoundedTerminationPass::Result result;

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
            
          }
        }
      }
    }
  }

  return result;
}

llvm::PreservedAnalyses
BoundedTerminationPrinter::run(llvm::Function &F,
                               llvm::FunctionAnalysisManager &FAM) {
  BoundedTerminationPass::Result &result =
      FAM.getResult<BoundedTerminationPass>(F);
  auto demangled_fn = demangle(F.getName());
  OS << demangled_fn << " blocks with loops:\n";
  OS << "\t"
     << "[\n";
  for (const auto &block_name : result.blocks_with_loops) {
    OS << "\t" << friendly_name_block(block_name) << ",\n";
  }
  OS << "\t"
     << "]\n";
  OS << "metadata\n";

  return llvm::PreservedAnalyses::all();
}

llvm::PassPluginLibraryInfo getBoundedTerminationPassPluginInfo() {
  using namespace ::llvm;
  return {LLVM_PLUGIN_API_VERSION, "bounded-termination", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=print<static-cc>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<bounded-termination>") {
                    FPM.addPass(BoundedTerminationPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            // #2 REGISTRATION FOR "MAM.getResult<StaticCallCounter>(Module)"
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return BoundedTerminationPass(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getBoundedTerminationPassPluginInfo();
}
