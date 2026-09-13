# CA2 — Wi-Fi Performance Simulation

Wireless network experiments in ns-3 comparing Wi-Fi 5 and Wi-Fi 6 under different traffic and network settings. Measurements include throughput, delay, packet loss, and Jain's fairness index. A Python Deep Q-Network agent extends the experiments by adjusting the best-effort contention window through a local UDP bridge.

[Assignment](Computer%20Network-%20Programming%2002.pdf) · [Report](Report.pdf)

## Experiments

| Source | Description |
| --- | --- |
| `scratch/phase1.cc` | IEEE 802.11ac baseline with UDP echo traffic |
| `scratch/phase2.cc` | IEEE 802.11ax with scheduled uplink OFDMA and buffer-status polling |
| `scratch/phase3.cc` | Dense Wi-Fi 6 scenario with 40 stations and randomized placement |
| `scratch/wifi6.cc` | Wi-Fi 6 simulation connected to the learning agent |
| `scratch/dqn_agent.py` | PyTorch DQN with replay memory, target network, and TensorBoard logging |

## Run the simulations

Requirements: Linux/WSL and a configured ns-3.38 installation. Copy this project's C++ files into the simulator's `scratch/` directory, then run these commands from the ns-3.38 root:

```bash
./ns3 build
./ns3 run scratch/phase1
./ns3 run scratch/phase2
./ns3 run scratch/phase3
```

Simulation settings such as station count, distance, packet size, and traffic interval are constants in each source file. The default simulation duration is 10 simulated seconds. Metrics are printed to the terminal.

## Run the learning experiment

The agent requires Python 3, PyTorch, NumPy, and TensorBoard. In a Python environment with these packages installed, run from `CA2/scratch/`:

```bash
python3 dqn_agent.py
```

After the agent starts listening, run from the ns-3.38 root in another terminal:

```bash
./ns3 run scratch/wifi6
```

Run both processes in the same Linux/WSL environment. They exchange state and actions over `127.0.0.1:9999`, with one decision every 0.1 simulated seconds. The agent can decrease, retain, or increase the contention window.

TensorBoard logs are written to `runs/uora_experiment/`. View them with `tensorboard --logdir runs`. Stop the agent with Ctrl+C to save its current weights. Generated logs and model weights are excluded from Git.
