//
// Created by Johnny on 06/09/2018.
//

//
// Created by Johnny on 04/09/2018.
//

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <getopt.h>
#include <cstdio>
#include <getopt.h>
#include <cstdlib>
#include <dirent.h>
#include <ctime>
#include <cstddef>

#include "lirc_private.h"


static const logchannel_t logchannel = LOG_APP;

static int _directory_len;
static bool NO_CONFIG;
static struct ir_remote MY_REMOTE = {0};
static int BIT_DELIMITER;
static int SIGNAL_DELIMITER;
static char* CHECKSUM_ALG;
static char* DIRECTORY;
#define DIRECTORY_LEN (DIRECTORY ? (_directory_len == 0 ? (_directory_len = strlen(DIRECTORY)) : _directory_len) : 0)

static const struct option options[] = {
        { "name",               required_argument,  NULL, 'n'},
        { "signal_delimiter",   required_argument,  NULL, 's'},
        { "bit_delimiter",      required_argument,  NULL, 'b'},
        { "help",               no_argument,        NULL, 'h'},
        { "decode",             no_argument,        NULL, 'd'},
        { 0, 0, 0, 0}
};

static const char* const help =
        "\n"
        "Creates a new configuration file that can be used by irsend \"STATEFUL_SEND\" directive\n"
        "Usage: stateful_create [options] <directory>\n\n"
        "[options]\n"
        "-n: remote name\n"
        "-h: help\n"
        "-d: Creates a file will all signals decoded in binary. A configuration file will not be created"
        "-s: signal delimiter (this value is summed with phead in order to understand where a signal ends. "
        "Default: 5000)\n"
        "-b: bit delimiter (if the distance between pulse and space is greater than this value "
        "then is a \"1\" bit, otherwise is \"0\" bit. Default 400)\n"
        "-c: checksum algorithm (the name of the algorithm that will calculate the last byte)\n"
        "\n"
        "The <directory> should contain:\n"
        "Config File:\n"
        "A file named \"lircd.conf\". This file is created during this process.\n"
        "If you already have this file, you can omit the Main File by including directly this file.\n\n"
        "If you don't have a Config File you must include a Main File:\n"
        "A file named \"main.sa\" that contains as many signals as possible from different remote buttons.\n"
        "This file will be used to understand some mandatory variables. "
        "Use mode2 to create this file. (i.e. mode2 --driver default --device /dev/lirc0 -m > name.sa)\n"
        "NOTE 1: -m option is mandatory.\n\n"
        "Button Files:\n"
        "One file per button (i.e. mode2 --driver default --device /dev/lirc0 -m > power.sa).\n"
        "Each file should contain all possible states that the current button can have.\n"
        "Do not change other settings of the remote while registering.\n"
        "The first state you register, will be the default one.\n"
        "The default ones are used the first time when you don't specify any command. (i.e. STATEFUL_SEND MY_REMOTE \"\")\n"
        "All the next times, the previous state will be used.\n";

static void init(){
    MY_REMOTE.eps = 30;
    MY_REMOTE.aeps = 100;
    MY_REMOTE.name = (char*)"STATEFUL_REMOTE";
    SIGNAL_DELIMITER = 0;
    BIT_DELIMITER = 0;
    DIRECTORY = NULL;
    CHECKSUM_ALG = NULL;
    _directory_len = 0;
    NO_CONFIG = false;
}

bool compare_files(void* arg1, void* arg2) {
    return equals_to(*(const char **) arg1, (const char *) arg2, false);
}

int compare_ir_ncode_name(const void *arg1, const void *arg2){
    struct ir_ncode *a = *(struct ir_ncode**)arg1;
    struct ir_ncode *b = *(struct ir_ncode**)arg2;

    int *a_info = get_info_from_command_name(a->name);
    int *b_info = get_info_from_command_name(b->name);
    int i;
    int ret = 0;

    for(i = 0; i < 3 && ret == 0; i++){
        if(a_info[i] < b_info[i])
            ret = -1;
        else if(a_info[i] > b_info[i])
            ret = 1;
    }

    free(a_info);
    free(b_info);

    if(ret == 0){
        if(a->code < b->code)
            ret = -1;
        else if(a->code > b->code)
            ret = 1;
    }

    return ret;
}

