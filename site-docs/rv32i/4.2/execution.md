# § 4.2.2. Execution and Measurement

[Benchmark index](index.md) · [Clock analysis](timing.md) · [Verification](verification/index.md)

## Configuration

| Selection | Contract |
| --- | --- |
| Core | `single`, `baseline`, `advanced` |
| Predictor | Advanced only: `not-taken`, `taken`, `backward-taken`, `one-bit`, `two-bit` |
| Predictor entries | Power of two from 1 to 256; default 16 |
| Workload | Fixed `--program 1..22`, generated `--workload NAME`, or `--elf FILE` |
| Fidelity | `behavioral` or `reference` |
| Memory | Default zero-wait FF; delayed technologies require advanced |
| Clock | Even period at least 4 ticks; candidate `auto` or a separately certified campaign clock |
| Region | Whole program, explicit retirement-PC interval, or generated body interval |
| Limits | Cycles, processed events, allocation limit; campaign adds process timeout |

## Initialization and memory

Each process constructs a fresh core and predictor. Instruction and data endpoints default to 64K × 32 bits each (256 KiB). The runner clears both images to binary zero, provisions the workload, resets the CPU, and lets reset settle before measuring. CPU reset does not erase the image.

The two logical memories do not alias. ELF segments are initially copied into both; a later data store does not patch instruction memory. Fixed fixtures do not assume an ABI stack. Generated calls and compiled startup code explicitly initialize one.

Single-cycle and baseline require zero-wait FF endpoints and cannot simply be attached to a delayed device. Advanced supports memory waits. The main comparison holds memory constant with zero-wait FF and **no caches**. ROM/flash are not substitutes for ordinary writable data RAM.

Simulator, register-file and memory histories are disabled for long headless runs. Functional state and counters remain active; the absence of a timeline must not be interpreted as an empty program.

## Fidelity

| Profile | Expanded detail | Interpretation |
| --- | --- | --- |
| `behavioral` | Compact core and storage implementations | Fast software execution of the core's microarchitecture |
| `reference` | Structural CPU/combinational datapath, compact FF/register/storage abstractions and large FF endpoints | Shared timing reference and affordable structural cross-check |

Reference is **not** a fully expanded gate-level memory/register machine. Both fidelities must use the same clock when compared. A component's manifest records the actual implementation; the profile label alone is insufficient provenance.

## Completion and correctness

| Workload | Completion |
| --- | --- |
| Fixed program | Terminal halt or expected trap; fixture-specific final assertions live in native fixture tests |
| Generated program | Terminal EBREAK, with oracle/final-memory checks and independent kernel expectations where supplied |
| ELF | Successful `tohost=1` verdict; `tohost=3` fails |
| Bounded prefix | Cycle limit reached with `--allow-incomplete`; never a passing complete benchmark |

A pipeline may perform a store in MEM before its instruction retires. ELF completion therefore waits for the verdict store's retirement, not merely an early visible memory value. Halting or trapping before the verdict is failure. The runner steps the oracle once per instruction attempt and compares architectural state; after execution it checks the complete data image.

## Counter definitions

| Field | Definition |
| --- | --- |
| `cycles` | Clock edges in the selected measurement window |
| `whole_program_cycles` | Edges after reset through completion, including work outside that window |
| `instruction_attempts` | Attempts in the window, including terminal halt/trap attempts |
| `retired_instructions` | Attempts excluding terminal halt/trap attempts |
| `terminal_attempts` | Terminal attempts counted separately |
| `cpi`, `ipc` | Cycles per successful retirement and successful retirements per cycle |
| `observations` | Settled pre-edge stalls, flushes, predictor events, stage-valid bits and port waits |
| `scheduled_time_ticks` | Cycles × selected scheduling period |
| `modeled_time_ticks` | Campaign time at its declared analyzed clock basis; absent for arbitrary explicit-period comparisons |
| Host times | Setup/reset, execution, and verification costs reported separately |
| Simulator work | Processed events, evaluations, wire changes, maximum queue depth |
| `peak_rss_kib` | High-water resident process memory, not the allocation cap |

Predictor misses compare the predicted and actual **next-PC addresses**. Occupancy counters concern four interstage registers, not five named stages. Stall observations overlap: their sum is not an additive CPI breakdown.

Visualizer checkpoint tables count attempts and use their own recording clocks. Their times are not the headless campaign's cycle periods, and old attempt-based CPI must not be mixed with successful-retirement CPI.

## Windows and warmup

Whole-program measurement excludes reset but includes pipeline fill and termination. A start/stop-PC region begins just before retirement at the start and ends before retirement at the stop. Missing required markers are errors.

For parameterized kernels, `--measure-body` supplies those markers. `--training-iterations T` adds T real prefix iterations at the same PCs; measurement begins on the next visit. The prefix is executed and verified, and its state is retained. Expected final results include both prefix and measured iterations.

Host warmups are separate subprocesses with fresh CPUs: they do **not** warm the next run's predictor. Upstream Embench's `warm_caches` is a third, in-program notion of warmup. With FF-only memory it does not demonstrate any simulated cache.

## Bounded cycle traces

`--trace-cycles --max-cycles N` accepts N ≤ 4096. It records every cycle, including bubbles, rather than only the measured region. Each entry contains pre-edge active ports/control/stage PCs and post-edge state for an attempted retirement. `retire.valid` includes terminal attempts. Unknown-valued active pins fail tracing; inactive payload and unselected store bytes are excluded.

Trace collection adds no simulated events but has host overhead. Disable it for simulator-speed measurements. [Cycle equivalence](verification/equivalence.md) documents the 46-pair regression and the exact comparison boundary.

## Resource and failure rules

Use serial processes and explicit budgets. The differential suite uses at most 1,024 cycles per run and a 512 MiB address-space cap. The CLI allows allocation caps from 128 to 4096 MiB; this limits virtual address space, not just resident bytes. Neither an event limit, timeout, failed self-check nor interrupted campaign is evidence of successful completion.

Small runs can use `--period auto` for convenient candidate timing. Research campaigns should explicitly select their [clock basis](timing.md) and retain the certificate or candidate report, not copy a period from a historical result table.
