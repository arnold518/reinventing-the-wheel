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
    display = backend.create_device_component('device.framebuffer', 'DISPLAY',
        {'width': str(width), 'height': str(height)})
    assert display.get_type_name() == 'FramebufferDisplay'
    key = server._component_layout_type(display)
    assert key == 'FramebufferDisplay' if (width, height) == (4, 4) else key.startswith(
        f'FramebufferDisplay[config-width={width},height={height},storage=')

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
    router = backend.create_device_component('bus.word.address-router.32', 'ROUTER',
        {'video_bytes': str(size)})
    expected = 'AddressRouter<32>' + (f'[config-video_bytes={size}]' if size != 4096 else '')
    assert server._component_layout_type(router) == expected

for contract, params in (
    ('device.framebuffer', {'width': '0'}),
    ('device.framebuffer', {'width': '4096', 'height': '65'}),
    ('device.framebuffer', {'storage_bytes': '8'}),
    ('device.fifo.8', {'depth': '8'}),
    ('device.console', {'fifo_depth': '0'}),
    ('device.buttons', {'fifo_depth': '8'}),
    ('bus.word.address-router.32', {'video_bytes': '12345'}),
    ('device.timer', {'divisor': '0'}),
    ('device.framebuffer', {'unknown': '1'}),
    ('memory.ff.endpoint.32', {}),
):
    rejects(lambda: backend.create_device_component(contract, 'BAD', params))
rejects(lambda: backend.create_device_component('device.framebuffer', 'BAD', {}, 'behavioral'))
rejects(lambda: backend.create_device_component('device.fifo.8', 'BAD', {}, 'unknown'))
print('PASS: Python catalog parameters, larger displays, queue fidelities, stable/default layout keys and invalid declarations')

for width in (8,16,32):
    for operations in ('extended','integer'):
        for topology in ('ripple','prefix'):
            alu=backend.create_alu('ALU',width,operations,topology)
            assert alu.get_input_pins()['A'].get_width()==width
            assert ('C_IN' in alu.get_input_pins()) == (operations=='extended')
rejects(lambda:backend.create_alu('BAD',12))
rejects(lambda:backend.create_alu('BAD',16,'integer','prefix',True,'behavioral'))
rejects(lambda:backend.create_alu('BAD',16,'extended','prefix',False))
for width in (8,16,32):
    router=backend.create_device_component(f'bus.word.address-router.{width}','MAP',{'video_base':str(0x30000000),'video_bytes':'8192'})
    assert 'VIDEO_805306368:8192' in backend.component_layout_variant(router)
    rejects(lambda:backend.create_device_component(f'bus.word.address-router.{width}','BAD',{'video_base':'0'}))
assert backend.device_view_kind(backend.create_device_component('device.fifo.16','QUEUE')) == 'queue'
print('PASS: ALU contracts/topologies, router maps and device capability metadata')
