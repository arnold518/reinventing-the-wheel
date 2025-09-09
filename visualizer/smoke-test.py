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

    # --- 2. Print Pin Info (if applicable) ---
    # NOTE: Your original code checked for BasicComponent. It's better to check for
    # the more general IOComponent to see pins on composite components too.
    if isinstance(component, circuit_backend.IOComponent):
        if isinstance(component, circuit_backend.BasicComponent):
            print(f"{prefix}  - Delay: {component.get_delay()}")
        
        input_pins = component.get_input_pins()
        if input_pins:
            print(f"{prefix}  - Input Pins:")
            for name, pin in input_pins.items():
                val_str = str(pin.get_value())
                print(f"{prefix}    - Pin '{name}' (Value: {val_str})")

        output_pins = component.get_output_pins()
        if output_pins:
            print(f"{prefix}  - Output Pins:")
            for name, pin in output_pins.items():
                val_str = str(pin.get_value())
                print(f"{prefix}    - Pin '{name}' (Value: {val_str})")

    # --- 3. Print Wire Info ---
    # --- FIX: Removed the "if indent_level == 0" check ---
    # Wires can be owned by any component in the hierarchy, so we must check every time.
    wires = component.get_wires()
    if wires:
        print(f"{prefix}  - Wires:")
        for wire in wires:
            source_pin = wire.get_source_pin()
            
            if source_pin:
                source_owner = source_pin.get_owner()
                source_info = f"from {source_owner.get_name()}.{source_pin.get_name()}"
            else:
                source_info = "from [CONSTANT]" # e.g., GND/VCC
            
            sink_info_list = []
            # Assuming get_sink_pins() is bound and returns a list of strong pointers
            for sink_pin in wire.get_sink_pins(): 
                sink_owner = sink_pin.get_owner()
                sink_info_list.append(f"{sink_owner.get_name()}.{sink_pin.get_name()}")
            
            sinks_str = ", ".join(sink_info_list) if sink_info_list else "[UNCONNECTED]"
            
            print(f"{prefix}    - Wire '{wire.get_name()}': {source_info} -> [{sinks_str}]")

    # --- 4. Recurse into Children ---
    children = component.get_children()
    if children:
        print(f"{prefix}  - Children:")
        for child in children:
            format_circuit_structure(child, indent_level + 1)


if __name__ == "__main__":
    print("--- Python Structural Discovery Test ---")

    # 1. Instantiate the specific C++ test scenario.
    try:
        test_scenario = circuit_backend.HalfAdderTest()
        print("SUCCESS: Instantiated circuit_backend.HalfAdderTest")
    except AttributeError:
        print("\n[FATAL ERROR]")
        print("Could not find 'HalfAdderTest' in the 'circuit_backend' module.")
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