void free_ir_ncode(struct ir_ncode *codes){
    if(codes){
        if(codes->name)
            free(codes->name);
        free(codes);
    }
}

struct ir_ncode *create_ir_ncode(int byte, int from, int to, char* bitArr){
    struct ir_ncode *code = (struct ir_ncode *)malloc(sizeof(struct ir_ncode));
    code->name = (char*)malloc(sizeof(char) * MAX_STR);
    sprintf(code->name, CMD_FRMT, SUFFIX, byte, from, to);
    code->length = 0;
    code->signals = NULL;
    code->code = (ir_code)strtol(bitArr, NULL, 2);
    code->next = NULL;
    return code;
}

void remove_duplicate_ir_ncodes(struct ir_ncode **codes){
    if(!(*codes)) return;

    struct ir_ncode *prev = *codes;
    struct ir_ncode *curr = (*codes)->next_ncode;

    while (curr){
        if(equals_to(prev->name, curr->name, false) && prev->code == curr->code){
            struct ir_ncode *tmp = curr;
            curr = curr->next_ncode;
            prev->next_ncode = curr;
            free_ir_ncode(tmp);
        } else {
            prev = curr;
            curr = curr->next_ncode;
        }
    }
}

void sort_ir_ncodes_by_name(struct ir_ncode **codes){
    struct ir_ncode *p = *codes;

    if(!p) return;

    int len = 1;
    //count;
    while((p = p->next_ncode)) len++;

    p = *codes;
    struct ir_ncode *codes_arr[len];
    int i = 0;
    while(p){
        codes_arr[i++] = p;
        p = p->next_ncode;
    }

    qsort(codes_arr, len, sizeof(struct ir_ncode*), compare_ir_ncode_name);
    for(i = 0; i < len; i++){
        if(i == len - 1){
            codes_arr[i]->next_ncode = NULL;
        } else {
            codes_arr[i]->next_ncode = codes_arr[i + 1];
        }
    }
    *codes = codes_arr[0];
}

void add_missing_ir_ncodes(struct ir_ncode **codes, const int* def_bits, int bits){
    int lastByte = 8;
    bits -= lastByte;
    struct ir_ncode *curr = *codes;
    struct ir_ncode *prev = NULL;
    int *info = NULL;
    int i = 0;

    while(curr){

        info = get_info_from_command_name(curr->name);

        while(i < bits){
            int currByte = i / 8 + 1;
            int currBit = i % 8 + 1;
            int toBit;

            if(currByte == info[0]){
                if(currBit >= info[1] && currBit <= info[2]){
                    i++;
                    //go to next bit
                    continue;
                }
                if(currBit > info[2]){
                    //go to next ir code
                    break;
                }
                toBit = info[1] - 1;
            } else if(currByte < info[0]){
                toBit = 8;
            } else {
                //Go to next ir code
                break;
            }

            int bitArrSize = toBit - currBit + 1;
            char bitArr[bitArrSize + 1];
            int j;
            for(j=0; j < bitArrSize; j++) bitArr[j] = (char)(def_bits[i++] + '0');
            bitArr[j] = '\0';

            struct ir_ncode *code = create_ir_ncode(currByte, currBit, toBit, bitArr);
            code->next_ncode = curr;

            if(!prev) *codes = code;
            else prev->next_ncode = code;

            prev = code;
        }

        free(info);
        prev = curr;
        curr = curr->next_ncode;
    }

    if(!prev) return;
    info = get_info_from_command_name(prev->name);
    //finish previous byte if not finished yet
    {
        if(info[2] != 8){
            int bitArrSize = 8 - info[2];
            char bitArr[bitArrSize + 1];
            int j;
            for(j=0; j < bitArrSize; j++) bitArr[j] = (char)(def_bits[i++] + '0');
            bitArr[j] = '\0';
            struct ir_ncode *code = create_ir_ncode(info[0], info[2] + 1, 8, bitArr);
            code->next_ncode = NULL;
            prev->next_ncode = code;
            prev = code;
        }
    }
    //add all next bytes
    while(i < bits){
        int currByte = i / 8 + 1;
        char bitArr[9];
        int j;
        for(j=0; j < 8; j++) bitArr[j] = (char)(def_bits[i++] + '0');
        bitArr[j] = '\0';

        struct ir_ncode *code = create_ir_ncode(currByte, 1, 8, bitArr);
        code->next_ncode = NULL;
        prev->next_ncode = code;
        prev = code;
    }
}

