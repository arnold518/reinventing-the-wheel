#!/usr/bin/env python3
"""The committed guest header equals the one generated from its description."""
import pathlib, subprocess, sys
tool, root = sys.argv[1], pathlib.Path(__file__).resolve().parents[2]
for name, header in [('games-machine', 'tools/rv32im/guest/system.h'),
                     ('games-machine-leds', 'tools/rv32im/guest/system-leds.h')]:
    generated = subprocess.run([tool, name], check=True, capture_output=True, text=True).stdout
    committed = (root / header).read_text()
    if generated != committed:
        sys.exit(f'{header} differs from the header generated from systems/{name}.json; '
                 f'regenerate with: system_header {name} > {header}')
print('guest headers match their descriptions')
