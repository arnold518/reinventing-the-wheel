import pygame
import circuit_backend

STATE_COLORS = {
    circuit_backend.LogicValue.HIGH: (76, 175, 80),
    circuit_backend.LogicValue.LOW: (211, 47, 47),
    circuit_backend.LogicValue.UNKNOWN: (158, 158, 158),
    circuit_backend.LogicValue.HIGH_Z: (3, 155, 229)
}

BUS_MIXED_COLOR = (255, 193, 7)
BADGE_BG_COLOR = (18, 24, 32, 220)
BADGE_BORDER_COLOR = (220, 230, 240)
BADGE_TEXT_COLOR = (245, 245, 245)

def _logic_token(value):
    if value == circuit_backend.LogicValue.HIGH:
        return "1"
    if value == circuit_backend.LogicValue.LOW:
        return "0"
    if value == circuit_backend.LogicValue.HIGH_Z:
        return "Z"
    return "X"

def _signal_width(cpp_handle):
    try:
        return max(1, int(cpp_handle.get_width()))
    except (AttributeError, TypeError, ValueError):
        return 1

def _signal_bits(cpp_handle):
    width = _signal_width(cpp_handle)
    bits = []
    for index in range(width):
        try:
            bits.append(cpp_handle.get_bit(index))
        except (AttributeError, IndexError, RuntimeError):
            if index == 0:
                bits.append(cpp_handle.get_value())
            else:
                bits.append(circuit_backend.LogicValue.UNKNOWN)
    return bits

def _format_signal_value(cpp_handle):
    width = _signal_width(cpp_handle)
    bits = _signal_bits(cpp_handle)

    if width == 1:
        return _logic_token(bits[0])

    return "".join(_logic_token(bit) for bit in reversed(bits))

def _aggregate_signal_color(cpp_handle):
    bits = _signal_bits(cpp_handle)
    if not bits:
        return STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]

    first = bits[0]
    if all(bit == first for bit in bits):
        return STATE_COLORS.get(first, STATE_COLORS[circuit_backend.LogicValue.UNKNOWN])

    if any(bit == circuit_backend.LogicValue.UNKNOWN for bit in bits):
        return STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
    if any(bit == circuit_backend.LogicValue.HIGH_Z for bit in bits):
        return STATE_COLORS[circuit_backend.LogicValue.HIGH_Z]
    return BUS_MIXED_COLOR

def _draw_text_badge(screen, text, screen_pos, font_size, padding=4):
    font = VisualComponent.get_font(max(8, int(font_size)))
    text_surf = font.render(text, True, BADGE_TEXT_COLOR)
    rect = text_surf.get_rect(center=(int(screen_pos.x), int(screen_pos.y)))
    rect.inflate_ip(padding * 2, padding)

    badge = pygame.Surface(rect.size, pygame.SRCALPHA)
    badge.fill(BADGE_BG_COLOR)
    screen.blit(badge, rect)
    pygame.draw.rect(screen, BADGE_BORDER_COLOR, rect, 1, border_radius=3)
    screen.blit(text_surf, text_surf.get_rect(center=rect.center))

def _draw_text_box(screen, text, screen_pos, font_size, anchor="midleft", padding=6):
    font = VisualComponent.get_font(max(8, int(font_size)))
    text_surf = font.render(text, True, BADGE_TEXT_COLOR)
    rect = text_surf.get_rect()
    setattr(rect, anchor, (int(screen_pos.x), int(screen_pos.y)))
    rect.inflate_ip(padding * 2, padding)
    rect.clamp_ip(screen.get_rect().inflate(-4, -4))

    box = pygame.Surface(rect.size, pygame.SRCALPHA)
    box.fill(BADGE_BG_COLOR)
    screen.blit(box, rect)
    pygame.draw.rect(screen, BADGE_BORDER_COLOR, rect, 1, border_radius=4)
    screen.blit(text_surf, text_surf.get_rect(center=rect.center))

