/* Reduced-work equivalence entry point, NOT a full Embench timing run.
 * Include an unchanged, pinned upstream translation unit so its private
 * benchmark_body is accessible without patching or duplicating the algorithm.
 * The original data set and verifier remain intact.
 */
#ifndef RITW_EMBENCH_SOURCE
#error "Select a source from embench-middle.json"
#endif
#ifndef RITW_KERNEL_REPETITIONS
#error "Specify a positive kernel repetition count"
#endif
#if RITW_KERNEL_REPETITIONS < 1
#error "The complete kernel must run at least once"
#endif

#include RITW_EMBENCH_SOURCE

int main(void)
{
    initialise_board();
    initialise_benchmark();
    start_trigger();
    int result = benchmark_body(RITW_KERNEL_REPETITIONS);
    stop_trigger();
    /* A missing verifier (-1) must never be accepted as success. */
    return verify_benchmark(result) != 1;
}
