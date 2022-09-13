#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>
#include <execinfo.h>
#include <getopt.h>

#include "libpressio.h"
#include "sz.h"

/*
 * GLOBAL VARIABLES
 */
// Print out extra options (1 for True, 0 for False)
int DEBUG = 0;
// Inject into the compression (1 for True, 0 for False)
int INJECT = 1;

// Initial Data Pointer
float *DATA;
// Faulted Decompressed Data Pointer
float *RET_DATA;

/*
 * Function: sigHandler
 * -------------------------------------------------------------------------------
 * Handles segmantation faults due to decompression errors.
 *
 * sig: the signal value of the error.
 * -------------------------------------------------------------------------------
 */
void sigHandler(int sig) {
	printf("Receiving Sig %d: ", sig);
	void *buffer[512];
	char **strings;
	int j, nptrs;

	nptrs = backtrace(buffer, 512);

	strings = backtrace_symbols(buffer, nptrs);
	if (strings == NULL){
		printf("Error with backtrace_symbols\n");
		exit(-1);
	}

	for (j = 0; j < nptrs; j++){
		printf("%s <- ", strings[j]);
	}
	printf("\n");
	free(strings);

	if (DATA){
		free(DATA);
	}
	if (RET_DATA){
		free(RET_DATA);
	}
	exit(sig);
}

/*
 * Function: startsWith
 * -------------------------------------------------------------------------------
 * Checks if one string starts with a substring
 *
 * returns: True or False
 * -------------------------------------------------------------------------------
 */
int startsWith(const char *a, const char *b) {
   if(strncmp(a, b, strlen(b)) == 0) return 1;
   return 0;
}

/*
 * Function: printBits
 * -------------------------------------------------------------------------------
 * Prints the bits of a given variable.
 *
 * size: the sizeof the variable
 * ptr: the pointer to the variable
 * -------------------------------------------------------------------------------
 */
void printBits(size_t const size, void const * const ptr){
	unsigned char *b = (unsigned char*) ptr;
	unsigned char byte;
	int i, j;

	for (i=size-1;i>=0;i--){
		for (j=7;j>=0;j--){
			byte = (b[i] >> j) & 1;
			printf("%u", byte);
		}
	}
}


/*
 * Function: szCompressionInjection
 * -------------------------------------------------------------------------------
 * Compresses the given data, injects a fault based on given
 * parameters, and returns the new decompressed data.
 *
 * compressor_choice: Compressor to use (sz, zfp)
 * error_bounding_mode: Error bound to use with compressor (sz=ABS,PSNR,PW_REL, zfp=Accuracy,Rate,Precision)
 * data_dimensions: Array of the 5 dimensions of the data.
 * char_loc: The byte position in the commpressed data 
 * to inject into.
 * flip_loc: The bit of the chosen byte to flip.
 * -------------------------------------------------------------------------------
 */
