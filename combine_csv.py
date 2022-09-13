import os
import sys
import pula

def main():
	results_path = sys.argv[1].strip()
	final_output = sys.argv[2].strip()

	file_list = pula.get_file_names_in_directory(results_path)

	print("The following {} files will be pulled together:\n".format(len(file_list)))
	for f in file_list:
		print(f)
	input("Press ENTER to continue. . .\n")

	with open(final_output, "w+") as final_results:
		result_file = 0
		for result in file_list:
			file_path = results_path + result
			with open(file_path) as res_file:
				count = 0
				lines = 0 
				for line in res_file:
					if count != 0:
						final_results.write(line)
					else:
						if result_file == 0:
							count = 1
							result_file = 1
							final_results.write(line)
						else:
							count = 1
					lines = lines + 1
			print("Lines {}\n".format(lines))

	

if __name__ == '__main__':
	main()
