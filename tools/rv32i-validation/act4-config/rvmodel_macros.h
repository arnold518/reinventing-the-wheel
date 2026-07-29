#ifndef _RVMODEL_MACROS_H
#define _RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION                                  \
  .pushsection .tohost,"aw",@progbits;                        \
  .balign 8; .global tohost; tohost: .dword 0;                \
  .balign 8; .global fromhost; fromhost: .dword 0;            \
  .popsection

/*
 * CircuitSim currently validates the unprivileged I instruction group.
 * Avoid the framework's standard privileged boot/cleanup path.
 */
#define RVMODEL_BOOT
#define RVMODEL_BOOT_TO_MMODE

/* ACT4/HTIF convention: low word 1 passes, low word 3 fails. */
#define RVMODEL_HALT_PASS                                     \
  li x1, 1;                                                   \
  la t0, tohost;                                              \
write_tohost_pass:                                            \
  sw x1, 0(t0);                                               \
  sw x0, 4(t0);                                               \
  j write_tohost_pass;

#define RVMODEL_HALT_FAIL                                     \
  li x1, 3;                                                   \
  la t0, tohost;                                              \
write_tohost_fail:                                            \
  sw x1, 0(t0);                                               \
  sw x0, 4(t0);                                               \
  j write_tohost_fail;

#define RVMODEL_IO_INIT(_R1, _R2, _R3)
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR)

/*
 * ACT4 requires every platform to declare its interrupt hooks even when the
 * selected extension set contains no interrupt tests. CircuitSim has no
 * interrupt controller, so these hooks deliberately assemble to nothing.
 */
#define RVMODEL_INTERRUPT_LATENCY 10
#define RVMODEL_TIMER_INT_SOON_DELAY 100
#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)
#define RVMODEL_SET_MSW_INT(_R1, _R2)
#define RVMODEL_CLR_MSW_INT(_R1, _R2)
#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif
