import pygame
import sys
import circuit_backend  # Import your actual, compiled C++ module

from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent

pygame.init()

# --- Layout Definition ---
# This dictionary provides the positions and sizes for the components
# that will be created by your C++ FullCircuitTest scenario.
# The keys MUST match the names you gave them in `buildCircuit()`.
COMPONENT_LAYOUTS = {
    'Full_Circuit_Test': {'pos': (50, 50),   'size': (500, 400)},
    'AND1':              {'pos': (100, 100), 'size': (120, 80), 'color': (67, 175, 255, 100)},
    'DFF1':              {'pos': (270, 150), 'size': (120, 100), 'color': (255, 67, 148, 100)},
    'AND2':              {'pos': (100, 250), 'size': (120, 80), 'color': (67, 175, 255, 100)},
    'CLK_GEN':           {'pos': (270, 70), 'size': (120, 60),  'color': (67, 255, 148, 100)},
}

def build_visual_hierarchy(cpp_component, parent_vc=None, created_map=None):
    """
    Recursively builds a tree of VisualComponents by reading the C++ hierarchy.
    This function remains the same, as its job is still valid.
    """
    if created_map is None:
        created_map = {}
        
    name = cpp_component.get_name()
    layout = COMPONENT_LAYOUTS.get(name)
    
    if not layout:
        print(f"Warning: No layout defined for component '{name}'. Skipping visual creation.")
        # We still need to process children even if the parent isn't drawn
        for cpp_child in cpp_component.get_children():
            build_visual_hierarchy(cpp_child, parent_vc, created_map)
        return None

    # Create the Python visual object using the C++ handle
    visual_comp = VisualComponent(
        pos=layout['pos'],
        size=layout['size'],
        cpp_handle=cpp_component,
        parent=parent_vc,
        color=layout.get('color', (61, 90, 128, 100))
    )
    # Store the component so we can get a flat list later
    created_map[name] = visual_comp
    
    # Recurse into children
    for cpp_child in cpp_component.get_children():
        child_vc = build_visual_hierarchy(cpp_child, parent_vc=visual_comp, created_map=created_map)
        if child_vc:
            visual_comp.add_child(child_vc)
            
    return visual_comp

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
# This section is the core of the new integration.
print("--- Initializing C++ Backend ---")
try:
    # 1. Instantiate the specific C++ test scenario you want to visualize.
    test_scenario = circuit_backend.FullCircuitTest()

    # 2. Call the setup method to run buildCircuit() and setInitialState() in C++.
    print("Running C++ setup_circuit()...")
    test_scenario.setup_circuit()

    # 3. Get the fully constructed root component from the C++ test object.
    print("Retrieving root component from C++...")
    root_cpp_component = test_scenario.get_root()

    if not root_cpp_component:
        raise RuntimeError("C++ test scenario did not produce a root component!")

    # 4. Build the Python visual hierarchy from the C++ data model.
    all_components_map = {}
    build_visual_hierarchy(root_cpp_component, created_map=all_components_map)
    all_components = list(all_components_map.values())
    print("--- C++ Backend Initialized Successfully ---")

except Exception as e:
    print(f"\n[FATAL ERROR] Could not initialize the C++ backend: {e}")
    print("Please ensure 'circuit_backend' module is built and accessible.")
    pygame.quit()
    sys.exit(1)


# --- Main loop (No Changes) ---
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
    
    # Draw top-level components (they will recursively draw their children)
    for component in all_components:
        if component.parent is None:
            component.draw(screen, camera)

    for element in ui_elements:
        element.draw(screen)

    pygame.display.flip()

pygame.quit()