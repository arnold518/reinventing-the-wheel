import pygame
import sys
import circuit_backend

from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent, VisualIOComponent, VisualWire

pygame.init()

# --- Layout Definition ---
COMPONENT_LAYOUTS = {
    'Full_Circuit_Test': {'pos': (50, 150),   'size': (600, 500)}, # Adjusted height
    'AND1':              {'pos': (100, 180), 'size': (120, 100), 'color': (67, 175, 255, 100)},
    'DFF1':              {'pos': (300, 230), 'size': (120, 120), 'color': (255, 67, 148, 100)},
    'AND2':              {'pos': (100, 360), 'size': (120, 100), 'color': (67, 175, 255, 100)},
    'CLK_GEN':           {'pos': (300, 150), 'size': (120, 60),  'color': (67, 255, 148, 100)},
}

# --- Hierarchy Builder (No Changes) ---
def build_visual_hierarchy(cpp_component, parent_vc=None, created_map=None):
    if created_map is None: created_map = {}
    all_pins_in_hierarchy = []
    name = cpp_component.get_name()
    layout = COMPONENT_LAYOUTS.get(name)
    if not layout:
        for cpp_child in cpp_component.get_children():
            _, child_pins = build_visual_hierarchy(cpp_child, parent_vc, created_map)
            all_pins_in_hierarchy.extend(child_pins)
        return None, all_pins_in_hierarchy
    VisualComponentClass = VisualIOComponent if hasattr(cpp_component, 'get_input_pins') else VisualComponent
    visual_comp = VisualComponentClass(pos=layout['pos'], size=layout['size'], cpp_handle=cpp_component, parent=parent_vc, color=layout.get('color', (61, 90, 128, 100)))
    created_map[name] = visual_comp
    if isinstance(visual_comp, VisualIOComponent): all_pins_in_hierarchy.extend(visual_comp.get_all_pins())
    for cpp_child in cpp_component.get_children():
        child_vc, child_pins = build_visual_hierarchy(cpp_child, parent_vc=visual_comp, created_map=created_map)
        if child_vc:
            visual_comp.add_child(child_vc)
            all_pins_in_hierarchy.extend(child_pins)
    return visual_comp, all_pins_in_hierarchy

# --- Screen and UI Setup ---
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Circuit Simulator - C++ Backend Driven")
clock = pygame.time.Clock()
camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))

# --- HIERARCHY & SIMULATION SETUP ---
try:
    test_scenario = circuit_backend.FullCircuitTest()
    test_scenario.setup_circuit()
    root_cpp_component = test_scenario.get_root()
    if not root_cpp_component: raise RuntimeError("C++ test scenario did not produce a root component!")
    all_components_map = {}
    _, all_visual_pins = build_visual_hierarchy(root_cpp_component, created_map=all_components_map)
    all_components = list(all_components_map.values())
    pin_map = {vpin.cpp_handle: vpin for vpin in all_visual_pins}
    all_wires = [VisualWire(cw) for cw in root_cpp_component.get_wires()]
    for wire in all_wires: wire.connect(pin_map)
    print("--- Pre-processing Simulation ---")
    simulator = test_scenario.get_simulator()
    max_sim_time = test_scenario.get_run_duration()
    simulator.run_and_record(max_sim_time)
    event_timestamps = simulator.get_unique_timestamps()
    simulator.set_circuit_state_at_time(0)
    print(f"Simulation recorded. Found {len(event_timestamps)} keyframes.")
except Exception as e:
    print(f"\n[FATAL ERROR] Could not initialize: {e}")
    pygame.quit()
    sys.exit(1)

# --- UI & STATE MANAGEMENT ---
current_time_index = 0
previous_time_index = -1
is_playing = False
playback_interval = 250
playback_timer = 0

def on_play_pause_click():
    """--- FIX 2 --- Uses the new set_text method to ensure proper centering."""
    global is_playing
    is_playing = not is_playing
    new_text = "Pause" if is_playing else "Play"
    play_pause_button.set_text(new_text)

def on_step_forward_click():
    global current_time_index, is_playing
    if is_playing: on_play_pause_click()
    current_time_index = min(len(event_timestamps) - 1, current_time_index + 1)
    
