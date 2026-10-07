# Self-Checking ELF Smoke Test

[Compiled workload catalogue](index.md)

## Purpose

Check the external ELF boundary with an assembly-authored verdict independent of the simulator's instruction oracle.

## Setup

| Property | Value |
| --- | --- |
| Image | `tests/fixtures/rv32i/external-smoke/self-check.elf` |
| Source | `self-check.S`, alongside linker/readelf/checksum files |
| Entry / data | Entry zero; scratch word 0x1000 |
| Completion | tohost at 0x3ffc0: 1 pass, 3 fail |

## Implementation

1. Add 7 and 5 and compare the result with 12.
2. Store that value at 0x1000, load it back, and compare.
3. Call a subroutine with JAL; increment the result and return with JALR.
4. Require 13 after the return.
5. Store the pass/fail verdict, then loop forever.

The infinite loop is deliberate: the runner observes the committed verdict store rather than requiring EBREAK.

## Expected result

The scratch word remains 12 and the returned arithmetic result is 13. Success writes 1 to tohost; any failed branch comparison writes 3. No ABI stack is needed by this tiny assembly image.

## Running and verification

```sh
build/rv32i_experiment --core single \
  --elf tests/fixtures/rv32i/external-smoke/self-check.elf \
  --period 4000 --allocation-limit-mib 512 --max-cycles 1024
```

The source-level self-check and the runner's oracle checks are separate evidence. The fixture validates a few instructions and the ELF protocol, not the full ISA or a large compiled application.

## Source and tests

`RV32IElfLoaderTest` checks loader acceptance/rejection. `RV32IExternalValidationSmokeTest` exercises this image through its self-check path. `BenchmarkCampaignCLITest` checks an ELF-directory campaign and common artifacts. See [test details](../verification/workloads.md).
