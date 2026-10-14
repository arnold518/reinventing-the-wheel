# Benchmark system

The complete published reference is now [§ 4.2. Benchmarking and Verification](../../site-docs/rv32i/4.2/index.md): execution/fidelity contracts, every fixed/generated workload, compiled sources, verification suites, and campaign artifacts. This file remains a compact developer overview.

The benchmark system uses one result envelope for processor, memory and future
accelerator measurements. Domain runners retain their own execution rules.

| Part | Responsibility |
|---|---|
| Workload | Inputs, parameters, expected result and completion rule |
| Target | Core, memory hierarchy or accelerator configuration |
| Runner | Domain-specific driving and validation |
| Run record | Common status, modeled ticks, host time, metrics and fingerprints |
| Campaign | Serial matrix execution, provenance and durable output files |

The RV32I catalog contains the 22 fixed programs, three legacy loops and 12
parameterized workload families. External ELFs use the same preparation and
execution path, with a retired `tohost` verdict as their completion rule.

Memory benchmarks use validated component scenarios and the same result schema.
Their wall-clock measurements describe simulator cost, not physical memory
latency. A later cache can be tested alone with a memory request workload and
inside an RV32I target without changing the common record format.

The common types are deliberately data-only. Instruction retirement remains in
the RV32I runner; request/response completion remains in memory runners; a
future accelerator runner can add operation and utilization metrics without a
generic hardware interpreter.

Use `tools/benchmarks/run.py` for campaigns. `benchmark-runs.jsonl` is the
portable result stream. A `COMPLETE` file means every selected process
finished; its absence means the directory is partial evidence.

## Bounded equivalence checks

Use short, targeted workloads to validate the compact models, and long workloads
to measure their performance. A long behavioral run alone cannot establish
agreement with the structural implementation.

`RV32IDifferentialTest` uses the same RV32I workload loader and runner as the
benchmark campaigns. It adds 46 complete behavioral/reference pairs:

| Cases | Configurations | Pairs |
|---|---|---:|
| Four seeded instruction sequences: ALU dependencies, byte lanes, load-use, x0 and squashed stores/traps | All three cores | 12 |
| Seven existing fault programs: illegal instruction and load/store/instruction alignment or access faults | All three cores | 21 |
| Predictor outcome reversals and table-index aliases | Five prediction policies | 10 |
| Small compiled C program: stack, calls, returns and an independently checked result | All three cores | 3 |

Each pair uses the same binary-startup-certified clock period and zero-wait FF
endpoints. The reference retains the structural datapath but uses compact
register/storage implementations: it is not a fully expanded gate-level machine.
The test compares, in order, every cycle's exposed control signals, valid-stage
PCs, active memory transactions, retirement instruction/PC, architectural register
state after retirement, and halt/trap state. Aggregate counters and final-state
fingerprints must also match. Unused bus payload and unselected store bytes are
excluded because they are not part of the interface contract.
Unknown-valued active pins cause tracing to fail rather than silently becoming
zero. The advanced core must reject misaligned loads and stores before issuing
a request. Single-cycle and baseline cores instead permit an aligned word-read
probe while checking alignment locally; a faulting store must never issue a
write. Equivalence compares fidelities of the **same** core, not bus traffic
between different microarchitectures.

```sh
cmake --build build --target rv32i_experiment SimulationTestRunner -j1
ctest --test-dir build -R '^(RV32IDifferentialTest|RV32IProfileToggleSweepTest)$' \
  --output-on-failure -j1
```

Processes run serially with a 512 MiB address-space cap and 1,024-cycle execution
limit for each differential case. A mismatch names its first cycle and field.
The separate profile-toggle audit covers individual implementation substitutions,
including both compact and expanded-controller `memory.ff.scalable-endpoint.32` endpoints.

This strengthens regression evidence, not a mathematical proof of equivalence.
It does not establish internal gate-waveform equivalence or cover every memory
technology, wait-state configuration, or unknown-valued input. The single-cycle
behavioral model shares the ISA oracle; the structural comparison and independent
C result check therefore matter. Existing campaign `--reference-check` still
compares run signatures, not per-cycle traces; tracing is opt-in and should be
disabled for host-speed measurements.
