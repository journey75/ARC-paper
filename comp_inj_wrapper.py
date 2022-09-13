import subprocess
import re
from datetime import datetime
import time
import math
import os
import sys

def main():
	print("{} Arguements Found".format(len(sys.argv)))
	if len(sys.argv) != 13:
		print("Incorrect Number of Arguements. . .")
		exit(-1)
	

	# Get input needed to run the experiment	
	nodes = int(sys.argv[1])
	percentage = float(sys.argv[2])
	data_path = sys.argv[3].strip()
	dims_input = sys.argv[4].strip()
	compressor = sys.argv[5].strip()
	error_mode = sys.argv[6].strip()
	error_bound = float(sys.argv[7])
	default_bound = float(sys.argv[8])
	compression_size = int(sys.argv[9])
	unique_experiment_id = sys.argv[10].strip()
	output_file_name = sys.argv[11].strip()
	timeout_limit = int(sys.argv[12])

	# Palmetto PBS script options
	#ncpus = "8"
	#phase = "5a"
	#mem = "30gb"
	#walltime = "24:00:00"

	ncpus = "16"
	phase = "9"
	mem = "125gb"
	walltime = "24:00:00"

	# Determine which percentage of the data we want to sample and how many bins to split that into
	print("Splitting {}% of the data into {} nodes".format(percentage*100,nodes))

	# Calculate bytes to hit and bin widths
	hit_ranges = []
	hit_bytes = math.ceil(compression_size * percentage)
	skip_bytes = math.floor(compression_size - hit_bytes)
	hit_bytes_range_size = math.floor(hit_bytes / nodes)
	skip_bytes_range_size = math.floor(skip_bytes / nodes)

	counter = 0
	mode = 0
	while True:
		prev_counter = counter
		if mode == 0:
			counter = counter + hit_bytes_range_size
			if counter >= compression_size:
				break
			hit_ranges.append([prev_counter, counter])
			mode = 1
		elif mode == 1:
			counter = counter + skip_bytes_range_size
			if counter >= compression_size:
				break
			mode = 0

	temp_directory = "subprocess_results/{}".format(unique_experiment_id)
	if not os.path.exists(temp_directory):
    		os.makedirs(temp_directory)

	# Write each range out to a comp_inj_runner PBS script
	node_id = 0
	for targets in hit_ranges:
		start_range = targets[0]
		end_range = targets[1]
		node_pbs_name = "{}_temp_pbs_node_{}.pbs".format(unique_experiment_id,node_id)
		
		with open(node_pbs_name, 'w+') as pbs_script:
			pbs_script.write("#!/bin/bash\n\n")
			pbs_script.write("#PBS -N Compression_Injection_Experiment\n")
			param_line = "#PBS -l select=1:ncpus={}:phase={}:mem={},walltime={}\n\n".format(ncpus, phase, mem, walltime)
			pbs_script.write(param_line)
			# GCC 8.3.1 is the default now
			#pbs_script.write("module load gcc/8.2.0\n")
			#pbs_script.write("source ./git/spack/share/spack/setup-env.sh\n") 
			#pbs_script.write("source ./git/spack/share/spack/spack-completion.bash\n")
			#pbs_script.write("spack load libpressio@0.38.1 arch=$(spack arch)\n")
			# Commented out with libpressio using random access sz: libpressio /52tna67
			
			# To use standard sz: libpressio /kj7vnnw
			pbs_script.write("spack load libpressio /kj7vnnw\n")
			# To use sz random access libpressio /52tna67
			#pbs_script.write("spack load libpressio /52tna67\n")

			# Python 3.6.8 is default now
			#pbs_script.write("module load python/3.4\n")
			pbs_script.write("cd $PBS_O_WORKDIR\n")
			run_line = "python3 comp_inj_runner.py {} {} \"{}\" {} {} {} {} {} {} {} {} {}\n".format(node_id, data_path, dims_input, compressor, error_mode, error_bound, default_bound, start_range, end_range, unique_experiment_id, output_file_name, timeout_limit)
			print(run_line)
			node_id = node_id + 1
			pbs_script.write(run_line)
		
		time.sleep(5)
		os.system("qsub {}".format(node_pbs_name))
			
	print("Jobs Have Been Submitted")
if __name__ == '__main__':
	main()
