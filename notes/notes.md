
### understanding analysis pass registration

Register analysis pass beforehand; requires an object (factory)

Use `ID` and `AnalysisInfoMixin` to create registry - effectively "type to object"

Then `getResults` doesn't have any objects, just types


```c++

#include "llvm/Pass.h" 
// Curiously Recurring Template Pattern:

struct AnalysisPass : public llvm::AnalysisInfoMixin<AnalysisPass> {
    using Result = int;

private:
    static llvm::AnalysisKey key;
    friend struct llvm::AnalysisInfoMixin<AnalysisPass>;
};


// AnalysisKey* is unique per type,
// which allows us to have a mapping from "type" to "concrete object"
// once the analysis is registered
AnalysisKey* llvm::AnalysisInfoMixin<AnalysisPass>::ID();

class xxAnalysisManager {
  public:
    template<typename PassT>
    void registerPass(std::function<PassT()> factory) {
        // AnalysisKey* -> std::unique_ptr<something inheriting from AnalysisMixin>
        AnalysisKey* key = PassT::ID();
        AnalysisPass[key] = factory();
    }

    template<typename PassT>
    PassT::Result getResult(xx &m) {
        AnalysisKey* key = PassT::ID();
        auto &analyzer = AnalysisPass[key];
        return analyzer->run(m);
    }

  private:
    // _effectively_ a mapping of "type" to "object",
    // filled by registerPass
    //
    // Implemented as AnalysisKey* -> object mapping
    AnalysisPassMapT AnalysisPass;

};


// Any method 
template<typename PassT>
void do_something() {
    // I can look up PassT::ID
}
```

`AnalysisPassModel`, which is-a `AnalysisPassConcept` but is templated on `PassT`, is doing the lifting between the dynamic-dispatch universe (`AnalysisPassConcept`) and the static dispatch universe (by its template argument)

`AnalysisPassModel<..., PassT>` has-a `PassT`,
inherits from `AnalysisPassConcept`

Why all this, instead of just "please implement this interface"? ...so that your class hierarchy does't need to include LLVM stuff _necessarily_? Eh.

Mixins: "add behaviors" but don't have anything `virtual` or declare any instance members (instance variables etc.). So: they add behavior, but don't really impact inheritance... "mixin", pretty safe to have anywhere in your inheritance

Scala: single "superclass", additional mixins?

If the mixin:
- Does not declare any `virtual` methods
    -> Will not have any effect on the vtable
- Does not declare any member variables
    -> Will not have any effect on object layout

--> It's safe to "mix in" as many of these as you like, in any order; in addition to a "normal" superclass / interface

These mixins use CRTP to access their "subclass" members (`AnalysisKey *Key`) so that... they aren't relying on anything in the vtable.

---

How to set up clangd? Simplest way is `compiler_flags.txt`, with `compiler_commands.json`
as a build system _output_ the fancier version.

Charles is pretty bare-bones as far as build systems go - he'd like to
understand exactly what step gets what information - so a generated CMake etc
is somewhat less attractive than e.g. `redo`.

(Hm. Write `compile_commands.json` by hand, and use `jq` to do default redo
rules, as we get more?)

For now, we have [redo rules](https://redo.readthedocs.io/en/latest/); can
invoke as `./do <thing you want>` without installing anything.


### next milestones (4/2)
- Actually count basic blocks in a function
- Make use of annotations: Add LLVM attributes in the source to do something where
we add an annotation in one function to request an analysis of another function
- Graph basic blocks of a function