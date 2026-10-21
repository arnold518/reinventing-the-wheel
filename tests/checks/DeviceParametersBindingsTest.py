"""Declare device parameters through the same catalog path as cache helpers."""
import circuit_backend as backend
import server


def rejects(call):
    try:
        call()
    except (ValueError, RuntimeError, OverflowError):
        return
    raise AssertionError('Unsupported device parameters were accepted')


for width, height in ((4, 4), (128, 96), (320, 200), (97, 65)):
    display = backend.create_device_component('device.framebuffer.32', 'DISPLAY',
        {'width': str(width), 'height': str(height)})
    assert display.get_type_name() == 'FramebufferDisplay32'
    key = server._component_layout_type(display)
    assert key == 'FramebufferDisplay32' if (width, height) == (4, 4) else key.startswith(
        f'FramebufferDisplay32[config-width={width},height={height},storage=')

for width in (8, 32):
    for depth in (1, 3, 4, 7):
        for fidelity in ('structural', 'behavioral'):
            fifo = backend.create_device_component(f'device.fifo.{width}', 'FIFO',
                {'depth': str(depth)}, fidelity)
            assert fifo.get_type_name() == f'SynchronousFifo<{width}>'
            key = server._component_layout_type(fifo)
            expected = f'SynchronousFifo<{width}>'
            if fidelity == 'structural' and depth != 4:
                expected += f'[config-depth={depth}]'
            assert key == expected, (key, expected)

for size in (4096, 16384, 262144):
    router = backend.create_device_component('bus.address-router.32', 'ROUTER',
        {'video_bytes': str(size)})
    expected = 'AddressRouter32' + (f'[config-video_bytes={size}]' if size != 4096 else '')
    assert server._component_layout_type(router) == expected

for contract, params in (
    ('device.framebuffer.32', {'width': '0'}),
    ('device.framebuffer.32', {'width': '4096', 'height': '65'}),
    ('device.framebuffer.32', {'storage_bytes': '8'}),
    ('device.fifo.8', {'depth': '8'}),
    ('device.console.32', {'fifo_depth': '0'}),
    ('device.buttons.32', {'fifo_depth': '8'}),
    ('bus.address-router.32', {'video_bytes': '12345'}),
    ('device.timer.32', {'divisor': '0'}),
    ('device.framebuffer.32', {'unknown': '1'}),
    ('memory.ff.endpoint.32', {}),
):
    rejects(lambda: backend.create_device_component(contract, 'BAD', params))
rejects(lambda: backend.create_device_component('device.framebuffer.32', 'BAD', {}, 'behavioral'))
rejects(lambda: backend.create_device_component('device.fifo.8', 'BAD', {}, 'unknown'))
print('PASS: Python catalog parameters, larger displays, queue fidelities, stable/default layout keys and invalid declarations')