int* decode_signal(int* signal) {
    int *current_signal_decoded = (int*)malloc(sizeof(int)*MY_REMOTE.bits);
    int *return_me = current_signal_decoded;
    int c = 0, pulse = 0, space = 0;
    while (*signal != 0) {
        if (c <= 1) {
            //skip heading pulse and space
            c++;
            signal++;
            continue;
        }
        if (!pulse) {
            pulse = *signal;
        } else {
            space = *signal;
            int sum = pulse + space;
            int val = abs((sum - (MY_REMOTE.pone + MY_REMOTE.sone))) < abs((sum - (MY_REMOTE.pzero + MY_REMOTE.szero)));
            *current_signal_decoded++ = val;
            pulse = 0;
        }
        signal++;
    }
    //last pulse is ignored as it represents the trailing pulse
    return return_me;
}

void add_ir_ncodes(struct ir_ncode **codes, char* name, dynamic_list *signals, int bits){
    if(!signals->head) {
        //No signals. Nothing to do
        return;
    }

    // Getting changing bits
    int i, j = 0;
    int lastByteLen = 8;
    bits -= lastByteLen;

    int changedBits[bits];
    memset(changedBits, 0, sizeof(changedBits));

    for(i=0; i<bits; i++)
    {
        dynamic_list_node *signalNode = signals->head;
        int prevBitVal = ((int*)signalNode->data)[i];
        signalNode = signalNode->next;
        while(signalNode != NULL && ((int*)signalNode->data)[i] == prevBitVal) signalNode = signalNode->next;
        if(signalNode) changedBits[j++] = i;
    }

    if(j == 0){
        // todo some logging
        return;
    }

    //Getting range from changing bits
    int range[j];
    int rangeIndex = 0;
    memset(range, 0, sizeof(range));

    int prevByte = 0;
    int a = 0;
    for(i = 0; i < j; i++) {
        int currByte = changedBits[i]/8+1;
        if(currByte != prevByte){
            if(a) {
                range[rangeIndex++] = changedBits[i-1];
                a = 0;
            }
            prevByte = currByte;
        }
        if(!a) {
            range[rangeIndex++] = changedBits[i];
            a = 1;
        }
    }
    if(a) range[rangeIndex++] = changedBits[i-1];

    dynamic_list_node *node = signals->head;

    while(node){
        for(i=0; i<rangeIndex; i+=2){
            int from = range[i];
            int to = range[i+1];
            char bitArr[to-from+2];
            int i_range = 0;
            while(from <= to){
                bitArr[i_range++] = (char)(((int*)node->data)[from] + '0');
                from++;
            }
            bitArr[i_range] = '\0';
            struct ir_ncode *code = create_ir_ncode(range[i]/8+1, range[i]%8+1, range[i+1]%8+1, bitArr);
            code->next_ncode = *codes;
            *codes = code;
        }
        node = node->next;
    }
    //print the range for the user
    printf("For file %s, valid codes are:\n", name);
    for(i=0; i<rangeIndex; i+=2){
        printf(CMD_FRMT, SUFFIX, range[i]/8+1, range[i]%8+1, range[i+1]%8+1);
        printf("\t");
    }
    printf("\n");
}

