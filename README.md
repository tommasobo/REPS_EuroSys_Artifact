# REPS: Recycled Entropy Packet Spraying for Adaptive Load Balancing and Failure Mitigation
Next-generation datacenters require highly efficient network load balancing to manage the growing scale of artificial intelligence (AI) training and general datacenter traffic. However, existing Ethernet-based solutions, such as Equal Cost Multi-Path (ECMP) and oblivious packet spraying (OPS), struggle to maintain high network utilization due to both increasing traffic demands and the expanding scale of datacenter topologies, which also exacerbate network failures. To address these limitations, we propose REPS, a lightweight decentralized per-packet adaptive load balancing algorithm designed to optimize network utilization while ensuring rapid recovery from link failures. REPS adapts to network conditions by caching good-performing paths. In case of a network failure, REPS re-routes traffic away from it in less than 100 microseconds. REPS is designed to be deployed with next-generation out-of-order transports, such as Ultra Ethernet, and uses less than 25 bytes of per-connection state regardless of the topology size. We extensively evaluate REPS in large-scale simulations and FPGA-based NICs.

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

# Testing the artifacts
To test the artifacts we provide a series of Bash files that can be run from the ```artifact_scripts/``` directory of the project.

- ```reps_quick.sh``` which runs quickly (less than 2 hour) and generates Figure 1, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14.
- ```reps_medium.sh``` which runs slower (around 6 hours) and generates Figure 1, 2, 3, 5, 6, 8, 9, 10, 11, 12, 13, 14.
- ```reps_full.sh``` which runs all the experiments of the paper but takes very long (potentially up to a day).

Exact running times depend on HW performance.

Alternatively, the user can run individual runs using the corresponding Python script from inside the ```artifact_scripts/``` folder.

# Results
Results are automatically generated in the ```artifact_results/``` folder. There, each experiment will have its own subfolder with the plots and raw_data (if applicable).