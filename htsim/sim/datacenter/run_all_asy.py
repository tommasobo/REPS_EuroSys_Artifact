import os
import argparse


# === BEGIN ARGPARSE SETUP ===
parser = argparse.ArgumentParser()
parser.add_argument('--save_folder',                default=None,
                    help='folder to save figures (defaults to <output_folder>_figures)')
parser.add_argument('--filename',                  default=None,
                    help='optional filename to forward to plot scripts')
args = parser.parse_args()



topolgy_size = 128
output_folder = "FINAL_OS_RUN_ASYM"
plot_only = False
save_folder   = args.save_folder or (output_folder + "_figures")
filename      = args.filename

# Symmetric No Trimming
if "micro" in args.filename:

    if not plot_only:
        os.system(f"python3 run_symmetric.py --trim off --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}asy_notrim_synth --skip_asy on")
        print("Synthetic No Trimming Done")
        # Simmetric Trimming
        """ os.system(f"python3 run_symmetric.py --trim on --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}asy_yestrim_synth --skip_asy on")
        print("Synthetic Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_symmetric.py --input_folder_exp {output_folder}asy_notrim_synth --name asynotrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """ os.system(f"python3 plot_symmetric.py --input_folder_exp {output_folder}asy_yestrim_synth --name asyyestrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """
# DC Gen
if "dc" in args.filename:

    if not plot_only:
        os.system(f"python3 ../../../traffic_gen/gen_files_remote.py --duration 0.005 --size_topo {topolgy_size}")
        print("DC Gen Done")
        # Symmetric DC Traces No Trimming
        os.system(f"python3 run_dc.py --trim off --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}asy_notrim_dc")
        print("DC No Trimming Done")
        # Symmetric DC Traces Trimming
        """ os.system(f"python3 run_dc.py --trim on --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}asy_yestrim_dc")
        print("DC Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_load.py --input_folder_exp {output_folder}asy_notrim_dc --name asynotrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """ os.system(f"python3 plot_load.py --input_folder_exp {output_folder}asy_yestrim_dc --name asyyestrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """

# Collective Gen
if "ai" in args.filename:
    if not plot_only:
        os.system(f"python3 connection_matrices/gen_files.py --size_topo {topolgy_size} --all_red_size {topolgy_size}")
        print("Collective Gen Done")
        # Symmetric Collectives No Trimming
        os.system(f"python3 run_collective.py --trim off --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}symm_notrim_coll")
        print("Collectives No Trimming Done")
        # Symmetric Collectives Trimming
        """ os.system(f"python3 run_collective.py --trim on --asymmetry_links 4 --size_topo {topolgy_size} --output_folder {output_folder}symm_yestrim_coll")
        print("Collectives Trimming Done") """
    # Plotting
    tmp_filename = filename + "_" + "notrim"
    os.system(f"python3 plot_collective.py --input_folder_exp {output_folder}symm_notrim_coll --name asynotrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """ os.system(f"python3 plot_collective.py --input_folder_exp {output_folder}symm_yestrim_coll --name asyyestrim --dont_show  --save_folder {save_folder} --filename {tmp_filename}")
    """