bool create_lircd_cfg(dynamic_list files) {
    char decoded_path[DIRECTORY_LEN + 20];
    sprintf(decoded_path, "%s/decoded.txt", DIRECTORY);

    if(NO_CONFIG){
        printf("Creating %s\n", decoded_path);
        FILE *decode = fopen("decode.txt", "w");
        if(!decode){
            printf("Could't create decode.txt\n");
            perror("");
            return false;
        }
        fclose(decode);
    }
    else
    {
        printf("Creating %s\n", LIRC_CFG_FILE);
    }

    struct ir_ncode *codes = NULL;
    if(!MY_REMOTE.pone || !MY_REMOTE.sone || !MY_REMOTE.pzero || !MY_REMOTE.szero || !MY_REMOTE.bits)
    {
        printf("Unable to create %s. Undefined mandatory variables\n", LIRC_CFG_FILE);
        return false;
    }

    if(MY_REMOTE.bits % 8){
        printf("Unable to create lircd.config. Incorrect number of bits\n");
        return false;
    }

    dynamic_list_node *fileNode = files.head;
    int signal_len = MY_REMOTE.bits * 2 + 4;
    int current_signal[signal_len];
    int *first_code = NULL;

    size_t current_signal_size = sizeof(current_signal);
    dynamic_list signals;
    char l[LINE_LENGTH];

    while(fileNode){
        //do not read main file or configuration file
        if(compare_files(fileNode->data, (void*)MAIN_FILE) ||
           compare_files(fileNode->data, (void*)LIRC_CFG_FILE)) {
            fileNode = fileNode->next;
            continue;
        }
        char* file_name = *(char**)fileNode->data;
        char file_path[DIRECTORY_LEN + strlen(file_name) + 2];
        sprintf(file_path, "%s/%s", DIRECTORY, file_name);
        FILE *file = fopen(file_path, "r");
        if(!file){
            printf("Could't open %s\n", file_path);
            perror("");
            return false;
        }

        bool first_line = true;
        int counter = 0;
        list_new(&signals, sizeof(int) * MY_REMOTE.bits, NULL);
        memset(current_signal, 0, current_signal_size);

        while(fgets(l, sizeof(l), file)){
            char* line = l;
            if(first_line)
            {
                // This is the time elapsed between the recording start
                // and the arrival of the first IR signal
                // This line can be ignored
                first_line = false;
                continue;
            }
            if(strlen(line) <= 1){
                //Empty Row
                continue;
            }

            char* token = strtok(line, " ");
            while(token){
                int val = to_integer(trim(token), 0);
                if(counter >= signal_len){
                    // skip till the end of this signal
                    if(val > SIGNAL_DELIMITER){
                        counter = 0;
                        memset(current_signal, 0, current_signal_size);
                    }
                } else {

                    if(val > SIGNAL_DELIMITER){
                        if(counter == signal_len - 1){
                            list_append(&signals, decode_signal(current_signal));
                        }
                        counter = 0;
                        memset(current_signal, 0, current_signal_size);
                    } else {
                        current_signal[counter++] = val;
                    }
                }
                token = strtok(NULL, " ");
            }
        }
        if(counter == signal_len - 1){
            list_append(&signals, decode_signal(current_signal));
        }
        fclose(file);

        if(list_size(&signals)){
            if(NO_CONFIG){
                FILE *decoded = fopen(decoded_path, "a");
                dynamic_list_node *signalNode = signals.head;
                int i;
                fprintf(decoded, "File: %s\n", file_name);
                while(signalNode != NULL) {
                    for(i = 0; i < MY_REMOTE.bits; i++)
                        fprintf(decoded, "%i", ((int*)signalNode->data)[i]);
                    fprintf(decoded, "\n");
                    signalNode = signalNode->next;
                }
                fclose(decoded);
            } else {
                add_ir_ncodes(&codes, file_name, &signals, MY_REMOTE.bits);
                if(!first_code){
                    //save first code for future use
                    int i;
                    first_code = (int*)malloc(sizeof(int) * MY_REMOTE.bits);
                    for(i = 0; i < MY_REMOTE.bits; i++) first_code[i] = ((int*)signals.head->data)[i];
                }
            }
        }
        list_destroy(&signals);
        fileNode = fileNode->next;
    }

    if(NO_CONFIG){
        printf("Created.\n");
        return true;
    }

    if(!codes){
        printf("No valid signals found.\n");
        return false;
    }

    sort_ir_ncodes_by_name(&codes);
    remove_duplicate_ir_ncodes(&codes);
    add_missing_ir_ncodes(&codes, first_code, MY_REMOTE.bits);
    //convert list to array to use default dump function
    // also add here the sequence number
    struct ir_ncode *tmp = codes;
    int len = 0;
    int i;
    while(tmp){
        len++;
        tmp = tmp->next_ncode;
    }
    tmp = codes;
    struct ir_ncode codesArr[len + 1];
    int seq = 1;
    char* prev_name = NULL;
    for(i = 0; i < len; i++){
        char buffer[strlen(tmp->name) + 12];
        memset(buffer, 0, sizeof(buffer));

        if(prev_name){
            if(equals_to(prev_name, tmp->name, false))
                seq++;
            else
                seq = 1;
            free(prev_name);
            prev_name = tmp->name;
        } else {
            prev_name = tmp->name;
        }

        sprintf(buffer, "%s_%i", tmp->name, seq);
        tmp->name = strdup(buffer);

        codesArr[i] = *tmp;
        tmp = tmp->next_ncode;
    }
    if(prev_name){
        free(prev_name);
    }
    struct ir_ncode *last_ncode = (struct ir_ncode*)malloc(sizeof(struct ir_ncode));
    last_ncode->name = NULL;
    codesArr[i] = *last_ncode;

    MY_REMOTE.codes = &codesArr[0];

    char file_path[DIRECTORY_LEN + LIRC_CFG_FILE_LEN + 2];
    sprintf(file_path, "%s/%s", DIRECTORY, LIRC_CFG_FILE);
    FILE *file = fopen(file_path, "w");
    if(!file){
        printf("Could't open %s\n", file_path);
        perror("");
        while (codes){
            free_ir_ncode(codes);
            codes = codes->next_ncode;
        }
        return false;
    }

    fprint_remote(file, &MY_REMOTE, "");
    fclose(file);

    while (codes){
        free_ir_ncode(codes);
        codes = codes->next_ncode;
    }
    //todo free MY_REMOTE
    printf("%s created.\n", file_path);
    return true;
}

