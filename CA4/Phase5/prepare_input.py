import random
from pathlib import Path

OUTPUT_FILE = Path("sample_input.bin")
FILE_SIZE = 200000
RANDOM_SEED = 2026

random_generator = random.Random(RANDOM_SEED)
data = bytearray(
    random_generator.getrandbits(8)
    for _ in range(FILE_SIZE)
)

OUTPUT_FILE.write_bytes(data)

print("Created", OUTPUT_FILE)
print("Size:", FILE_SIZE, "bytes")
print("Seed:", RANDOM_SEED)
