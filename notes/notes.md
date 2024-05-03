
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


### Milestones (4/15)

- [x]   Graph basic blocks of a function
- [ ]   Understand LLVM attributes [1](https://llvm.org/docs/HowToUseAttributes.html),
  or do I mean metadata or something else? [2](https://blog.yossarian.net/2021/11/29/LLVM-internals-part-4-attributes-and-attribute-groups)
- [ ]   Pick CFG root to render based on an attribute
- [ ]   Understand how calls are represented in the LLVM-IR. Represent them
        in the CFG rendering
- [ ]   Understand how to control optimization passes, e.g. inlining

- Actually count basic blocks in a function
- Make use of annotations: Add LLVM attributes in the source to do something where
we add an annotation in one function to request an analysis of another function

## Cross-function analysis?

On a per-function basis:
- Assert that the CFG (basic blocks) form a DAG
    (initial, "conservative" version)
- Return all called functions

On a per-module basis (cross-module basis?)
- Assert that the graph of function calls is a DAG
    (again, conservative - excludes recursion and mutual recursion)
- Assert that the closure of functions called by an always-terminates function are also always-terminates

Lattices?

definitely does not terminate
may or may not terminate
does terminate

Combine two functions... eh, no, it's more complicated: result depends on whether call is conditional.

Join over TerminationResults:

- If equal, pick either

- If not equal:
    - If either is Unknown: then pick Unknown
    - Else if either is Unevaluated, then pick the other
    - Else pick Unknown

Alternatively:

Sort into min and max (check which one is the lesser element)

- If the min argument is Unevaluated, then pass the other explanation forward

- If the min argument is Terminates, then:
    - If the second argument is Unbounded, then Unknown and explanation says joined with Unbounded branch
    - Otherwise, pass the other one forward

- If the min argument is Unbounded, then:
    - If the second argument is Unbounded, then Unbounded and explanation says joined with another Unbounded branch
    - Otherwise, pass the other one forward

- Pass min forward

## Alternative/additional

http://rgrig.blogspot.com/2009/10/dtfloatleftclearleft-summary-of-some.html

Instead of "CFG is a DAG", if it's a reducible flow graph: results from
structured programming. `for` or `while`; only excludes gotos.

(Non-Reduceable: strongly connected subgraph with more than one entry)

Loop is a strongly connected subgraph: _path_ (not edge) from each node to each other node

Can we do a heuristic on the entry point-
what the induction variable of the loop is, and run induction on it?

Do we (initially) defer on indirect calls?

## Call graph analysis

https://llvm.org/doxygen/classllvm_1_1LazyCallGraph.html
and 
https://llvm.org/doxygen/classllvm_1_1LazyCallGraph_1_1SCC.html

https://eli.thegreenplace.net/2013/09/16/analyzing-function-cfgs-with-llvm


Inline of a different member - invoke vs. call?

[Terminator instructions](https://llvm.org/docs/LangRef.html#terminator-instructions):
Because the calee may go to the normal branch or the exception branch

How are exceptions thrown? By `__cxa_throw`... after callocating an exception structure:
- C++ information - thrown object, type info (because the catcher can pattern-match on the type info),
  and libunwind info

catch blocks match typeid to either catch, or keep propagating up.

What is in cxa_throw?

We say "IDK" for `invoke`. Just look at call; not tail calls.

## On attributes/annotations

Can we overload `mustprogress` or `mustreturn` to mean what we want?

- These are LLVM attributes.
- Can they be applied in a header, so they show up for external functions even when LLVM isn't privy to the definition?
    - `noreturn` is a Clang attribute.
    - `pure` can be applied manually (though that doesn't imply "terminates"...)
    - `annotate` is Clang attribute...?

    - [ ] AI: cceckman to look through clang attributes

We treat "noreturn" as "treat this branch as no".
We treat "mustreturn" and/or "mustprogress" as "yes".

"Color" each path through the function as "yes, maybe, no". Join along paths.

Color an SCC locally, based on vectorizeablity? And/or loop annotations (i.e. llvm.loop.mustprogress?)
And then color the DAG of SCCs

- Color blocks based on structure and calls
- Color SCCs based on
    1. Color of blocks + cycle: if present, IDK (conservative)
    2. Inductive analysis (not conservative; better analysis); mustprogress
- Walk DAG and join branches
- Apply to function as a whole


## Custom attributes

BPF wanted to use annotate, stymied by not having documentation: https://groups.google.com/g/llvm-dev/c/CqpXrMUMiR8/m/3hAsOn-cBQAJ
Pointed to https://blog.quarkslab.com/implementing-a-custom-directive-handler-in-clang.html

...but that doesn't seem to map to the LLVM IR annotation concept?

Ah! unless you run https://llvm.org/doxygen/structllvm_1_1Annotation2MetadataPass.html perhaps?

## On inlining

Clang/LLVM really likes to inline within a module - at least for the stuff we've tried.
This means two things:

1.  It's important that we be able to run analysis on _regions of functions_, e.g. between
    call and destructor.

    - [x] AI: how does a RAII object look?

2.  We shouldn't expect a lot of information about calls - only the external signature.
    We probably can't _analyze_ the call target unless we're doing an LTO pass.
    And even then we can't necessarily analyze the call target (if it's dynamically linked).

    That said: if the call target is dynamically linked, that's going to make the execution
    time highly variable too. So probably out of scope to call dynamically-linked functions
    in a critical section - we don't need to correctly analyze that result,
    just return "IDK".

    - [ ] AI: Can we do an LTO pass?


## Monotone

Our computation is monotone (https://www.cse.psu.edu/~gxt29/teaching/cse597s21/slides/08monotoneFramework.pdf)
Worklist; on update, add successors to worklist/queue
Until worklist is empty


Provenance:
-   When we generate _de novo_ a ruling - "I doubt this will complete" or "this will not complete"-
    Capture that source, i.e. the instruction / block that gave us that determination.
    What call was it? What loop condition was it?
-   When we join, include an back-reference to the most-conservative version;
    "This if/else may not complete... because the if branch has this for loop...
    which is not canonical."

This might be annotations? Or might be something else?


## Loop handling

We want to call loop.getBounds - but that requires a ScalarEvolution object. What's that?

https://www.npopov.com/2023/10/03/LLVM-Scalar-evolution.html
https://llvm.org/devmtg/2018-04/slides/Absar-ScalarEvolution.pdf

"symbolic technique" - OK, this is doing the KLEE thing for us!

https://llvm.org/doxygen/ScalarEvolution_8cpp_source.html

TODO: This shows how to have CLI options- `static cl:opt<bool>`, something we could use for targeting?

Scalar evolution pass is a _function_ pass, which returns a ScalarEvolution...
and that's what we need. So we have to run the pass to get the result.

ScalarEvolution is a legacy pass, though. Can we "just" get results from the old pass manager??



Also of interest, Loop strength reduction: https://www.cs.cornell.edu/courses/cs6120/2019fa/blog/strength-reduction-pass-in-llvm/


## Call handling

- [1/2] Intramodule
      - Does this require analyzing at the "module" scope,
        and automatically marking anything where there's a CGSCC as "unknown"?
        What happens if we try it with mutually recursive calls?
        (Does LLVM blow up?)

        1. Write as a module pass
        2. Look at CGSCC; tag anything in a CGSCC as "unknown"
        3. Analyze remaining functions

        Currently: handling via just recursing to FAM.
        Need GCSCC to avoid mutual recursion

- [ ] LLVM intrinsics - some of these can / should be assumed to be safe
- [ ] Intermodule:
      - Can we annotate calls, or headers, with "assume this"?
      - ...and check intraprocedurally, assume extraprocedurally?


## Future: symbolic execution

At some point, break out KLEE...
