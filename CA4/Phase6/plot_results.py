import csv
import re
import sys
from pathlib import Path

import matplotlib.pyplot as plt


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
result_directory = Path("results") / "phase6" / test_name
plot_directory = result_directory / "plots"
plot_directory.mkdir(parents=True, exist_ok=True)
summary = read_summary(result_directory / "sender_summary.txt")
loss_percent = summary.get("loss_percent", "?")
ack_delay_ms = summary.get("ack_delay_ms", "?")

cwnd_time = []
cwnd_values = []
ssthresh_values = []
timeout_time = []
timeout_cwnd = []
fast_time = []
fast_cwnd = []

with (result_directory / "cwnd.csv").open(newline="", encoding="utf-8") as file:
    reader = csv.DictReader(file)
    for row in reader:
        elapsed = float(row["elapsed_ms"])
        cwnd = float(row["cwnd"])
        cwnd_time.append(elapsed)
        cwnd_values.append(cwnd)
        ssthresh_values.append(float(row["ssthresh"]))
        if row["event"] == "TIMEOUT_RESET":
            timeout_time.append(elapsed)
            timeout_cwnd.append(cwnd)
        if row["event"] == "FAST_RETRANSMIT":
            fast_time.append(elapsed)
            fast_cwnd.append(cwnd)

plt.figure(figsize=(9, 5))
plt.plot(cwnd_time, cwnd_values, label="cwnd")
plt.plot(cwnd_time, ssthresh_values, label="ssthresh")
if timeout_time:
    plt.scatter(timeout_time, timeout_cwnd, label="timeout reset", marker="x")
if fast_time:
    plt.scatter(fast_time, fast_cwnd, label="fast retransmit", marker="^")
plt.xlabel("Time (ms)")
plt.ylabel("Window size (packets)")
plt.title(f"Congestion Window - {test_name}\nLoss {loss_percent}%, ACK delay {ack_delay_ms} ms")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(plot_directory / "cwnd_time.png", dpi=180)
plt.close()

throughput_time = []
interval_values = []
average_values = []
with (result_directory / "throughput.csv").open(newline="", encoding="utf-8") as file:
    reader = csv.DictReader(file)
    for row in reader:
        throughput_time.append(float(row["elapsed_ms"]))
        interval_values.append(float(row["interval_mbps"]))
        average_values.append(float(row["average_mbps"]))

plt.figure(figsize=(9, 5))
plt.plot(throughput_time, interval_values, label="Interval throughput")
plt.plot(throughput_time, average_values, label="Average throughput")
plt.xlabel("Time (ms)")
plt.ylabel("Throughput (Mbps)")
plt.title(f"Throughput - {test_name}\nLoss {loss_percent}%, ACK delay {ack_delay_ms} ms")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(plot_directory / "throughput_time.png", dpi=180)
plt.close()

print("Created", plot_directory / "cwnd_time.png")
print("Created", plot_directory / "throughput_time.png")
