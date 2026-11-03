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

routes = {'access': 'data', 'routes': 'RAM_@0x0+0x1000,DEVICE_@0x10000000+0x100'}
router = backend.create_device_component('bus.router.32', 'ROUTER', routes)
key = server._component_layout_type(router)
assert key.startswith('BusRouter<32>[config-'), key
other = backend.create_device_component('bus.router.32', 'ROUTER', {'access': 'data', 'routes': 'RAM_@0x0+0x2000'})
assert server._component_layout_type(other) != key

for contract, params in (
    ('device.framebuffer', {'width': '0'}),
    ('device.framebuffer', {'width': '4096', 'height': '65'}),
    ('device.framebuffer', {'storage_bytes': '8'}),
    ('device.fifo.8', {'depth': '8'}),
    ('device.console', {'fifo_depth': '0'}),
    ('device.buttons', {'fifo_depth': '8'}),
    ('bus.router.32', {'access': 'data', 'routes': 'RAM_@0x0+0x1001'}),
    ('bus.bridge.32', {'registered': '2'}),
    ('device.timer', {'divisor': '0'}),
    ('device.framebuffer', {'unknown': '1'}),
    ('memory.ff.endpoint.32', {}),
):
    rejects(lambda: backend.create_device_component(contract, 'BAD', params))
# The compact display uses compact pixel storage at every size, 4x4 included.
assert backend.create_device_component('device.framebuffer', 'SMALL', {}, 'behavioral').get_type_name() == 'FramebufferDisplay'
rejects(lambda: backend.create_device_component('device.fifo.8', 'BAD', {}, 'unknown'))
print('PASS: Python catalog parameters, larger displays, queue fidelities, stable/default layout keys and invalid declarations')

for width in (8,16,32):
    alu=backend.create_word_component('alu','ALU',width)
    assert alu.get_input_pins()['A'].get_width()==width and 'C_IN' in alu.get_input_pins()
rejects(lambda:backend.create_word_component('alu','BAD',12))
assert backend.create_word_component('alu','ALU16',16,'behavioral').get_type_name()=='ALU<16>'  # compact ALU at 8, 16 and 32 bits
assert 'C_IN' not in backend.create_rv32i_alu('ALU').get_input_pins()
for width in (8,16,32):
    router=backend.create_device_component(f'bus.router.{width}','MAP',{'access':'fetch','routes':'RAM_@0x0+0x1000'})
    assert router.get_type_name()==f'BusFetchRouter<{width}>' and backend.component_layout_variant(router)
    rejects(lambda:backend.create_device_component(f'bus.router.{width}','BAD',{'access':'data','routes':'RAM_@0x10+0x100'}))
assert backend.device_view_kind(backend.create_device_component('device.fifo.16','QUEUE')) == 'queue'
print('PASS: ALU contracts/topologies, router maps and device capability metadata')
