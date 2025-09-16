import pygame
import sys
import json
import circuit_backend
import math
import re
from camera import Camera
from ui_elements import Button, Slider
from visual_component import VisualComponent, VisualIOComponent, VisualWire

class LayoutManager:
    def __init__(self, filepath):
        self.filepath = filepath
        try:
            with open(filepath, 'r') as f:
                data = json.load(f)
        except FileNotFoundError:
            data = {}
        
        self.settings = data.get("default_settings", {})
        self.type_layouts = data.get("type_layouts", {})
        self.instance_layouts = data.get("instance_layouts", {})
        self.root_layout_config = data.get("root_layout_config", {"pos": [50, 150], "width": 800})

    def get_layout_for(self, component_type, instance_key):
        if instance_key in self.instance_layouts:
            return self.instance_layouts[instance_key]
        return self.type_layouts.get(component_type, {}).copy()

    def update_root_position(self, new_pos):
        self.root_layout_config['pos'] = [new_pos[0], new_pos[1]]

    def update_root_width(self, new_width):
        self.root_layout_config['width'] = new_width

    def update_child_position(self, parent_key, child_name, new_rel_pos):
        self.instance_layouts.setdefault(parent_key, {}).setdefault("children", {}).setdefault(child_name, {})['rel_pos'] = new_rel_pos

    def update_child_width(self, parent_key, child_name, new_rel_width):
        self.instance_layouts.setdefault(parent_key, {}).setdefault("children", {}).setdefault(child_name, {})['rel_width'] = new_rel_width

    def save_layout(self):
        data = {
            "default_settings": self.settings,
            "type_layouts": self.type_layouts,
            "instance_layouts": self.instance_layouts,
            "root_layout_config": self.root_layout_config
        }

        pretty_json_string = json.dumps(data, indent=4)
        def format_list_content(match):
            list_content = match.group(1)
            compact_content = re.sub(r'\s+', '', list_content)
            formatted_content = compact_content.replace(',', ', ')
            return f"[{formatted_content}]"

        final_json_string = re.sub(
            r'\[(.*?)\]',      # Regex to find everything between '[' and ']'
            format_list_content, # The function to call for each match
            pretty_json_string,  # The string to process
            flags=re.DOTALL
        )
        
        with open(self.filepath, 'w') as f:
            f.write(final_json_string)
        print(f"Layout saved to {self.filepath}")

