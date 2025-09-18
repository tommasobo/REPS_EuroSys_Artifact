import os
import argparse 

parser = argparse.ArgumentParser(description='Read and parse a JSON file containing experiments.')
parser.add_argument('--output_folder', required=False, help='Parent output folder where to save all results', default="experiments")
parser.add_argument('--trim', required=False, help='Trim Active Or not', default="on")
parser.add_argument('--duration', type=float, required=False, help='Duration', default=0.0045)
parser.add_argument('--size_topo', type=int, required=True, help='Size Topo', default=128)
parser.add_argument('--asymmetry_links', required=False, help='Num downgraded links', default=0)
args = parser.parse_args()

nodes_to_use = int(args.size_topo)
duration = args.duration
file_to_use = "WebSearch_distribution.txt"
load = ["40", "60", "80", "100"]

for load_value in load:
    load_num = int(load_value) / 100
    output_file = os.path.join("connection_matrices", f"{load_value}load.cm")
    if os.path.exists(output_file):
        continue
    os.makedirs(os.path.dirname(output_file), exist_ok=True)
    os.system(f"python ../../../traffic_gen/traffic_gen.py -n {nodes_to_use} -c ../../../traffic_gen/cdf_files/{file_to_use} -l {load_num} -b 400G -t 0 -d {duration} -s 42 -o {output_file}")