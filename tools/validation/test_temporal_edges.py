"""Source contracts for Native-AA edge history rejection."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SOURCE = (ROOT / 'octaryn-client/Shaders/Temporal/Inputs.slang').read_text()

assert 'depth[pixel-int2(1,0)]' in SOURCE
assert 'depth[pixel+int2(1,0)]' in SOURCE
assert 'depthEdge*.8' in SOURCE
assert 'reactive[id.xy]' in SOURCE
print('temporal edge reactive mask: passed')
