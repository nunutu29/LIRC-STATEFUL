//
// Created by Johnny on 06/09/2018.
//

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "stateful_checksum_alg.h"
#include <stdbool.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lirc_log.h"
#include "stateful_utils.h"
#include <stddef.h>

static const logchannel_t logchannel = LOG_LIB;

void* log_malloc(size_t size);
char* log_strdup(const char* str);

void* ca_sum(void *arg);
void* ca_right(void *arg);
void* ca_left(void *arg);
void* ca_xor(void *arg);
void* ca_count1(void *arg);
void* ca_count0(void *arg);
void* ca_flip(void *arg);
void* ca_reverse(void *arg);

struct ca_aux_algorithm{
    const char* name;
    void* (*algorithm)(void* arg);
};

static const char *algorithms[] = {
        "SUM_RIGHT",
        "SUM_LEFT",
        "XOR",
        "COUNT1_RIGHT",
        "COUNT1_LEFT",
        "COUNT0_RIGHT",
        "COUNT0_LEFT",
        "FLIP_SUM_RIGHT_FLIP",
        "FLIP_SUM_LEFT_FLIP",
        "REVERSE_SUM_RIGHT_REVERSE",
        "REVERSE_SUM_LEFT_REVERSE",
        "FLIP_REVERSE_SUM_RIGHT_FLIP_REVERSE",
        "FLIP_REVERSE_SUM_LEFT_FLIP_REVERSE",
        "FLIP_REVERSE_XOR_FLIP_REVERSE",
        NULL
};

static const struct ca_aux_algorithm inner_algorithms[] = {
        {"SUM",      ca_sum},
        {"RIGHT",    ca_right},
        {"LEFT",     ca_left},
        {"XOR",      ca_xor},
        {"COUNT1",   ca_count1},
        {"COUNT0",   ca_count0},
        {"FLIP",     ca_flip},
        {"REVERSE",  ca_reverse},
        {NULL, NULL}
};

char* checksum(const char* checksum_alg, const char* bit_array){
    int i;
    //check if given algorithm name exists
    i = 0;
    bool exists = false;
    while (algorithms[i] && !exists) exists = strcmp(algorithms[i++], checksum_alg) == 0;

    if(!exists){
        log_error("The algorithm %s doesn't exists.", checksum_alg);
        return NULL;
    }

    i = (int)strlen(bit_array);
    if(i % 8){
        log_error("Invalid array. Bits = %i", i);
        return NULL;
    }

    char* alg_name = log_strdup(checksum_alg);
    if(!alg_name) return NULL;

	char* p;
    char* token = strtok_r(alg_name, "_", &p);
    void* arg[2];
    int arg_i = 0;
    arg[arg_i] = log_strdup(bit_array);
    if(!arg[arg_i]) return NULL;


    while(token){
        i = 0;
        while(strcmp(inner_algorithms[i].name, token) != 0) i++;

        if(!inner_algorithms[i].name){
            log_error("Invalid inner algorithm: %s", token);
            if(arg[arg_i]) free(arg[arg_i]);
            free(alg_name);
            return NULL;
        }
        arg[1 - arg_i] = inner_algorithms[i].algorithm(arg[arg_i]);
        if(arg[arg_i]) free(arg[arg_i]);
        arg_i = 1 - arg_i;
        token = strtok_r(NULL, "_", &p);
    }

    char *buffer = NULL;
    if(arg[arg_i]){
        size_t a = sizeof(char) * strlen(bit_array);
        size_t b = sizeof(char) * 8;
        buffer = (char*)log_malloc(a + b + sizeof(char));
        if(buffer){
            strncpy(buffer, bit_array, a);
            strncpy(buffer + a, (char*)arg[arg_i], b);
            memset(buffer + a + b, 0, 1);
        }
        free(arg[arg_i]);
    }
    free(alg_name);
    return buffer;
}

int ca_count(char* bitArr, char x){
    int len = (int)strlen(bitArr);
    int i;
    int ret = 0;
    for(i = 0; i < len; i++){
        ret += (int)(bitArr[i] == x);
    }
    return ret;
}

/**
 * Allocates memory and logs if doesn't succeed
 * @param size
 * @return
 */
void *log_malloc(size_t size){
    void* x = malloc(size);
    if(!x) log_error("Out of memory");
    return x;
}

char *log_strdup(const char* str){
    char* x = strdup(str);
    if(!x) log_error("Out of memory");
    return x;
}


/**
 * Must free
 * @param arg binary ir code
 * @return binary number of the sum
 */
