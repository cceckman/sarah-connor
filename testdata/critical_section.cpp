#include "critical_section.hpp"

volatile uint64_t dma_address = 0;
volatile uint64_t dma_data = 0;
volatile uint64_t __attribute__((annotate("start-dma"))) dma_go = 0;


[[gnu::nothrow]] [[clang::annotate("run-nonpreempting")]] void run_with_preemption_disabled() {
    CriticalSection c;
    dma_address = 0x400;
    dma_data = 0x800;
    dma_go = 1;
}

int main() {
    run_with_preemption_disabled();
}