static void parse_options(int argc, char* argv[]) {
    int c;
    const char *const optstring = "n:s:b:c:hd";
    init();

    while (true) {
        c = getopt_long(argc, argv, optstring, options, NULL);
        if (c == -1)
            break;
        switch (c) {
            case 'n':
                //no need of strdup as we don't alter this value
                MY_REMOTE.name = optarg;
                break;
            case 's':
                SIGNAL_DELIMITER = to_integer(optarg, 0);
                break;
            case 'b':
                BIT_DELIMITER = to_integer(optarg, 0);;
                break;
            case 'c':
                //no need of strdup as we don't alter this value
                CHECKSUM_ALG = optarg;
                break;
            case 'd':
                NO_CONFIG = true;
                break;
            case 'h':
                printf("%s\n", help);
                exit(EXIT_SUCCESS);
            default:
                return;
        }
    }
    if(!SIGNAL_DELIMITER) SIGNAL_DELIMITER = _SIGNAL_DELIMITER;
    if(!BIT_DELIMITER) BIT_DELIMITER = _BIT_DELIMITER;
    //no need of strdup as we don't alter this value
}

dynamic_list get_all_files(DIR* dir) {
    dynamic_list lst;
    struct dirent *entry;

    list_new(&lst, sizeof(char *), NULL);

    while ((entry = readdir(dir))) {
        //get only *.sa and the lircd.conf file

        if (!ends_with(entry->d_name, ".sa") && !equals_to(entry->d_name, LIRC_CFG_FILE, false))
            continue;

        char *name = strdup(entry->d_name);
        list_append(&lst, &name);
    }
    return lst;
}

