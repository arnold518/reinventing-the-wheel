import pygame
import sys
import json
import circuit_backend

from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent, VisualIOComponent, VisualWire

class LayoutManager:
    def __init__(self, filepath):
        self.filepath = filepath
        with open(filepath, 'r') as f: data = json.load(f)
        self.settings = data.get("default_settings", {})
        self.type_layouts = data.get("type_layouts", {})
        self.instance_layouts = data.get("instance_layouts", {})
        
        self.root_layout_config = {
            "pos": [50, 150],
            "width": 800
        }

    def get_layout_for(self, component_type, instance_path_key):
        is_generic = component_type in ["Component", "IOComponent", "BasicComponent"]
        if is_generic: return self.instance_layouts.get(instance_path_key, {})
        else: return self.type_layouts.get(component_type, {})

    def update_root_position(self, new_pos):
        """Updates the root's in-memory position."""
        self.root_layout_config['pos'] = [new_pos[0], new_pos[1]]

    def update_child_position(self, parent_key, parent_type, child_name, new_rel_pos):
        is_parent_generic = parent_type in ["Component", "IOComponent", "BasicComponent"]
        target_dict = self.instance_layouts if is_parent_generic else self.type_layouts
        key = parent_key if is_parent_generic else parent_type
        if key in target_dict and child_name in target_dict[key].get('children', {}):
            target_dict[key]['children'][child_name]['rel_pos'] = new_rel_pos
        else:
            print(f"DEBUG (LayoutManager): WARNING - Could not find key '{key}' or child '{child_name}' to update position.")

    def save_layout(self):
        data = {"default_settings": self.settings, "type_layouts": self.type_layouts, "instance_layouts": self.instance_layouts}
        with open(self.filepath, 'w') as f: json.dump(data, f, indent=2)
        print(f"Layout saved to {self.filepath}")

def build_visual_hierarchy(layout_manager, cpp_root):
    all_components_map, all_visual_pins = {}, []
    root_type, root_name = cpp_root.get_type_name(), cpp_root.get_name()
    is_root_generic = root_type in ["Component", "IOComponent", "BasicComponent"]
    root_key = root_name if is_root_generic else root_type
    root_layout = layout_manager.get_layout_for(root_type, root_key)

    root_pos = layout_manager.root_layout_config['pos']
    root_width = layout_manager.root_layout_config['width']

    # Calculate height using the aspect ratio from the layout file, just like a child
    root_aspect_ratio = root_layout.get('aspect_ratio', 1.0) # Default to 1.0 if not specified
    root_height = root_width * root_aspect_ratio

    root_rect = pygame.Rect(root_pos, (root_width, root_height))
    
    RootClass = VisualIOComponent if hasattr(cpp_root, 'get_input_pins') else VisualComponent
    root_vc = RootClass(rect=root_rect, cpp_handle=cpp_root, settings=layout_manager.settings, layout_key=root_key, rel_info={}, depth=0, color=root_layout.get('color'))
    all_components_map[root_vc.name] = root_vc
    if isinstance(root_vc, VisualIOComponent): all_visual_pins.extend(root_vc.get_all_pins())
    for child_cpp in cpp_root.get_children():
        child_vc, child_pins = build_recursive_step(child_cpp, root_rect, root_layout, layout_manager, all_components_map, root_vc, 1, root_key)
        if child_vc: root_vc.add_child(child_vc); all_visual_pins.extend(child_pins)
    all_components = list(all_components_map.values())
    pin_map = {f"{vpin.parent.name}.{vpin.name}": vpin for vpin in all_visual_pins}
    all_wires = [VisualWire(cw) for cw in cpp_root.get_wires()]
    for wire in all_wires: wire.connect(pin_map)
    return all_components, all_visual_pins, all_wires, root_vc

def build_recursive_step(cpp_comp, parent_rect, parent_layout, lm, created_map, parent_vc, depth, parent_key):
    all_pins = []
    my_name, my_type = cpp_comp.get_name(), cpp_comp.get_type_name()
    child_info = parent_layout.get("children", {}).get(my_name)
    if not child_info: return None, []
    is_generic = my_type in ["Component", "IOComponent", "BasicComponent"]
    my_key = f"{parent_key}.{my_name}" if is_generic else my_type
    my_layout = lm.get_layout_for(my_type, my_key)
    if not my_layout: return None, []
    rel_pos, rel_width = child_info['rel_pos'], child_info['rel_width']
    aspect_ratio = my_layout.get('aspect_ratio', 1.0)
    abs_w, abs_h = parent_rect.width * rel_width, (parent_rect.width * rel_width) * aspect_ratio
    abs_x, abs_y = parent_rect.x + parent_rect.width * rel_pos[0], parent_rect.y + parent_rect.height * rel_pos[1]
    my_rect = pygame.Rect(abs_x, abs_y, abs_w, abs_h)
    VC_Class = VisualIOComponent if hasattr(cpp_comp, 'get_input_pins') else VisualComponent
    vc = VC_Class(rect=my_rect, cpp_handle=cpp_comp, settings=lm.settings, layout_key=my_key, rel_info=child_info, depth=depth, parent=parent_vc, color=my_layout.get('color'))
    created_map[my_name] = vc
    if isinstance(vc, VisualIOComponent): all_pins.extend(vc.get_all_pins())
    for child_cpp in cpp_comp.get_children():
        child_vc, child_pins = build_recursive_step(child_cpp, my_rect, my_layout, lm, created_map, vc, depth+1, my_key)
        if child_vc: vc.add_child(child_vc); all_pins.extend(child_pins)
    return vc, all_pins

