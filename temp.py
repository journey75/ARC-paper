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

def main():

    command = './comp_inj -i ./data/xx.dat2 -d "2869440" -c sz -m ABS -e 0.001 -b 0 -f 0 -a 1'
    timeout_limit = 20
    #command = './comp_inj -i {} -d {} -c {} -m {} -e {} -b {} -f {} -a {}'.format(data_path, dims_input, compressor, error_mode, error_bound, byte, bit, 1)
    #proc_result, child_process_id = popen_timeout(['./comp_inj', '-i', data_path, '-d', dims_input, '-c', compressor, '-m', error_mode, '-e', error_bound, '-b', str(byte), '-f', str(bit), '-a', 1], timeout_limit)
    proc_result, child_process_id = popen_timeout(command, timeout_limit)
    response = str(proc_result[0])
    print(response)

    print("Finished")


if __name__ == '__main__':
	main()