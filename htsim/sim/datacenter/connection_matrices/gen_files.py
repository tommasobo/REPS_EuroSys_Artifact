import os
import argparse 

all_to_all_n = [1, 2, 4, 8, 16]
all_to_all_name = ["alltoall1", "alltoall2", "alltoall4", "alltoall8", "alltoall16"]
all_reduce_name = ["allreduce", "allreduce_t"]

parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
parser.add_argument('--size_topo', type=int, required=True, help='Size Topo', default=128)
parser.add_argument('--all_red_size', type=int, required=False, help='All Red', default=128)

args = parser.parse_args()
topo_size = args.size_topo
job_size = topo_size
if (args.all_red_size != topo_size):
    job_size = args.all_red_size

# All To All
for idx, parllale_conn,  in enumerate(all_to_all_n):
    all_name = all_to_all_name[idx]
    all_to_all_size = int(4000000 / 2)
    half = int(topo_size / 1)
    os.system(f"python3 connection_matrices/gen_serialn_alltoall.py connection_matrices/{all_name}.cm {topo_size} {half} {half} {parllale_conn} {all_to_all_size} 1 42")

# All Reduce
ring_size = int(4000000 / 1)
fly_size = int(40000000 / 2)
half = int(topo_size / 2)

os.system(f"python3 connection_matrices/gen_allreduce.py connection_matrices/allreduce.cm {topo_size} {half} {half} {ring_size} 0 42")
os.system(f"python3 connection_matrices/gen_allreduce_butterfly.py connection_matrices/allreduce_t.cm  {topo_size} 1 {topo_size} {fly_size} 1 42")