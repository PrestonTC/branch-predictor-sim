#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sim_bp.h"

/*  argc holds the number of command line arguments
    argv[] holds the commands themselves

    Example:-
    sim bimodal 6 gcc_trace.txt
    argc = 4
    argv[0] = "sim"
    argv[1] = "bimodal"
    argv[2] = "6"
    ... and so on
*/
void init_predictor(bp_params *params) {
    if (strcmp(params->bp_name, "bimodal") == 0) {
        unsigned long size = 1 << params->M2;
        params->bimodal_table = (unsigned char*)malloc(size * sizeof(unsigned char));
        for (unsigned long i = 0; i < size; i++) {
            params->bimodal_table[i] = 2;
        }
    }
    else if (strcmp(params->bp_name, "gshare") == 0) {
        unsigned long size = 1 << params->M1;
        params->gshare_table = (unsigned char*)malloc(size * sizeof(unsigned char));
        for (unsigned long i = 0; i < size; i++) {
            params->gshare_table[i] = 2;
        }
        params->global_history = 0;
    }
    else if (strcmp(params->bp_name, "hybrid") == 0) {
        unsigned long chooser_size = 1 << params->K;
        params->chooser_table = (unsigned char*)malloc(chooser_size * sizeof(unsigned char));
        for (unsigned long i = 0; i < chooser_size; i++) {
            params->chooser_table[i] = 1;
        }
        unsigned long gshare_size = 1 << params->M1;
        params->gshare_table = (unsigned char*)malloc(gshare_size * sizeof(unsigned char));
        for (unsigned long i = 0; i < gshare_size; i++) {
            params->gshare_table[i] = 2;
        }
        unsigned long bimodal_size = 1 << params->M2;
        params->bimodal_table = (unsigned char*)malloc(bimodal_size * sizeof(unsigned char));
        for (unsigned long i = 0; i < bimodal_size; i++) {
            params->bimodal_table[i] = 2;
        }
        params->global_history = 0;
    }
}

void free_predictor(bp_params *params) {
    if (params->bimodal_table) free(params->bimodal_table);
    if (params->gshare_table) free(params->gshare_table);
    if (params->chooser_table) free(params->chooser_table);
}

void print_final_contents(bp_params *params) {
    if (strcmp(params->bp_name, "bimodal") == 0) {
        printf("FINAL BIMODAL CONTENTS\n");
        unsigned long size = 1 << params->M2;
        for (unsigned long i = 0; i < size; i++) {
            printf("%lu      %u\n", i, params->bimodal_table[i]);
        }
    }
    else if (strcmp(params->bp_name, "gshare") == 0) {
        printf("FINAL GSHARE CONTENTS\n");
        unsigned long size = 1 << params->M1;
        for (unsigned long i = 0; i < size; i++) {
            printf("%lu      %u\n", i, params->gshare_table[i]);
        }
    }
    else if (strcmp(params->bp_name, "hybrid") == 0) {
        printf("FINAL CHOOSER CONTENTS\n");
        unsigned long chooser_size = 1 << params->K;
        for (unsigned long i = 0; i < chooser_size; i++) {
            printf("%lu      %u\n", i, params->chooser_table[i]);
        }
        printf("FINAL GSHARE CONTENTS\n");
        unsigned long gshare_size = 1 << params->M1;
        for (unsigned long i = 0; i < gshare_size; i++) {
            printf("%lu      %u\n", i, params->gshare_table[i]);
        }
        printf("FINAL BIMODAL CONTENTS\n");
        unsigned long bimodal_size = 1 << params->M2;
        for (unsigned long i = 0; i < bimodal_size; i++) {
            printf("%lu      %u\n", i, params->bimodal_table[i]);
        }
    }
}

int bimodal_predict(bp_params *params, unsigned long int addr, char outcome) {
    unsigned long index = (addr >> 2) & ((1 << params->M2) - 1);
    unsigned char prediction = params->bimodal_table[index];
    int pred_taken = prediction >= 2;
    if (outcome == 't') {
        if (prediction < 3) params->bimodal_table[index]++;
    }
    else {
        if (prediction > 0) params->bimodal_table[index]--;
    }
    return pred_taken == (outcome == 't');
}

int gshare_predict(bp_params *params, unsigned long int addr, char outcome) {
    unsigned long pc_upper_n = (addr >> (params->M1 - params->N + 2)) & ((1 << params->N) - 1);
    unsigned long xor_result = pc_upper_n ^ (params->global_history & ((1 << params->N) - 1));
    unsigned long mlessn_bits = (addr >> 2) & ((1 << (params->M1 - params->N)) - 1);
    unsigned long index = (xor_result << (params->M1 - params->N)) | mlessn_bits;
    unsigned char prediction = params->gshare_table[index];
    int pred_taken = prediction >= 2;
    if (outcome == 't') {
        if (prediction < 3) params->gshare_table[index]++;
    } else {
        if (prediction > 0) params->gshare_table[index]--;
    }
    if (outcome == 't') {
        params->global_history = ((1 << (params->N - 1)) | (params->global_history >> 1)) & ((1 << params->N) - 1);
    } else {
        params->global_history = (params->global_history >> 1) & ((1 << params->N) - 1);
    }
    return pred_taken == (outcome == 't');
}