bool process_cfg(){
    FILE* f;

    char file_path[DIRECTORY_LEN + LIRC_CFG_FILE_LEN + 2];
    sprintf(file_path, "%s/%s", DIRECTORY, LIRC_CFG_FILE);
    f = fopen(file_path, "r");

    if(f == NULL){
        printf("Could't open %s\n", file_path);
        perror("");
        return false;
    }

    struct ir_remote *tmp = read_config(f, file_path);

    if(!tmp){
        fclose(f);
        return false;
    }

    MY_REMOTE.aeps = tmp->aeps;
    MY_REMOTE.eps = tmp->eps;
    MY_REMOTE.phead = tmp->phead;
    MY_REMOTE.shead = tmp->shead;
    MY_REMOTE.bits = tmp->bits;
    MY_REMOTE.pzero = tmp->pzero;
    MY_REMOTE.szero = tmp->szero;
    MY_REMOTE.pone = tmp->pone;
    MY_REMOTE.sone = tmp->sone;
    MY_REMOTE.ptrail = tmp->ptrail;
    MY_REMOTE.gap = tmp->gap;
    if(tmp->checksum_alg)
    {
        MY_REMOTE.checksum_alg = strdup(tmp->checksum_alg);
    }

    SIGNAL_DELIMITER += MY_REMOTE.phead;

    free_config(tmp);
    fclose(f);
    return true;
}

bool calculate_header(dynamic_list signals){
    if(!list_size(&signals))
    {
        printf("%s does not contain enough signals.", MAIN_FILE);
        return false;
    }

    int one_pulse_c = 0,
            one_space_c = 0,
            zero_pulse_c = 0,
            zero_space_c = 0,
            trailing_pulse_c = 0,
            p_head = 0, s_head = 0,
            p_zero = 0, s_zero = 0,
            p_one = 0, s_one = 0,
            p_trail = 0;
    dynamic_list_node *node = signals.head;
    while(node) {
        int pulse = 0; // odd position (starting with 1)
        int space = 0; // even position
        int j = 0;
        int *signal = (int *) node->data;

        while (*(signal) != 0) {
            if (j == 0) {
                // first pulse
                p_head += *signal++;
                j++;
                continue;
            }
            if (j == 1) {
                //first space
                s_head += *signal++;
                j++;
                continue;
            }
            // we are somewhere in the middle of the signal
            if (!pulse) {
                pulse = *signal++;
            } else {
                space = *signal++;

                if (abs(pulse - space) < BIT_DELIMITER) {
                    //this is a zero bit
                    p_zero += pulse;
                    s_zero += space;
                    zero_pulse_c++;
                    zero_space_c++;
                } else {
                    // this is a one bit
                    p_one += pulse;
                    s_one += space;
                    one_pulse_c++;
                    one_space_c++;
                }
                pulse = 0;
            }
        }
        if (pulse) {
            p_trail += pulse;
            trailing_pulse_c++;
        }
        node = node->next;
    }
    MY_REMOTE.phead = p_head / list_size(&signals);
    MY_REMOTE.shead = s_head / list_size(&signals);
    MY_REMOTE.ptrail = roundUpDown(p_trail / trailing_pulse_c, 5);
    MY_REMOTE.pone = roundUpDown(p_one / one_pulse_c, 5);
    MY_REMOTE.sone = roundUpDown(s_one / one_space_c, 5);
    MY_REMOTE.pzero = roundUpDown(p_zero / zero_pulse_c, 5);
    MY_REMOTE.szero =  roundUpDown(s_zero / zero_space_c, 5);

    return true;
}

