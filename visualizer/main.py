# main.py

import pygame
from camera import Camera
from ui_elements import Button, Slider

pygame.init()

# Screen settings
SCREEN_WIDTH, SCREEN_HEIGHT = 1920, 1080
screen = pygame.display.set_mode((SCREEN_WIDTH, SCREEN_HEIGHT))
pygame.display.set_caption("Circuit Simulator - Corrected")

clock = pygame.time.Clock()
camera = Camera((SCREEN_WIDTH, SCREEN_HEIGHT))

# --- UI Element Callbacks ---
def on_reset_button_click():
    """Resets the camera and updates the slider to match."""
    camera.reset()
    zoom_slider.val = camera.zoom
    zoom_slider._update_handle_pos()

def on_zoom_slider_change(new_zoom):
    """Updates the camera's zoom level from the slider."""
    # To prevent a feedback loop, only set camera zoom if it's different
    if abs(camera.zoom - new_zoom) > 0.01:
        camera.zoom = new_zoom

# --- UI Element Instances ---
reset_button = Button(
    x=20, y=20, width=150, height=50,
    text='Reset View',
    on_click=on_reset_button_click
)

zoom_slider = Slider(
    x=20, y=100, width=200, height=10,
    min_val=camera.min_zoom,
    max_val=camera.max_zoom,
    initial_val=camera.zoom,
    on_change=on_zoom_slider_change
)
# A list to easily manage all UI elements
ui_elements = [reset_button, zoom_slider]


# Dummy gate positions (world coordinates)
gate_positions = [
    (100, 100),
    (300, 200),
    (500, 400),
]

# Main loop
running = True
while running:
    dt = clock.tick(60)

    # --- Event Handling ---
    mouse_pos = pygame.mouse.get_pos()
    # Check if the mouse is over any part of the UI. Note we check the slider's handle and track.
    mouse_over_ui = any(
        (hasattr(elem, 'rect') and elem.rect.collidepoint(mouse_pos)) or \
        (hasattr(elem, 'handle_rect') and elem.handle_rect.collidepoint(mouse_pos))
        for elem in ui_elements
    )

    for event in pygame.event.get():
        if event.type == pygame.QUIT:
            running = False
        
        # 1. Pass all events to UI elements for them to process
        reset_button.handle_event(event)
        zoom_slider.handle_event(event)
        
        # 2. Pass events to the camera, but ONLY if the mouse is not over the UI
        is_mouse_event = event.type in (pygame.MOUSEBUTTONDOWN, pygame.MOUSEBUTTONUP, pygame.MOUSEMOTION, pygame.MOUSEWHEEL)
        if not (is_mouse_event and mouse_over_ui):
            camera.handle_event(event)

    # --- State Updates ---
    # Sync slider with camera zoom if it was changed by the mouse wheel (not by dragging the slider)
    if not zoom_slider.is_dragging and abs(zoom_slider.get_value() - camera.zoom) > 0.01:
        zoom_slider.val = camera.zoom
        zoom_slider._update_handle_pos()

    # --- Drawing ---
    screen.fill((30, 30, 30))

    # Draw World (affected by camera)
    camera.draw_grid(screen)

    for pos in gate_positions:
        screen_pos = camera.apply(pos)
        rect = pygame.Rect(screen_pos.x - 20, screen_pos.y - 20, 40, 40)
        pygame.draw.rect(screen, (0, 200, 255), rect)

    # Draw UI (not affected by camera)
    for element in ui_elements:
        element.draw(screen)

    # --- Update Display ---
    pygame.display.flip()

pygame.quit()