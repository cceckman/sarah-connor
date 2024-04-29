#include <atomic>

static std::atomic<int> preemption_disable_count = 0;

inline void disable_preemption() {
#ifdef ARCH_ARM
    asm("cpsid or whatever that instruction is")
#endif
}

inline void enable_preemption() {
#ifdef ARCH_ARM
    asm("cpsie or whatever that instruction is")
#endif
}

/// Example critical-section that disables preemption.
/// The body of the critical section, between its constructor and destructor,
/// should be
class CriticalSection {
public:
  // Provides metadata https://llvm.org/docs/LangRef.html#annotation-metadata
 [[gnu::nothrow]] __attribute__((annotate("my annotation"))) CriticalSection() {
    if(preemption_disable_count.fetch_add(1, std::memory_order_seq_cst) == 0) {
        disable_preemption();
    }
  }
 [[gnu::nothrow]] [[clang::annotate("critical-section-start")]] ~CriticalSection() {
    if(preemption_disable_count.fetch_sub(1, std::memory_order_seq_cst) == 1) {
        enable_preemption();
    }
  }
};
