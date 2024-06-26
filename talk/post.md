---
title: "Sarah Connor: Solving the maybe-halting problem"
authors:
  - "Charles Eckman <charles@cceckman.com>"
  - "Anvay Grover"
summary: >
    Sarah Connor is an LLVM analysis pass that tries to determine if
    functions terminate -- but presents its findings humbly.

---

In our time at and after [RC], Charles and Anvay worked on an [LLVM] analysis
pass to detect whether functions terminate. As with other program analyses,
[the analysis can't always produce an answer][rice], but we think it's useful
enough to share.

This writeup includes how we got to this problem and what we made: the algorithm,
tools, and limitations.

## Motivated Program Analysis

Prior to [RC], Charles' job included writing embedded software in a custom [RTOS].
The RTOS included preemptive multithreading and hardware-directed interrupts.

In that kind of context, we care a lot about timing. We usually want a guarantee
that interrupts will be serviced promptly, i.e. that any preemption-disabled
region has a bounded length. We also want a guarantee that the
interrupt service routine (ISR) will have a bounded length.[^promptness]
Let's call any of these times that we want to have bounded length a "critical section".

[^promptness]: What we _really_ want is even stronger -- that it has a _small_
    bounded length in real-time terms. That's even harder to reason about,
    requiring information about the instruction, memory, and bus latencies
    of the platform in use. In this analysis, we're stopping at
    "there's a bounded number of instructions."

### Bounding program length

How could we bounds on a critical section? There's some formal
methods and some informal ones.

- **Empiricism:** Informally, we could measure how long each critical section
    takes, and see if it's small enough that we don't care.

- **Code review:** Slightly more formally, we could write our code as normal, and conduct
    code review, convincing ourselves that the critical sections are bounded --
    or if we aren't conviced, rewriting them until we are.

- **Program analysis:** Further towards the formal end, we could conduct
    automated program analysis: write our code, then execute some algorithm that judges it.

- **Correct by construction:** On the very formal end, we could start writing
    our code in a formal setting like [Rocq] or [Lean], prove its steps are bounded,
    and then translate it to the embedded environment.

### The halting problem and the maybe-halting problem

You could _mistake_ the program analysis described above for [the halting problem]:
"is this critical section bounded" is _kinda_ the same as "will this program terminate".

But there's a key difference. The program in the halting problem must produce a binary answer:

> Is there a program G(F) that, for any program F, returns:
> - "F terminates", or
> - "F does not terminate"
> ?

(The well-established answer is "no such program can exist".)

In the real world, we can be more flexible in analyzing programs.
The "maybe-halting problem" asks:

> Is there a program G(F) that, for any program F, returns:
> - "F terminates", or
> - "F does not terminate", or
> - **"G cannot determine whether or not F terminates"**
> ?

The answer to this is "yes" -- trivially:

```python
def g(f):
    return "I don't know if f terminates. Have you tried asking ChatGPT?"
```

### Useful, not universal

This isn't very useful, on its own. But we can always refine it
for _some_ programs `f` -- which makes it useful for those programs.
Program analysis can be _useful_ without being _universal_.

For example -- take Rust's borrow-checker (`borrowck`).
When you compile a Rust program, the borrow-checker will give one of two results:

- "Yes, `borrowck` has determined all borrows are okay."
- "No, `borrowck` has not determined all borrows are okay." (And the program fails to compile.)

When you get a "no" result, that doesn't mean your borrows are incorrect; it just means
that the borrow-checker can't reason about them.
When presented with that result, you have a few options:

- **Rewrite.** Change your code to make it checkable.

  This is Charles' first instinct: "make it compile".

- **Except / Assert.** Assert that your code is correct, but outside of the verifier's knowledge.

  In Rust, you can say `unsafe` to escape the borrow-checker's rules.

- **Extend.** Update the verifier to allow it to reason about more programs.

  Initially, the borrow-checker rejected some valid programs; updates such as
  [non-lexical lifetimes][NLL] and [split borrows][split-borrows] have accepted more
  programs, while still rejecting invalid programs.

Even though the borrow-checker can't correctly analyze every program, it's still _useful_.

## Reasoning about termination

How do we go from `(f) => "I don't know"` to something reasonably useful?

We can try to identify the subset of programs for which we can definitively say,
"yes, this terminates". There's three layers of that.

### Inter-function termination

TODO: Every function terminates, and the call graph is acyclic.
If there's a call cycle, report "unknown": we don't know whether or not the
recursion is bounded.

### Intra-function termination (no loops)

TODO: Every instruction terminates, and the basic block graph is acyclic.
Conservative: loops may be bounded, report Unknown if loops present.

### Intra-function termination (with loops)

TODO: Every instruction terminates, and all loops terminate.
Conservative: we may be wrong about loops.
We currently report "unknown" for any loop for which LLVM cannot determine
a bound. We could be more precise and report "unbounded" if the loop has no
exit edges, i.e. represents an infinite loop.

### Instruction termination

We assume that every instruction will terminate.

This isn't strictly true -- Charles has worked with a hardware peripheral
that stalls a "load" instruction until an external event occurs.
We consider this kind of behavior to be outside the scope of the analyzer;
the containing call could/should be [annotated](#escape).

## Implementation notes

TODO: LLVM pass. Run after loop canonicalization.
Outputs as text (today).

### LLVM analyses

TODO: LLVM module/function/basic-block structure; LLVM call-graph and loop analysis.

### Result promotion

TODO: Not-quite-a-lattice hierarchy for promoting results.

### Worklist (and not)

TODO: Iteration over basic blocks, implications of order and starting points w/rt infinite loops.

## Limitations

**You shouldn't use this today.**

### Known issue: invalidation

We haven't implemented LLVM's analysis-invalidation API,
so this isn't "safe" to run alongside transform passes.

### Cross-module calls, indirect calls, intrinsics

TODO: Doesn't know how to analyze calls across LLVM modules;
could run as whole-program analysis, we haven't tried.

TODO: Indirect or dynamically-linked calls; unknown target "poisons"
indirect calls, intrinsics.

Leads in to:

### Escape / assertion / control {#escape}

TODO: Need have an assertion / escape mechanism to make it useful.
Annotate with "assume this thing".

### Global analysis

TODO: analyzes everything, instead of "just the relevant bits".
Anotation for "analyze me".

## Bonus: gotchas

There's several things that tripped us up during development.
To close out, here's some fun things we learned, _or didn't_.

TODO:

- Clang + LLVM inline very aggressively -- pretty much whenever it can in our test programs.
- Clang and LLVM report a free-form "annotation" attribute, but it doesn't seem to make it from C to the IR.
  - We wanted -- and still want -- to use this for labeling/ escapes.
- Module-level vs. function-level passes.
  - A function-level pass cannot get mutable module-level results. Even if they're cached and still valid.
  - Module-level passes cover external functions (`declare`) as well as local (`define`)...
  - ...and the lopo analysis pass doesn't terminate when given an empty-bodied function. :-D
- Loop analysis only works after loop canonicalization. So we're running on the output of -01.




[RTOS]: 
[LLVM]: 
[rice]: https://en.wikipedia.org/wiki/Rice%27s_theorem
[RC]: https://recurse.com
