import pygame
import sys
import json
import circuit_backend

from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent, VisualIOComponent, VisualWire

# (LayoutManager and build_visual_hierarchy_recursive are unchanged)
class LayoutManager:
    def __init__(self, filepath):
        with open(filepath, 'r') as f: data = json.load(f)
        self.settings = data.get("default_settings", {})
        self.root_name = data.get("root_component_name")
        self.root_layout = data.get("root_layout", {})
        self.layouts = data.get("component_layouts", {})
    def get_layout_for(self, component_type):
        return self.layouts.get(component_type, {})

def build_visual_hierarchy_recursive(cpp_component, parent_abs_rect, parent_layout, layout_manager, created_map, parent_vc=None, depth=0):
    all_pins = []
    my_name, my_type = cpp_component.get_name(), cpp_component.get_type_name()
    my_layout = layout_manager.get_layout_for(my_type)
    child_layout_info = parent_layout.get("children", {}).get(my_name)
    if not child_layout_info: return None, []
    rel_pos, rel_width = child_layout_info['rel_pos'], child_layout_info['rel_width']
    aspect_ratio = my_layout.get('aspect_ratio', 1.0)
    abs_width = parent_abs_rect.width * rel_width
    abs_height = abs_width * aspect_ratio
    abs_x = parent_abs_rect.x + parent_abs_rect.width * rel_pos[0]
    abs_y = parent_abs_rect.y + parent_abs_rect.height * rel_pos[1]
    my_abs_rect = pygame.Rect(abs_x, abs_y, abs_width, abs_height)
    ComponentClass = VisualIOComponent if hasattr(cpp_component, 'get_input_pins') else VisualComponent
    visual_comp = ComponentClass(rect=my_abs_rect, cpp_handle=cpp_component, settings=layout_manager.settings, depth=depth, parent=parent_vc, color=my_layout.get('color', (80, 80, 80)))
    created_map[my_name] = visual_comp
    if isinstance(visual_comp, VisualIOComponent): all_pins.extend(visual_comp.get_all_pins())
    for child_cpp in cpp_component.get_children():
        child_vc, child_pins = build_visual_hierarchy_recursive(child_cpp, my_abs_rect, my_layout, layout_manager, created_map, visual_comp, depth + 1)
        if child_vc:
            visual_comp.add_child(child_vc); all_pins.extend(child_pins)
    return visual_comp, all_pins

pygame.init()
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Circuit Simulator - C++ Backend Driven")
clock = pygame.time.Clock()
camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))

try:
    layout_manager = LayoutManager("layout.json")
    test_scenario = circuit_backend.FullCircuitTest()
    test_scenario.setup_circuit()
    root_cpp = test_scenario.get_root()
    if not root_cpp: raise RuntimeError("C++ test scenario did not produce a root component!")
    all_components_map = {}
    root_rect = pygame.Rect(layout_manager.root_layout['pos'], layout_manager.root_layout['size'])
    root_layout = layout_manager.get_layout_for(layout_manager.root_name)
    RootClass = VisualIOComponent if hasattr(root_cpp, 'get_input_pins') else VisualComponent
    root_vc = RootClass(rect=root_rect, cpp_handle=root_cpp, settings=layout_manager.settings, depth=0, color=root_layout.get('color'))
    all_components_map[root_vc.name] = root_vc
    all_visual_pins = root_vc.get_all_pins() if isinstance(root_vc, VisualIOComponent) else []
    for child_cpp in root_cpp.get_children():
        child_vc, child_pins = build_visual_hierarchy_recursive(child_cpp, root_rect, root_layout, layout_manager, all_components_map, root_vc, 1)
        if child_vc:
            root_vc.add_child(child_vc); all_visual_pins.extend(child_pins)
    all_components = list(all_components_map.values())
    pin_map = {f"{vpin.parent.name}.{vpin.name}": vpin for vpin in all_visual_pins}
    all_wires = [VisualWire(cw) for cw in root_cpp.get_wires()]
    for wire in all_wires: wire.connect(pin_map)
    simulator = test_scenario.get_simulator()
    simulator.run_and_record(test_scenario.get_run_duration())
    event_timestamps = simulator.get_unique_timestamps()
    simulator.set_circuit_state_at_time(0)
except Exception as e:
    print(f"\n[FATAL ERROR] Could not initialize: {e}"); pygame.quit(); sys.exit(1)