pygame.init()
# (Screen setup unchanged)
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080; screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT)); pygame.display.set_caption("Circuit Simulator - C++ Backend Driven")
clock = pygame.time.Clock(); camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))

try:
    layout_manager = LayoutManager("layout.json")
    test_scenario = circuit_backend.FullCircuitTest()
    test_scenario.setup_circuit()
    root_cpp = test_scenario.get_root()
    if not root_cpp: raise RuntimeError("C++ test scenario did not produce a root component!")
    all_components, all_visual_pins, all_wires, root_vc = build_visual_hierarchy(layout_manager, root_cpp)
    simulator = test_scenario.get_simulator(); simulator.run_and_record(test_scenario.get_run_duration())
    event_timestamps = simulator.get_unique_timestamps(); simulator.set_circuit_state_at_time(0)
except Exception as e:
    print(f"\n[FATAL ERROR] Could not initialize: {e}"); pygame.quit(); sys.exit(1)

# (UI setup unchanged)
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
save_button = Button(x=400, y=SCREEN_HEIGHT-65, width=150, height=50, text='Save Layout', on_click=lambda: layout_manager.save_layout())
ui_elements = [time_slider, play_pause_button, step_backward_button, step_forward_button, reset_sim_button, zoom_slider, reset_button, save_button]

# --- Main loop ---
running = True
active_interaction_component = None
while running:
    dt = clock.tick(60); mouse_pos = pygame.mouse.get_pos()
    mouse_over_ui = any((hasattr(e, 'rect') and e.rect.collidepoint(mouse_pos)) or (hasattr(e, 'handle_rect') and e.handle_rect.collidepoint(mouse_pos)) for e in ui_elements)
    
    hovered_item_for_interaction = None
    if not active_interaction_component and not mouse_over_ui:
        world_mouse_pos = camera.screen_to_world(mouse_pos)
        for item in all_visual_pins + all_wires + all_components: item.is_hovered = False
        hovered_pin = next((p for p in all_visual_pins if p.rect.inflate(4, 4).collidepoint(world_mouse_pos)), None)
        hovered_wire = None if hovered_pin else next((w for w in all_wires if w.collidepoint(world_mouse_pos, camera)), None)
        hovered_component = None if (hovered_pin or hovered_wire) else next((c for c in reversed(all_components) if c.rect.collidepoint(world_mouse_pos)), None)
        if hovered_pin: hovered_pin.is_hovered = True
        elif hovered_wire: hovered_wire.is_hovered = True
        elif hovered_component:
            hovered_component.is_hovered = True
            hovered_item_for_interaction = hovered_component

    event_consumed, layout_changed = False, False
    for event in pygame.event.get():
        if event.type == pygame.QUIT: running = False
        for e in ui_elements: e.handle_event(event)
        
        interaction_target = active_interaction_component or hovered_item_for_interaction
        if not mouse_over_ui and interaction_target:
            consumed, changed = interaction_target.handle_event(event, camera, layout_manager)
            if consumed: event_consumed = True
            if changed: layout_changed = True
            if event.type == pygame.MOUSEBUTTONDOWN and (interaction_target.is_dragging or hasattr(interaction_target, 'is_resizing') and interaction_target.is_resizing):
                active_interaction_component = interaction_target
            elif event.type == pygame.MOUSEBUTTONUP:
                active_interaction_component = None
        
        if layout_changed:
            all_components, all_visual_pins, all_wires, root_vc = build_visual_hierarchy(layout_manager, root_cpp)

        is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
        if is_mouse_event and not mouse_over_ui and not event_consumed:
             camera.handle_event(event)

    # (State Update and Drawing are unchanged)
    if is_playing and pygame.time.get_ticks() - playback_timer > playback_interval:
        current_time_index = min(len(event_timestamps) - 1, current_time_index + 1); playback_timer = pygame.time.get_ticks()
        if current_time_index == len(event_timestamps) - 1: on_play_pause_click()
    if current_time_index != previous_time_index:
        simulator.set_circuit_state_at_time(event_timestamps[current_time_index])
        if not time_slider.is_dragging: time_slider.val = current_time_index; time_slider._update_handle_pos()
        previous_time_index = current_time_index
    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom; zoom_slider._update_handle_pos()
    screen.fill((30, 30, 30)); camera.draw_grid(screen)
    for wire in all_wires: wire.draw(screen, camera)
    if root_vc: root_vc.draw(screen, camera)
    time_text = f"Time: {event_timestamps[current_time_index]}"
    time_surf = time_font.render(time_text, True, (220, 220, 220))
    screen.blit(time_surf, time_surf.get_rect(left=370, centery=play_pause_button.rect.centery))
    for e in ui_elements: e.draw(screen)
    pygame.display.flip()

pygame.quit()