class VisualPin:
    def __init__(self, parent_component, cpp_pin_handle, pin_type: str):
        self.parent = parent_component
        self.cpp_handle = cpp_pin_handle
        self.pin_type = pin_type
        self.name = self.cpp_handle.get_name()
        self.width = _signal_width(self.cpp_handle)
        
        # Geometry & Hitbox
        self.rect = pygame.Rect(0, 0, 1, 1) # Integer rect for events
        self.pos = pygame.Vector2(0, 0)     # Float center position
        
        # Stub Geometries: Tuples of (PointOnPin, PointOnBoundary)
        self.outer_stub = (pygame.Vector2(0,0), pygame.Vector2(0,0))
        self.inner_stub = (pygame.Vector2(0,0), pygame.Vector2(0,0))
        
        # Appearance
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
        self.text_color = (220, 220, 220)
        self.rel_points = []
        self._text_surf = None
        self._last_font_size = -1
        self._last_label_text = None
        self.is_hovered = False

    def update_state(self):
        self.color = _aggregate_signal_color(self.cpp_handle)

    def _display_name(self):
        if self.width == 1:
            return self.name
        return f"{self.name}[{self.width}]"

    def _label_text(self):
        return self._display_name()

    def _tooltip_text(self):
        return f"{self._display_name()} = {_format_signal_value(self.cpp_handle)}"

    def _draw_hover_tooltip(self, screen, camera, screen_center):
        if not self.is_hovered:
            return

        offset = max(12, int(self.rect.width * camera.zoom * 0.75))
        if self.pin_type == 'input':
            tooltip_pos = screen_center + pygame.Vector2(offset, -18)
            anchor = "midleft"
        else:
            tooltip_pos = screen_center + pygame.Vector2(-offset, -18)
            anchor = "midright"

        _draw_text_box(screen, self._tooltip_text(), tooltip_pos, 14, anchor=anchor)

    def draw(self, screen, camera):
        # Draw only the Triangle and Text. Wires/Stubs are drawn by VisualWire.
        screen_center = camera.apply(self.pos)
        zoomed_points = [screen_center + p * camera.zoom for p in self.rel_points]
        
        pygame.draw.polygon(screen, self.color, zoomed_points)
        if self.is_hovered:
            pygame.draw.polygon(screen, (255, 255, 0), zoomed_points, width=2)
        
        font_size = int(self.parent.rect.width * (self.parent.font_width_ratio * 0.7) * camera.zoom)
        if font_size >= 8:
            label_text = self._label_text()
            if font_size != self._last_font_size or label_text != self._last_label_text:
                font = VisualComponent.get_font(font_size)
                self._text_surf = font.render(label_text, True, self.text_color)
                self._last_font_size = font_size
                self._last_label_text = label_text
            if self._text_surf:
                if self.pin_type == 'input':
                    # Text inside body (Right of left-edge pin)
                    text_pos = screen_center + pygame.Vector2((self.rect.width * 0.8 * camera.zoom), 0)
                    text_rect = self._text_surf.get_rect(midleft=text_pos)
                else:
                    # Text inside body (Left of right-edge pin)
                    text_pos = screen_center - pygame.Vector2((self.rect.width * 0.8 * camera.zoom), 0)
                    text_rect = self._text_surf.get_rect(midright=text_pos)
                screen.blit(self._text_surf, text_rect)

        self._draw_hover_tooltip(screen, camera, screen_center)

