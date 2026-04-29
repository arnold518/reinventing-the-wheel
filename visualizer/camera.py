import math

import pygame


class Camera:
    def __init__(self, screen_size):
        self.offset = pygame.Vector2(0, 0)
        self.zoom = 1.0
        self.dragging = False
        self.last_mouse_pos = None
        self.screen_size = pygame.Vector2(screen_size)

    def handle_event(self, event):
        if event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 1:
                self.dragging = True
                self.last_mouse_pos = pygame.Vector2(event.pos)

        elif event.type == pygame.MOUSEBUTTONUP:
            if event.button == 1:
                self.dragging = False
                self.last_mouse_pos = None

        elif event.type == pygame.MOUSEMOTION:
            if self.dragging:
                mouse_pos = pygame.Vector2(event.pos)
                delta = (mouse_pos - self.last_mouse_pos) / self.zoom
                self.offset -= delta
                self.last_mouse_pos = mouse_pos

        elif event.type == pygame.MOUSEWHEEL:
            mouse_screen = pygame.Vector2(pygame.mouse.get_pos())
            self._zoom_at_point(1.1 if event.y > 0 else 0.9, mouse_screen)

    def _zoom_at_point(self, zoom_factor, screen_point):
        before_zoom_world = self.screen_to_world(screen_point)
        self.zoom *= zoom_factor
        after_zoom_world = self.screen_to_world(screen_point)
        self.offset += before_zoom_world - after_zoom_world

    def apply(self, world_pos):
        return (pygame.Vector2(world_pos) - self.offset) * self.zoom

    def screen_to_world(self, screen_pos):
        return pygame.Vector2(screen_pos) / self.zoom + self.offset

    def frame_target(self, target_rect, padding=0.1):
        if not target_rect or target_rect.width == 0 or target_rect.height == 0:
            self.offset = pygame.Vector2(0, 0)
            self.zoom = 1.0
            return

        padded_screen_w = self.screen_size.x * (1 - padding)
        padded_screen_h = self.screen_size.y * (1 - padding)

        zoom_x = padded_screen_w / target_rect.width
        zoom_y = padded_screen_h / target_rect.height
        self.zoom = min(zoom_x, zoom_y)

        screen_center = self.screen_size / 2
        world_center = pygame.Vector2(target_rect.center)
        self.offset = world_center - screen_center / self.zoom

    def draw_grid(self, screen):
        target_pixels_per_line = 100
        world_units_per_line = target_pixels_per_line / self.zoom

        power_of_ten = 10 ** math.floor(math.log10(world_units_per_line))
        grid_spacing = power_of_ten
        if world_units_per_line / power_of_ten < 0.5:
            grid_spacing = power_of_ten / 2
        if world_units_per_line / power_of_ten > 2:
            grid_spacing = power_of_ten * 2

        line_color = (45, 45, 45)
        bold_line_color = (60, 60, 60)

        world_view_width = self.screen_size.x / self.zoom
        world_view_height = self.screen_size.y / self.zoom

        x_start = int(self.offset.x // grid_spacing) * grid_spacing
        y_start = int(self.offset.y // grid_spacing) * grid_spacing
        x_end = x_start + world_view_width + grid_spacing
        y_end = y_start + world_view_height + grid_spacing
        step = max(1, int(grid_spacing))

        num = 0
        for x in range(int(x_start), int(x_end), step):
            start = self.apply((x, y_start))
            end = self.apply((x, y_end))
            color = bold_line_color if num % 10 == 0 else line_color
            pygame.draw.line(screen, color, start, end)
            num += 1

        num = 0
        for y in range(int(y_start), int(y_end), step):
            start = self.apply((x_start, y))
            end = self.apply((x_end, y))
            color = bold_line_color if num % 10 == 0 else line_color
            pygame.draw.line(screen, color, start, end)
            num += 1
