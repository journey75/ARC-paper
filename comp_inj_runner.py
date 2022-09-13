import concurrent.futures
import subprocess
import re
from datetime import datetime
import time
import math
import os
import sys

# Calls C Program, checks to see if it finished near instantly.
# If not then wait the timeout period and check every .1 seconds.
def popen_timeout(command, timeout):
	p = subprocess.Popen(command, stdout=subprocess.PIPE)
	time.sleep(.03)
	if p.poll() is not None:
		return p.communicate(), p.pid
	else:
		for t in range(timeout*10):
			time.sleep(.1)
			if p.poll() is not None:
				return  p.communicate(), p.pid
		p.kill()
		return ["Timeout"], p.pid


# Call C Program and write results to the output file.
def experiment(process_id, subprocess_id, data_path, dims_input, compressor, error_mode, error_bound, default_bound, start, end, unique_experiment_id, timeout_limit):

	# Get output information to save results
	output_file = "subprocess_results/{}/process_{}_{}_subprocess_{}_results.csv".format(unique_experiment_id, process_id, unique_experiment_id, subprocess_id)
	output = open(output_file, "w+")
	output.write("DataSize,CompressionRatio,ErrorInfo,ByteLocation,FlipLocation,DecompressionTime,Incorrect,MaxDifference,RMSE,PSNR,Status,Traceback\n")

	print("Running Trials. . .\n", flush=True)

	print("Hitting {} to {}".format(start, end), flush=True)
	for byte in range(start, end+1):
		for bit in range(8):
			# Call c program
			try:
				proc_result, child_process_id = popen_timeout(['./comp_inj', '-i', data_path, '-d', dims_input, '-c', compressor, '-m', error_mode, '-e', str(error_bound), '-x', str(default_bound), '-b', str(byte), '-f', str(bit), '-a', str(1)], timeout_limit)
				response = str(proc_result[0])
			except Exception as e:
				print(e)
				exit(-1)


			if "Timeout" not in response:
				# Parse response
				try:
					data_size = re.findall(r"(?<=Original Data Size in Bytes: ).+?(?=\\n)", response)[0]
					error_info = re.findall(r"(?<=Error Bounding Value: ).+?(?=\\n)", response)[0]
					byte_location = re.findall(r"(?<=Byte Location: ).+?(?=\\n)", response)[0]
					flip_location = re.findall(r"(?<=Flip Location: ).+?(?=\\n)", response)[0]
					try:
						compressed_ratio = re.findall(r"(?<=Compression Ratio: ).+?(?=\\n)", response)[0]
						time_taken = re.findall(r"(?<=Time to Decompress: ).+?(?=\\n)", response)[0]
						num_incorrect = re.findall(r"(?<=Number of Incorrect: ).+?(?=\\n)", response)[0]
						abs_diff = re.findall(r"(?<=Maximum Absolute Difference: ).+?(?=\\n)", response)[0]
						rmse = re.findall(r"(?<=Root Mean Squared Error: ).+?(?=\\n)", response)[0]
						psnr = re.findall(r"(?<=PSNR: ).+?(?=\\n)", response)[0]
						status = "Completed"	
						traceback = "NA"
					except:
						core_file = "core.{}".format(child_process_id)
						if os.path.exists(core_file):
							#print("Coredump Occured\n")
							compressed_ratio = "-1"
							time_taken = "-1"
							num_incorrect = "-1"
							abs_diff = "-1"
							rmse = "-1"
							psnr = "-1"
							status = "CoreDump"
							traceback = "NA"
							os.system("rm {}".format(core_file))	
						elif "Wrong version" in response:
							#print("Version Error Occured\n")
							compressed_ratio = "-1"
							time_taken = "-1"
							num_incorrect = "-1"
							abs_diff = "-1"
							rmse = "-1"
							psnr = "-1"
							status = "VersionError"
							traceback = "NA"
						elif "Sig 11" in response:
							#print("Segfault Occured\n")
							compressed_ratio = "-1"
							time_taken = "-1"	
							num_incorrect = "-1"
							abs_diff = "-1"
							rmse = "-1"
							psnr = "-1"
							status = "SegFault"
							try:
								traceback = re.findall(r"(?<=Receiving Sig 11: ).+?(?=\\n)", response)[0]
								traceback = re.findall(r"\/+.*?\)+", traceback)
								traceback = ' <- '.join(traceback)	
							except:
								traceback = "Unknown"
						elif "stepLength" in response:
							#print("StepLength Error Occured\n")
							compressed_ratio = "-1"
							time_taken = "-1"	
							num_incorrect = "-1"
							abs_diff = "-1"
							rmse = "-1"
							psnr = "-1"
							status = "StepLengthError"
							traceback = "NA"
						else:
							print("Unkown Output\n")
							print("Byte: {} Bit: {}\n".format(byte, bit))
							print(response)
							print("\n")
							compressed_ratio = "-1"
							time_taken = "-1"
							num_incorrect = "-1"
							abs_diff = "-1"
							rmse = "-1"
							psnr = "-1"
							status = "Unknown"
							traceback = "NA"
				except:
					core_file = "core.{}".format(child_process_id)
					if os.path.exists(core_file):
						#print("HardSegFault Occured\n")
						error_info = error_mode
						byte_location = str(byte)
						flip_location = str(bit)
						data_size = "NA"
						compressed_ratio = "-1"
						time_taken = "-1"
						num_incorrect = "-1"
						abs_diff = "-1"
						rmse = "-1"
						psnr = "-1"
						status = "HardSegFaultCoreDump"
						traceback = "NA"
						os.system("rm {}".format(core_file))
					else:
						print("Check Error Occured\n")
						print("Byte: {} Bit: {}\n".format(byte, bit))
						#print("Child ID: {}\n".format(child_process_id))
						error_info = error_mode
						byte_location = str(byte)
						flip_location = str(bit)
						data_size = "NA"
						compressed_ratio = "-1"
						time_taken = "-1"
						num_incorrect = "-1"
						abs_diff = "-1"
						rmse = "-1"
						psnr = "-1"
						status = "CheckError"
						traceback = "NA"
			else:
				print("TimeOut Occurred\n")
				print("Byte: {} Bit: {}\n".format(byte, bit))
				error_info = error_mode
				byte_location = str(byte)
				flip_location = str(bit)
				data_size = "NA"
				compressed_ratio = "-1"
				time_taken = "{}".format(timeout_limit)
				num_incorrect = "-1"
				abs_diff = "-1"
				rmse = "-1"
				psnr = "-1"
				status = "Timeout"
				traceback = "NA"
	
			# Print out to file
			output_row = "{},{},{},{},{},{},{},{},{},{},{},{}\n".format(data_size, compressed_ratio, error_info, byte_location, flip_location, time_taken, num_incorrect, abs_diff, rmse, psnr, status, traceback)
			output.write(output_row)
			output.flush()
		
	#Close output file
	output.close()