class VisualWire:
    def __init__(self, cpp_wire_handle):
        self.cpp_handle = cpp_wire_handle
        self.source_vpin = None
        self.sink_vpins = []
        self.width = _signal_width(self.cpp_handle)
        self.color = STATE_COLORS[circuit_backend.LogicValue.UNKNOWN]
        self.is_hovered = False
        self._world_paths = []

    def connect(self, pin_map):
        source_cpp = self.cpp_handle.get_source_pin()
        if source_cpp and (owner := source_cpp.get_owner()):
            key = f"{owner.get_id()}.{source_cpp.get_name()}"
            self.source_vpin = pin_map.get(key)
        
        for sink_cpp in self.cpp_handle.get_sink_pins():
            if (owner := sink_cpp.get_owner()):
                key = f"{owner.get_id()}.{sink_cpp.get_name()}"
                if (vpin := pin_map.get(key)):
                    self.sink_vpins.append(vpin)

    def update_state(self):
        self.color = _aggregate_signal_color(self.cpp_handle)

    def _base_line_width(self):
        return 2

    def _point_at_fraction(self, path, fraction):
        if not path:
            return pygame.Vector2(0, 0)

        segment_lengths = []
        total_length = 0
        for start, end in zip(path, path[1:]):
            length = (end - start).length()
            segment_lengths.append(length)
            total_length += length

        if total_length <= 0:
            return pygame.Vector2(path[0])

        target = total_length * fraction
        traversed = 0
        for index, length in enumerate(segment_lengths):
            if traversed + length >= target:
                local_t = (target - traversed) / length if length else 0
                return path[index].lerp(path[index + 1], local_t)
            traversed += length
        return pygame.Vector2(path[-1])

    def _draw_bus_slash(self, screen, camera, world_pos):
        center = camera.apply(world_pos)
        length = max(6, int(10 * camera.zoom))
        half = length / 2
        start = center + pygame.Vector2(-half, half)
        end = center + pygame.Vector2(half, -half)
        pygame.draw.line(screen, BADGE_TEXT_COLOR, start, end, max(1, int(2 * camera.zoom)))

    def _draw_bus_annotations(self, screen, camera, path_points):
        if self.width <= 1:
            return

        self._draw_bus_slash(screen, camera, self._point_at_fraction(path_points, 0.33))
        self._draw_bus_slash(screen, camera, self._point_at_fraction(path_points, 0.66))

        if camera.zoom < 0.25 and not self.is_hovered:
            return

        width_pos = camera.apply(self._point_at_fraction(path_points, 0.40)) + pygame.Vector2(0, -14 * camera.zoom)
        _draw_text_badge(screen, str(self.width), width_pos, 12 * camera.zoom)

        if camera.zoom >= 0.45 or self.is_hovered:
            value_text = _format_signal_value(self.cpp_handle)
            value_pos = camera.apply(self._point_at_fraction(path_points, 0.55)) + pygame.Vector2(0, 14 * camera.zoom)
            _draw_text_badge(screen, value_text, value_pos, 12 * camera.zoom)

    def collidepoint(self, world_pos, camera):
        threshold_sq = (max(5, self._base_line_width() * 2) / camera.zoom) ** 2
        for path in self._world_paths:
            for i in range(len(path) - 1):
                p1, p2 = path[i], path[i+1]
                line_vec = p2 - p1
                line_len_sq = line_vec.length_squared()
                if line_len_sq == 0: continue
                
                point_vec = world_pos - p1
                t = max(0, min(1, point_vec.dot(line_vec) / line_len_sq))
                projection = p1 + t * line_vec
                
                if (world_pos - projection).length_squared() < threshold_sq:
                    return True
        return False

    def _get_active_stub(self, pin, is_source):
        # LOGIC:
        # Source (Input Pin) -> Parent Passthrough -> Use INNER
        # Source (Output Pin) -> Child Output -> Use OUTER
        # Sink (Input Pin) -> Child Input -> Use OUTER
        # Sink (Output Pin) -> Parent Passthrough -> Use INNER
        
        is_input = (pin.pin_type == 'input')
        
        if is_source:
            if is_input: return pin.inner_stub 
            else:        return pin.outer_stub
        else: # is sink
            if is_input: return pin.outer_stub
            else:        return pin.inner_stub

    def _dedupe_path(self, path):
        deduped = []
        for point in path:
            point = pygame.Vector2(point)
            if not deduped or (point - deduped[-1]).length_squared() > 0.01:
                deduped.append(point)
        return deduped

    def _route_adaptive_2segment(self, start_pos, end_pos):
        start = pygame.Vector2(start_pos)
        end = pygame.Vector2(end_pos)
        dx = end.x - start.x

        if dx > 10:
            path = [start, pygame.Vector2(end.x, start.y), end]
        else:
            mid_x = (start.x + end.x) / 2
            path = [start, pygame.Vector2(mid_x, start.y), pygame.Vector2(mid_x, end.y), end]

        return self._dedupe_path(path)

    def _add_rounded_corners(self, path, radius=5):
        path = self._dedupe_path(path)
        if len(path) < 3:
            return path

        rounded = [path[0]]
        for index in range(1, len(path) - 1):
            prev_point = pygame.Vector2(path[index - 1])
            corner = pygame.Vector2(path[index])
            next_point = pygame.Vector2(path[index + 1])

            incoming = corner - prev_point
            outgoing = next_point - corner
            incoming_len = incoming.length()
            outgoing_len = outgoing.length()

            if incoming_len < radius * 2 or outgoing_len < radius * 2:
                rounded.append(corner)
                continue

            incoming_dir = incoming.normalize()
            outgoing_dir = outgoing.normalize()
            if abs(incoming_dir.dot(outgoing_dir)) > 0.999:
                rounded.append(corner)
                continue

            corner_radius = min(radius, incoming_len / 2, outgoing_len / 2)
            arc_start = corner - incoming_dir * corner_radius
            arc_end = corner + outgoing_dir * corner_radius
            rounded.append(arc_start)

            for step in range(1, 4):
                t = step / 4
                point = ((1 - t) ** 2 * arc_start
                         + 2 * (1 - t) * t * corner
                         + t ** 2 * arc_end)
                rounded.append(point)

            rounded.append(arc_end)

        rounded.append(path[-1])
        return self._dedupe_path(rounded)
        
    def draw(self, screen, camera):
        self.update_state()
        if not self.source_vpin or not self.sink_vpins:
            return

        line_width = max(1, int(self._base_line_width() * camera.zoom))
        if self.is_hovered:
            line_width *= 2

        self._world_paths.clear()

        # 1. Determine Source Geometry
        src_pin_pt, src_bound_pt = self._get_active_stub(self.source_vpin, is_source=True)

        # Draw Source Stub (Pin <-> Boundary)
        self._world_paths.append([src_pin_pt, src_bound_pt])
        pygame.draw.line(screen, self.color, camera.apply(src_pin_pt), camera.apply(src_bound_pt), line_width)

        # 2. Route to Sinks
        for sink_pin in self.sink_vpins:
            dst_pin_pt, dst_bound_pt = self._get_active_stub(sink_pin, is_source=False)
            
            # Draw Sink Stub (Boundary <-> Pin)
            self._world_paths.append([dst_bound_pt, dst_pin_pt])
            pygame.draw.line(screen, self.color, camera.apply(dst_bound_pt), camera.apply(dst_pin_pt), line_width)
            
            path_points = self._add_rounded_corners(
                self._route_adaptive_2segment(src_bound_pt, dst_bound_pt)
            )
            self._world_paths.append(path_points)
            screen_points = [camera.apply(p) for p in path_points]
            if len(screen_points) == 2:
                pygame.draw.line(screen, self.color, screen_points[0], screen_points[1], line_width)
            else:
                pygame.draw.lines(screen, self.color, False, screen_points, line_width)
            self._draw_bus_annotations(screen, camera, path_points)

