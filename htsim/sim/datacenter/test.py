import os


os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo ecmp > 40e")
print("1/6")
os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo ecmp > 100e")
print("2/6")

os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo oblivious > 40o")
print("3/6")    
os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo oblivious > 100o")
print("4/6")

os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/40load.cm -load_balancing_algo freezing > 40r")
print("5/6")
os.system("./htsim_uec -sack_threshold 4000 -end 90000 -seed 1019 -paths 4096 -sender_cc_only -sender_cc_algo mprdma -topo topologies/reps/fat_tree_64_1os_2t_400g.topo -linkspeed 400000 -ecn 25 76 -q 100 -cwnd 151 -tm connection_matrices/100load.cm -load_balancing_algo freezing > 100r")
print("6/6")    