bool process_main() {
    char file_path[DIRECTORY_LEN + MAIN_FILE_LEN + 2];
    char l[LINE_LENGTH];
    dynamic_list signals;

    int delimiter = 0, counter = 0, gap = 0, sum_gap = 0, gap_counter = 0;
    sprintf(file_path, "%s/%s", DIRECTORY, MAIN_FILE);
    FILE *file = fopen(file_path, "r");

    if (file == NULL) {
        printf("Could't open %s\n", file_path);
        perror("");
        return false;
    }
    //skip first line;
    if(!fgets(l, LINE_LENGTH, file)){
        printf("File empty: %s", file_path);
        return false;
    }
    //get signal len
    int signal_len = 0;
    while (fgets(l, LINE_LENGTH, file)) {
        if (strlen(l) <= 1)
        {
            if(signal_len) break;
            continue;
        }
        char *token = strtok(l, " ");
        if (!delimiter) delimiter = SIGNAL_DELIMITER + to_integer(token, 0);

        while (token) {
            if(to_integer(trim(token), 0) > delimiter) break;
            signal_len++;
            token = strtok(NULL, " ");
        }
    }

    int current_signal[++signal_len];
    size_t current_signal_size = sizeof(current_signal);

    list_new(&signals, current_signal_size, NULL);
    memset(current_signal, 0, current_signal_size);

    while (fgets(l, sizeof(l), file)) {
        char* line = l;

        if(strlen(line) <= 1){
            //empty line
            gap = 0;
            continue;
        }

        char* token = strtok(line, " ");
        while(token){
            token = trim(token);
            int val = to_integer(token, 0);

            if(val > delimiter){
                gap = val;
                list_append(&signals, current_signal);
                counter = 0;
                memset(current_signal, 0, current_signal_size);
            } else {
                if(gap) {
                    sum_gap += gap;
                    gap_counter++;
                    gap = 0;
                }
                current_signal[counter++] = val;
                if(counter == signal_len){
                    //invalid main file. terminate now
                    list_destroy(&signals);
                    printf("%s not valid. All signals should have the same length.", MAIN_FILE);
                    return false;
                }
            }
            token = strtok(NULL, " ");
        }
    }
    if(counter) list_append(&signals, current_signal);

    SIGNAL_DELIMITER = delimiter;
    // subtract head pulse and space, 0 terminator and trailing pulse
    MY_REMOTE.bits = (signal_len - 4) / 2;

    if(gap_counter) {
        MY_REMOTE.gap = (uint32_t)roundUpDown(sum_gap / gap_counter, 5);
    }
    bool ret = calculate_header(signals);
    list_destroy(&signals);
    return ret;
}

void make_backup(const char* file){
    struct timeval t;
    size_t len = strlen(file);
    char _old[DIRECTORY_LEN + len + 2];
    char _new[DIRECTORY_LEN + len + 66];
    gettimeofday(&t, NULL);
    sprintf(_old, "%s/%s", DIRECTORY, LIRC_CFG_FILE);
    sprintf(_new, "%s/%s.%ld", DIRECTORY, LIRC_CFG_FILE, t.tv_sec);
    rename(_old, _new);
}

int main(int argc, char* argv[]){
    dynamic_list files;
    DIR *dp;

    parse_options(argc, argv);
    if (optind >= argc) {
        printf("Not enough arguments.\n%s\n", help);
        return EXIT_FAILURE;
    }

    DIRECTORY = argv[optind];
    if (!(dp = opendir(DIRECTORY))) {
        printf("Could not open directory: %s\n", DIRECTORY);
        perror("");
        return EXIT_FAILURE;
    }

    files = get_all_files(dp);
    closedir(dp);

    if (!list_size(&files)) {
        printf("No valid files found\n");
        list_destroy(&files);
        return EXIT_FAILURE;
    }

    if (list_contains(&files, (void*)LIRC_CFG_FILE, compare_files)){
        if(!process_cfg()){
            if(MY_REMOTE.checksum_alg){
                free((void*)MY_REMOTE.checksum_alg);
            }
            list_destroy(&files);
            return EXIT_FAILURE;
        }
        //backup this config file only if a new one will be created
        if(!NO_CONFIG)
        {
            make_backup(LIRC_CFG_FILE);
        }
    } else if (list_contains(&files, (void*)MAIN_FILE, compare_files)) {
        if (!process_main()) {
            list_destroy(&files);
            return  EXIT_FAILURE;
        }
    } else {
        printf("%s and %s missing in %s. Use -h for help.\n", MAIN_FILE, LIRC_CFG_FILE, DIRECTORY);
        list_destroy(&files);
        return EXIT_FAILURE;
    }

    //if we got this as input, we must force it
    if(CHECKSUM_ALG){
        if(MY_REMOTE.checksum_alg)
            free((void*)MY_REMOTE.checksum_alg);
        MY_REMOTE.checksum_alg = strdup(CHECKSUM_ALG);
    } else if(!MY_REMOTE.checksum_alg){
        //add default value
        MY_REMOTE.checksum_alg = strdup("REVERSE_SUM_RIGHT_REVERSE");
    }
    create_lircd_cfg(files);

    free((void*)MY_REMOTE.checksum_alg);
    list_destroy(&files);
    return EXIT_SUCCESS;
}