def main():
	if len(sys.argv) != 13:
		print("Incorrect Number of Arguements. . .")
		exit(-1)

	# Get input needed to run the experiment	
	process_id = int(sys.argv[1])
	data_path = sys.argv[2].strip()
	dims_input = sys.argv[3].strip()
	compressor = sys.argv[4].strip()
	error_mode = sys.argv[5].strip()
	error_bound = float(sys.argv[6])
	default_bound = float(sys.argv[7])
	start_range = int(sys.argv[8])
	end_range = int(sys.argv[9])
	unique_experiment_id = sys.argv[10].strip()
	output_file_name = sys.argv[11].strip()
	timeout_limit = int(sys.argv[12])

	print(dims_input)

	print("Starting Experiment:")
	startTime = datetime.now()

	# Split given range of byte positions into maximum number of cpu's
	bins = os.cpu_count()
	print("Splitting {}-{} of the data into {} bins".format(start_range,end_range,bins))

	bin_ranges = []
	positions_per_range = math.floor((end_range - start_range) / bins)
	start_value = start_range
	end_value = start_value + positions_per_range

	first = 0
	while True:
		if first == 0:
			first = 1
			start_value = start_range
			end_value = start_value + positions_per_range 
		else:
			start_value = end_value + 1
			end_value = start_value + positions_per_range
		if (end_value >= end_range):
			end_value = end_range
			bin_ranges.append([start_value, end_value])
			break
		else:
			bin_ranges.append([start_value, end_value])

	print("Ranges are as follows:\n")
	for item in bin_ranges:
		print(item)

	# Start running all of the subprocesses
	output_files = []
	subprocess_id = 0

	
	with concurrent.futures.ProcessPoolExecutor(max_workers=os.cpu_count()) as executor:
		for ranges in bin_ranges:
			start = ranges[0]
			end = ranges[1]
			print("Sending {} to {} to subprocess #{}".format(start, end, subprocess_id), flush=True)
			executor.submit(experiment, process_id, subprocess_id, data_path, dims_input, compressor, error_mode, error_bound, default_bound, start, end, unique_experiment_id, timeout_limit)
			output_files.append("subprocess_results/{}/process_{}_{}_subprocess_{}_results.csv".format(unique_experiment_id, process_id, unique_experiment_id, subprocess_id))
			subprocess_id = subprocess_id + 1
	

	# For debugging
	#experiment(process_id, subprocess_id, data_path, dims_input, compressor, error_mode, error_bound, bin_ranges[0][0], bin_ranges[0][1], unique_experiment_id, timeout_limit)

	print("Time To Completion:\n")
	print(datetime.now() - startTime)

	# When they have all completed put them into a single file for this subprocess
	print("Cleaning up subprocess files\n")

	process_output_file = "results/" + str(process_id) + "_" + output_file_name
	with open(process_output_file, "w+") as process_results:
		result_file = 0
		for result in output_files:
			with open(result) as res_file:
				count = 0
				for line in res_file:
					if count != 0:
						process_results.write(line)
					else:
						if result_file == 0:
							count = 1
							result_file = 1
							process_results.write(line)
						else:
							count = 1

	
	# Clear results for this experiment
	#os.system("rm subprocess_results/{}/process_{}_{}_subprocess*".format(unique_experiment_id, process_id, unique_experiment_id))
	#print("Temporary Files have been removed!")	
	

if __name__ == '__main__':
	main()
