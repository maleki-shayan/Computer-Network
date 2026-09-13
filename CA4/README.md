# CA4 — Reliable File Transfer over UDP

A series of C++ implementations that build a reliable file-transfer protocol over UDP. The phases introduce sequence numbers, checksums, acknowledgements, retransmission timers, sliding windows, congestion control, and performance analysis.

[Assignment](Computer%20Network-%20Programming%2004.pdf) · [Report](report.pdf)

## Phases

| Directory | Focus |
| --- | --- |
| `Phase1/` | Stop-and-wait transmission and timeout-based retransmission |
| `Phase2/` | Sliding window with cumulative acknowledgements |
| `Phase3/` | Receiver-side simulated packet loss and ACK delay |
| `Phase4/` | Slow start, congestion avoidance, and performance logging |
| `Phase5/` | Repeatable experiments, plots, and comparison metrics |
| `Phase6/` | Fast retransmit and fast recovery |

Each phase contains its own sender, receiver, packet format, timer, logger, and Makefile.

## Run Phases 1–4

Requirements: Linux or Ubuntu/WSL, GNU Make, and a C++17 compiler. From the desired phase directory:

```bash
make
./receiver
```

In another terminal, from the same directory:

```bash
./sender
```

The sender targets `127.0.0.1:8080` over UDP. Run one phase at a time. After the transfer finishes, verify the files with:

```bash
cmp sample_input.txt sample_output.txt
```

For Phase 4, run `python3 plot_results.py` to generate congestion-window and throughput plots. Matplotlib is required.

## Run Phases 5–6

Requirements also include Python 3, Matplotlib, Bash, and standard Linux utilities. From either phase directory:

```bash
make sample
make test
```

The sample generator creates a deterministic 200000-byte binary input. The test script builds the programs, runs the transfer, compares the files and SHA-256 hashes, creates plots, and records summary metrics.

Edit `experiment.hpp` to set the test name, loss percentage, and ACK delay. Phase 6 also provides switches for fast retransmit and fast recovery. Settings are compiled into the programs; `make test` rebuilds after header changes.

[Phase5/TESTING.txt](Phase5/TESTING.txt) describes the normal, medium, and high loss/delay scenarios. Reuse the same generated input when comparing scenarios and use distinct test names to retain separate results.

Results are written under `results/phaseN/`, with per-test subdirectories in Phases 5–6. Logs, CSVs, plots, generated binary inputs, and received files are excluded from Git. Source files, generators, analysis scripts, text sample inputs, and the final report are retained.
