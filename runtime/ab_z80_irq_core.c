/* Pinned-core adapter, not a modified upstream copy. This translation unit
 * deliberately exposes its private interrupt sampler separately from stepping.
 * The scheduler can then sample a timed IRQ at the completed instruction's
 * actual clock, including instructions that overrun a requested clock slice.
 * z80_step_no_interrupts already applies EI inhibition; do not sample twice
 * when that function reports an inhibited boundary. */
#include "z80.c"
void ab_z80_core_sample_interrupts(z80 *cpu){process_interrupts(cpu);}
