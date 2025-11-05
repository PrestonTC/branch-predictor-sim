#ifndef SIM_BP_H
#define SIM_BP_H

typedef struct bp_params{
    unsigned long int K;
    unsigned long int M1;
    unsigned long int M2;
    unsigned long int N;
    char*             bp_name;
    unsigned char     *bimodal_table;
    unsigned char     *gshare_table;
    unsigned char     *chooser_table;
    unsigned int      global_history;
}bp_params;

void init_predictor(bp_params *params);
void free_predictor(bp_params *params);
void print_final_contents(bp_params *params);
int bimodal_predict(bp_params *params, unsigned long int addr, char outcome);
int gshare_predict(bp_params *params, unsigned long int addr, char outcome);
int hybrid_predict(bp_params *params, unsigned long int addr, char outcome);

#endif
