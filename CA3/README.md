# CA3 — GNS3 Network Configuration Labs

GNS3 labs exploring LAN connectivity, VLANs, static routing, and OSPF. The report includes configuration explanations, connectivity tests, routing-table observations, and link-failure experiments.

[Assignment](CN_CA_3.pdf) · [Report](report.pdf)

## Topologies

| File | Description |
| --- | --- |
| `CN_CA_3_Part2.gns3` | LAN with three switches and four hosts |
| `CN_CA_3_Part3.gns3` | VLAN 10/20 with inter-VLAN routing |
| `CN_CA_3_Part4.gns3` | Static routing with an alternate path |
| `CN_CA_3_Part5.gns3` | OSPF routing and recovery after a link failure |

## Open a lab

Requirements: GNS3, Dynamips, VPCS, and a locally supplied Cisco c7200 IOS image. The topology files were saved with GNS3 2.2.59 and reference `c7200-adventerprisek9-mz.151-4.M1.image`.

1. Configure the router image and template in GNS3.
2. Open the desired `.gns3` file, keeping its matching `project-files/` data alongside it.
3. Use the assignment and report for the addressing and device configuration, then start the nodes and test connectivity.

Useful checks include `ping`, `show ip interface brief`, `show ip route`, and `show ip ospf neighbor`. Save router configuration with `write memory` and VPCS configuration with `save`.

## Repository files

Keep the `.gns3` topologies, router `*_startup-config.cfg` files, VPCS `startup.vpc` files, PDFs, LaTeX sources, and report figures in Git. The `.gitignore` preserves startup configuration files within `project-files/`.

Router images, simulator runtime files, captures in the runtime capture directory, and LaTeX build output are excluded. IOS images are supplied locally; see [GNS3's image guidance](https://docs.gns3.com/docs/troubleshooting-faq/where-do-i-get-ios-images).

## Report source

`report/template.tex`, `report/setting.tex`, and `report/IMGs/` contain the editable report. Build from `report/` using XeLaTeX with the required packages and Times New Roman installed:

```bash
xelatex template.tex
xelatex template.tex
```

`report.pdf` in the project root is the published report. The duplicate `report/template.pdf` is excluded from Git.