void* ca_sum(void *arg){
    if(!arg)
        return NULL;

    char *bitArr = (char*)arg;
    int bytes = (int)strlen(bitArr) / 8;
    long dec = 0;
    int i;
    char* buffer = (char*) log_malloc(sizeof(char) * 9);

    if(!buffer)
        return NULL;

    buffer[8] = 0;
    for(i = 0; i < bytes; i++){
        strncpy(buffer, bitArr + i*8, sizeof(char)*8);
        dec += bin2long(buffer);
    }
    free(buffer);
    return long2bin(dec);
}
/**
 * Must free
 * @param arg binary number
 * @return last 8 chars of the binary number
 * if there are no 8 chars, the string is filled with 0
 */
void* ca_right(void *arg){
    if(!arg)
        return NULL;

    char* bitArr = (char*)arg;
    char* buffer = (char*) log_malloc(sizeof(char) * 9);

    if(!buffer)
        return NULL;

    buffer[8] = 0;
    size_t len = (int)strlen(bitArr);

    if(len <= 8) {
        memset(buffer, '0', 8 - len);
        strcpy(buffer + 8 - len, bitArr);
    } else{
        strncpy(buffer, bitArr + len - 8, sizeof(char) * 8);
    }
    return buffer;
}
/**
 * Must free
 * @param arg binary number
 * @return first 8 chars of the binary number
 * if there are no 8 chars, the string is filled with 0
 */
void* ca_left(void *arg){
    if(!arg)
        return NULL;

    char* bitArr = (char*)arg;
    char* buffer = (char*) log_malloc(sizeof(char) * 9);

    if(!buffer)
        return NULL;

    buffer[8] = 0;
    size_t len = (int)strlen(bitArr);

    if(len <= 8) {
        memset(buffer, '0', 8 - len);
        strcpy(buffer + 8 - len, bitArr);
    } else{
        strncpy(buffer, bitArr, sizeof(char) * 8);
    }
    return buffer;
}
/**
 *  Must free
 * @param arg
 * @return 1 byte obtained by doing xor with all bytes
 */
void* ca_xor(void *arg){
    if(!arg)
        return NULL;

    char* bitArr = (char*)arg;
    int bytes = (int)strlen(bitArr) / 8;
    int i, j;
    char *buffer = (char*)malloc(sizeof(char) * 9);

    if(!buffer)
        return NULL;

    int arr[8];
    buffer[8] = 0;

    //get first byte
    for(j = 0; j < 8; j++)
        arr[j] = (int)bitArr[j] - '0';

    //xor with next bytes
    for(j = 0; j < 8; j++)
        for(i = 1; i < bytes; i++)
            arr[j] ^= ((int)bitArr[i * 8 + j] - '0');

    //convert to char
    for(i = 0; i < 8; i++)
        buffer[i] = (char)(arr[i] ? '1' : '0');

    return buffer;
}
/**
 * Must free
 * @param arg
 * @return binary representation of count 1
 */
void* ca_count1(void *arg){
    if(!arg)
        return NULL;

    return long2bin(ca_count((char*)arg, '1'));
}
/**
 * Must free
 * @param arg
 * @return binary representation of count 0
 */
void* ca_count0(void *arg){
    if(!arg)
        return NULL;

    return long2bin(ca_count((char*)arg, '0'));
}
/**
 * Must free
 * @param arg
 * @return arg with all bytes flipped
 */
void* ca_flip(void *arg){
    if(!arg)
        return NULL;

    char* bitArr = (char*)arg;
    int len = (int)strlen(bitArr);
    char *buffer = (char*) log_malloc(sizeof(char) * (len + 1));

    if(!buffer)
        return NULL;

    int i;
    for(i = 0; i < len; i++){
        buffer[i] = (char)(1 - ((int)bitArr[i] - '0') + '0');
    }
    buffer[len] = 0;
    return buffer;
}
/**
 * Must free
 * @param arg the code to reverse
 * @return arg with all bytes reversed
 */
void* ca_reverse(void *arg){
    if(!arg)
        return NULL;

    char *bitArr = (char*)arg;
    int j = (int)strlen(bitArr);
    char* buffer = (char*)malloc(sizeof(char) * (j + 1));

    if(!buffer)
        return NULL;

    char byte[9];
    int byte_index;
    int byte_counter = j / 8;
    size_t byte_size = sizeof(char) * 8;

    byte[8] = 0;
    for(byte_index = 0; byte_index < byte_counter; byte_index++){
        strncpy(byte, bitArr + byte_index * 8, byte_size);
        reverse_str(byte);
        strncpy(buffer + byte_index * 8, byte, byte_size);
    }
    buffer[j] = 0;
    return buffer;
}