int hybrid_predict(bp_params *params, unsigned long int addr, char outcome) {
    unsigned long pc_upper_n = (addr >> (params->M1 - params->N + 2)) & ((1 << params->N) - 1);
    unsigned long xor_result = pc_upper_n ^ (params->global_history & ((1 << params->N) - 1));
    unsigned long mlessn_bits = (addr >> 2) & ((1 << (params->M1 - params->N)) - 1);
    unsigned long gshare_index = (xor_result << (params->M1 - params->N)) | mlessn_bits;
    unsigned char gshare_prediction = params->gshare_table[gshare_index];
    int gshare_taken = gshare_prediction >= 2;
    unsigned long bimodal_index = (addr >> 2) & ((1 << params->M2) - 1);
    unsigned char bimodal_prediction = params->bimodal_table[bimodal_index];
    int bimodal_taken = bimodal_prediction >= 2;
    unsigned long chooser_index = (addr >> 2) & ((1 << params->K) - 1);
    unsigned char chooser = params->chooser_table[chooser_index];
    int final_prediction;
    if (chooser >= 2) {
        final_prediction = gshare_taken;
    } else {
        final_prediction = bimodal_taken;
    }
    if (chooser >= 2) {
        if (outcome == 't') {
            if (gshare_prediction < 3) params->gshare_table[gshare_index]++;
        } else {
            if (gshare_prediction > 0) params->gshare_table[gshare_index]--;
        }
    } else {
        if (outcome == 't') {
            if (bimodal_prediction < 3) params->bimodal_table[bimodal_index]++;
        } else {
            if (bimodal_prediction > 0) params->bimodal_table[bimodal_index]--;
        }
    }
    if (outcome == 't') {
        params->global_history = ((1 << (params->N - 1)) | (params->global_history >> 1)) & ((1 << params->N) - 1);
    } else {
        params->global_history = (params->global_history >> 1) & ((1 << params->N) - 1);
    }
    int gshare_correct = (gshare_taken == (outcome == 't'));
    int bimodal_correct = (bimodal_taken == (outcome == 't'));

    if (gshare_correct && !bimodal_correct) {
        if (chooser < 3) params->chooser_table[chooser_index]++;
    } else if (bimodal_correct && !gshare_correct) {
        if (chooser > 0) params->chooser_table[chooser_index]--;
    }
    return final_prediction == (outcome == 't');
}

int main (int argc, char* argv[]) {
    FILE *FP;               // File handler
    char *trace_file;       // Variable that holds trace file name;
    bp_params params;       // look at sim_bp.h header file for the the definition of struct bp_params
    char outcome;           // Variable holds branch outcome
    unsigned long int addr; // Variable holds the address read from input file
    unsigned int predictions = 0, mispredictions = 0;
    
    if (!(argc == 4 || argc == 5 || argc == 7)) {
        printf("Error: Wrong number of inputs:%d\n", argc-1);
        exit(EXIT_FAILURE);
    }
    params.bp_name  = argv[1];

    if(strcmp(params.bp_name, "bimodal") == 0) {
        if(argc != 4) {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M2       = strtoul(argv[2], NULL, 10);
        trace_file      = argv[3];
        printf("COMMAND\n%s %s %lu %s\n", argv[0], params.bp_name, params.M2, trace_file);
        init_predictor(&params);
    }
    else if(strcmp(params.bp_name, "gshare") == 0) {
        if(argc != 5) {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.M1       = strtoul(argv[2], NULL, 10);
        params.N        = strtoul(argv[3], NULL, 10);
        trace_file      = argv[4];
        printf("COMMAND\n%s %s %lu %lu %s\n", argv[0], params.bp_name, params.M1, params.N, trace_file);
        init_predictor(&params);
    }
    else if(strcmp(params.bp_name, "hybrid") == 0) {
        if(argc != 7) {
            printf("Error: %s wrong number of inputs:%d\n", params.bp_name, argc-1);
            exit(EXIT_FAILURE);
        }
        params.K        = strtoul(argv[2], NULL, 10);
        params.M1       = strtoul(argv[3], NULL, 10);
        params.N        = strtoul(argv[4], NULL, 10);
        params.M2       = strtoul(argv[5], NULL, 10);
        trace_file      = argv[6];
        printf("COMMAND\n%s %s %lu %lu %lu %lu %s\n", argv[0], params.bp_name, params.K, params.M1, params.N, params.M2, trace_file);
        init_predictor(&params);
    }
    else {
        printf("Error: Wrong branch predictor name:%s\n", params.bp_name);
        exit(EXIT_FAILURE);
    }
    
    // Open trace_file in read mode
    FP = fopen(trace_file, "r");
    if(FP == NULL) {
        // Throw error and exit if fopen() failed
        printf("Error: Unable to open file %s\n", trace_file);
        free_predictor(&params);
        exit(EXIT_FAILURE);
    }
    char str[2];
    while(fscanf(FP, "%lx %s", &addr, str) != EOF) {
        outcome = str[0];
        predictions++;
        int correct = 0;
        if (strcmp(params.bp_name, "bimodal") == 0) {
            correct = bimodal_predict(&params, addr, outcome);
        } else if (strcmp(params.bp_name, "gshare") == 0) {
            correct = gshare_predict(&params, addr, outcome);
        } else if (strcmp(params.bp_name, "hybrid") == 0) {
            correct = hybrid_predict(&params, addr, outcome);
        }
        if (!correct) mispredictions++;
    }
    printf("OUTPUT\n");
    printf("Number of predictions: %u\n", predictions);
    printf("Number of mispredictions: %u\n", mispredictions);
    printf("Misprediction rate: %.2f%%\n", (double)mispredictions / predictions * 100);
    print_final_contents(&params);
    fclose(FP);

    return 0;
}
