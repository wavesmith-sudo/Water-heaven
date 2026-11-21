#!/usr/bin/env python3
import sys
from pathlib import Path
config, fragment = map(Path, sys.argv[1:3])
lines = config.read_text().splitlines()
new = [s for s in fragment.read_text().splitlines() if s.startswith('BR2_')]
keys = {s.split('=', 1)[0] for s in new}
lines = [s for s in lines if not (s.startswith('BR2_') and s.split('=',1)[0] in keys) and not (s.startswith('# BR2_') and s.split(' ')[1] in keys)]
config.write_text('\n'.join(lines + new) + '\n')
