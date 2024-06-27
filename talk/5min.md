# Solving the Maybe-Halting problem

...in 5 minutes.

---

## Motivation

Charles used to work on embedded software.

- **Interrupt-service routines**: Stop regular processing while running.
- **Preemption-disabled regions**: Disable interrupts while running.
- **Mutexes**: Only one can run using this at a time.

As a design principle, we always want these to run in **a bounded amount of time**.

**Can we check it automatically?**

---

## Bounding boundedness

---

### The Halting Problem

*Isn't that the halting problem?*

<!-- ...is what my coworkers said. Quick refresher: -->

> Is it possible to write a program `A` that,  
> 
> given any program `B` and input `I`,
>
> reports whether or not `B(I)` terminates?

Answer:

<!-- Alan Turing's big thesis. Conclusively established: -->

> **No, you cannot write such a program.**

<!-- ...but we're not actually trying to solve that problem. -->

Rice's theorem generalizes this to other properties.

---

### The Maybe Halting Problem

<!-- We just want something that can _sometimes_ give an answer. -->

> Is it possible to write a program `A` that,
>
> given any program `B` and input `I`,
>
> reports one of:
>
> - `B(I)` terminates
> - `B(I)` does not terminate
> - `A` cannot determine whether `B(I)` terminates

Can we do that?


> **Yes.**

```python
def analyze(b, i):
  return "I don't know"
```

<!--
---

### Borrow checker

Rust's borrow-checker says one of

1.  "I have proven this is safe"
2.  "I have not proven this is safe"

In (2), address the problem by

1. **Change code** into provably-safe subset
2. **Escape** into unsafe (axiomatize)
3. **Make a better prover** add non-lexical lifetimes, Polonius, etc.

We can do (3)!
-->

---

## It works!

```c
volatile int x = 0;

int bounded_loop() {
    for(int i = 0; i < 10000; i++) {
        x += i;
    }
    return x;
}

int unbounded_loop() {
    while(1) {
        x += 1;
    }
    return x;
}

int main() {
    int x;
    [[clang::noinline]] x = bounded_loop();
    [[clang::noinline]] x = unbounded_loop();
    x += 1;
    return x;
}
```

---

## It works!

```
Function name: bounded_loop
Result: Bounded
Explanation: includes a loop, but it has a fixed bound

Function name: unbounded_loop
Result: Unknown
Explanation: includes loop with indeterminate bounds

Function name: main
Result: Unknown
Explanation: via call to unbounded_loop: includes loop with indeterminate bounds
```

---

## Sarah Connor

LLVM analysis pass (C, C++, Rust, Zig if you want)

If:

- The call graph terminates
- Each function terminates
- Each instruction terminates

The program terminates!

---

### Call graph (good)

```c
int add(int a, int b) {
  return a + b;
}

int mult(int a, int b) {
  int x = 0;
  for(int i = 0; i < a; i++) {
    x = add(x, b);
  }
  return x;
}
```

![Call graph of the above code](mult.svg)

---

### Call graph (bad: recursion)

```c
int add(int a, int b) {
  return a + b;
}

int fib(int n) {
  return add(fib(n-1), fib(n-2));
}
```

![](fib.svg)

**"I don't know"**

<!-- Diagram?

If the call graph is a DAG, and each function completes, eventually the program completes.

If there's a loop, there's recursion. Completes if recursion is bounded.

We haven't implemented any reasoning about whether recursion is bounded,
so we classify this as "I don't know."

(Also conservative for embedded b/c of stack size; recursion needs to be bounded for other reasons.)

--->

---

### Control flow

```c
int mult(int a, int b) {
  int x = 0;
  for(int i = 0; i < a; i++) {
    x = add(x, b);
  }
  return x;
}
```

![](mult-cfg2.svg)

**Is the loop bounded?**

"I don't know"

---

### Control flow (better)

```c
int mult(int a, int b) {
  int x = 0;
  for(int i = 0; i < a; i++) {
    x = add(x, b);
  }
  return x;
}
```

![](mult-cfg2.svg)

**Is the loop bounded?**

Use LLVM analysis!

"Loop is bounded" --> "Function terminates"

<!-- 

There's a decent amount of literature on loop unrolling and/or bounding.
It winds up being an important optimization that compilers can perform.

LLVM has built-in analysis passes; we can request their results.
Including "is this loop bounded"!

LLVM's analysis is, like ours, conservative; it sometimes can't find a bound
when one in principle exists. That's OK; we treat "unbounded loop" as
"I don't know".

-->

---

### Instructions terminate

**Assumed.**

Not actually true in an embedded context!

But out of scope of this checker.

### It doesn't work!

- Escape hatches (own code, LLVM intrinsics)
  - We couldn't figure out the LLVM infrastructure for this
- LLVM result invalidation (bookkeeping)

???

Some stuff that's necessary for it to be really usable, but we haven't worked out.

---

### It could be better!

- Cross-module analysis
- Recursion
- Indirect calls
- Test it on Rust (in principle "just works"?)

???

A bunch of ways that it could be better.


