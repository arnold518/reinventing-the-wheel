import simulation  # This is the compiled C++ module

sim = simulation.Simulator(0)

for _ in range(5):
    print(sim.tick())  # You could instead send this to a GUI component

# g++ -O3 -Wall -shared -std=c++17 -fPIC \
#     $(python3 -m pybind11 --includes) \
#     simulation.cpp \
#     -o simulation$(python3-config --extension-suffix)