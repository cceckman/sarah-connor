
Notes on debugging the issue from 06c96c44:

>   Bisect issue with function-level termination

>   The same pass (`FunctionBoundedTermination`) completes as expected when
>   invoked from a function-level transform pass
>   (`print<function-bounded-termination>`), but not when invoked from a
>   module-level transform pass `print<bounded-termination>`

>   Is this something weird with the proxy?

FunctionTerminationPass gets ScalarEvolutionAnalysis and LoopAnalysis.

On call_to_bounded_.loops, using print<bounded-termination> (module-level analysis), ScalarEvolutionAnalysis completes.
LoopAnalysis times out? No, it doesn't...

not for "the first function", whichever that is. `get_value`, looks like.
`simple.c` does not have any loops!

On `main`... Seems like ScalarEvolutionAnalysis is the problem?
No; both analyses complete... `getLoopFor`?

Also completes for `main`.

But! apparently we also run for `malloc` in `simple.loops`...
and that's where the problem comes up. ScalarEvolutionAnalysis on malloc
runs dominator-tree analysis, which gets stuck:

```
(lldb) expr F.getName()
(llvm::StringRef) $9 = (Data = "malloc", Length = 6)
(lldb) expr &F.BasicBlocks.Sentinel
(llvm::ilist_sentinel<llvm::ilist_detail::node_options<llvm::BasicBlock, false, false, void> > *) $10 = 0x000055555563d0a0
(lldb) expr F.BasicBlocks.Sentinel
(llvm::ilist_sentinel<llvm::ilist_detail::node_options<llvm::BasicBlock, false, false, void> >) $11 = {
  llvm::ilist_node_impl<llvm::ilist_detail::node_options<llvm::BasicBlock, false, false, void> > = {
    llvm::ilist_detail::node_options<llvm::BasicBlock, false, false, void>::node_base_type = {
      Prev = 0x000055555563d0a0
      Next = 0x000055555563d0a0
    }
  }
}
```

Which looks like "an empty body" (sentinel value of a linked list, pointing at itself).

Can we detect which functions are externally-linked, and avoid analyzing them?
Or should we avoid analyzing things that are "bodyless"?

Easy version: `if(F.empty()) { unknown }`