# UI & State Management (unchanged)
current_time_index, previous_time_index, is_playing, playback_timer = 0, -1, False, 0; playback_interval = 250
def on_play_pause_click(): globals()['is_playing'] = not globals()['is_playing']; play_pause_button.set_text("Pause" if globals()['is_playing'] else "Play")
def on_step_forward(): globals()['current_time_index'] = min(len(event_timestamps)-1, globals()['current_time_index']+1); is_playing and on_play_pause_click()
def on_step_backward(): globals()['current_time_index'] = max(0, globals()['current_time_index']-1); is_playing and on_play_pause_click()
def on_reset_click(): globals()['current_time_index'] = 0; is_playing and on_play_pause_click()
def on_time_slider_change(index): globals()['current_time_index'] = int(index)
time_slider = Slider(x=20, y=20, width=SCREEN_WIDTH-40, height=15, min_val=0, max_val=len(event_timestamps)-1, initial_val=0, step=1, on_change=on_time_slider_change)
play_pause_button = Button(x=20, y=50, width=100, height=50, text='Play', on_click=on_play_pause_click)
step_backward_button = Button(x=130, y=50, width=50, height=50, text='<', text_size=36, on_click=on_step_backward)
step_forward_button = Button(x=190, y=50, width=50, height=50, text='>', text_size=36, on_click=on_step_forward)
reset_sim_button = Button(x=250, y=50, width=100, height=50, text='Reset', on_click=on_reset_click)
time_font = VisualComponent.get_font(32)
zoom_slider = Slider(x=20, y=SCREEN_HEIGHT-30, width=200, height=10, min_val=camera.min_zoom, max_val=camera.max_zoom, initial_val=camera.zoom, step=0.05, on_change=lambda z: setattr(camera, 'zoom', z))
reset_button = Button(x=230, y=SCREEN_HEIGHT-65, width=150, height=50, text='Reset View', on_click=lambda: camera.reset())
ui_elements = [time_slider, play_pause_button, step_backward_button, step_forward_button, reset_sim_button, zoom_slider, reset_button]

# --- Main loop ---
running = True
while running:
    dt = clock.tick(60)
    mouse_pos = pygame.mouse.get_pos()
    
    # --- NEW: Prioritized Hover Detection ---
    mouse_over_ui = any((hasattr(e, 'rect') and e.rect.collidepoint(mouse_pos)) or (hasattr(e, 'handle_rect') and e.handle_rect.collidepoint(mouse_pos)) for e in ui_elements)
    
    # Reset all hover states
    for pin in all_visual_pins: pin.is_hovered = False
    for wire in all_wires: wire.is_hovered = False
    for comp in all_components: comp.is_hovered = False

    hovered_component_for_drag = None # Separate variable for dragging logic

    if not mouse_over_ui:
        world_mouse_pos = camera.screen_to_world(mouse_pos)
        
        # 1. Check for hovered Pin
        hovered_pin = next((pin for pin in all_visual_pins if pin.rect.inflate(4, 4).collidepoint(world_mouse_pos)), None)
        
        # 2. Check for hovered Wire if no pin
        hovered_wire = None
        if not hovered_pin:
            hovered_wire = next((wire for wire in all_wires if wire.collidepoint(world_mouse_pos, camera)), None)

        # 3. Check for hovered Component if nothing else
        hovered_component = None
        if not hovered_pin and not hovered_wire:
            hovered_component = next((c for c in reversed(all_components) if c.rect.collidepoint(world_mouse_pos)), None)
        
        # Apply the single highest-priority hover state
        if hovered_pin: hovered_pin.is_hovered = True
        elif hovered_wire: hovered_wire.is_hovered = True
        elif hovered_component: hovered_component.is_hovered = True
        
        hovered_component_for_drag = hovered_component # Allow dragging only for components

    # Event Handling
    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        for e in ui_elements: e.handle_event(event)
        consumed = False
        if not mouse_over_ui and hovered_component_for_drag:
            if hovered_component_for_drag.handle_event(event, camera): consumed = True
        if event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL) and not (mouse_over_ui or consumed): camera.handle_event(event)

    # State Update Logic (unchanged)
    if is_playing and pygame.time.get_ticks() - playback_timer > playback_interval:
        current_time_index = min(len(event_timestamps) - 1, current_time_index + 1)
        playback_timer = pygame.time.get_ticks()
        if current_time_index == len(event_timestamps) - 1: on_play_pause_click()

    if current_time_index != previous_time_index:
        simulator.set_circuit_state_at_time(event_timestamps[current_time_index])
        if not time_slider.is_dragging: time_slider.val = current_time_index; time_slider._update_handle_pos()
        previous_time_index = current_time_index

    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom; zoom_slider._update_handle_pos()

    # Drawing
    screen.fill((30, 30, 30)); camera.draw_grid(screen)
    for wire in all_wires: wire.draw(screen, camera)
    for component in all_components:
        if component.parent is None: component.draw(screen, camera)
    
    time_text = f"Time: {event_timestamps[current_time_index]}"
    time_surf = time_font.render(time_text, True, (220, 220, 220))
    screen.blit(time_surf, time_surf.get_rect(left=370, centery=play_pause_button.rect.centery))
    
    for e in ui_elements: e.draw(screen)
    pygame.display.flip()

pygame.quit()