class App:
    def __init__(self):
        pygame.init()
        self.screen_width, self.screen_height = 1920, 1080
        self.screen = pygame.display.set_mode((self.screen_width, self.screen_height))
        pygame.display.set_caption("Circuit Simulator")
        self.clock = pygame.time.Clock()
        self.camera = Camera((self.screen_width, self.screen_height))
        self.running = True

        self.test_scenario = None
        self.root_vc = None
        self.simulator = None
        
        self.all_components = []
        self.all_visual_pins = []
        self.all_wires = []
        self.event_timestamps = []
        self.active_interaction_component = None
        
        self.current_time_index = 0
        self.previous_time_index = -1
        self.is_playing = False
        self.playback_timer = 0
        self.playback_interval = 250

        self.layout_manager = LayoutManager("layout.json")

    def initialize_circuit(self):
        self.test_scenario = circuit_backend.HalfAdderTest()
        self.test_scenario.setup_circuit()
        root_cpp = self.test_scenario.get_root()
        if not root_cpp:
            raise RuntimeError("C++ test scenario did not produce a root component!")

        self.root_vc, self.all_components, self.all_visual_pins, self.all_wires = self._build_visual_hierarchy(root_cpp)
        
        self.simulator = self.test_scenario.get_simulator()
        self.simulator.run_and_record(self.test_scenario.get_run_duration())
        self.event_timestamps = self.simulator.get_unique_timestamps()
        self.simulator.set_circuit_state_at_time(0)

    def _build_visual_hierarchy(self, cpp_root):
        all_components_map = {}
        all_visual_pins = []

        root_id = cpp_root.get_id()
        root_layout = self.layout_manager.get_layout_for(cpp_root.get_type_name(), root_id)
        
        root_pos = self.layout_manager.root_layout_config['pos']
        root_width = self.layout_manager.root_layout_config['width']
        
        default_color = (61, 90, 128, 100)
        root_color = root_layout.get('color', default_color)
        aspect_ratio = root_layout.get('aspect_ratio', 0.75)
        root_height = root_width * aspect_ratio
        root_rect = pygame.Rect(root_pos, (root_width, root_height))
        
        RootClass = VisualIOComponent if hasattr(cpp_root, 'get_input_pins') else VisualComponent
        root_vc = RootClass(rect=root_rect, cpp_handle=cpp_root, settings=self.layout_manager.settings, 
                            layout_key=root_id, rel_info={}, depth=0, color=root_color, aspect_ratio=aspect_ratio)

        all_components_map[root_id] = root_vc
        if isinstance(root_vc, VisualIOComponent):
            all_visual_pins.extend(root_vc.get_all_pins())
        
        self._build_recursive_step(root_vc, all_components_map, all_visual_pins)
        
        all_components = list(all_components_map.values())
        pin_map = {pin.parent.cpp_handle.get_id() + "." + pin.name: pin for pin in all_visual_pins}
        
        all_wires = []
        def collect_wires_recursively(component):
            all_wires.extend([VisualWire(w) for w in component.get_wires()])
            for child in component.get_children():
                collect_wires_recursively(child)
        collect_wires_recursively(cpp_root)

        for wire in all_wires:
            wire.connect(pin_map)
            
        return root_vc, all_components, all_visual_pins, all_wires

    def _build_recursive_step(self, parent_vc, all_components_map, all_visual_pins):
        parent_layout = self.layout_manager.get_layout_for(parent_vc.cpp_handle.get_type_name(), parent_vc.layout_key)
        
        children_cpp = parent_vc.cpp_handle.get_children()
        num_siblings = len(children_cpp)
        if num_siblings == 0: return

        cols = int(math.ceil(math.sqrt(num_siblings)))
        rows = int(math.ceil(num_siblings / cols))
        
        for i, child_cpp in enumerate(children_cpp):
            child_name = child_cpp.get_name()
            child_info = parent_layout.get("children", {}).get(child_name, {})

            if 'rel_pos' not in child_info or 'rel_width' not in child_info:
                padding = 0.1
                cell_w = (1.0 - padding * (cols + 1)) / cols if cols > 0 else 0
                cell_h = (1.0 - padding * (rows + 1)) / rows if rows > 0 else 0
                col, row = i % cols, i // cols
                child_info['rel_width'] = cell_w
                child_info['rel_pos'] = [padding + col * (cell_w + padding), padding + row * (cell_h + padding)]
                
                self.layout_manager.update_child_position(parent_vc.layout_key, child_name, child_info['rel_pos'])
                self.layout_manager.update_child_width(parent_vc.layout_key, child_name, child_info['rel_width'])


            child_id = child_cpp.get_id()
            my_layout = self.layout_manager.get_layout_for(child_cpp.get_type_name(), child_id)
            
            aspect_ratio = my_layout.get('aspect_ratio', 1.0)
            color = my_layout.get('color', (61, 90, 128, 100))
            
            rel_pos, rel_width = child_info['rel_pos'], child_info['rel_width']
            
            abs_w = parent_vc.rect.width * rel_width
            abs_h = abs_w * aspect_ratio
            abs_x = parent_vc.rect.x + parent_vc.rect.width * rel_pos[0]
            abs_y = parent_vc.rect.y + parent_vc.rect.height * rel_pos[1]
            child_rect = pygame.Rect(abs_x, abs_y, abs_w, abs_h)

            VC_Class = VisualIOComponent if hasattr(child_cpp, 'get_input_pins') else VisualComponent
            child_vc = VC_Class(rect=child_rect, cpp_handle=child_cpp, settings=self.layout_manager.settings,
                                layout_key=child_id, rel_info=child_info, depth=parent_vc.depth + 1, 
                                parent=parent_vc, color=color, aspect_ratio=aspect_ratio)
            
            parent_vc.add_child(child_vc)
            all_components_map[child_id] = child_vc
            
            if isinstance(child_vc, VisualIOComponent):
                all_visual_pins.extend(child_vc.get_all_pins())
            
            self._build_recursive_step(child_vc, all_components_map, all_visual_pins)

    def _create_ui_elements(self):
        self.time_slider = Slider(x=20, y=20, width=self.screen_width-40, height=15, min_val=0, max_val=len(self.event_timestamps)-1, initial_val=0, on_change=self.on_time_slider_change)
        self.play_pause_button = Button(x=20, y=50, width=100, height=50, text='Play', on_click=self.on_play_pause_click)
        self.step_backward_button = Button(x=130, y=50, width=50, height=50, text='<', text_size=36, on_click=self.on_step_backward)
        self.step_forward_button = Button(x=190, y=50, width=50, height=50, text='>', text_size=36, on_click=self.on_step_forward)
        self.reset_sim_button = Button(x=250, y=50, width=100, height=50, text='Reset', on_click=self.on_reset_click)
        
        self.reset_view_button = Button(x=20, y=self.screen_height-65, width=150, height=50, text='Reset View', on_click=self.on_reset_view)
        self.zoom_out_button = Button(x=180, y=self.screen_height-65, width=50, height=50, text='-', text_size=48, on_click=self.on_zoom_out)
        self.zoom_in_button = Button(x=240, y=self.screen_height-65, width=50, height=50, text='+', text_size=48, on_click=self.on_zoom_in)
        self.save_layout_button = Button(x=310, y=self.screen_height-65, width=150, height=50, text='Save Layout', on_click=self.layout_manager.save_layout)
        
        self.ui_elements = [self.time_slider, self.play_pause_button, self.step_backward_button, self.step_forward_button,
                            self.reset_sim_button, self.reset_view_button, self.zoom_in_button, self.zoom_out_button, self.save_layout_button]
        self.time_font = VisualComponent.get_font(32)

    def on_zoom_in(self):
        screen_center = self.camera.screen_size / 2
        self.camera._zoom_at_point(1.25, screen_center)

    def on_zoom_out(self):
        screen_center = self.camera.screen_size / 2
        self.camera._zoom_at_point(0.8, screen_center)

    def on_reset_view(self):
        if self.root_vc:
            self.camera.frame_target(self.root_vc.rect)

    def on_play_pause_click(self):
        self.is_playing = not self.is_playing
        self.play_pause_button.set_text("Pause" if self.is_playing else "Play")

    def on_step_forward(self):
        self.current_time_index = min(len(self.event_timestamps) - 1, self.current_time_index + 1)
        if self.is_playing: self.on_play_pause_click()

    def on_step_backward(self):
        self.current_time_index = max(0, self.current_time_index - 1)
        if self.is_playing: self.on_play_pause_click()

    def on_reset_click(self):
        self.current_time_index = 0
        if self.is_playing: self.on_play_pause_click()

    def on_time_slider_change(self, index):
        self.current_time_index = int(index)

    def _update_state(self):
        if self.is_playing and pygame.time.get_ticks() - self.playback_timer > self.playback_interval:
            self.current_time_index = min(len(self.event_timestamps) - 1, self.current_time_index + 1)
            self.playback_timer = pygame.time.get_ticks()
            if self.current_time_index == len(self.event_timestamps) - 1:
                self.on_play_pause_click()

        if self.current_time_index != self.previous_time_index:
            self.simulator.set_circuit_state_at_time(self.event_timestamps[self.current_time_index])
            if not self.time_slider.is_dragging:
                self.time_slider.val = self.current_time_index
                self.time_slider._update_handle_pos()
            self.previous_time_index = self.current_time_index

    def _handle_input(self):
        mouse_pos = pygame.mouse.get_pos()
        world_mouse_pos = self.camera.screen_to_world(mouse_pos)
        
        mouse_over_ui = any(e.rect.collidepoint(mouse_pos) for e in self.ui_elements if hasattr(e, 'rect'))

        hovered_item = None
        if not self.active_interaction_component and not mouse_over_ui:
            for item in self.all_visual_pins + self.all_wires + self.all_components:
                item.is_hovered = False
            
            hovered_pin = next((p for p in self.all_visual_pins if p.rect.inflate(4, 4).collidepoint(world_mouse_pos)), None)
            hovered_wire = None if hovered_pin else next((w for w in self.all_wires if w.collidepoint(world_mouse_pos, self.camera)), None)
            hovered_component = None if (hovered_pin or hovered_wire) else next((c for c in reversed(self.all_components) if c.rect.collidepoint(world_mouse_pos)), None)
            
            if hovered_pin: hovered_pin.is_hovered = True; hovered_item = hovered_pin
            elif hovered_wire: hovered_wire.is_hovered = True; hovered_item = hovered_wire
            elif hovered_component: hovered_component.is_hovered = True; hovered_item = hovered_component
        
        resize_border = None
        if isinstance(hovered_item, VisualComponent) and not self.active_interaction_component:
             resize_border = hovered_item.get_hovered_border(world_mouse_pos, self.camera)
        
        if resize_border in ['left', 'right']: pygame.mouse.set_cursor(pygame.SYSTEM_CURSOR_SIZEWE)
        elif resize_border in ['top', 'bottom']: pygame.mouse.set_cursor(pygame.SYSTEM_CURSOR_SIZENS)
        else: pygame.mouse.set_cursor(pygame.SYSTEM_CURSOR_ARROW)

        event_consumed_by_component = False
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
            
            for e in self.ui_elements: e.handle_event(event)
            
            interaction_target = self.active_interaction_component or (hovered_item if isinstance(hovered_item, VisualComponent) else None)
            
            if not mouse_over_ui and interaction_target:
                if interaction_target.handle_event(event, self.camera, self.layout_manager):
                    event_consumed_by_component = True
                
                if event.type == pygame.MOUSEBUTTONDOWN and (interaction_target.is_dragging or interaction_target.is_resizing):
                    self.active_interaction_component = interaction_target
                elif event.type == pygame.MOUSEBUTTONUP:
                    if self.active_interaction_component:
                        self.active_interaction_component.is_dragging = False
                        self.active_interaction_component.is_resizing = False
                    self.active_interaction_component = None
            
            is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
            if is_mouse_event and not mouse_over_ui and not event_consumed_by_component:
                self.camera.handle_event(event)
            
    def _draw_graphics(self):
        self.screen.fill((30, 30, 30))
        self.camera.draw_grid(self.screen)
        
        for wire in self.all_wires:
            wire.draw(self.screen, self.camera)
            
        if self.root_vc:
            self.root_vc.draw(self.screen, self.camera)
        
        time_text = f"Time: {self.event_timestamps[self.current_time_index]}"
        time_surf = self.time_font.render(time_text, True, (220, 220, 220))
        self.screen.blit(time_surf, time_surf.get_rect(left=370, centery=self.play_pause_button.rect.centery))
        
        for e in self.ui_elements:
            e.draw(self.screen)
            
        pygame.display.flip()

    def run(self):
        try:
            self.initialize_circuit()
            self._create_ui_elements()
        except Exception as e:
            print(f"\n[FATAL ERROR] Could not initialize: {e}")
            pygame.quit()
            sys.exit(1)

        while self.running:
            self.clock.tick(60)
            self._handle_input()
            self._update_state()
            self._draw_graphics()
        
        pygame.quit()

if __name__ == '__main__':
    app = App()
    app.run()