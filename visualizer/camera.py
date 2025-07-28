# camera.py

import pygame

class Camera:
    def __init__(self, screen_size):
        self.offset = pygame.Vector2(0, 0)
        self.zoom = 1.0
        self.min_zoom = 0.2
        self.max_zoom = 5.0
        self.dragging = False
        self.last_mouse_pos = None
        self.screen_size = pygame.Vector2(screen_size)

    def handle_event(self, event):
        # This method is now simpler. It no longer tries to guess UI locations.
        if event.type == pygame.MOUSEBUTTONDOWN:
            if event.button == 1:  # Left click starts drag
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
            before_zoom = self.screen_to_world(mouse_screen)

            zoom_factor = 1.1 if event.y > 0 else 0.9
            new_zoom = self.zoom * zoom_factor
            self.zoom = max(self.min_zoom, min(self.max_zoom, new_zoom))

            after_zoom = self.screen_to_world(mouse_screen)
            self.offset += before_zoom - after_zoom

    def apply(self, world_pos):
        """Convert world coordinates to screen coordinates."""
        return (pygame.Vector2(world_pos) - self.offset) * self.zoom

    def screen_to_world(self, screen_pos):
        """Convert screen coordinates to world coordinates."""
        return pygame.Vector2(screen_pos) / self.zoom + self.offset

    def reset(self):
        self.offset = pygame.Vector2(0, 0)
        self.zoom = 1.0

    def draw_grid(self, screen, grid_spacing=50, grid_color=(50, 50, 50)):
        """Draws an optimized grid onto the screen, only rendering what is visible."""
        world_view_width = self.screen_size.x / self.zoom
        world_view_height = self.screen_size.y / self.zoom
        world_view = pygame.Rect(
            self.offset.x, self.offset.y,
            world_view_width, world_view_height
        )

        x_start = int(world_view.left // grid_spacing) * grid_spacing
        x_end = int(world_view.right // grid_spacing) * grid_spacing + grid_spacing
        y_start = int(world_view.top // grid_spacing) * grid_spacing
        y_end = int(world_view.bottom // grid_spacing) * grid_spacing + grid_spacing

        for x in range(x_start, x_end, grid_spacing):
            start = self.apply((x, y_start))
            end = self.apply((x, y_end))
            pygame.draw.line(screen, grid_color, start, end)

        for y in range(y_start, y_end, grid_spacing):
            start = self.apply((x_start, y))
            end = self.apply((x_end, y))
            pygame.draw.line(screen, grid_color, start, end)