class VisualComponent:
    _font_cache = {}
    
    @staticmethod
    def get_font(size):
        size = int(size)
        if size not in VisualComponent._font_cache:
            try:
                VisualComponent._font_cache[size] = pygame.font.SysFont('arial', size, bold=False)
            except pygame.error:
                VisualComponent._font_cache[size] = pygame.font.Font(None, size)
        return VisualComponent._font_cache[size]

    def __init__(self, rect, cpp_handle, settings, layout_key, depth=0, parent=None, color=(61, 90, 128, 100), aspect_ratio=1.0):
        self.rect = pygame.Rect(rect)
        self.cpp_handle = cpp_handle
        self.parent = parent
        self.children = []
        self.is_dragging = False
        self.is_resizing = False
        self._is_hovered = False
        self.drag_offset = pygame.Vector2(0, 0)
        self.depth = depth
        self.resize_mode = None
        self.min_width = 60
        
        self.title_bar_ratio = settings.get('title_bar_ratio', 0.15)
        self.font_width_ratio = settings.get('font_width_ratio', 0.18)
        self.p_ratio = settings.get('boundary_area_ratio', 0.15)
        self.pin_size_ratio = settings.get('pin_size_ratio', 0.10)

        self.base_color = tuple(color)
        self.hover_color = self.base_color[:3] + (150,)
        self.border_color = (238, 244, 255)
        self.text_color = (240, 240, 240)
        title_rgb = [max(0, c - 20) for c in self.base_color[:3]]
        self.title_bar_color = tuple(title_rgb + [self.base_color[3]])
        
        self._render_is_dirty = True
        self._body_surface = None
        self._title_surface = None
        self._last_zoom = -1
        
        self.layout_key = layout_key
        self.aspect_ratio = aspect_ratio

    @property
    def name(self): return self.cpp_handle.get_name()
    @property
    def is_hovered(self): return self._is_hovered
    @is_hovered.setter
    def is_hovered(self, value):
        if self._is_hovered != value:
            self._is_hovered = value
            self._render_is_dirty = True

    def _render_surfaces(self, zoom):
        zoomed_w = max(1, int(self.rect.width * zoom))
        zoomed_h = max(1, int(self.rect.height * zoom))
        zoomed_title_h = max(1, int(zoomed_w * self.title_bar_ratio))
        body_h = max(1, zoomed_h - zoomed_title_h)
        
        fill_color = self.hover_color if self.is_hovered else self.base_color
        self._body_surface = pygame.Surface((zoomed_w, body_h), pygame.SRCALPHA)
        self._body_surface.fill(fill_color)
        
        self._title_surface = pygame.Surface((zoomed_w, zoomed_title_h), pygame.SRCALPHA)
        self._title_surface.fill(self.title_bar_color)
        self._render_is_dirty = False
        
    def draw(self, screen, camera):
        if camera.zoom != self._last_zoom:
            self._render_is_dirty = True
            self._last_zoom = camera.zoom
        if self._render_is_dirty or self._body_surface is None:
            self._render_surfaces(camera.zoom)
        
        screen_pos = camera.apply(self.rect.topleft)
        zoomed_size = (self._body_surface.get_width(), self._body_surface.get_height() + self._title_surface.get_height())
        zoomed_title_h = self._title_surface.get_height()

        screen.blit(self._body_surface, (screen_pos.x, screen_pos.y + zoomed_title_h))
        screen.blit(self._title_surface, screen_pos)
        pygame.draw.rect(screen, self.border_color, (screen_pos, zoomed_size), 4 if self.is_hovered else 2, border_radius=5)
        
        font_size = self.rect.width * self.font_width_ratio * camera.zoom
        if font_size >= 10:
            font = self.get_font(font_size)
            text_surf = font.render(self.name, True, self.text_color)
            title_center = (screen_pos.x + zoomed_size[0]/2, screen_pos.y + zoomed_title_h/2)
            screen.blit(text_surf, text_surf.get_rect(center=title_center))
            
        for child in self.children:
            child.draw(screen, camera)
            
        # if self.is_hovered:
        #     p_width = self.rect.width * self.p_ratio
        #     left_zone = pygame.Rect(self.rect.left - p_width, self.rect.top, 2*p_width, self.rect.height)
        #     right_zone = pygame.Rect(self.rect.right - p_width, self.rect.top, 2*p_width, self.rect.height)
        #     l_scr = camera.apply(left_zone.topleft)
        #     l_sz = (left_zone.width * camera.zoom, left_zone.height * camera.zoom)
        #     pygame.draw.rect(screen, (255, 50, 50), (l_scr, l_sz), 1)
        #     r_scr = camera.apply(right_zone.topleft)
        #     r_sz = (right_zone.width * camera.zoom, right_zone.height * camera.zoom)
        #     pygame.draw.rect(screen, (255, 50, 50), (r_scr, r_sz), 1)

    def get_content_rect(self):
        title_height = self.rect.width * self.title_bar_ratio
        margin_x = self.rect.width * self.p_ratio
        return pygame.Rect(
            self.rect.left + margin_x, 
            self.rect.top + title_height, 
            self.rect.width - (2 * margin_x), 
            self.rect.height - title_height
        )

    def add_child(self, child_vc):
        self.children.append(child_vc)

    def get_hovered_border(self, world_pos, camera):
        if not self.is_hovered: return None
        threshold = 5 / camera.zoom
        on_left = abs(world_pos.x - self.rect.left) < threshold
        on_right = abs(world_pos.x - self.rect.right) < threshold
        on_top = abs(world_pos.y - self.rect.top) < threshold
        on_bottom = abs(world_pos.y - self.rect.bottom) < threshold
        in_y_range = self.rect.top < world_pos.y < self.rect.bottom
        in_x_range = self.rect.left < world_pos.x < self.rect.right
        
        if on_left and in_y_range: return 'left'
        if on_right and in_y_range: return 'right'
        if on_top and in_x_range: return 'top'
        if on_bottom and in_x_range: return 'bottom'
        return None

    def handle_event(self, event, camera, lm):
        world_mouse_pos = camera.screen_to_world(pygame.mouse.get_pos())
        if event.type == pygame.MOUSEBUTTONDOWN and event.button == 1 and self.is_hovered:
            border = self.get_hovered_border(world_mouse_pos, camera)
            if border:
                self.is_resizing, self.is_dragging = True, False
                self.resize_mode = border
            else:
                self.is_resizing, self.is_dragging = False, True
                self.drag_offset = world_mouse_pos - pygame.Vector2(self.rect.topleft)
            return True
        
        if event.type == pygame.MOUSEMOTION:
            if self.is_resizing:
                self.resize(self.resize_mode, world_mouse_pos, lm)
                return True
            if self.is_dragging:
                new_pos = world_mouse_pos - self.drag_offset
                self.move(new_pos, lm)
                return True
        return False

    def resize(self, mode, world_mouse_pos, lm):
        if not self.parent: return

        p_safe = self.parent.get_content_rect()
        q = self.p_ratio 

        ideal_rect = self.rect.copy()
        
        if mode == 'right':
            ideal_rect.width = max(self.min_width, world_mouse_pos.x - self.rect.left)
            ideal_rect.height = ideal_rect.width * self.aspect_ratio
        elif mode == 'left':
            new_w = max(self.min_width, self.rect.right - world_mouse_pos.x)
            ideal_rect.left = self.rect.right - new_w
            ideal_rect.width = new_w
            ideal_rect.height = new_w * self.aspect_ratio
        elif mode == 'bottom':
            ideal_rect.height = max(self.min_width * self.aspect_ratio, world_mouse_pos.y - self.rect.top)
            ideal_rect.width = ideal_rect.height / self.aspect_ratio
        elif mode == 'top':
            new_h = max(self.min_width * self.aspect_ratio, self.rect.bottom - world_mouse_pos.y)
            ideal_rect.top = self.rect.bottom - new_h
            ideal_rect.height = new_h
            ideal_rect.width = new_h / self.aspect_ratio

        max_w_from_left = (p_safe.right - ideal_rect.left) / (1 + q)
        max_w_from_right = (ideal_rect.right - p_safe.left) / (1 + q)
        
        final_w, final_h = ideal_rect.width, ideal_rect.height

        if mode == 'right':
            limit_w = max_w_from_left
            final_w = min(ideal_rect.width, limit_w)
            final_h = final_w * self.aspect_ratio
            if self.rect.top + final_h > p_safe.bottom:
                final_h = p_safe.bottom - self.rect.top
                final_w = final_h / self.aspect_ratio

        elif mode == 'left':
            limit_w = max_w_from_right
            final_w = min(ideal_rect.width, limit_w)
            final_h = final_w * self.aspect_ratio
            ideal_rect.left = self.rect.right - final_w 
            if self.rect.top + final_h > p_safe.bottom:
                final_h = p_safe.bottom - self.rect.top
                final_w = final_h / self.aspect_ratio
                ideal_rect.left = self.rect.right - final_w

        self.rect.width = final_w
        self.rect.height = final_h
        if mode == 'left': self.rect.left = ideal_rect.left
        
        new_rel_pos = [(self.rect.left - self.parent.rect.left) / self.parent.rect.width, 
                       (self.rect.top - self.parent.rect.top) / self.parent.rect.height]
        new_rel_width = self.rect.width / self.parent.rect.width
        
        if self.depth == 1:
            lm.update_instance_child_layout(self.parent.layout_key, self.name, new_rel_pos, new_rel_width)
        else:
            lm.update_type_child_layout(self.parent.cpp_handle.get_type_name(), self.name, new_rel_pos, new_rel_width)

    def move(self, new_abs_pos, lm):
        potential_pos = pygame.Vector2(new_abs_pos)
        
        if self.parent:
            p_safe = self.parent.get_content_rect()
            q_margin = self.rect.width * self.p_ratio
            
            min_x = p_safe.left + q_margin
            max_x = p_safe.right - self.rect.width - q_margin
            min_y = p_safe.top
            max_y = p_safe.bottom - self.rect.height
            
            if min_x > max_x: potential_pos.x = (min_x + max_x) / 2 
            else: potential_pos.x = max(min_x, min(potential_pos.x, max_x))
            potential_pos.y = max(min_y, min(potential_pos.y, max_y))
            
            new_rel_pos = [(potential_pos.x - self.parent.rect.left) / self.parent.rect.width, 
                           (potential_pos.y - self.parent.rect.top) / self.parent.rect.height]
            rel_width = self.rect.width / self.parent.rect.width
            
            if self.depth == 1:
                lm.update_instance_child_layout(self.parent.layout_key, self.name, new_rel_pos, rel_width)
            else:
                lm.update_type_child_layout(self.parent.cpp_handle.get_type_name(), self.name, new_rel_pos, rel_width)
        else:
            lm.update_root_position(potential_pos)

    def update_geometry(self, lm):
        self._render_is_dirty = True
        parent_type = self.cpp_handle.get_type_name()
        parent_key = self.layout_key
        
        for child in self.children:
            child_layout = lm.get_child_layout(parent_type, parent_key, child.name)
            rel_pos = child_layout.get('rel_pos', [0, 0])
            rel_width = child_layout.get('rel_width', 0.5)
            
            child.rect.width = self.rect.width * rel_width
            child.rect.height = child.rect.width * child.aspect_ratio
            child.rect.left = self.rect.x + self.rect.width * rel_pos[0]
            child.rect.top = self.rect.y + self.rect.height * rel_pos[1]
            child.update_geometry(lm)

        if isinstance(self, VisualIOComponent):
            self._layout_pins()

