
#include "llvm/IR/PassInstrumentation.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_ostream.h"

//------------------------------------------------------------------------------
// New PM interface
//------------------------------------------------------------------------------

struct IntWrapper {
  int result;
};

struct BlockGraphPass : public llvm::AnalysisInfoMixin<BlockGraphPass> {
  using Result = IntWrapper;
  Result run(llvm::Function &F, llvm::FunctionAnalysisManager &);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  // A special type used by analysis passes to provide an address that
  // identifies that particular analysis pass type.
  static llvm::AnalysisKey Key;
  friend struct llvm::AnalysisInfoMixin<BlockGraphPass>;
};

//------------------------------------------------------------------------------
// New PM interface for the printer pass
//------------------------------------------------------------------------------
class BlockGraphPrinter : public llvm::PassInfoMixin<BlockGraphPrinter> {
public:
  explicit BlockGraphPrinter(llvm::raw_ostream &OutS) : OS(OutS) {}
  llvm::PreservedAnalyses run(llvm::Function &F,
                              llvm::FunctionAnalysisManager &FAM);
  // Part of the official API:
  //  https://llvm.org/docs/WritingAnLLVMNewPMPass.html#required-passes
  static bool isRequired() { return true; }

private:
  llvm::raw_ostream &OS;
};

llvm::AnalysisKey BlockGraphPass::Key;

BlockGraphPass::Result BlockGraphPass::run(llvm::Function &F,
                                           llvm::FunctionAnalysisManager &) {
  return BlockGraphPass::Result();
}

llvm::PreservedAnalyses
BlockGraphPrinter::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  BlockGraphPass::Result &result = FAM.getResult<BlockGraphPass>(F);

  OS << "BIG NEWS!\n";
  OS << "Function "  << F.getName() << " has " << result.result << " basic blocks\n";

  return llvm::PreservedAnalyses::all();
}


llvm::PassPluginLibraryInfo getBlockGraphPassPluginInfo() {
  using namespace ::llvm;
  return {LLVM_PLUGIN_API_VERSION, "block-graph-pass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            // #1 REGISTRATION FOR "opt -passes=print<static-cc>"
            PB.registerPipelineParsingCallback(
                [&](StringRef Name, FunctionPassManager &FPM,
                    ArrayRef<PassBuilder::PipelineElement>) {
                  if (Name == "print<block-graph-pass>") {
                    FPM.addPass(BlockGraphPrinter(llvm::errs()));
                    return true;
                  }
                  return false;
                });
            // #2 REGISTRATION FOR "MAM.getResult<StaticCallCounter>(Module)"
            PB.registerAnalysisRegistrationCallback(
                [](FunctionAnalysisManager &FAM) {
                  FAM.registerPass([&] { return BlockGraphPass(); });
                });
          }};
};

extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo() {
  return getBlockGraphPassPluginInfo();
}