void szCompressionInjection(char * compressor_choice, char * error_bounding_mode, float error_bound, size_t * dims, int num_dims, int char_loc, int flip_loc, int injection_active){	

	// Initialize Pressio
	if (DEBUG){
		printf("Initializing Pressio\n");
	}

	if (strcmp(compressor_choice, "sz") == 0){
		// Configure SZ compressor
		if (DEBUG){
			printf("Configuring SZ Compressor\n");
		}
		// Initialize Pressio with SZ
		struct pressio* library = pressio_instance();
    	struct pressio_compressor* compressor = pressio_get_compressor(library, "sz");
		// Set compression metric to print
		const char* metrics[] = { "size" };
    	struct pressio_metrics* metrics_plugin = pressio_new_metrics(library, metrics, 1);
    	pressio_compressor_set_metrics(compressor, metrics_plugin);
		// Set values for SZ compression operations
		struct pressio_options* sz_options = pressio_compressor_get_options(compressor);
		if (strcmp(error_bounding_mode, "ABS") == 0){
			pressio_options_set_integer(sz_options, "sz:error_bound_mode", ABS);
			pressio_options_set_double(sz_options, "sz:abs_err_bound", error_bound);
		} else if (strcmp(error_bounding_mode, "PW_REL") == 0){
			pressio_options_set_integer(sz_options, "sz:error_bound_mode", PW_REL);
			pressio_options_set_double(sz_options, "sz:pw_rel_err_bound", error_bound);
		} else if (strcmp(error_bounding_mode, "PSNR") == 0){
			pressio_options_set_integer(sz_options, "sz:error_bound_mode", PSNR);
			pressio_options_set_double(sz_options, "sz:psnr_err_bound", error_bound);
		} else {
			printf("Invalid Error Bounding Mode...\n");
			printf("Exiting\n");
			exit(1);
		}
		// Check compression operation configurations
		if (pressio_compressor_check_options(compressor, sz_options)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		if (pressio_compressor_set_options(compressor, sz_options)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}

		// Convert input data to a pressio_data object
		struct pressio_data* input_data = pressio_data_new_move(pressio_float_dtype, DATA, num_dims, dims, pressio_data_libc_free_fn, NULL);
		// creates an output dataset pointer
		struct pressio_data* compressed_data = pressio_data_new_empty(pressio_byte_dtype, 0, NULL);
		// configure the decompressed output area
		struct pressio_data* decompressed_data = pressio_data_new_empty(pressio_float_dtype, num_dims, dims);

		// Compress data
		struct timeval c_start, c_stop;
		gettimeofday(&c_start, NULL);
		if (DEBUG){
			printf("Compressing Data\n");
		}
		if (pressio_compressor_compress(compressor, input_data, compressed_data)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		gettimeofday(&c_stop, NULL);

		// Get pointer to compressed data as well as the number of bytes
		size_t compressed_size;
		uint8_t * data = (uint8_t *)pressio_data_ptr(compressed_data, &compressed_size);

		// Generate flip mask based on flip_loc
		uint8_t mask = 0;
		uint8_t one = 1;
		mask = mask | (one << flip_loc);

		// XOR with area of byte array and save back to the array
		if (INJECT && injection_active){
			data[char_loc] = data[char_loc] ^ mask;
		}

		// Decompress data
		struct timeval d_start, d_stop;
		gettimeofday(&d_start, NULL);
		if (DEBUG){
			printf("Decompressing Data\n");
		}
		if (pressio_compressor_decompress(compressor, compressed_data, decompressed_data)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		gettimeofday(&d_stop, NULL);

		// Store newly decompressed data in ret data
		size_t out_bytes;
    	RET_DATA = (float *)pressio_data_copy(decompressed_data, &out_bytes);

		if (DEBUG){
			printf("Gathering Metrics\n");
		}
		// Get compression ratio and print
		struct pressio_options* metric_results = pressio_compressor_get_metrics_results(compressor);
		double compression_ratio = 0;
		if (pressio_options_get_double(metric_results, "size:compression_ratio", &compression_ratio)) {
			printf("Failed to get compression ratio\n");
		}
		printf("Compression Ratio: %lf\n", compression_ratio);
		printf("Compressed Data Size: %zu\n", compressed_size);

		// Calculate time taken to decompress and print
		double time_taken_compress = (double)(c_stop.tv_usec - c_start.tv_usec) / 1000000 + (double)(c_stop.tv_sec - c_start.tv_sec);
		double time_taken_decompress = (double)(d_stop.tv_usec - d_start.tv_usec) / 1000000 + (double)(d_stop.tv_sec - d_start.tv_sec);
		printf("Time to Compress: %lf\n", time_taken_compress);
		printf("Time to Decompress: %lf\n", time_taken_decompress);

		// Free un-nessecary structs
		pressio_data_free(decompressed_data);
    	pressio_data_free(compressed_data);
    	pressio_options_free(sz_options);
    	pressio_options_free(metric_results);
    	pressio_compressor_release(compressor);
    	pressio_release(library);

	} else if (strcmp(compressor_choice, "zfp") == 0){
		// Configure ZFP compressor
		if (DEBUG){
			printf("Configuring ZFP Compressor\n");
		}
		// Initialize Pressio with ZFP
		struct pressio* library = pressio_instance();
    	struct pressio_compressor* compressor = pressio_get_compressor(library, "zfp");
		// Set compression metric to print
		const char* metrics[] = { "size" };
    	struct pressio_metrics* metrics_plugin = pressio_new_metrics(library, metrics, 1);
    	pressio_compressor_set_metrics(compressor, metrics_plugin);
		// Set values for SZ compression operations
		struct pressio_options* zfp_options = pressio_compressor_get_options(compressor);
		if (strcmp(error_bounding_mode, "Accuracy") == 0){
			pressio_options_set_double(zfp_options, "zfp:accuracy", error_bound);
		} else if (strcmp(error_bounding_mode, "Rate") == 0){
			pressio_options_set_uinteger(zfp_options, "zfp:type", (unsigned int)3);
			pressio_options_set_uinteger(zfp_options, "zfp:dims", (unsigned int)num_dims);
			pressio_options_set_integer(zfp_options, "zfp:wra", 1);
			pressio_options_set_double(zfp_options, "zfp:rate", (double)error_bound);
		} else if (strcmp(error_bounding_mode, "Precision") == 0){
			pressio_options_set_uinteger(zfp_options, "zfp:precision", error_bound);
		} else {
			printf("Invalid Error Bounding Mode...\n");
			printf("Exiting\n");
			exit(1);
		}
		// Check compression operation configurations
		if (pressio_compressor_check_options(compressor, zfp_options)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		if (pressio_compressor_set_options(compressor, zfp_options)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}

		// Convert input data to a pressio_data object
		struct pressio_data* input_data = pressio_data_new_move(pressio_float_dtype, DATA, num_dims, dims, pressio_data_libc_free_fn, NULL);
		// creates an output dataset pointer
		struct pressio_data* compressed_data = pressio_data_new_empty(pressio_byte_dtype, 0, NULL);
		// configure the decompressed output area
		struct pressio_data* decompressed_data = pressio_data_new_empty(pressio_float_dtype, num_dims, dims);

		// Compress data
		struct timeval c_start, c_stop;
		gettimeofday(&c_start, NULL);
		if (DEBUG){
			printf("Compressing Data\n");
		}
		if (pressio_compressor_compress(compressor, input_data, compressed_data)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		gettimeofday(&c_stop, NULL);

		// Get pointer to compressed data as well as the number of bytes
		size_t compressed_size;
		uint8_t * data = (uint8_t *)pressio_data_ptr(compressed_data, &compressed_size);

		// Generate flip mask based on flip_loc
		uint8_t mask = 0;
		uint8_t one = 1;
		mask = mask | (one << flip_loc);

		// XOR with area of byte array and save back to the array
		if (INJECT && injection_active){
			data[char_loc] = data[char_loc] ^ mask;
		}

		// Decompress data
		struct timeval d_start, d_stop;
		gettimeofday(&d_start, NULL);
		if (DEBUG){
			printf("Decompressing Data\n");
		}
		if (pressio_compressor_decompress(compressor, compressed_data, decompressed_data)) {
			printf("%s\n", pressio_compressor_error_msg(compressor));
			exit(pressio_compressor_error_code(compressor));
		}
		gettimeofday(&d_stop, NULL);

		// Store newly decompressed data in ret data
		size_t out_bytes;
    	RET_DATA = (float *)pressio_data_copy(decompressed_data, &out_bytes);

		if (DEBUG){
			printf("Gathering Metrics\n");
		}
		// Get compression ratio and print
		struct pressio_options* metric_results = pressio_compressor_get_metrics_results(compressor);
		double compression_ratio = 0;
		if (pressio_options_get_double(metric_results, "size:compression_ratio", &compression_ratio)) {
			printf("Failed to get compression ratio\n");
		}
		printf("Compression Ratio: %lf\n", compression_ratio);
		printf("Compressed Data Size: %zu\n", compressed_size);

		// Calculate time taken to decompress and print
		double time_taken_compress = (double)(c_stop.tv_usec - c_start.tv_usec) / 1000000 + (double)(c_stop.tv_sec - c_start.tv_sec);
		double time_taken_decompress = (double)(d_stop.tv_usec - d_start.tv_usec) / 1000000 + (double)(d_stop.tv_sec - d_start.tv_sec);
		printf("Time to Compress: %lf\n", time_taken_compress);
		printf("Time to Decompress: %lf\n", time_taken_decompress);

		// Free un-nessecary structs
		pressio_data_free(decompressed_data);
    	pressio_data_free(compressed_data);
    	pressio_options_free(zfp_options);
    	pressio_options_free(metric_results);
    	pressio_compressor_release(compressor);
    	pressio_release(library);
	} else {
		printf("Invalid Compressor...\n");
		printf("Exiting\n");
		exit(1);
	}
	
	if (DEBUG) {
		printf("Successfully decompressed data\n");
	}
}

/* 
 * Function: main
 * -------------------------------------------------------------------------------
 * Takes user input and will compress data, inject a fault into the compressed 
 * data, and attempt to decompress it. Metrics on the outcome of the decompression
 * will be printed out throughout the process.
 *
 * -------------------------------------------------------------------------------
 */
int main(int argc, char *argv[]){
	int i;
	//Catches segmentation faults and other signals
	if (signal (SIGSEGV, sigHandler) == SIG_ERR){
        	printf("Error setting segfault handler...\n");
	}

	printf("Starting Experiment\n");

	// PARSE USER INPUT
	// *******************
	// Data Characteristics
	char *data_path;
    char * data_dimensions;
    size_t * dims;
    int data_size = 1;
	// Compressor Characteristics
	char * compressor;
	char * error_bounding_mode;
	float error_bound;
	float default_bound;
	// Fault Injection Characteristics
	int char_loc = 0;
	int flip_loc = 0;
	int injection_active = 0;

	// Parse input with getopt
	int option_index = 0;
    while (( option_index = getopt(argc, argv, "i:d:c:m:e:x:b:f:a:")) != -1){
        switch (option_index) {
            case 'i':
                data_path = optarg;
                break;
            case 'd':
                data_dimensions = optarg;
                break;
            case 'c':
                compressor = optarg;
                break;
            case 'm':
                error_bounding_mode = optarg;
                break;
            case 'e':
                error_bound = atof(optarg);
                break;
			case 'x':
                default_bound = atof(optarg);
                break;
            case 'b':
                char_loc = atoi(optarg);
                break;
            case 'f':
                flip_loc = atoi(optarg);
				if (flip_loc > 7){
					printf("ERROR: Flip Range Out of Bounds. . . \n");
					exit(-1);
				}
                break;
			case 'a':
				injection_active = atoi(optarg);
				break;
            default:
                printf("Options incorrect\n");
                return 1;
        }
    } 

	// Parse out dims from data_dimensions string
	int data_dimensions_temp[5] = {0};
    char *pt;
    int num_dims = 0;
	pt = strtok(data_dimensions, " ");
    while (pt != NULL) {
        data_dimensions_temp[num_dims] = atoi(pt);
        num_dims++;
        pt = strtok (NULL, " ");
    }

    dims = malloc(sizeof(size_t) * num_dims);
	for (i = 0; i < num_dims; i++){
		dims[i] = (size_t)data_dimensions_temp[i];
	}

	//Determine Data Size from dimensions
	for (i = 0; i < 5; i++){
		if (data_dimensions_temp[i] != 0){
			data_size = data_size * data_dimensions_temp[i];
		}	
	}

	// COMPRESS & INJECT
	// *******************
	// Read data from binary file
	FILE *fp;
	DATA= malloc(sizeof(float) * data_size);
	fp = fopen(data_path,"rb");
	if (fp == NULL){
		perror("ERROR: ");
		exit(-1);
	} else {
		fread(DATA, 4,  data_size, fp);
		fclose(fp);
	}

	// Print out all parameters
	printf("Data File: %s\n", data_path);
	printf("Data Dimensions: %d x %d x %d x %d x %d\n", data_dimensions_temp[0], data_dimensions_temp[1], data_dimensions_temp[2], data_dimensions_temp[3], data_dimensions_temp[4]);
	printf("Original Data Size in Bytes: %ld\n", sizeof(float)*data_size);
	printf("Compression Algorithm: %s\n", compressor);
	printf("Error Bounding Mode: %s\n", error_bounding_mode);
	printf("Error Bounding Value: %0.12f\n", error_bound);
	printf("Byte Location: %d\n", char_loc);
	printf("Flip Location: %d\n", flip_loc);

	// Call compression injection function
	szCompressionInjection(compressor, error_bounding_mode, error_bound, dims, num_dims, char_loc, flip_loc, injection_active);

	// Print small before and after if debugging is turned on
	if (DEBUG){	
		printf("Original Data:\n");
		for (i = 0; i < 10; i++){
			printf("%f\n", DATA[i]);
		}
		printf("New data:\n");
		for (i = 0; i < 10; i++){
			printf("%f\n", RET_DATA[i]);
		}
	}

	// CALCULATE METRICS
	// *******************
	int number_of_incorrect = 0;
	float max_diff = 0;
	float rmse_sum = 0;
	float max_val = -1;
	float min_val = -1;

	if(startsWith(error_bounding_mode, "ABS")){
		for (i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}

			float diff = fabs(a - b);
			if(diff > error_bound){
				if (DEBUG){
					printf("Before: %f\n", a);
					printf("After: %f\n", b);
					printf("Difference: %f\n", diff);
					printf("Err Bound:  %f\n", error_bound);
					return 0;
				}
				number_of_incorrect++;
			}
			if(diff > max_diff){
				max_diff = diff;
			}
		}
	}else if(startsWith(error_bounding_mode, "PSNR")){
		number_of_incorrect = -1;
		for (i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}

			float diff = fabs(a - b);
			if(diff > max_diff){
				max_diff = diff;
			}	
		}
	}else if(startsWith(error_bounding_mode, "PW_REL")){
		for(i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}
		
			float diff = fabs(a - b);
			float rel_bound = fabs(error_bound * a);
			if(diff > rel_bound){
				if (DEBUG){
					printf("Before: %f\n", a);
					printf("After: %f\n", b);
					printf("Difference: %f\n", diff);
					printf("Rel Bound:  %f\n", rel_bound);
					return 0;
				}
				number_of_incorrect++;
			}
			if (diff > max_diff){
				max_diff = diff;
			}				
		}
	}else if(startsWith(error_bounding_mode, "Accuracy")){
		for (i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}

			float diff = fabs(a - b);
			if(diff > error_bound){
				if (DEBUG){
					printf("Before: %f\n", a);
					printf("After: %f\n", b);
					printf("Difference: %f\n", diff);
					printf("Err Bound:  %f\n", error_bound);
					return 0;
				}
				number_of_incorrect++;
			}
			if(diff > max_diff){
				max_diff = diff;
			}
		}
	} else if(startsWith(error_bounding_mode, "Rate")){
		for (i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}

			float diff = fabs(a - b);
			if(diff > default_bound && default_bound != -1){
				if (DEBUG){
					printf("Before: %f\n", a);
					printf("After: %f\n", b);
					printf("Difference: %f\n", diff);
					printf("Err Bound:  %f\n", default_bound);
					return 0;
				}
				number_of_incorrect++;
			}
			if(diff > max_diff){
				max_diff = diff;
			}	
		}
	} else if(startsWith(error_bounding_mode, "Precision")){
		number_of_incorrect = -1;
		for (i = 0; i < data_size; i++){
			float a = DATA[i];
			float b = RET_DATA[i];

			// RMSE Work
			float rmse_diff = a - b;
			rmse_diff = rmse_diff * rmse_diff;
			rmse_sum = rmse_sum + rmse_diff;
			
			//PSNR Work
			if(a > max_val || max_val == -1){
				max_val = a;
			}
			if(a < min_val || min_val == -1){
				min_val = a;
			}

			float diff = fabs(a - b);
			if(diff > max_diff){
				max_diff = diff;
			}	
		}
	}
	

	//Calculate Root Mean Square Error 
	float rmse = rmse_sum / (data_size - 1);
	rmse = sqrt(rmse);

	//Check RMSE for bad values
	if (fpclassify(rmse) == FP_INFINITE) {
		rmse = FLT_MAX;
	} else if (fpclassify(rmse) == FP_NAN) {
		rmse = FLT_MAX;
	}

	//Calculate PSNR
	float psnr_control_value = 10000;
	float psnr = 0;
	if (rmse == 0){
		psnr = psnr_control_value;
	} else {
		psnr = 20 * log10((max_val - min_val) / rmse);	
	}	

	//Check PSNR for bad values
	if (fpclassify(psnr) == FP_INFINITE){
		psnr = psnr_control_value * -1;
	} else if (fpclassify(psnr) == FP_NAN) {
		psnr = psnr_control_value * -1;
	}
	
	//Check Maximum Difference for bad values
	if (fpclassify(max_diff) == FP_INFINITE){
		max_diff = FLT_MAX;
	} else if (fpclassify(max_diff) == FP_NAN) {
		max_diff = FLT_MAX;
	}

	//Print Metrics
	printf("Number of Incorrect: %d\n", number_of_incorrect);
	printf("Maximum Absolute Difference: %f\n", max_diff);
	printf("Root Mean Squared Error: %f\n", rmse);
	printf("PSNR: %f\n", psnr);

	if(DATA){
		free(DATA);
	}
	if (RET_DATA){
		free(RET_DATA);
	}
	printf("End of Experiment\n");
	return 0;
}
