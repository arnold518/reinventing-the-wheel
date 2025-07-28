import simulation  # This is the compiled C++ module

sim = simulation.Simulator(0)

for _ in range(5):
    print(sim.tick())  # You could instead send this to a GUI component
