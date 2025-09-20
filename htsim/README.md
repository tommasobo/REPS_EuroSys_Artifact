# htsim Network Simulator

htsim is a high performance discrete event simulator, inspired by ns2, but much faster, primarily intended to examine congestion control algorithm behaviour.  It was originally written by [Mark Handley](http://www0.cs.ucl.ac.uk/staff/M.Handley/) to allow [Damon Wishik](https://www.cl.cam.ac.uk/~djw1005/) to examine TCP stability issues when large numbers of flows are multiplexed.  It was extended by [Costin Raiciu](http://nets.cs.pub.ro/~costin/) to examine [Multipath TCP performance](http://nets.cs.pub.ro/~costin/files/mptcp-nsdi.pdf) during the MPTCP standardization process, and models of datacentre networks were added to [examine multipath transport](http://nets.cs.pub.ro/~costin/files/mptcp_dc_sigcomm.pdf) in a variety of datacentre topologies.  [NDP](http://nets.cs.pub.ro/~costin/files/ndp.pdf) was developed using htsim, and simple models of DCTCP, DCQCN were added for comparison.  Later htsim was adopted by Correct Networks (now part of Broadcom) to develop [EQDS](http://nets.cs.pub.ro/~costin/files/eqds.pdf), and switch models were improved to allow a variety of forwarding methods.  Support for a simple RoCE model, PFC, Swift and HPCC were added.

It was recently extended to be used also as part of the [Ultra Ethernet](https://github.com/ultraethernet/uec-transport-simulation-code) effort.

htsim is written in C++, and has no dependencies.  It should compile and run with g++ or clang on MacOS or Linux.  To compile htsim, cd into the sim directory and run make.

To get started with running experiments, take a look in the experiments directory where there are some examples.  These examples generally require bash, python3 and gnuplot.

# Main Changes Introduced by the REPS paper
For the REPS paper, we introduce several changes to htsim, both in terms of code quality and features. We report below a list of the main changes:

- ```uec.cpp``` and ```uec.h``` --> These contain the new contributions in terms of handling different load balancing algorithms, the interaction with congestion control and the logic for logging and saving data.

- ```main_uec.cpp``` --> This provides the entry point for the simulation. We modify it to support more paramters (for example for the newly introduces load balancing schemes).

- ```fat_tree_topology.cpp``` and ```fat_tree_topology.h``` --> We update the topology generator to support introducing failures at runtime for different components (switches, links or a combination of the two)

- ```buffer_reps.cpp``` and ```buffer_reps.h``` --> This is a pair of support files used by the REPS logic to implement its circular buffer.

- ```failuregenerator.cpp``` and ```failuregenerator.h``` --> These files are used to actually generate, store and load failure modes. 