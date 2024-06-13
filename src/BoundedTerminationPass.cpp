
#include "llvm/ADT/SCCIterator.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Analysis/CallGraph.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
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
#include <algorithm>
#include <map>
#include <string>
#include <vector>

//------------------------------------------------------------------------------
// Type definitions
//------------------------------------------------------------------------------

// Key result for the bounded termination pass:
// Does this X terminate / do we know?
enum class DoesThisTerminate {
  // We haven't evaluated this X yet.
  // Bottom of the lattice.
  Unevaluated,

  // Definitely terminates in a statically-bounded amount of time.
  // We assume:
  // - All memory operations (load/store) complete;
  //   though this may not be true in all systems (embedded),
  //   we consider using such transactions in an unbounded context undefined.
  Bounded,

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

// Complete result for a termination evaluation:
// an enum result, plus an explanation of reasoning.
//
// Function-level analysis results are contingent:
// they assume that every function this function calls
// is Bounded.
//
// TODO: This may be invalidated by loop transforms - implement `invalidate`
struct TerminationPassResult {
  DoesThisTerminate elt = DoesThisTerminate::Unevaluated;
  std::string explanation = "unevaluated";
};

// Results from analyzing the full module,
// including call-graph analysis.
struct ModuleTerminationPassResult {
  std::map<const llvm::Function *, TerminationPassResult> per_function_results;

