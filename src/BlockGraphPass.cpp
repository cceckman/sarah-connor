
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
#include <cstdio>
#include <unordered_map>
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
  size_t instruction_count;
  int id;
  bool is_external;
  bool is_entry;

  void print(llvm::raw_ostream &os) const {
    os << id << ": \t" << instruction_count << "i\t "
       << (is_entry ? " . " : "   ") << name;
  }

  void print_node(llvm::raw_ostream &os) const {
    os << "  block_" << id << "[";
    // Properties:
    os << "label=\"" << name << "\""
       << ",";
    os << "shape=";
    if (is_external) {
      os << "rect";
    } else if (is_entry) {
      os << "doublecircle";
    } else {
      os << "circle";
    }
    os << "]";
  }

  static Block from_llvm(int *counter, bool external,
                         const llvm::BasicBlock &block) {
    auto id = *counter;
    counter++;
    auto friendly = friendly_name_block(block.getName());
    return Block{
        .name = friendly,
        .instruction_count = block.size(),
        .id = id,
        .is_external = external,
        .is_entry = block.isEntryBlock(),
    };
  }
};

struct BlockGraphResult {
  std::vector<Block> blocks;
  std::vector<std::pair<int, int>> edges;
};

struct BlockGraphPass : public llvm::AnalysisInfoMixin<BlockGraphPass> {
  using Result = BlockGraphResult;
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
  std::unordered_map<const llvm::BasicBlock *, Block> blocks;
  int block_id = 0;
  for (const auto &block : F) {
    auto friendly = friendly_name_block(block.getName());
    blocks.insert(
        {&block, Block::from_llvm(&block_id, /*external=*/false, block)});
    block_id++;
  }

  std::vector<std::pair<int, int>> edges;
  for (const auto &block : F) {
    for (const llvm::BasicBlock *successor : llvm::successors(&block)) {
      auto successor_it = blocks.find(successor);
      if (successor_it == blocks.end()) {
        auto result = blocks.insert(
            {&block,
             Block::from_llvm(&block_id, /*external=*/true, *successor)});
        successor_it = result.first;
      }
      const auto &block_info = blocks.at(&block);
      edges.push_back({block_info.id, successor_it->second.id});
    }
  }

  std::vector<Block> out_blocks;
  out_blocks.resize(blocks.size());
  for (auto &&[_, block] : blocks) {
    out_blocks[block.id] = std::move(block);
  }

  return BlockGraphPass::Result{
      .blocks = out_blocks,
      .edges = edges,
  };
}

llvm::PreservedAnalyses
BlockGraphPrinter::run(llvm::Function &F, llvm::FunctionAnalysisManager &FAM) {
  const static std::string MY_NAME = "BlockGraphPrinter::run(llvm::Function&, "
                                     "llvm::AnalysisManager<llvm::Function>&)";
  const static std::string PASS_NAME =
      "BlockGraphPass::run(llvm::Function&, "
      "llvm::AnalysisManager<llvm::Function>&)";

  auto demangled_fn = demangle(F.getName());
  if (demangled_fn == "main") {
    BlockGraphPass::Result &result = FAM.getResult<BlockGraphPass>(F);

    // TODO: This is almost certainly redundant with a pass that uses
    // https://llvm.org/doxygen/GraphWriter_8h_source.html

    OS << "digraph {\n  label=\"" << demangle(F.getName()) << "\"\n\n";
    for (const auto &block : result.blocks) {
      block.print_node(OS);
      OS << "\n";
    }
    OS << "// Edges: \n";
    for (const auto &[out, in] : result.edges) {
      OS << "  "
         << "block_" << out << " -> "
         << "block_" << in << ";\n";
    }

    OS << "} // end digraph\n";
  }

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