def on_step_backward_click():
    global current_time_index, is_playing
    if is_playing: on_play_pause_click()
    current_time_index = max(0, current_time_index - 1)

def on_reset_click():
    global current_time_index, is_playing
    if is_playing: on_play_pause_click()
    current_time_index = 0

def on_time_slider_change(index):
    global current_time_index
    current_time_index = int(index)

# --- UI LAYOUT (NEW) ---
# Top Row: Time Controls
time_slider = Slider(x=20, y=20, width=SCREEN_WIDTH - 40, height=15, min_val=0, max_val=len(event_timestamps) - 1, initial_val=0, step=1, on_change=on_time_slider_change)
play_pause_button = Button(x=20, y=50, width=100, height=50, text='Play', on_click=on_play_pause_click)
step_backward_button = Button(x=130, y=50, width=50, height=50, text='<', text_size=36, on_click=on_step_backward_click)
step_forward_button = Button(x=190, y=50, width=50, height=50, text='>', text_size=36, on_click=on_step_forward_click)
reset_sim_button = Button(x=250, y=50, width=100, height=50, text='Reset', on_click=on_reset_click)
time_font = VisualComponent.get_font(32)

# Bottom-Left: View Controls
zoom_slider = Slider(x=20, y=SCREEN_HEIGHT - 30, width=200, height=10, min_val=camera.min_zoom, max_val=camera.max_zoom, initial_val=camera.zoom, step=0.05, on_change=lambda z: setattr(camera, 'zoom', z))
reset_button = Button(x=20, y=SCREEN_HEIGHT - 100, width=150, height=50, text='Reset View', on_click=lambda: camera.reset())

ui_elements = [time_slider, play_pause_button, step_backward_button, step_forward_button, reset_sim_button, zoom_slider, reset_button]

# --- Main loop ---
running = True
while running:
    dt = clock.tick(60)
    mouse_pos = pygame.mouse.get_pos()
    
    # Event Handling
    mouse_over_ui = any((hasattr(e, 'rect') and e.rect.collidepoint(mouse_pos)) or (hasattr(e, 'handle_rect') and e.handle_rect.collidepoint(mouse_pos)) for e in ui_elements)
    hovered_component = None
    if not mouse_over_ui:
        world_mouse_pos = camera.screen_to_world(mouse_pos)
        for component in reversed(all_components):
            if component.rect.collidepoint(world_mouse_pos):
                hovered_component = component; break
    for component in all_components: component.is_hovered = (component == hovered_component)

    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        for element in ui_elements: element.handle_event(event)
        event_was_consumed = False
        if not mouse_over_ui and hovered_component:
            if hovered_component.handle_event(event, camera): event_was_consumed = True
        is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
        if not (is_mouse_event and (mouse_over_ui or event_was_consumed)): camera.handle_event(event)

    # Main Loop Logic
    if is_playing and pygame.time.get_ticks() - playback_timer > playback_interval:
        current_time_index = min(len(event_timestamps) - 1, current_time_index + 1)
        playback_timer = pygame.time.get_ticks()
        if current_time_index == len(event_timestamps) - 1: on_play_pause_click()

    if current_time_index != previous_time_index:
        target_time = event_timestamps[current_time_index]
        simulator.set_circuit_state_at_time(target_time)
        if not time_slider.is_dragging:
            time_slider.val = current_time_index
            time_slider._update_handle_pos()
        previous_time_index = current_time_index

    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom
        zoom_slider._update_handle_pos()

    # --- Drawing ---
    screen.fill((30, 30, 30))
    camera.draw_grid(screen)
    for wire in all_wires: wire.draw(screen, camera)
    for component in all_components:
        if component.parent is None: component.draw(screen, camera)
    
    current_sim_time = event_timestamps[current_time_index]
    time_text = f"Time: {current_sim_time}"
    time_surf = time_font.render(time_text, True, (220, 220, 220))
    time_rect = time_surf.get_rect(left=370, centery=play_pause_button.rect.centery)
    screen.blit(time_surf, time_rect)
    
    for element in ui_elements: element.draw(screen)

    pygame.display.flip()

pygame.quit()