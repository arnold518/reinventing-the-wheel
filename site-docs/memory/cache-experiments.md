# § 3.8. Cache Experiments

What does a cache do for memory accesses on its own, and what does a second
level add? A test driver reads a fixed stream of addresses, one request at a
time, with no processor, using the real gate-level caches over DRAM main
memory in both realistic [timing profiles](../timing.md#timing-profiles).
How caches serve a processor, in five arrangements and four cache behaviors,
is in [§ 4.8](../rv32i/experiments.md#memory-systems).

All results are **clock cycles** at one fixed clock, so fewer cycles means
faster. The experiment was fixed before any run, and no memory timing or
cache setting was changed after seeing results.

## In short

- **A cache pays off only when the data is reused or nearby.** Reading
  consecutive words through a cache costs about 5 cycles per read against 8
  from DRAM; reading one word from each cache line costs 14, because every
  miss fetches a whole line to use one word.
- **A second level adds a lookup to every miss.** Behind an L1, an L2 hit
  costs as much as reading an open DRAM row, and a read stream that misses
  the L1 rarely finds its line in the L2.

## The setup

| Part | Configuration |
| --- | --- |
| First-level cache (L1) | 8 sets × 2 ways × 16-byte lines, 256 B |
| Second-level cache (L2) | 64 sets × 2 ways × 16-byte lines, 2 KiB |
| Cache stores | SRAM tag and data stores |
| Cache behavior | A miss fills the whole line in one burst; one request at a time |
| Main memory | One DRAM of 65,536 words with the datasheet's row size (512 words in `embedded`, 1,024 in `desktop`) and refresh |
| Clock | 4,104 ps in `embedded`, 2,858 ps in `desktop` |
| Checks | Every read returned the expected word |

**Latency of one read**, in cycles:

| Read | `embedded` | `desktop` |
| --- | ---: | ---: |
| L1 hit | 2 | 2 |
| L2 hit after an L1 miss | 8 | 8 |
| DRAM, row open | 8 | 8 |
| DRAM, row closed | 14 | 15 |
| DRAM, other row open | 20 | 22 |
| L1 miss to DRAM, row open / closed | 14 / 20 | 14 / 21 |
| L1 and L2 miss to DRAM, row open / closed | 17 / 23 | 17 / 24 |

Behind an L1, an L2 hit costs as much as reading an open DRAM row, and a
miss adds the caches' own lookups to the DRAM access. Whether a cache helps therefore
depends on how often it hits. [3.6 Memory Timing](timing.md#measured-latency)
gives the memories alone.

## Access patterns

A test driver issues 2,048 reads, one at a time, in one of three patterns,
along one of three paths:

- **S0**: straight to DRAM;
- **S1**: through one L1;
- **S2**: through an L1 and the L2.

**Cycles per read** (`embedded` / `desktop`):

| Access pattern | S0 DRAM | S1 L1 | S2 L1 + L2 |
| --- | ---: | ---: | ---: |
| Sequential | 8.05 / 8.03 | 5.08 / 5.26 | 5.83 / 6.07 |
| One word per cache line | 8.12 / 8.08 | 14.29 / 14.79 | 17.32 / 17.97 |
| One word per DRAM row | 19.93 / 21.94 | 26.20 / 29.31 | 29.19 / 32.46 |

<!-- diagram: cache-access-patterns -->

What it shows:

- **Sequential reads** are the case caches are made for: one miss brings in
  four words, and the next three reads hit. An L1 makes them about 1.6× faster
  than DRAM.
- **One word per cache line** reuses nothing: every read misses and fetches
  four words to use one. The cache makes these reads 1.8× slower than reading
  DRAM directly.
- **One word per DRAM row** is the worst pattern with or without a cache,
  because every read has to close one DRAM row and open another.
- **The L2 only adds cost here**: S2 is slower than S1 in every pattern. A
  read the L1 misses rarely finds its line in the L2 either, and each miss
  passes one more lookup on its way to DRAM.

## What these results do not show

- **Reads only, one at a time.** The driver waits for each read before the
  next; writes and their cache behaviors are measured with programs in
  [§ 4.8](../rv32i/experiments.md#memory-systems).
- **One kind of main memory.** The DRAM keeps one row open at a time; a DRAM
  with several banks would change the cost of row changes.
- **These inputs only.** Three read patterns at one clock per profile; other
  patterns and sizes need their own runs.
