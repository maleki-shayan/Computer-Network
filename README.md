# Computer Networks Course Projects

Course projects exploring socket programming, wireless network simulation, routing, and reliable data transfer. Each project includes its source files or network topologies, assignment specification, and report.

| Project | Description | Tools |
| --- | --- | --- |
| [CA1 — TCP Chat and File Sharing](CA1/README.md) | Multi-client group chat, private messaging, and file uploads/downloads | C++, POSIX sockets |
| [CA2 — Wi-Fi Performance Simulation](CA2/README.md) | Wi-Fi 5/6 experiments and reinforcement learning for contention-window adjustment | ns-3.38, C++, Python, PyTorch |
| [CA3 — Network Configuration Labs](CA3/README.md) | LAN switching, VLANs, static routing, and OSPF | GNS3, Dynamips, VPCS |
| [CA4 — Reliable Transfer over UDP](CA4/README.md) | Reliability, sliding windows, congestion control, and retransmission experiments | C++17, Python |

## Getting started

Open each project's README for its requirements and run instructions. The C++ networking projects use Linux/POSIX APIs and can be run on Linux or Ubuntu under WSL. CA2 uses an external ns-3 installation; CA3 uses GNS3 with a locally supplied router image.

The root [`.gitignore`](.gitignore) excludes compiled binaries, simulator runtime files, generated experiment output, and temporary report files while retaining source code, saved network configurations, and final reports.
