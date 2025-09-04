import os
import argparse

# === BEGIN ARGPARSE SETUP ===
parser = argparse.ArgumentParser()
parser.add_argument('--save_folder',                default=None,
                    help='folder to save figures (defaults to <output_folder>_figures)')
parser.add_argument('--filename',                  default=None,
                    help='optional filename to forward to plot scripts')
args = parser.parse_args()

topolgy_size = 32
output_folder = "test_run_32"
plot_only = False
save_folder   = args.save_folder or (output_folder + "_figures")
filename      = args.filename

# Symmetric No Trimming
if "micro" in args.filename:
    if not plot_only:
        os.system("rm -rf ../failures_input/saved/*")
        os.system(f"python3 connection_matrices/gen_permutation.py connection_matrices/permutation_size8388608B.cm {topolgy_size} {topolgy_size} 8388608 0 42")
        os.system(f"python3 run_failures.py --cache on --trim off --cm connection_matrices/permutation_size8388608B.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_synth")
        os.system(f"python3 run_failures.py --trim off --cm connection_matrices/permutation_size8388608B.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_synth")
        print("Synthetic No Trimming Done")
        # Simmetric Trimming
        """ os.system(f"python3 run_failures.py --trim on --cm connection_matrices/permutation_size8388608B.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_yestrim_synth")
        print("Synthetic Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_failures.py --text 'Speedup vs OPS' --input_folder_exp {output_folder}fail_notrim_synth --name notrim_fail_synth --dont_show --save_folder {save_folder}  --filename {tmp_filename}")
    """ os.system(f"python3 plot_failures.py --text 'Speedup vs OPS' --input_folder_exp {output_folder}fail_yestrim_synth --name yestrim_fail_synth --dont_show --save_folder {save_folder}")
    """
# DC Gen
if "dc" in args.filename:
    if not plot_only:
        os.system(f"python3 ../../../traffic_gen/gen_files_remote.py --duration 0.0035 --size_topo {topolgy_size}")
        os.system("rm -rf ../failures_input/saved/*")
        os.system(f"python3 run_failures.py --cache on --trim off --cm connection_matrices/100load.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_dc")
        os.system(f"python3 run_failures.py --trim off --cm connection_matrices/100load.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_dc")
        print("DC No Trimming Done")
        # DC Trimming
        """ os.system(f"python3 run_failures.py --trim on --cm connection_matrices/100load.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_yestrim_dc")
        print("DC Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_failures.py --text 'Speedup vs OPS (Avg FCT)' --input_folder_exp {output_folder}fail_notrim_dc --name notrim_fail_dc --dont_show --save_folder {save_folder}  --filename {tmp_filename}")
    """ os.system(f"python3 plot_failures.py --text 'Speedup vs OPS (Avg FCT)' --input_folder_exp {output_folder}fail_yestrim_dc --name yestrim_fail_dc --dont_show --save_folder {save_folder}")
    """

# ALLRED
if "ai" in args.filename:
    if not plot_only:
        os.system(f"python3 connection_matrices/gen_files.py --size_topo {topolgy_size} --all_red_size {topolgy_size}")
        os.system("rm -rf ../failures_input/saved/*")
        os.system(f"python3 run_failures.py --cache on --trim off --cm connection_matrices/allreduce.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_allred")
        os.system(f"python3 run_failures.py --trim off --cm connection_matrices/allreduce.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_notrim_allred")
        print("DC No Trimming Done")
        # DC Trimming
        """ os.system(f"python3 run_failures.py --trim on --cm connection_matrices/allreduce.cm --size_topo {topolgy_size} --output_folder {output_folder}fail_yestrim_allred")
        print("DC Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_failures.py --text 'Speedup vs OPS' --input_folder_exp {output_folder}fail_notrim_allred --name notrim_fail_coll --dont_show --save_folder {save_folder} --filename {tmp_filename}")
    """ os.system(f"python3 plot_failures.py --text 'Speedup vs OPS' --input_folder_exp {output_folder}fail_yestrim_allred --name yestrim_fail_coll --dont_show --save_folder {save_folder}") """