class VisualIOComponent(VisualComponent):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.input_pins, self.output_pins = [], []
        self._create_pins()
        self._layout_pins()

    def _create_pins(self):
        for name, pin_handle in self.cpp_handle.get_input_pins().items():
            self.input_pins.append(VisualPin(self, pin_handle, 'input'))
        for name, pin_handle in self.cpp_handle.get_output_pins().items():
            self.output_pins.append(VisualPin(self, pin_handle, 'output'))

    def _layout_pins(self):
        content_rect = self.get_content_rect()
        
        p_width = self.rect.width * self.p_ratio
        pin_w = self.rect.width * self.pin_size_ratio
        pin_h = pin_w 
        w2 = pin_w / 2
        h2 = pin_h / 2
        
        def setup_pin(pin, y_center, is_input):
            pin.rect.size = (int(pin_w), int(pin_h))
            
            if is_input:
                # INPUT (Left Edge, x=0)
                # Pin spans [-w/2, w/2] centered at Left Edge
                pin.pos = pygame.Vector2(self.rect.left, y_center)
                pin.rect.center = (int(self.rect.left), int(y_center))
                pin.rel_points = [pygame.Vector2(w2, 0), pygame.Vector2(-w2, -h2), pygame.Vector2(-w2, h2)]
                
                # Outer Stub: [-p, -w2]
                outer_pt = pygame.Vector2(self.rect.left - p_width, y_center)
                pin_edge_outer = pygame.Vector2(self.rect.left - w2, y_center)
                pin.outer_stub = (pin_edge_outer, outer_pt)
                
                # Inner Stub: [w2, p]
                inner_pt = pygame.Vector2(self.rect.left + p_width, y_center)
                pin_edge_inner = pygame.Vector2(self.rect.left + w2, y_center)
                pin.inner_stub = (pin_edge_inner, inner_pt)
                
            else:
                # OUTPUT (Right Edge, x=W)
                # Pin spans [W-w/2, W+w/2] centered at Right Edge
                pin.pos = pygame.Vector2(self.rect.right, y_center)
                pin.rect.center = (int(self.rect.right), int(y_center))
                pin.rel_points = [pygame.Vector2(w2, 0), pygame.Vector2(-w2, -h2), pygame.Vector2(-w2, h2)]
                
                # Inner Stub: [1-p, 1-w2]
                inner_pt = pygame.Vector2(self.rect.right - p_width, y_center)
                pin_edge_inner = pygame.Vector2(self.rect.right - w2, y_center)
                pin.inner_stub = (pin_edge_inner, inner_pt)
                
                # Outer Stub: [1+w2, 1+p]
                outer_pt = pygame.Vector2(self.rect.right + p_width, y_center)
                pin_edge_outer = pygame.Vector2(self.rect.right + w2, y_center)
                pin.outer_stub = (pin_edge_outer, outer_pt)

        if self.input_pins:
            spacing = content_rect.height / (len(self.input_pins) + 1)
            for i, pin in enumerate(self.input_pins):
                y = content_rect.top + spacing * (i + 1)
                setup_pin(pin, y, True)
        
        if self.output_pins:
            spacing = content_rect.height / (len(self.output_pins) + 1)
            for i, pin in enumerate(self.output_pins):
                y = content_rect.top + spacing * (i + 1)
                setup_pin(pin, y, False)

    def get_all_pins(self):
        return self.input_pins + self.output_pins
        
    def draw(self, screen, camera):
        super().draw(screen, camera)
        for pin in self.get_all_pins():
            pin.update_state()
            pin.draw(screen, camera)
