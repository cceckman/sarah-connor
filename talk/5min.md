# Solving the Maybe-Halting problem

...in 5 minutes.

## Motivation

Charles used to work on embedded software.

- **Interrupt-service routines**: Stop regular processing while running.
- **Preemption-disabled regions**: Disable interrupts while running.
- **Mutexes**: Only one can run using this at a time.

As a design principle, we always want these to run in **a bounded amount of time**.

Charles' team enforced that via code review. **Can we check it automatically?**

## Bounding boundedness

### The Halting Problem

*Isn't that the halting problem?*

<!-- ...is what my coworkers said. Quick refresher: -->

> Is it possible to write a program `A` that, given any program `B` and input `I`,
>
> reports whether or not `B(I)` terminates?

Answer:

<!-- Alan Turing's big thesis. Conclusively established: -->

> **No, you cannot write such a program.**

<!-- ...but we're not actually trying to solve that problem. -->

Rice's theorem generalizes this to other properties.

### The Maybe-Halting Problem

<!-- We just want something that can _sometimes_ give an answer. -->

> Is it possible to write a program `A` that, given any program `B` and input `I`,
> reports one of:
>
> - `B(I)` terminates
> - `B(I)` does not terminate
> - `A` cannot determine whether `B(I)` terminates

Answer:

> **Yes.**
>
> ```python
> def analyze(b, i):
>   return "I don't know"
> ```

<!-- OK, but can we get anything other than a trivial IDK? Yes, for some programs. -->

### Program analysis: useful, not universal

<!--
Program analysis can be useful without being universal.
Even if you have an analysis that is precise for only some programs,
that's still useful for those programs.
-->

- Accuracy: gives true answers
  - "This checker can't check" is definitionally true :)
- Precision: gives the most specific answer possible

Accuracy is _required_, precision is _useful_.

<!-- Even an imprecise answer is actionable. -->

### e.g. `borrowck`

Rust's borrow-checker says either:

1.  "I have proven this is safe"
2.  "I have not proven this is safe"

Some safe programs hit (2). Either:

- **Change code** into provably-safe subset
- **Escape** into `unsafe` (axiomatize)
- **Make a better prover**: add non-lexical lifetimes, Polonius, etc.

<!-- We can do better than IDK for our prover too! -->

## Sarah Connor

LLVM analysis pass (C, C++, Rust, ...)

### Call graph loops

<!-- Diagram?

If the call graph is a DAG, and each function completes, eventually the program completes.

If there's a loop, there's recursion. Completes if recursion is bounded.

We haven't implemented any reasoning about whether recursion is bounded,
so we classify this as "I don't know."

(Also conservative for embedded b/c of stack size; recursion needs to be bounded for other reasons.)

--->

### Control flow loops

<!-- Diagram?

If the graph of basic blocks is a DAG, and each instruction completes,
eventually the function completes.

If there's a loop, it _may not_ complete.

Conservative approach would be: if there's a loop, we say "I don't know."

-->

### Control flow loops, but better

<!-- 

There's a decent amount of literature on loop unrolling and/or bounding.
It winds up being an important optimization that compilers can perform.

LLVM has built-in analysis passes; we can request their results.
Including "is this loop bounded"!

LLVM's analysis is, like ours, conservative; it sometimes can't find a bound
when one in principle exists. That's OK; we treat "unbounded loop" as
"I don't know".

-->

### Instructions terminate

Not true in an embedded context (!)

But out of scope of this checker.

## Results

If:

- The function call graph is acyclic, and
- Each function's control flow graph is either
  - acyclic, or
  - cyclic with computible bounds on the cycle, and
- Each instruction terminates

the program terminates.



### It works!

<!-- TODO: Show some code & results -->

### It doesn't work!

<!-- Some stuff that's kinda necessary, but we haven't worked out: -->

- Escape hatches (own code, LLVM intrinsics)
  - We couldn't figure out the LLVM infrastructure for this
- Result invalidation (an LLVM concept)
  - Time is precious :)

### It could be better!

<!-- Some enhancements that would make it more useful: -->

- Cross-module analysis
- Recursion
- Indirect calls
- Test it on Rust (in principle "just works"?)

