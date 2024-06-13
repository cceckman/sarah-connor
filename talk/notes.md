

- Motivation: Critical section termination to allow ISRs to run
- How to check that it's bounded? Code review, empirical, program analysis,
    
  "write it that way", in e.g. Coq

  What are we analyzing? "termination", Strong notion of progress; ignores e.g. volatile operations
  Critical Section terminates

- Isn't this this halting problem?

  Explain halting problem; and "maybe-halting problem"

  Trivial proof that you can do this

  https://en.wikipedia.org/wiki/Rice%27s_theorem

  useful not universal

- borrowck: example conservative though accurate, and here's how you get around non-universality:
  - escape and assume (`unsafe`)
  - restructure subject (rewrite your code)
  - enhance the checker (e.g. Polonius, NLL, Miri)

- Non-trivial analysis -- prove three things:

  1. Inter-function termination: the calls in the critical section
  2. Intra-function termination: the basic blocks in each function
  3. Instruction-level termination: the instructions in each basic block

- (1) Calls:
  - If all functions transitively called by the critical section terminate (2),
  - and the call graph is acyclic,
  then the critical section will terminate.

  Show a call graph; flowchart!

  (Long version: talk about SCCs, worklist algorithm.)

- (2) Intra-function termination, version 1:
  - If all instructions in the function terminate (3), and
  - the control-flow graph of the function is acyclic,
  then the critical section will terminate.

  Control flow: "basic blocks", contain instructions.
  Just like the call graph!

- (3) Instructions terminate

  This isn't actually true. But we assume it.
  It's a different _kind_ of bug if you get an incomplete memory operation.

  (footnote on cceckman's experience)

This is accurate, but conservative: it does not allow recursion (1)
or loops (2).

Come back to (2):

- (2) Intra-function termination, version 2:
  - If all instructions in the function terminate (3), and
  - all cycles escape in a bounded amount of iterations,
  then the critical section will terminate.

  Here we have "enhance"!

  Use LLVM loop canonicalization + analysis; LLVM uses these for loop unrolling.
  (Lots of existing research. We just built on it.)


Limitations:

- Calls outside of the module (compilation unit: crate for Rust, .o for C/C++)
    - LLVM can do whole-program analysis, we haven't tried it yet;
      expensive as programs get bigger
- ...including intrinsics, which show up everywhere
    - Solution we want: escape, `unsafe` equivalent:
      "Assume this terminates" or "assume this does not terminate"

- No recursion
  - May be an enhancement, with more research!
- Indirect calls
- Just LLVM IR
- Assumption of instruction progress
- We haven't implemented LLVM analysis invalidation yet, so don't run this with any transform passes!


Bonus: gotchas!

- Clang + LLVM inline very aggressively -- pretty much whenever it can in our test programs.
- Clang and LLVM report a free-form "annotation" attribute, but it doesn't seem to make it from C to the IR.
  - We wanted -- and still want -- to use this for labeling/ escapes.
- Module-level vs. function-level passes.
  - A function-level pass cannot get mutable module-level results. Even if they're cached and still valid.
  - Module-level passes cover external functions (`declare`) as well as local (`define`)...
  - ...and the lopo analysis pass doesn't terminate when given an empty-bodied function. :-D
- Loop analysis only works after loop canonicalization. So we're running on the output of -01.

