# RV32IM recorded programs

These short programs run on `RV32IMFiveStageCore` connected to separate
instruction and data memory endpoints. They appear under **RV32IM programs /
Recorded execution** in the existing scenario selector. The default view
shows structural architecture blocks; the explorer can select compact
implementations for individual components.

Every retiring PC/instruction, architectural register image and terminal
state is checked against the independent RV32IM instruction oracle. Final
memory is compared independently, with directed register and store
expectations. The timeline records reset, arithmetic waiting, every commit
and halt. Select **CORE → DECODE** or its **REGISTER_FILE** in the explorer
to inspect register values at the selected time.
These are bounded verification fixtures, not performance benchmarks.

## All eight M instructions

`rv32im-program-arithmetic` initializes x1 = −7 and x2 = 3, then executes
MUL, MULH, MULHSU, MULHU, DIV, DIVU, REM and REMU. A final multiply to x0
checks that the zero register stays zero. Inspect signed/unsigned high
products and the repeated execution stalls, then compare register values
at each commit.

<!-- simulator-program: rv32im-program-arithmetic RV32IMArithmeticProgramTest -->

<div class="simulator-embed"><iframe src="../../simulator/?scenario=rv32im-program-arithmetic&amp;embed=1" title="RV32IM arithmetic program" loading="lazy" allowfullscreen></iframe></div>

<a href="../../simulator/?scenario=rv32im-program-arithmetic" class="md-button">Open arithmetic program</a>

## Dependencies, memory and branch recovery

`rv32im-program-dependencies` multiplies 6 × 7, stores and reloads 42, divides
the loaded value by 3 and immediately multiplies the result back to 42.
The destination also serves as a source. A taken branch skips a wrong-path
multiply; x7 must remain zero. The final stores at 0x100 and 0x104 both
contain 42. Inspect forwarding, load-use and arithmetic waits, the branch
flush and the memory requests.

<!-- simulator-program: rv32im-program-dependencies RV32IMDependenciesProgramTest -->

<a href="../../simulator/?scenario=rv32im-program-dependencies" class="md-button">Open dependency program</a>

## Divide boundaries and signed products

`rv32im-program-divide-edges` checks INT_MIN/−1, its zero remainder,
unsigned division/remainder by zero, negative signed division/remainder,
and signed versus mixed-sign high products. Division by zero and signed
overflow produce the architectural results without trapping. Every program
ends with EBREAK and a clean halt.

<!-- simulator-program: rv32im-program-divide-edges RV32IMDivideEdgesProgramTest -->

<a href="../../simulator/?scenario=rv32im-program-divide-edges" class="md-button">Open divide boundary program</a>
