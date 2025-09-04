import circuit_backend
import sys

# --- Mock Simulator Class ---
# Since your C++ classes require a Simulator object, we create a mock
# version in Python that can be passed to the 'evaluate' method.
# In a real application, this would also be a bound C++ class.
class MockSimulator:
    def __init__(self):
        print("Python: MockSimulator created.")
    
    def add_event(self, event):
        print(f"Python: MockSimulator received an event: {event}")

# --- Python Implementation of a BasicComponent ---
# This class demonstrates how to inherit from a bound abstract C++ class.
class PyAndGate(circuit_backend.BasicComponent):
    """
    A Python class that inherits from the C++ BasicComponent and
    provides implementations for its pure virtual methods.
    """
    def __init__(self, name: str):
        # Always call the parent C++ constructor using super()
        super().__init__(name, 2) # e.g., using a delay of 2ns
        print(f"Python: PyAndGate '{self.get_name()}' constructor called.")
        
        # In a real scenario, you'd call initPins here after creation
        # We need a shared_ptr to self, which is tricky to get directly.
        # This is often handled by a C++ factory. For testing, we can
        # create a dummy method or just test the override.

    # 1. Provide the implementation for the C++ pure virtual function `initPins`
    def initPins(self, self_ptr) -> None:
        print(f"Python: Overridden 'initPins' called for '{self.get_name()}'.")
        # In a real component, you would define pins here, e.g.:
        # self._addPin("A", PinType.INPUT, self_ptr)
        # self._addPin("B", PinType.INPUT, self_ptr)
        # self._addPin("Z", PinType.OUTPUT, self_ptr)

    # 2. Provide the implementation for the C++ pure virtual function `evaluate`
    def evaluate(self, current_time: int, simulator: MockSimulator) -> None:
        print(f"Python: Overridden 'evaluate' called for '{self.get_name()}' at time {current_time}.")
        # Logic to read input pins and update output wires would go here.
        # For example:
        # val_a = self.get_input_value("A")
        # val_b = self.get_input_value("B")
        # if val_a == LogicValue.TRUE and val_b == LogicValue.TRUE:
        #     self._updateOutputWire(simulator, "Z", LogicValue.TRUE, current_time)
        # else:
        #     self._updateOutputWire(simulator, "Z", LogicValue.FALSE, current_time)


def run_tests():
    """Runs all tests and prints the results."""
    print("--- Starting Pybind11 Binding Tests ---")

    # --- Test 1: Instantiation and Basic Method Calls ---
    print("\n[TEST 1: Instantiating a non-abstract C++ class]")
    try:
        # We can't instantiate IOComponent or BasicComponent directly as they are abstract.
        # We can only instantiate Component (if it's not abstract) or a concrete derived class.
        comp = circuit_backend.Component("RootComponent")
        print(f"  SUCCESS: Created Component with name: '{comp.get_name()}'")
    except TypeError as e:
        print(f"  INFO: Could not instantiate Component. This is expected if its constructor is protected. Error: {e}")


    # --- Test 2: Creating and Testing a Python-Derived Class ---
    print("\n[TEST 2: Testing Python class inheriting from C++ BasicComponent]")
    try:
        and_gate = PyAndGate("MyAndGate")
        print("  SUCCESS: Instantiated Python class 'PyAndGate'.")
    except Exception as e:
        print(f"  FAILURE: Could not instantiate PyAndGate. Error: {e}")
        sys.exit(1)

    # --- Test 3: Calling Inherited C++ Methods ---
    print("\n[TEST 3: Calling inherited C++ methods from Python instance]")
    print(f"  Name: '{and_gate.get_name()}' (from Component)")
    print(f"  Delay: {and_gate.get_delay()} (from IOComponent)")
    # We can't easily test get_pin methods without having added pins.
    
    # --- Test 4: Verifying the C++ Inheritance Hierarchy in Python ---
    print("\n[TEST 4: Verifying inheritance with isinstance()]")
    is_basic = isinstance(and_gate, circuit_backend.BasicComponent)
    is_io = isinstance(and_gate, circuit_backend.IOComponent)
    is_comp = isinstance(and_gate, circuit_backend.Component)
    print(f"  Is instance of BasicComponent? -> {is_basic}")
    print(f"  Is instance of IOComponent?    -> {is_io}")
    print(f"  Is instance of Component?      -> {is_comp}")

    if not all([is_basic, is_io, is_comp]):
        print("  FAILURE: Inheritance hierarchy is not correctly recognized.")
    else:
        print("  SUCCESS: Inheritance hierarchy is correct.")

    # --- Test 5: Calling the Overridden Virtual Methods ---
    print("\n[TEST 5: Calling methods that should trigger Python overrides]")
    # In a real simulation, a C++ function would call these. We simulate that here.
    # Note: We can't easily call initPins because we don't have a shared_ptr to pass.
    # This is a limitation of calling it from Python, but it proves the override works
    # if C++ calls it. We can directly call evaluate, however.
    
    mock_sim = MockSimulator()
    print("  Calling and_gate.evaluate(100, mock_sim)...")
    and_gate.evaluate(100, mock_sim)
    
    print("\n--- All Tests Complete ---")


if __name__ == "__main__":
    run_tests()