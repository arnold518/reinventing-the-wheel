# § 5.1.4. Recorded Programs

Programs **23–27** continue the RV32I program numbering of
[§ 4.2](../../rv32i/4.2/index.md) and run on the
[RV32IM core](core.md) connected to separate instruction and data memory
endpoints, without buses or devices. They appear under **Computer System /
RV32IM programs** in the simulator. They open in the Visual profile: the
core's blocks are structural, while its register file and the memories are
drawn as their register and memory views; the explorer can select compact
implementations for individual components.

Every retiring PC/instruction, architectural register image and terminal
state is checked against the independent RV32IM instruction oracle. Final
memory is compared independently, with directed register and store
expectations. The timeline records reset, arithmetic waiting, every commit
and halt. Select **CORE → DECODE** or its **REGISTER_FILE** in the explorer
to inspect register values at the selected time.
These are bounded verification fixtures, not performance benchmarks.

Programs 23–25 cover instruction and pipeline boundaries. Programs 26–27
run algorithms with loops, arithmetic dependencies and memory results.

## Program 23: All eight M instructions

`rv32im-program23` initializes x1 = −7 and x2 = 3, then executes
MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM and REMU. A final multiply to x0
checks that the zero register stays zero. Inspect signed/unsigned high
products and the repeated execution stalls, then compare register values
at each commit.

<!-- simulator-program: rv32im-program-arithmetic RV32IMArithmeticProgramTest -->

<div class="simulator-embed"><iframe src="../../../simulator/?scenario=rv32im-program23&amp;embed=1" title="RV32IM arithmetic program" loading="lazy" allowfullscreen></iframe></div>

<a href="../../../simulator/?scenario=rv32im-program23" class="md-button">Open arithmetic program</a>

## Program 24: Dependencies, memory and branch recovery

`rv32im-program24` multiplies 6 × 7, stores and reloads 42, divides
the loaded value by 3 and immediately multiplies the result back to 42.
The destination also serves as a source. A taken branch skips a wrong-path
multiply; x7 must remain zero. The final stores at 0x100 and 0x104 both
contain 42. Inspect forwarding, load-use and arithmetic waits, the branch
flush and the memory requests.

<!-- simulator-program: rv32im-program-dependencies RV32IMDependenciesProgramTest -->

<a href="../../../simulator/?scenario=rv32im-program24" class="md-button">Open dependency program</a>

## Program 25: Divide boundaries and signed products

`rv32im-program25` checks INT_MIN/−1, its zero remainder,
unsigned division/remainder by zero, negative signed division/remainder,
and signed versus mixed-sign high products. Division by zero and signed
overflow produce the architectural results without trapping. Every program
ends with EBREAK and a clean halt.

<!-- simulator-program: rv32im-program-divide-edges RV32IMDivideEdgesProgramTest -->

<a href="../../../simulator/?scenario=rv32im-program25" class="md-button">Open divide boundary program</a>

## Program 26: Fibonacci by fast doubling

`rv32im-program26` computes F(10) and F(11) with fast doubling. It finds the
highest input bit using `DIVU` and a loop, then processes the bits from most
to least significant. For each bit it computes:

```text
c = a * (2*b - a)
d = a*a + b*b
(a, b) = (d, c+d) if the bit is 1, otherwise (c, d)
store(a, b)
mask = mask / 2
```

The three products use `MUL`; `DIVU` halves the mask after every step.
This fixture retires 12 multiplies and five unsigned divisions. It writes
four intermediate Fibonacci pairs at 0x100–0x11F, ending with **(55, 89)**.
The circuit checks every commit and the complete output array. Inspect the
repeated M waits, result forwarding, loop branches and stores.

<!-- simulator-program: rv32im-program26 RV32IMFibonacciProgramTest -->

<a href="../../../simulator/?scenario=rv32im-program26" class="md-button">Open Program 26: Fibonacci</a>

## Program 27: Euclidean GCD with quotient checks

`rv32im-program27` computes gcd(1071, 462). Each iteration uses `REMU` to
reduce the pair, `DIVU` to obtain the quotient and `MUL` to verify
`quotient * divisor + remainder == dividend` inside the hardware program.
A failed identity takes a separate error path with x10 = 1.

The program stores each remainder/quotient pair, then the final GCD **21**
at 0x118. It retires three each of `MUL`, `DIVU` and `REMU`, follows the
loop until the divisor reaches zero and finishes with x10 = 0. Inspect the
division/remainder ownership transitions, dependent multiplication and
branch recovery.

<!-- simulator-program: rv32im-program27 RV32IMGcdProgramTest -->

<a href="../../../simulator/?scenario=rv32im-program27" class="md-button">Open Program 27: GCD</a>

## Source and verification

[Program images, directed expectations and oracle-checked timelines](https://github.com/arnold518/reinventing-the-wheel/blob/alpha/src/tests/RV32IMProgramTests.cpp)
define these five tests. The M instruction result oracle is separate from
the [gate-built arithmetic units](arithmetic.md). They check commits and
final memory, not equal internal transient waveforms or game-device behavior.
They do not establish performance outside their short, fixed inputs.
