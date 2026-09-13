import csv
import os
import matplotlib.pyplot as plt

CWND_FILE = "results/phase4/cwnd.csv"
THROUGHPUT_FILE = "results/phase4/throughput.csv"
PLOT_DIRECTORY = "results/phase4/plots"

os.makedirs(PLOT_DIRECTORY, exist_ok=True)

cwnd_time = []
cwnd_values = []
ssthresh_values = []

with open(CWND_FILE, newline="", encoding="utf-8") as file:
    reader = csv.DictReader(file)
    for row in reader:
        cwnd_time.append(float(row["elapsed_ms"]))
        cwnd_values.append(float(row["cwnd"]))
        ssthresh_values.append(float(row["ssthresh"]))

plt.figure()
plt.plot(cwnd_time, cwnd_values, label="cwnd")
plt.plot(cwnd_time, ssthresh_values, label="ssthresh")
plt.xlabel("Time (ms)")
plt.ylabel("Packets")
plt.title("Congestion Window Over Time")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(os.path.join(PLOT_DIRECTORY, "cwnd_time.png"))
plt.close()

throughput_time = []
interval_values = []
average_values = []

with open(THROUGHPUT_FILE, newline="", encoding="utf-8") as file:
    reader = csv.DictReader(file)
    for row in reader:
        throughput_time.append(float(row["elapsed_ms"]))
        interval_values.append(float(row["interval_mbps"]))
        average_values.append(float(row["average_mbps"]))

plt.figure()
plt.plot(throughput_time, interval_values, label="Interval throughput")
plt.plot(throughput_time, average_values, label="Average throughput")
plt.xlabel("Time (ms)")
plt.ylabel("Throughput (Mbps)")
plt.title("Throughput Over Time")
plt.legend()
plt.grid(True)
plt.tight_layout()
plt.savefig(os.path.join(PLOT_DIRECTORY, "throughput_time.png"))
plt.close()

print("Plots created in", PLOT_DIRECTORY)
