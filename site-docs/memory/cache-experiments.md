# § 3.8. Cache Experiments

Do the [caches](caches.md) make memory faster, and which arrangement of
caches helps most? Two experiments answer this with the real gate-level caches
over DRAM main memory, in both realistic [timing profiles](../timing.md#timing-profiles):

1. [**Access patterns**](#experiment-1-access-patterns): a fixed stream of
   reads, with no processor, read directly from DRAM, through one cache, or
   through two levels of caches.
2. [**Programs over five memory arrangements**](#experiment-2-programs-over-five-memory-arrangements):
   four small programs whose instruction fetches and data accesses go through
   five different connections to the same DRAM.

All results are **clock cycles** at one fixed clock, so fewer cycles means
faster. Both experiments were fixed before any run, and no memory timing or
cache setting was changed after seeing results.

## In short

- **A cache pays off only when the data is reused or nearby.** Reading
  consecutive words through a cache costs about 5 cycles per read against 8
  from DRAM; reading one word from each cache line costs 14, because every
  miss fetches a whole line to use one word.
- **Sharing one DRAM between instruction fetches and data accesses costs
  1.4–2.2×**, because the two keep closing each other's open DRAM row.
- **Two small first-level caches make programs 3–4× faster** than the
  shared, uncached memory.
- **A second-level cache under them made three of four programs slightly
  slower (4–8 %)**, because every store is written through both levels while
  the program waits. This is a property of the simple caches built here, not
  of caches in general.

## The setup

| Part | Configuration |
| --- | --- |
| First-level caches (L1) | One for instructions, one for data: 8 sets × 2 ways × 16-byte lines, 256 B each |
| Second-level cache (L2) | Shared: 64 sets × 2 ways × 16-byte lines, 2 KiB |
| Cache stores | SRAM tag and data stores |
| Cache behavior | A miss fills the whole line in one burst; data caches **write through** and do not fetch a line on a write miss; one request at a time |
| Main memory | One DRAM of 65,536 words with the datasheet's row size (512 words in `embedded`, 1,024 in `desktop`) and refresh |
| Clock | 4,104 ps in `embedded`, 2,858 ps in `desktop` for every arrangement; the clock analysis certifies each one at that clock |
| Checks | Every run produced the expected results and final memory, with zero setup violations |

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

## Experiment 1: access patterns

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

## Experiment 2: programs over five memory arrangements

The same DRAM is connected in five ways to a processor's instruction port and
data port (the advanced pipeline of [§ 4.6](../rv32i/4.6/index.md); here it
only serves as a realistic source of requests):

<!-- diagram: cache-topologies -->

| Arrangement | Connection |
| --- | --- |
| A | Separate memories: each port has its own DRAM |
| B | Shared, uncached: both ports share one DRAM through an arbiter |
| C-ID | B with an instruction L1 and a data L1 above the arbiter |
| D-0 | B with a shared L2 below the arbiter, no L1 |
| D-ID | Both: the two L1s above the arbiter and the L2 below it |

The four programs differ in how they use memory:

| Program | What it does with memory |
| --- | --- |
| `calls` | Nested function calls: small code, return addresses saved on and restored from the stack (size 16, 256 measured iterations) |
| `sort` | Sorts 64 words: a 512-byte working set, larger than an L1 and smaller than the L2 (4 iterations) |
| `stream` | Walks 1,024 words once per iteration: larger than the L2 (4 iterations) |
| `pointer-chase` | Follows 512 nodes (4 KiB) in random order: each load depends on the previous one (4 iterations) |

Each measurement starts after one warm-up iteration.

**Where code and data sit.** With separate memories (A) each port's DRAM
holds the whole program image; in the shared arrangements the one DRAM holds
it once. Every block keeps its own address, so code and data share one
address space at the bottom of the 256 KiB memory:

| Bytes | Contents |
| --- | --- |
| from `0x0000` | Program code (all four programs fit well below `0x1000`) |
| `0x1000` | Input data or pointer-chase nodes (up to 4 KiB) |
| `0x2000` | Output data |
| `0x3000` | Result word |
| below `0x4000` | Stack, growing down (`calls`) |

A workload whose code and data would put different bytes at one address is
refused in every arrangement rather than run as a different program.

**Speed against the shared, uncached arrangement B** (higher is faster;
`embedded`, with `desktop` in brackets):

| Program | A | C-ID | D-0 | D-ID |
| --- | ---: | ---: | ---: | ---: |
| `calls` | 1.51× (1.57×) | **3.58×** (3.63×) | 3.46× (3.55×) | 3.39× (3.48×) |
| `sort` | 1.99× (2.08×) | **3.93×** (4.00×) | 3.71× (3.79×) | 3.65× (3.74×) |
| `stream` | 1.38× (1.40×) | **3.06×** (3.07×) | 2.91× (2.94×) | 2.82× (2.85×) |
| `pointer-chase` | 1.85× (2.19×) | 3.02× (3.79×) | **3.27×** (**3.95×**) | 3.14× (3.77×) |

<!-- diagram: cache-speedup -->

What it shows:

1. **Sharing one DRAM costs 1.4–2.2× (A against B).** The two ports rarely
   ask in the same cycle; the cost is DRAM rows. In `sort` (`embedded`), A's
   two memories open 796 rows and B's shared DRAM 18,377, because instruction
   fetches and data accesses interleave on one device. At about twelve cycles
   per row change, that accounts for most of B's extra cycles.
2. **The two L1s give 3–4× (C-ID).** The program code fits in the
   instruction L1, and the data L1 holds part of each working set.
3. **The L2 alone is almost as good (D-0),** because at this clock a hit in
   the larger L2 costs about as much as a hit in an L1.
4. **Both levels are 4–8 % slower than the L1s alone (D-ID against C-ID)** in
   `calls`, `sort` and `stream`. Every store goes through both levels to DRAM
   and the program waits for it; serving the L1 misses from the L2 saves less
   than that costs.
5. **Random access favors the bigger cache.** In `pointer-chase` the 256-byte
   L1 rarely holds a node and every miss fetches a whole line, so the L2 alone
   (D-0) is fastest. In `desktop` the whole 4 KiB array fits in one DRAM row,
   which helps every arrangement with a DRAM behind it.

??? info "Cycles and cycles per instruction of every run"

    Cycles in the measured window, cycles per instruction (CPI) and the speed
    against B.

    **calls**

    | Arrangement | `embedded` cycles | CPI | vs B | `desktop` cycles | CPI | vs B |
    | --- | ---: | ---: | ---: | ---: | ---: | ---: |
    | A | 62,529 | 12.21 | 1.51× | 65,944 | 12.88 | 1.57× |
    | B | 94,309 | 18.42 | 1.00× | 103,331 | 20.18 | 1.00× |
    | C-ID | 26,321 | 5.14 | 3.58× | 28,499 | 5.57 | 3.63× |
    | D-0 | 27,296 | 5.33 | 3.46× | 29,144 | 5.69 | 3.55× |
    | D-ID | 27,820 | 5.43 | 3.39× | 29,665 | 5.79 | 3.48× |

    **sort**

    | Arrangement | `embedded` cycles | CPI | vs B | `desktop` cycles | CPI | vs B |
    | --- | ---: | ---: | ---: | ---: | ---: | ---: |
    | A | 281,202 | 10.14 | 1.99× | 302,048 | 10.89 | 2.08× |
    | B | 559,942 | 20.19 | 1.00× | 628,588 | 22.67 | 1.00× |
    | C-ID | 142,446 | 5.14 | 3.93× | 157,293 | 5.67 | 4.00× |
    | D-0 | 150,892 | 5.44 | 3.71× | 165,890 | 5.98 | 3.79× |
    | D-ID | 153,511 | 5.54 | 3.65× | 168,220 | 6.07 | 3.74× |

    **stream**

    | Arrangement | `embedded` cycles | CPI | vs B | `desktop` cycles | CPI | vs B |
    | --- | ---: | ---: | ---: | ---: | ---: | ---: |
    | A | 410,643 | 14.31 | 1.38× | 451,632 | 15.73 | 1.40× |
    | B | 567,601 | 19.77 | 1.00× | 632,355 | 22.03 | 1.00× |
    | C-ID | 185,577 | 6.47 | 3.06× | 205,977 | 7.18 | 3.07× |
    | D-0 | 194,892 | 6.79 | 2.91× | 215,278 | 7.50 | 2.94× |
    | D-ID | 201,013 | 7.00 | 2.82× | 222,082 | 7.74 | 2.85× |

    **pointer-chase**

    | Arrangement | `embedded` cycles | CPI | vs B | `desktop` cycles | CPI | vs B |
    | --- | ---: | ---: | ---: | ---: | ---: | ---: |
    | A | 134,203 | 13.06 | 1.85× | 125,641 | 12.23 | 2.19× |
    | B | 248,769 | 24.22 | 1.00× | 275,375 | 26.81 | 1.00× |
    | C-ID | 82,295 | 8.01 | 3.02× | 72,584 | 7.07 | 3.79× |
    | D-0 | 75,971 | 7.40 | 3.27× | 69,702 | 6.79 | 3.95× |
    | D-ID | 79,289 | 7.72 | 3.14× | 73,041 | 7.11 | 3.77× |

## What these results do not show

- **One cache design.** The caches write through, wait for every store,
  serve one request at a time and have at most two ways. A cache that writes
  back changed lines later, a write buffer, or more requests in flight would
  be new designs with their own results; the L2 penalty in particular comes
  from writing through two levels.
- **One kind of main memory.** The DRAM keeps one row open at a time; a DRAM
  with several banks would change the cost of row changes.
- **These inputs only.** Four small programs and three read patterns at one
  clock per profile; other sizes and programs need their own runs.
