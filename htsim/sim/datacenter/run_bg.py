import os
import argparse

# === BEGIN ARGPARSE SETUP ===
parser = argparse.ArgumentParser()
parser.add_argument('--size_topo',     type=int,   default=128)
parser.add_argument('--output_folder',               default="bg_run_128")
parser.add_argument('--save_folder',                default=None,
                    help='folder to save figures')
parser.add_argument('--plot_only',    action='store_true',
                    help='only plot, skip running simulation')
parser.add_argument('--bg_type', type=int, choices=[0,1], default=0,
                    help='bg type for plotting')
parser.add_argument('--filename',    default=None,
                    help='optional filename to pass to plot_symmetric')
args = parser.parse_args()

topolgy_size   = args.size_topo
output_folder  = args.output_folder
save_folder    = args.save_folder or (output_folder + "_figures")
plot_only      = args.plot_only
bg_type        = args.bg_type
filename       = args.filename
# === END ARGPARSE SETUP ===

# Symmetric No Trimming
if not plot_only:
    os.system(
        f"python3 run_symmetric.py --size_topo {topolgy_size} "
        f"--output_folder {output_folder}symm_notrim_synth --enable_bg on"
    )
# Plotting
cmd = (
    f"python3 plot_symmetric.py --bg_mode --bg_type {bg_type} "
    f"--input_folder_exp {output_folder}symm_notrim_synth "
    f"--name notrim --dont_show --save_folder {save_folder} --filename {filename}"
)
print(cmd)
os.system(cmd)