  // Invalidated when:
  // - FunctionTerminationPass is invalidated
  // - LazyCallGraphPass analysis is invalidated
  bool invalidate(llvm::Module &IR, const llvm::PreservedAnalyses &PA,
                  llvm::ModuleAnalysisManager::Invalidator &) {
    // TODO: This is the worst result we could give here!
    return false;
  }
};

// Pass over functions: does this terminate:
struct FunctionTerminationPass
    : public llvm::AnalysisInfoMixin<FunctionTerminationPass> {
  using Result = TerminationPassResult;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend llvm::AnalysisInfoMixin<FunctionTerminationPass>;
};

struct ModuleTerminationPass
    : public llvm::AnalysisInfoMixin<ModuleTerminationPass> {
  using Result = ModuleTerminationPassResult;
  Result run(llvm::Module &IR, llvm::ModuleAnalysisManager &AM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend llvm::AnalysisInfoMixin<ModuleTerminationPass>;
};

// Printer pass for the module-level termination checker
class BoundedTerminationPrinter
    : public llvm::PassInfoMixin<BoundedTerminationPrinter> {
public:
  explicit BoundedTerminationPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Module &IR,
                              llvm::ModuleAnalysisManager &MAM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

// Printer pass for the module-level termination checker
class FunctionBoundedTerminationPrinter
    : public llvm::PassInfoMixin<FunctionBoundedTerminationPrinter> {
public:
  explicit FunctionBoundedTerminationPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &IR,
                              llvm::FunctionAnalysisManager &FAM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

//------------------------------------------------------------------------------
// Free functions
//------------------------------------------------------------------------------

bool operator<(const TerminationPassResult &a, const TerminationPassResult &b) {
  if (a.elt == b.elt) {
    return a.explanation < b.explanation;
  } else {
    return a.elt < b.elt;
  }
}

llvm::StringRef to_string(DoesThisTerminate t) {
  switch (t) {
  case DoesThisTerminate::Unevaluated:
    return "Unevaluated";
  case DoesThisTerminate::Bounded:
    return "Bounded";
  case DoesThisTerminate::Unbounded:
    return "Unbounded";
  case DoesThisTerminate::Unknown:
    return "Unknown";
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &os,
                              const DoesThisTerminate &dt) {
  os << to_string(dt);
  return os;
}

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

TerminationPassResult basicBlockClassifier(const llvm::BasicBlock &block) {
  for (const auto &I : block) {
    // Classify instructions based on whether we need to look at their metadata
    // In particular: call, invoke, callbr (these might have unbounded behavior)
    if (const auto *CI = llvm::dyn_cast<const llvm::CallBase>(&I)) {
      if (auto called_function = CI->getCalledFunction();
          called_function == nullptr) {
        // Indirect function call; we don't know what the properties of the
        // target are, and our CG analysis won't cover it.
        // (CG covers CGSCC/recursion detection, as well as propagating
        // unbounded backwards .)
        // TODO: Is this necesary, given that we're analyzing calls at the module layer?
        // The CallGraph has a lot more information about what calls are possible,
        // and includes a "null" node that indirect calls point to.
        // Whole-program devirtualization might even take an indirect call and turn it
        // into a direct call.
        return TerminationPassResult{.elt = DoesThisTerminate::Unknown,
                                     .explanation =
                                         "Performs an indirect function call"};
      }
    }
  }

  return TerminationPassResult{.elt = DoesThisTerminate::Bounded,
                               .explanation = "no indirect calls"};
}

TerminationPassResult join(TerminationPassResult res1,
                           TerminationPassResult res2) {
  TerminationPassResult minResult = std::min(res1, res2);
  TerminationPassResult maxResult = std::max(res1, res2);

  if (minResult.elt == DoesThisTerminate::Unevaluated) {
    return maxResult;
  }

  if (minResult.elt == DoesThisTerminate::Bounded) {
    if (maxResult.elt == DoesThisTerminate::Unbounded) {
      return TerminationPassResult{
          .elt = DoesThisTerminate::Unknown,
          .explanation =
              "Joined with Unbounded branch: " + maxResult.explanation,
      };
    }

    return maxResult;
  }

  if (minResult.elt == DoesThisTerminate::Unbounded) {
    if (maxResult.elt == DoesThisTerminate::Unbounded) {
      return TerminationPassResult{
          .elt = DoesThisTerminate::Unbounded,
          .explanation = "Joined two Unbounded branches: (" +
                         minResult.explanation + "), (" +
                         maxResult.explanation + ")",
      };
    }
  }

  return maxResult;
}

TerminationPassResult update(TerminationPassResult result,
                             std::vector<TerminationPassResult> pred_results) {

  TerminationPassResult predecessor_result;
  for (const auto &predecessor : pred_results) {
    predecessor_result = join(predecessor_result, predecessor);
  }

  // After joining all predecessors, we have one special exception to the
  // 'join' rule.
  //
  // 'join' works for symmetric results, but we have an asymmetry here:
  // if all predecessors are `Unbounded` and this is `Bounded`, then this node
  // is `Unbounded` as well,
  // TODO: This should be "just always unbounded", but adding an
  // unknown-to-unbounded transition would prevent convergence. What to do?
  if (result.elt == DoesThisTerminate::Bounded &&
      predecessor_result.elt == DoesThisTerminate::Unbounded) {
    return predecessor_result;
  } else {
    return join(result, predecessor_result);
  }
}

TerminationPassResult loopClassifier(const llvm::Loop &loop,
                                     llvm::ScalarEvolution &SE) {
  std::optional<llvm::Loop::LoopBounds> bounds = loop.getBounds(SE);
  if (!bounds.has_value()) {
    return TerminationPassResult{.elt = DoesThisTerminate::Unknown,
                                 .explanation =
                                     "includes loop with indeterminate bounds"};
  }
  return TerminationPassResult{.elt = DoesThisTerminate::Bounded,
                               .explanation =
                                   "includes a loop, but it has a fixed bound"};
}

bool isExitingBlock(const llvm::BasicBlock &B) {
  // Check whether the terminating instruction of the block is a
  // "return"-type. https://llvm.org/docs/LangRef.html#terminator-instructions
  const auto *terminator = B.getTerminator();
  if (terminator == nullptr) {
    // TODO: Can we print / capture an error here, or something?
    return true;
  }
  return terminator->willReturn();
}

//------------------------------------------------------------------------------
// Pass bodies
//------------------------------------------------------------------------------

FunctionTerminationPass::Result
FunctionTerminationPass::run(llvm::Function &F,
                             llvm::FunctionAnalysisManager &FAM) {
  if(F.empty()) {
    return FunctionTerminationPass::Result{
      .elt = DoesThisTerminate::Unknown,
      .explanation = "has no basic blocks in this module",
    };
  }

  llvm::ScalarEvolution &SE = FAM.getResult<llvm::ScalarEvolutionAnalysis>(F);
  llvm::LoopInfo &loop_info = FAM.getResult<llvm::LoopAnalysis>(F);

  std::map<llvm::BasicBlock *, TerminationPassResult> blocks_to_results;
  // SetVector preserves insertion order - which is nice because it makes this
  // deterministic.
  llvm::SetVector<llvm::BasicBlock *> outstanding_nodes;

  // Step 1 : do local basic block analysis
  for (auto &basic_block : F) {
    TerminationPassResult result = basicBlockClassifier(basic_block);
    blocks_to_results.insert_or_assign(&basic_block, result);
    outstanding_nodes.insert(&basic_block);
  }

  // Step 2 : do loop-level analysis. We need a ScalarEvolution to get the
  // loops.
  for (auto &basic_block : F) {
    llvm::Loop *loop = loop_info.getLoopFor(&basic_block);
    if (loop == nullptr) {
      continue;
    }
    auto result = loopClassifier(*loop, SE);
    auto updated = join(blocks_to_results.at(&basic_block), result);
    blocks_to_results.insert_or_assign(&basic_block, updated);
  }

  // Step 3 : worklist algorithm.
  while (!outstanding_nodes.empty()) {
    llvm::BasicBlock *block = outstanding_nodes.pop_back_val();
    auto original = blocks_to_results.at(block);
    std::vector<TerminationPassResult> results;
    for (llvm::BasicBlock *predecessor : llvm::predecessors(block)) {
      results.emplace_back(blocks_to_results.at(predecessor));
    }
    auto altered = update(original, std::move(results));
    blocks_to_results.insert_or_assign(block, altered);
    if (altered.elt != original.elt) {
      for (auto *successor : llvm::successors(block)) {
        outstanding_nodes.insert(successor);
      }
    }
  }

  // Step 4 : join results of exiting blocks
  TerminationPassResult aggregate_result = {.elt = DoesThisTerminate::Bounded,
                                            .explanation = ""};

  for (auto const &[key, value] : blocks_to_results) {
    if (isExitingBlock(*key)) {
      aggregate_result = join(aggregate_result, value);
    }
  }

  return aggregate_result;
}

ModuleTerminationPass::Result
ModuleTerminationPass::run(llvm::Module &IR, llvm::ModuleAnalysisManager &AM) {
  std::map<const llvm::Function *, TerminationPassResult> per_function_results;

  auto &function_analysis_manager_proxy =
      AM.getResult<llvm::FunctionAnalysisManagerModuleProxy>(IR);
  auto &FAM = function_analysis_manager_proxy.getManager();
  // Step 1 : function-local analysis
  for (llvm::Function &function : IR) {
    per_function_results.insert(
        {&function, FAM.getResult<FunctionTerminationPass>(function)});
  }

  // Step 2 : CGSCC analysis.
  // Take anything in a recursive group and force it Unknown.
  // See also NoRecursionCheck in clang-tidy
  llvm::CallGraph &CG = AM.getResult<llvm::CallGraphAnalysis>(IR);
  for(llvm::scc_iterator<llvm::CallGraph*> SCCI = llvm::scc_begin(&CG); !SCCI.isAtEnd(); ++SCCI) {
    if(!SCCI.hasCycle()) {
      // SCC doesn't have a loop. We don't need to update anything.
      continue;
    }
    // SCC has a loop. Update all functions to note they're mutually recursive.
    const std::vector<llvm::CallGraphNode *> &nextSCC = *SCCI;
    TerminationPassResult shared_result = {
      .elt = DoesThisTerminate::Unknown,
      .explanation = "part of a call graph that contains a loop: ",
    };
    int count = 0;
    for(llvm::CallGraphNode *node : nextSCC) {
      const llvm::Function *f = node->getFunction();
      // May be null:
      // 1. Exported functions get edges "in from" the null node, to represent external calls.
      //    (We don't care about these.)
      // 2. Calls that leave this module, or have an indirect function call,
      //    have calls out to another "null" node.
      //    Indirect functions are handled at the function-local layer.
      //    Extern functions are handled as "unknown body".
      // So we should be fine to skip?
      if(f == nullptr) {
        continue;
      }
      shared_result.explanation = (shared_result.explanation + llvm::demangle(f->getName()));
      if(count < nextSCC.size() - 1) {
        shared_result.explanation += ", ";
      } else {
        ++count;
      }
    }
    for(llvm::CallGraphNode *node : nextSCC) {
      llvm::Function *f = node->getFunction();
      const auto new_result =  update(per_function_results[f], {shared_result});
      per_function_results[f] = new_result;
    }
  }
  // Step 3 : worklist algorithm on the call graph.
  // Ideally we'd _just_ do worklist, but we don't have a "callers" list, alas.
  // So we just run N^2: scan through each function, update from callees,
  // and run again if we updated something.
  bool stale = true;
  while(stale) {
    stale = false;
    for(auto &F : IR) {
      TerminationPassResult original = per_function_results[&F];
      const llvm::CallGraphNode *CGNode = CG[&F];
      std::vector<TerminationPassResult> results;

      // Update this node from its successors.
      for(const auto &it : *CGNode) {
        llvm::CallGraphNode *callee = it.second;
        if(auto *CalleeF = callee->getFunction(); CalleeF != nullptr) {
          const auto &result = per_function_results[CalleeF];
          results.emplace_back(TerminationPassResult{
            .elt = result.elt,
            .explanation = "via call to " + llvm::demangle(CalleeF->getName()) + ": " + result.explanation,
          });
        }
      }
      auto altered = update(original, std::move(results));
      if (altered.elt != original.elt) {
        per_function_results[&F] = altered;
        stale = true;
      }
    }
  }

  return ModuleTerminationPassResult{per_function_results};
}

llvm::PreservedAnalyses
BoundedTerminationPrinter::run(llvm::Module &IR,
                               llvm::ModuleAnalysisManager &AM) {
  auto &module_results = AM.getResult<ModuleTerminationPass>(IR);
  for (const auto &[function, result] : module_results.per_function_results)
  {
    OS << "Function name: " << llvm::demangle(function->getName()) << "\n";
    OS << "Result: " << result.elt << "\n";
    OS << "Explanation: " << result.explanation << "\n\n";
  }

  return llvm::PreservedAnalyses::all();
}

llvm::PreservedAnalyses
FunctionBoundedTerminationPrinter::run(llvm::Function &IR,
                                       llvm::FunctionAnalysisManager &AM) {
  auto &results = AM.getResult<FunctionTerminationPass>(IR);
  OS << "For function: " << llvm::demangle(IR.getName())
     << "got result: " << results.elt << "\n";

  return llvm::PreservedAnalyses::all();
}

//------------------------------------------------------------------------------
// Static / wiring
//------------------------------------------------------------------------------

llvm::AnalysisKey FunctionTerminationPass::Key;
llvm::AnalysisKey ModuleTerminationPass::Key;

llvm::PassPluginLibraryInfo getBoundedTerminationPassPluginInfo() {
  using namespace ::llvm;
  return {LLVM_PLUGIN_API_VERSION, "bounded-termination", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, ModulePassManager &PM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<bounded-termination>") {
                    PM.addPass(BoundedTerminationPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &PM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<function-bounded-termination>") {
                    PM.addPass(FunctionBoundedTerminationPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &AM) {
                  AM.registerPass([&] { return FunctionTerminationPass(); });
                });
            PB.registerAnalysisRegistrationCallback(
                [](ModuleAnalysisManager &AM) {
                  AM.registerPass([&] { return ModuleTerminationPass(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getBoundedTerminationPassPluginInfo();
}
