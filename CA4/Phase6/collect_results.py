import csv
import re
import sys
from pathlib import Path


def current_test_name():
    text = Path("experiment.hpp").read_text(encoding="utf-8")
    match = re.search(r'const string TEST_NAME = "([^"]+)";', text)
    if match is None:
        raise RuntimeError("TEST_NAME was not found")
    return match.group(1)


def read_summary(path):
    values = {}
    for line in path.read_text(encoding="utf-8").splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            values[key] = value
    return values


test_name = sys.argv[1] if len(sys.argv) > 1 else current_test_name()
root = Path("results") / "phase6"
result_directory = root / test_name
sender = read_summary(result_directory / "sender_summary.txt")
receiver = read_summary(result_directory / "receiver_summary.txt")
verification = read_summary(result_directory / "verification.txt")
comparison_file = root / "comparison.csv"

fieldnames = [
    "test_name",
    "fast_retransmit_enabled",
    "fast_recovery_enabled",
    "loss_percent",
    "ack_delay_ms",
    "input_bytes",
    "total_transmissions",
    "retransmissions",
    "fast_retransmits",
    "fast_recovery_entries",
    "timeouts",
    "transfer_time_ms",
    "throughput_mbps",
    "dropped_packets",
    "file_match",
    "sha256",
]

row = {
    "test_name": test_name,
    "fast_retransmit_enabled": sender["fast_retransmit_enabled"],
    "fast_recovery_enabled": sender["fast_recovery_enabled"],
    "loss_percent": sender["loss_percent"],
    "ack_delay_ms": sender["ack_delay_ms"],
    "input_bytes": sender["input_bytes"],
    "total_transmissions": sender["total_transmissions"],
    "retransmissions": sender["retransmissions"],
    "fast_retransmits": sender["fast_retransmits"],
    "fast_recovery_entries": sender["fast_recovery_entries"],
    "timeouts": sender["timeouts"],
    "transfer_time_ms": sender["transfer_time_ms"],
    "throughput_mbps": sender["throughput_mbps"],
    "dropped_packets": receiver["dropped_packets"],
    "file_match": verification["file_match"],
    "sha256": verification["input_sha256"],
}

rows = []
if comparison_file.exists():
    with comparison_file.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
rows = [old for old in rows if old["test_name"] != test_name]
rows.append(row)
order = {"fast_retransmit_off": 0, "fast_retransmit_on": 1}
rows.sort(key=lambda item: order.get(item["test_name"], 99))

with comparison_file.open("w", newline="", encoding="utf-8") as file:
    writer = csv.DictWriter(file, fieldnames=fieldnames)
    writer.writeheader()
    writer.writerows(rows)

print("Updated", comparison_file)
