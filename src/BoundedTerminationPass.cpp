
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
  explicit FunctionBoundedTerminationPrinter(llvm::raw_ostream &OutS)
      : OS(OutS) {}
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

TerminationPassResult join(TerminationPassResult res1,
                           TerminationPassResult res2) {
  TerminationPassResult minResult = std::min(res1, res2);
  TerminationPassResult maxResult = std::max(res1, res2);

  if (minResult.elt == DoesThisTerminate::Unevaluated) {
    return maxResult;
  }

  // Does not take the interior edge as part of the lattice.
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

// TODO: If we fix propagation back towards the BB source,
// can this be fully replaced with `join`?
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

// Reports whether the loop is bounded, or unknown.
// TODO: Report "unbounded" if there is no exit from the loop.
// TODO: ...and handle the "this function has a loop with no exit";
// means that we have to propagate back to the entry block, look there.
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

//------------------------------------------------------------------------------
// Pass bodies
//------------------------------------------------------------------------------

FunctionTerminationPass::Result
FunctionTerminationPass::run(llvm::Function &F,
                             llvm::FunctionAnalysisManager &FAM) {
  if (F.empty()) {
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

  // Step 1 : do local basic block analysis.
  // We don't need to do this? Assume every instruction terminates,
  // including call instructions. (We'll handle them at the call-graph layer.)
  // for (auto &basic_block : F) {
  //   TerminationPassResult result = basicBlockClassifier(basic_block);
  //   blocks_to_results.insert_or_assign(&basic_block, result);
  //   outstanding_nodes.insert(&basic_block);
  // }

  // Step 2 : do loop-level analysis.
  // We need a ScalarEvolution to get the loops.
  // The blocks_to_results map is empty before we start this.
  for (auto &basic_block : F) {
    llvm::Loop *loop = loop_info.getLoopFor(&basic_block);
    if (loop == nullptr) {
      // Block is (locally) bounded.
      blocks_to_results.insert_or_assign(&basic_block,
                                         TerminationPassResult{
                                             .elt = DoesThisTerminate::Bounded,
                                             .explanation = "",
                                         });
      continue;
    }
    // If the loop is bounded, we count this node as bounded too.
    auto result = loopClassifier(*loop, SE);
    blocks_to_results.insert_or_assign(&basic_block, result);
  }
  // All blocks are labeled:
  // - Bounded if not part of a loop.
  // - Unbounded if an infinite loop (no exit) -- note, hypothetical, this is a TODO
  // - Unknown if a loop bound cannot be determined

  // Step 3 : aggregate results.
  // In order to accurately capture:
  /*
  * void maybe_hold(bool stall) {
  *   if(stall) {
  *     while(true) {}
  *   } else {
  *     return;
  *   }
  */
  // We need to propagate towards the entry block,
  // then use the entry block to determine the function's result.
  // Is "worklist towards the entry" equivalent "`join` over all results"?
  // No, we still do need 'update', with its slightly-different semantics from 'join'.

  // Worklist version (towards source):
  for(llvm::BasicBlock &block : F) {
    // We add everything, even the non-exit nodes,
    // so that we make sure we eventually get back to the entry block.
    // Consider:
    // void does_not_terminate(bool stall) {
    //   if(stall) { // entry block: B1, successors are B2/B3
    //     while(true) {} // B2: predecessors are is B1, B2, successor is B2
    //   } else {
    //     while(true) {} // B3: predecessors are B1, B3, successor is B3
    //  }
    //  // B4? Exit block? May not exist, has no predecessors
    // }
    // We need to make sure that we propagate from non-exiting paths
    // (Unbounded) to the entry block; so, add everything to begin with,
    // and just iterate the worklist until we hit a fixpoint.
    // We still know it will quiesce due to the convergent nature of update().
    outstanding_nodes.insert(&block);
  }
  while (!outstanding_nodes.empty()) {
    llvm::BasicBlock *block = outstanding_nodes.pop_back_val();
    auto original = blocks_to_results.at(block);
    std::vector<TerminationPassResult> results;
    for (llvm::BasicBlock *successor : llvm::successors(block)) {
      results.emplace_back(blocks_to_results.at(successor));
    }
    auto altered = update(original, std::move(results));
    blocks_to_results.insert_or_assign(block, altered);
    if (altered.elt != original.elt) {
      for (auto *predecessor : llvm::predecessors(block)) {
        outstanding_nodes.insert(predecessor);
      }
    }
  }

  return blocks_to_results.at(&F.getEntryBlock());
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
  for (llvm::scc_iterator<llvm::CallGraph *> SCCI = llvm::scc_begin(&CG);
       !SCCI.isAtEnd(); ++SCCI) {
    if (!SCCI.hasCycle()) {
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
    for (llvm::CallGraphNode *node : nextSCC) {
      const llvm::Function *f = node->getFunction();
      // May be null:
      // 1. Exported functions get edges "in from" the null node, to represent
      // external calls.
      //    (We don't care about these.)
      // 2. Calls that leave this module, or have an indirect function call,
      //    have calls out to another "null" node.
      //    Indirect functions are handled at the function-local layer.
      //    Extern functions are handled as "unknown body".
      // So we should be fine to skip?
      if (f == nullptr) {
        continue;
      }
      shared_result.explanation =
          (shared_result.explanation + llvm::demangle(f->getName()));
      if (count < nextSCC.size() - 1) {
        shared_result.explanation += ", ";
      } else {
        ++count;
      }
    }
    for (llvm::CallGraphNode *node : nextSCC) {
      llvm::Function *f = node->getFunction();
      const auto new_result = update(per_function_results[f], {shared_result});
      per_function_results[f] = new_result;
    }
  }
  // Step 3 : worklist algorithm on the call graph.
  // Ideally we'd _just_ do worklist, but we don't have a "callers" list, alas.
  // So we just run N^2: scan through each function, update from callees,
  // and run again if we updated something.
  bool stale = true;
  while (stale) {
    stale = false;
    for (auto &F : IR) {
      TerminationPassResult original = per_function_results[&F];
      const llvm::CallGraphNode *CGNode = CG[&F];
      std::vector<TerminationPassResult> results;

      // Update this node from its successors.
      for (const auto &it : *CGNode) {
        llvm::CallGraphNode *callee = it.second;
        if (auto *CalleeF = callee->getFunction(); CalleeF != nullptr) {
          const auto &result = per_function_results[CalleeF];
          results.emplace_back(TerminationPassResult{
              .elt = result.elt,
              .explanation = "via call to " +
                             llvm::demangle(CalleeF->getName()) + ": " +
                             result.explanation,
          });
        } else {
          // Callee is nullptr. Does that mean it's indirect?
          // TODO: Not sure; we need more testing of indirect calls.
          results.emplace_back(TerminationPassResult{
              .elt = DoesThisTerminate::Unknown,
              .explanation = "via call to unknown function",
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
  for (const auto &[function, result] : module_results.per_function_results) {
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
