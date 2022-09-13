#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>
#include <execinfo.h>

#include "libpressio.h"
#include "sz.h"

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

int main(int argc, char* argv[]) {
	//Catches segmentation faults and other signals
	if (signal (SIGSEGV, sigHandler) == SIG_ERR){
        	printf("Error setting segfault handler...\n");
	}

	printf("Starting Experiment\n");

    // PARSE USER INPUT
	// *******************
    char *data_path = "./data/CESM/CLDLOW_1_3600_1800.f32";
    size_t dims[] = { 6480000 };

    printf("Initializing Pressio\n");
    // get a handle to a compressor
    struct pressio* library = pressio_instance();
    struct pressio_compressor* compressor = pressio_get_compressor(library, "sz");

    printf("Setting Metrics\n");

    // configure metrics
    const char* metrics[] = { "size" };
    struct pressio_metrics* metrics_plugin = pressio_new_metrics(library, metrics, 1);
    pressio_compressor_set_metrics(compressor, metrics_plugin);

    printf("Setting SZ Parameters\n");

    // configure the compressor
    struct pressio_options* sz_options = pressio_compressor_get_options(compressor);

    pressio_options_set_integer(sz_options, "sz:error_bound_mode", ABS);
    // pressio_options_set_integer(sz_options, "sz:error_bound_mode", PW_REL);
    // pressio_options_set_integer(sz_options, "sz:error_bound_mode", PSNR);
    pressio_options_set_double(sz_options, "sz:abs_err_bound", 0.001);

    if (pressio_compressor_check_options(compressor, sz_options)) {
        printf("%s\n", pressio_compressor_error_msg(compressor));
        exit(pressio_compressor_error_code(compressor));
    }
    if (pressio_compressor_set_options(compressor, sz_options)) {
        printf("%s\n", pressio_compressor_error_msg(compressor));
        exit(pressio_compressor_error_code(compressor));
    }

    printf("Reading Data From File\n");

    // Read data from binary file
	FILE *fp;
	DATA= malloc(sizeof(float) * dims[0]);
	fp = fopen(data_path,"rb");
	if (fp == NULL){
		perror("ERROR: ");
		exit(-1);
	} else {
		fread(DATA, 4,  dims[0], fp);
		fclose(fp);
	}

    printf("Creating Data Structures\n");

    struct pressio_data* input_data = pressio_data_new_move(pressio_float_dtype, DATA, 1, dims, pressio_data_libc_free_fn, NULL);
    // creates an output dataset pointer
    struct pressio_data* compressed_data = pressio_data_new_empty(pressio_byte_dtype, 0, NULL);
    // configure the decompressed output area
    struct pressio_data* decompressed_data = pressio_data_new_empty(pressio_float_dtype, 1, dims);

    printf("Compressing Data\n");

    // compress the data
    if (pressio_compressor_compress(compressor, input_data, compressed_data)) {
        printf("%s\n", pressio_compressor_error_msg(compressor));
        exit(pressio_compressor_error_code(compressor));
    }

    printf("Decompressing Data\n");

    // decompress the data
    if (pressio_compressor_decompress(compressor, compressed_data, decompressed_data)) {
        printf("%s\n", pressio_compressor_error_msg(compressor));
        exit(pressio_compressor_error_code(compressor));
    }

    size_t out_bytes;
    RET_DATA = (float *)pressio_data_copy(decompressed_data, &out_bytes);

    printf("Get compression Ratio\n");

    // get the compression ratio
    struct pressio_options* metric_results = pressio_compressor_get_metrics_results(compressor);
    double compression_ratio = 0;
    if (pressio_options_get_double(metric_results, "size:compression_ratio", &compression_ratio)) {
        printf("failed to get compression ratio\n");
        exit(1);
    }
    printf("compression ratio: %lf\n", compression_ratio);

    printf("First Element of Original Data: %f\n", DATA[0]);
    printf("First Element of Decompressed Data: %f\n", RET_DATA[0]);

    printf("Freeing Data Pointers\n");
    // free the input, decompressed, and compressed data
    pressio_data_free(decompressed_data);
    pressio_data_free(compressed_data);
    pressio_data_free(input_data);

    printf("Freeing Options\n");
    // free options and the library
    pressio_options_free(sz_options);
    pressio_options_free(metric_results);
    pressio_compressor_release(compressor);
    pressio_release(library);

    printf("Freeing Other Data\n");
    //if(DATA){
    //	free(DATA);
    //}
    if (RET_DATA){
    	free(RET_DATA);
    }
    printf("End of Experiment\n");
    return 0;
}
