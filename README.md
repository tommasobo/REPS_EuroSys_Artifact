# REPS: Recycled Entropy Packet Spraying for Adaptive Load Balancing and Failure Mitigation
Next-generation datacenters require highly efficient network load balancing to manage the growing scale of artificial intelligence (AI) training and general datacenter traffic. However, existing Ethernet-based solutions, such as Equal Cost Multi-Path (ECMP) and oblivious packet spraying (OPS), struggle to maintain high network utilization due to both increasing traffic demands and the expanding scale of datacenter topologies, which also exacerbate network failures. To address these limitations, we propose REPS, a lightweight decentralized per-packet adaptive load balancing algorithm designed to optimize network utilization while ensuring rapid recovery from link failures. REPS adapts to network conditions by caching good-performing paths. In case of a network failure, REPS re-routes traffic away from it in less than 100 microseconds. REPS is designed to be deployed with next-generation out-of-order transports, such as Ultra Ethernet, and uses less than 25 bytes of per-connection state regardless of the topology size. We extensively evaluate REPS in large-scale simulations and FPGA-based NICs.

# Main Contributions
Our main contributions in terms of software related to the artifact are:

- We extend and build on the [original htsim](https://github.com/Broadcom/csg-htsim) simulator to support REPS and all the other load balancing algorithms present in the paper. More details about the changes in the ```README``` inside the ```htsim``` folder.
- We build a custom Python tool to generate traffic traces (in the format of connection matrices) using CDFs as input (exact sources cited in the paper). More details inside the ```README``` of the ```traffic_gen``` folder. 

# Installing Requirements
From the repository root, install the required Python packages using the command below. To avoid conflicts with local packages, we recommend running it inside a clean Python environment created with ```venv```.
```
python3 -m venv .venv
source .venv/bin/activate
./reps_pkg_install.sh
```

Then compile the project. Run the following command from the ```/htsim/sim``` directory (feel free to adjust the number of parallel jobs).

```
make clean && cd datacenter/ && make clean && cd .. && make -j 8 && cd datacenter/ && make -j 8 && cd ..
```

You might see some warnings during the compilation, it is safe to ignore them for now, we plan on fixing them at a later date.

# Tested Setup
### Hardware
No specialized hardware is required, no GPUs or accelerators. All experiments were executed on a commodity x86 CPU. Our reference machine had 40 GB of RAM, but the simulator runs comfortably with 32 GB as well. Systems with 16 GB can still reproduce the results; you may just need to lower the parallelism of your runs (e.g., reduce `--workers`) to keep memory usage in check.

### Software
All experiments were run locally on Ubuntu 22.04 LTS using WSL 2. The project requires a C++17-capable compiler and Python 3.8 (or newer) to build `htsim` and to execute the accompanying analysis scripts. If you’re on a comparable Linux environment, the setup should behave the same. Different Python versions might require some manual tuning and handling of dependencies.


# Testing the artifacts
To test the artifacts we provide a series of Bash files that can be run from the ```artifact_scripts/``` directory of the project.

- ```reps_quick.sh``` which runs quickly (less than 2 hours) and generates Figure 1, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14.
- ```reps_medium.sh``` which runs slower (around 4 hours) and generates Figure 1, 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14.
- ```reps_full.sh``` which runs all the experiments of the paper but takes very long (around 10 hours).

Exact running times depend on HW performance.

Alternatively, the user can run individual runs using the corresponding Python script from inside the ```artifact_scripts/``` folder.

### Safety & Resource Usage
This software does not execute untrusted code, escalate privileges, or change system settings.
It performs no hidden installs and does not generate large datasets, only small outputs during testing.
No network connections are made unless explicitly configured.

# Results
Results are automatically generated in the ```artifact_results/``` folder. There, each experiment will have its own subfolder with the plots and raw_data (if applicable).

Please note that minor discrepancies from the submitted version may occur due to hardware or software differences and inherent randomness, despite our efforts to fix all random seeds. Additionally, some plots were manually annotated and enhanced during post-processing.