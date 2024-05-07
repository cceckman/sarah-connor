### Algorithm Sketch

The termination checker analysis pass for LLVM IR programs lives in BoundedTerminationPass.cpp. This is a function-level analysis that works on a couple different levels of the LLVM hierarchy. 

Although it doesn’t fit very cleanly into the lattice-based monotone framework for program analyses, it still uses a 4 element lattice and a worklist-based algorithm. Our lattice has 4 elements that we use to label basic blocks:

- Unevaluated
- Bounded (definitely terminates in a statically bounded amount of time)
- Unbounded (does not terminate in a statically bounded amount of time)
- Unknown (cannot reason about this)

The bottom element of this element is Unevaluated and the top element is Unknown.

We have a Join over the lattice which works as expected, by taking the least upper bound. 

We also have an Update function which Joins the label of a block with those of all of its predecessors. 

Note: We have an asymmetry here, which is that when all the predecessors of a block are Unbounded, then that the result of this block should also be Unbounded.  Presently, we update the block’s label from Bounded to Unbounded if this is the case. If it is Unknown, we let it remain Unknown, even though we think this should also be Unbounded because this update might make the algorithm diverge (this is where we are falling out of the symmetric lattice territory).

 

The algorithm proceeds in several stages:

- It starts with a basic block classifier, which operates over the basic blocks of a function and labels each block with element from the lattice. It does this by looking at each call instruction and calling the function analysis on the called function and Joining with that function’s result.
    - We currently don’t have a very good handle on mutual recursion —> this might blow up the stack
    - We also look at LLVM intrinsics more closely: which of these have any guarantees about bounded behavior?
- Next, we have a loop classifier. This uses LLVM’s ScalarEvolutionAnalysis and LoopInfoAnalysis passes. For each block, we call the loop classifier, which calls ScalarEvolution’s getBounds function. If SE is able to determine the loop’s bounds, then we label that loop as Bounded, otherwise we label it as Unknown.
    - We are repeating some work here because we try and get the loop that a basic block belongs to for each basic block, and then analyze that loop. A loop can contain more than 1 block.
- Thirdly, we run the worklist algorithm over the basic blocks of the function, updating the labels for each block until we reach a fixpoint. This proceeds in the standard way for a dataflow analysis; if the label of a block is updated, then we add the successors of that block back into the worklist.
- Finally, we Join over the labels of all the exiting basic blocks.