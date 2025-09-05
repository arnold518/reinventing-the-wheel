import pygame
import sys
import circuit_backend

# --- NEW ---
# We now import the more specific VisualIOComponent and the new VisualWire class
from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent, VisualIOComponent, VisualWire

pygame.init()

# --- Layout Definition (No Changes) ---
COMPONENT_LAYOUTS = {
    'Full_Circuit_Test': {'pos': (50, 50),   'size': (500, 400)},
    'AND1':              {'pos': (100, 100), 'size': (120, 80), 'color': (67, 175, 255, 100)},
    'DFF1':              {'pos': (270, 150), 'size': (120, 100), 'color': (255, 67, 148, 100)},
    'AND2':              {'pos': (100, 250), 'size': (120, 80), 'color': (67, 175, 255, 100)},
    'CLK_GEN':           {'pos': (270, 70), 'size': (120, 60),  'color': (67, 255, 148, 100)},
}

# --- NEW: Updated Hierarchy Builder ---
def build_visual_hierarchy(cpp_component, parent_vc=None, created_map=None):
    """
    Recursively builds a tree of VisualComponents and collects all VisualPins.
    Returns the root visual component and a flat list of all pins created.
    """
    if created_map is None:
        created_map = {}
    
    all_pins_in_hierarchy = []
    name = cpp_component.get_name()
    layout = COMPONENT_LAYOUTS.get(name)
    
    if not layout:
        print(f"Warning: No layout defined for component '{name}'. Skipping visual creation.")
        for cpp_child in cpp_component.get_children():
            _, child_pins = build_visual_hierarchy(cpp_child, parent_vc, created_map)
            all_pins_in_hierarchy.extend(child_pins)
        return None, all_pins_in_hierarchy

    # --- NEW: Type-aware component creation ---
    # Check if the component has pins and choose the correct visual class.
    if hasattr(cpp_component, 'get_input_pins'):
        VisualComponentClass = VisualIOComponent
    else:
        VisualComponentClass = VisualComponent

    visual_comp = VisualComponentClass(
        pos=layout['pos'],
        size=layout['size'],
        cpp_handle=cpp_component,
        parent=parent_vc,
        color=layout.get('color', (61, 90, 128, 100))
    )
    created_map[name] = visual_comp
    
    # --- NEW: Collect pins from the created component ---
    if isinstance(visual_comp, VisualIOComponent):
        all_pins_in_hierarchy.extend(visual_comp.get_all_pins())
    
    # Recurse into children
    for cpp_child in cpp_component.get_children():
        child_vc, child_pins = build_visual_hierarchy(cpp_child, parent_vc=visual_comp, created_map=created_map)
        if child_vc:
            visual_comp.add_child(child_vc)
            all_pins_in_hierarchy.extend(child_pins) # Add pins from children
            
    return visual_comp, all_pins_in_hierarchy

# --- Screen and UI Setup (No Changes) ---
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Circuit Simulator - C++ Backend Driven")
clock = pygame.time.Clock()
camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))

def on_reset_button_click():
    camera.reset()
    zoom_slider.val = camera.zoom
    zoom_slider._update_handle_pos()
    
def on_zoom_slider_change(new_zoom):
    if abs(camera.zoom - new_zoom) > 0.01:
        camera.zoom = new_zoom

reset_button = Button(x=20, y=20, width=150, height=50, text='Reset View', on_click=on_reset_button_click)
zoom_slider = Slider(x=20, y=100, width=200, height=10, min_val=camera.min_zoom, max_val=camera.max_zoom, initial_val=camera.zoom, on_change=on_zoom_slider_change)
ui_elements = [reset_button, zoom_slider]

# --- HIERARCHY CREATION (NEW ARCHITECTURE) ---
print("--- Initializing C++ Backend ---")
try:
    test_scenario = circuit_backend.FullCircuitTest()
    print("Running C++ setup_circuit()...")
    test_scenario.setup_circuit()
    root_cpp_component = test_scenario.get_root()

    if not root_cpp_component:
        raise RuntimeError("C++ test scenario did not produce a root component!")

    # 1. Build the component hierarchy and collect all visual pins.
    all_components_map = {}
    _, all_visual_pins = build_visual_hierarchy(root_cpp_component, created_map=all_components_map)
    all_components = list(all_components_map.values())
    print(f"Created {len(all_components)} visual components and found {len(all_visual_pins)} visual pins.")

    # --- NEW: Create and connect visual wires ---
    
    # 2. Create a mapping from C++ pin handles to Python VisualPin objects for quick lookups.
    pin_map = {vpin.cpp_handle: vpin for vpin in all_visual_pins}
    
    # 3. Get all C++ wire objects from the root component.
    cpp_wires = root_cpp_component.get_wires()
    
    # 4. Create a VisualWire for each C++ wire.
    all_wires = [VisualWire(cw) for cw in cpp_wires]
    print(f"Created {len(all_wires)} visual wires.")

    # 5. Connect the visual wires to their corresponding visual pins using the map.
    for wire in all_wires:
        wire.connect(pin_map)
    print("--- C++ Backend Initialized and Visualized Successfully ---")

except Exception as e:
    print(f"\n[FATAL ERROR] Could not initialize the C++ backend: {e}")
    print("Please ensure 'circuit_backend' module is built and accessible.")
    pygame.quit()
    sys.exit(1)

# --- Main loop ---
running = True
while running:
    dt = clock.tick(60)
    mouse_pos = pygame.mouse.get_pos()
    
    mouse_over_ui = any(
        (hasattr(elem, 'rect') and elem.rect.collidepoint(mouse_pos)) or
        (hasattr(elem, 'handle_rect') and elem.handle_rect.collidepoint(mouse_pos))
        for elem in ui_elements
    )

    world_mouse_pos = camera.screen_to_world(mouse_pos)
    hovered_component = None
    if not mouse_over_ui:
        for component in reversed(all_components):
            if component.rect.collidepoint(world_mouse_pos):
                hovered_component = component
                break
    for component in all_components:
        component.is_hovered = (component == hovered_component)

    event_was_consumed = False
    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        
        for element in ui_elements:
            element.handle_event(event)
        
        if not mouse_over_ui and hovered_component:
            if hovered_component.handle_event(event, camera):
                event_was_consumed = True
        
        is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
        if not (is_mouse_event and (mouse_over_ui or event_was_consumed)):
            camera.handle_event(event)

    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom
        zoom_slider._update_handle_pos()

    # --- Drawing ---
    screen.fill((30, 30, 30))
    camera.draw_grid(screen)
    
    # --- NEW: Draw all the wires first, so they appear behind the components.
    for wire in all_wires:
        wire.draw(screen, camera)
    
    # Draw top-level components (they will recursively draw their children and pins)
    for component in all_components:
        if component.parent is None:
            component.draw(screen, camera)

    for element in ui_elements:
        element.draw(screen)

    pygame.display.flip()

pygame.quit()