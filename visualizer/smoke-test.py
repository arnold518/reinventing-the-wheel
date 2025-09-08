import circuit_backend
import sys

def format_circuit_structure(component, indent_level=0):
    """
    A Python function that recursively explores and prints the structure of a C++
    circuit, similar to the C++ format() method.
    """
    prefix = "  " * indent_level
    
    # --- 1. Print Component Info ---
    comp_type = component.get_type_name()
    comp_name = component.get_name()
    print(f"{prefix}Component: '{comp_name}' (Type: {comp_type})")

    # print(f"{prefix} {type(component)}")
    # print(f"{prefix} Component?: {isinstance(component, circuit_backend.Component)}")
    # print(f"{prefix} IOComponent?: {isinstance(component, circuit_backend.IOComponent)}")
    # print(f"{prefix} BasicComponent?: {isinstance(component, circuit_backend.BasicComponent)}")

    # --- 2. Print Pin Info (if applicable) ---
    if isinstance(component, circuit_backend.BasicComponent):
        print(f"{prefix}  - Delay: {component.get_delay()}")
        
        # Get all input pins from the map
        input_pins = component.get_input_pins()
        if input_pins:
            print(f"{prefix}  - Input Pins:")
            for name, pin in input_pins.items():
                val_str = str(pin.get_value()) # Uses the __str__ we bound for LogicValue
                print(f"{prefix}    - Pin '{name}' (Value: {val_str})")

        # Get all output pins from the map
        output_pins = component.get_output_pins()
        if output_pins:
            print(f"{prefix}  - Output Pins:")
            for name, pin in output_pins.items():
                val_str = str(pin.get_value())
                print(f"{prefix}    - Pin '{name}' (Value: {val_str})")

    # --- 3. Print Wire Info ---
    # In your design, wires are attached to the root component.
    # We only process them at the top level to avoid duplicates.
    if indent_level == 0:
        wires = component.get_wires()
        if wires:
            print(f"{prefix}Wires:")
            for wire in wires:
                source_pin = wire.get_source_pin()
                
                # Check if the source pin exists
                if source_pin:
                    source_owner = source_pin.get_owner()
                    source_info = f"from {source_owner.get_name()}.{source_pin.get_name()}"
                else:
                    source_info = "from [PRIMARY INPUT]"
                
                # Gather all sink pins
                sink_info_list = []
                for sink_pin in wire.get_sink_pins():
                    # No more weak_ptr or .lock()!
                    sink_owner = sink_pin.get_owner()
                    sink_info_list.append(f"{sink_owner.get_name()}.{sink_pin.get_name()}")
                
                sinks_str = ", ".join(sink_info_list) if sink_info_list else "[PRIMARY OUTPUT]"
                
                print(f"{prefix}  - Wire '{wire.get_name()}': {source_info} -> [{sinks_str}]")

    # --- 4. Recurse into Children ---
    children = component.get_children()
    if children:
        print(f"{prefix}Children:")
        for child in children:
            # Recursive call
            format_circuit_structure(child, indent_level + 1)


if __name__ == "__main__":
    print("--- Python Structural Discovery Test ---")

    # 1. Instantiate the specific C++ test scenario.
    try:
        test_scenario = circuit_backend.FullCircuitTest()
        print("SUCCESS: Instantiated circuit_backend.FullCircuitTest")
    except AttributeError:
        print("\n[FATAL ERROR]")
        print("Could not find 'FullCircuitTest' in the 'circuit_backend' module.")
        print("Please ensure you have created and compiled the binding for this class.")
        sys.exit(1)
    except Exception as e:
        print(f"\n[FATAL ERROR] An unexpected error occurred: {e}")
        sys.exit(1)

    # 2. Run the C++ circuit setup methods.
    print("\nRunning C++ setup_circuit()...")
    test_scenario.setup_circuit()
    print("C++ setup complete.")

    # 3. Get the C++ root component.
    root_component = test_scenario.get_root()
    if not root_component:
        print("\n[FATAL ERROR] C++ test scenario did not produce a root component!")
        sys.exit(1)

    # 4. Run the discovery process and print the formatted structure.
    print("\n--- Discovered Circuit Structure ---")
    try:
        format_circuit_structure(root_component)
    except Exception as e:
        print(f"\n[FATAL ERROR] An error occurred during structure discovery: {e}")
        print("This likely indicates a problem with one of your C++ getters or a missing binding.")
        sys.exit(1)
    
    print("\n--- Discovery Complete: All structures accessible from Python. ---\n")