//
// Created by Johnny on 06/09/2018.
//

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef HAVE_KERNEL_LIRC_H
#include <linux/lirc.h>
#else
#include "media/lirc.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include "lirc_log.h"
#include "ir_remote_types.h"
#include "dictionary.h"
#include "stateful_send.h"
#include "stateful_utils.h"
#include "stateful_constants.h"
#include "stateful_checksum_alg.h"
#include "ir_remote.h"

static const logchannel_t logchannel = LOG_LIB;

/**
 * Adds a specific binary value to the byte
 * @param bin
 * @param byte
 * @param from
 * @param to
 */
void ss_add_to_byte(const char* bin, char *byte, int from, int to){
    if(from > to) return;

    int bin_len = (int)strlen(bin);
    int zero_to_add = to - from + 1 - bin_len;
    int x = from - 1 + zero_to_add; // initial byte position
    int y = to - 1;

    // check if all bits are unused, otherwise start from the last not used
    // this is to not overlap existing bits
    while(!byte[y] && y > x) y--;
    if(byte[y]) y++;

    // skip first y - x bits from bin
    int z = y - x;
    while (y < to && z < bin_len && y < BYTE_LEN) {
        byte[y++] = bin[z++];
    }
}
/**
 * Adds a byte to a specific array
 * @param arr
 * @param Byte
 * @param arr_position
 */
void ss_add_byte_to_arr(char* arr, char* Byte, int arr_position){
    int i;
    for(i = 0; i < 8; i++){
        if(!Byte[i]){
            Byte[i] = '0';
        }
    }
    strcpy(arr + arr_position, Byte);
    memset(Byte, 0, sizeof(char) * (BYTE_LEN + 1));
}
/**
 * checks if a != b
 * @param a
 * @param b
 * @return
 */
int ss_different_infos(const int *a, const int *b){
    if(!a && !b) return 0;
    if(!a || !b) return 1;
    return a[0] != b[0] || a[1] != b[1] || a[2] != b[2];
}
/**
 * @param str1
 * @param str2
 * @return 1 if everything ok, 0 otherwise
 *
 * str2 will contain the expected string
 * i.e. MODE_1 -> MODE
 */
int ss_get_group_name(const char *str1, char *str2){
    char* last_occurrence;
    size_t last_index;

    last_occurrence = strrchr(str1, SPLITTER_CHAR);

    if(!last_occurrence)
    {
        log_error("Command %s name not valid. It should contain %s", str1, (char*)SPLITTER_CHAR);
        return 0;
    }
    else
        last_index = (size_t) (last_occurrence - str1);

    if(last_index > MAX_STR)
    {
        //if this happens strncpy will go in error
        log_error("Input to big: %s. maximum length should be: %d", str1, MAX_STR);
        return 0;
    }

    strncpy(str2, str1, last_index);
    //null terminate str2
    memset(str2 + last_index, 0, 1);
    return 1;
}
/**
 * @param arr
 * @param size
 * @return 1 if there are no duplicates, 0 otherwise
 *
 * This function checks if arr contains duplicated group_name
 */
int ss_check_dup(char **arr, int size){
    char group_name[MAX_STR + 1];
    dictionary* groups;
    int i;

    if(size == 0)
    {
        //if the array is empty, there are no duplicates for sure
        return 1;
    }

    //use dictionary to increase speed
    groups = dictionary_new(size);
    memset(group_name, 0, MAX_STR + 1);

    for(i = 0; i < size; i++){
        if(!ss_get_group_name(arr[i], group_name)){
            dictionary_del(groups);
            return 0;
        }
        if(dictionary_get(groups, group_name, NULL) == NULL){
            dictionary_set(groups, group_name, "1");
        } else {
            dictionary_del(groups);
            return 0;
        }
    }
    dictionary_del(groups);
    return 1;
}
/**
 * Saves bits and signals into ncode + other NULL fields
 * @param ncode
 * @param bits
 * @param signals
 * @return
 */
void ss_create_ir_ncode(struct ir_remote *remote,
                        struct ir_ncode **ir_ncode,
                        char *name,
                        char *bitArr) {
    int i;
    int *signals;
    char* final_binary_array;

    struct ir_ncode *tmp = (struct ir_ncode*)malloc(sizeof(struct ir_ncode));
    if(!tmp){
        log_error("Out of memory");
        return;
    }

    tmp->name = name ? strdup(name) : strdup("undefined");
    tmp->length = (remote->bits * 2) + 3;;
    tmp->code = 0;
    tmp->next = NULL;
    final_binary_array = checksum(remote->checksum_alg, bitArr);

    if(final_binary_array){

        remote->flags |= RAW_CODES;

        signals = (int*)malloc(sizeof(int) * tmp->length);
        tmp->signals = signals;

        *signals++ = remote->phead;
        *signals++ = remote->shead;

        for(i = 0; i < remote->bits; i++){
            if((int)final_binary_array[i] - '0'){
                *signals++ = remote->pone;
                *signals++ = remote->sone;
            } else {
                *signals++ = remote->pzero;
                *signals++ = remote->szero;
            }
        }

        *signals = remote->ptrail;

        free(final_binary_array);
        (*ir_ncode) = tmp;
    } else{
        free(tmp);
    }
}

/**
 * Insert into destination the content in source + "," (comma)
 * @param destination
 * @param source
 * @return
 */
void ss_concat(char **dest, const char *src){
    char* tmp;
    size_t source_len = strlen(src);
    size_t destination_len;

    if(*dest == NULL){
        *dest = (char*)calloc(source_len + 1, sizeof(char));
        memcpy(*dest, src, source_len);
        return;
    }

    destination_len = strlen(*dest);

    tmp = (char*)calloc(destination_len + source_len + 2, sizeof(char));
    memcpy(tmp, *dest, destination_len);
    memcpy(tmp + destination_len, ",", 1);
    memcpy(tmp + destination_len + 1, src, source_len);
    free(*dest);
    *dest = tmp;
}

/**
 * Search into last_sent_code current group
 * @param remote
 * @param group_to_search
 * @return
 */
struct ir_ncode* ss_search_last_used_comand(struct ir_remote *remote, int byte, int from, int to){
    if(remote->last_sent_code == NULL){
        return NULL;
    }
    char* name;
    struct ir_ncode* return_ncode = NULL;
    int *info = NULL;
    char* token, *p = NULL;

    name = strdup(remote->last_sent_code->name);
    log_debug("Searching last [%i_%i_%i]", byte, from, to);

    token = strtok_r(name, SPLITTER_CHAR_2, &p);

    while(token != NULL) {
        info = get_info_from_command_name(token);

        if(info == NULL) {
            log_debug("Unabled to get info");
            continue;
        }
        if(info[0] == byte && info[1] == from && info[2] == to){
            return_ncode = get_code_by_name(remote, token);
            if(!return_ncode)
            {
                log_error("get_code_by_name failed");
            }
            break;
        }
        free(info);
        token = strtok_r(NULL, SPLITTER_CHAR_2, &p);
    }
    if(info) free(info);
    free(name);
    return return_ncode;
}

struct ir_ncode* ss_get_dynamics(struct ir_remote *remote, char **keys, int keys_size){
    if(!ss_check_dup(keys, keys_size))
    {
        //check that we have no repeated keys
        log_error("Duplicated keys found. Specify only one status per button");
        return NULL;
    }

    dictionary* keys_d;
    int i;
    keys_d = dictionary_new(keys_size);
    //Add keys to a dictionary for a faster search
    for(i = 0; i < keys_size; i++) dictionary_set(keys_d, keys[i], "1");

    char binary[remote->bits + 1];
    int *prev_info = NULL;
    int *curr_info = NULL;
    const struct ir_ncode* code = remote->codes;
    const struct ir_ncode* tmp;
    const struct ir_ncode* default_ncode = remote->codes;
    int index = 0;
    char *name = NULL;
    char Byte[BYTE_LEN + 1];
    int found = 0;

    binary[remote->bits] = 0;
    memset(Byte, 0, sizeof(char) * (BYTE_LEN + 1));

    while(code->name){
        curr_info = get_info_from_command_name(code->name);

        if(!curr_info){
            //check for error
            if(name)
                free(name);
            if(prev_info)
                free(prev_info);
            return NULL;
        }

        // check if previous info equals current
        // this doesn't mean they are both not NULL
        if(ss_different_infos(prev_info, curr_info)){
            if(prev_info) {
                //if previous info EXISTS
                if(!found){
                    //and if previous info HAS NOT BEEN ASKED BY THE USER
                    // => take the last one used
                    tmp = ss_search_last_used_comand(remote, prev_info[0], prev_info[1], prev_info[2]);

                    if(!tmp)
                    {
                        //if this is the first time, then take the default one
                        tmp = default_ncode;
                    }

                    char* bin = long2bin(tmp->code);
                    ss_add_to_byte(bin, Byte, prev_info[1], prev_info[2]);
                    ss_concat(&name, tmp->name);
                }

                if (prev_info[0] != curr_info[0]){
                    ss_add_byte_to_arr(binary, Byte, index);
                    index += 8;
                }
                free(prev_info);
            }
            prev_info = curr_info;
            default_ncode = code;
            found = 0; //obviously the current info hasn't been found
        }

        // we are considering the same state of the button
        // skip this state if the another one has already been found
        // otherwise, check if this state of the button has been asked
        // by the user
        if(!found && dictionary_get(keys_d, code->name, NULL) != NULL){
            found = 1;
            char* bin = long2bin(code->code);
            ss_add_to_byte(bin, Byte, curr_info[1], curr_info[2]);
            ss_concat(&name, code->name);
        }
        if(prev_info != curr_info)
        {
            //we don't need this info anymore
            free(curr_info);
        }
        code++;
    }

    //this if should always be true
    if(prev_info){
        if(!found){
            // we have just finished the loop, but it looks like our
            // last button didn't get a state.
            tmp = ss_search_last_used_comand(remote, prev_info[0], prev_info[1], prev_info[2]);
            if(!tmp)
                tmp = default_ncode;
            char* bin = long2bin(tmp->code);
            ss_add_to_byte(bin, Byte, prev_info[1], prev_info[2]);
            ss_concat(&name, tmp->name);
        }
        free(prev_info);
    }
    ss_add_byte_to_arr(binary, Byte, index);

    struct ir_ncode *ret_code = NULL;
    ss_create_ir_ncode(remote, &ret_code, name, binary);

    if(name)
        free(name);

    return ret_code;
}

/**
 * Gets default states
 * @param remote
 * @return ir_ncode*, or NULL on error
 */
struct ir_ncode* ss_get_defaults(struct ir_remote *remote){
    char binary[remote->bits + 1];
    int *prev_info = NULL;
    int *curr_info;
    const struct ir_ncode* code = remote->codes;
    const struct ir_ncode* tmp;
    int index = 0;
    char *name = NULL;
    char Byte[BYTE_LEN + 1];

    binary[remote->bits] = 0;
    memset(Byte, 0, sizeof(char) * (BYTE_LEN + 1));

    while(code->name){
        curr_info = get_info_from_command_name(code->name);

        if(!curr_info){
            //check for error
            if(name)
                free(name);
            if(prev_info)
                free(prev_info);
            return NULL;
        }

        if (prev_info && prev_info[0] != curr_info[0]){
            ss_add_byte_to_arr(binary, Byte, index);
            index += 8;
        }
        if(ss_different_infos(prev_info, curr_info)){
            tmp = ss_search_last_used_comand(remote, curr_info[0], curr_info[1], curr_info[2]);
            if(!tmp) tmp = code;
            char* bin = long2bin(tmp->code);
            ss_add_to_byte(bin, Byte, curr_info[1], curr_info[2]);
            ss_concat(&name, tmp->name);

            if(prev_info) free(prev_info);

            prev_info = curr_info;
        }

        if(prev_info != curr_info){
            free(curr_info);
        }

        code++;
    }

    if(prev_info) free(prev_info);

    ss_add_byte_to_arr(binary, Byte, index);

    struct ir_ncode *ret_code = NULL;
    ss_create_ir_ncode(remote, &ret_code, name, binary);
    return ret_code;
}

struct ir_ncode* get_stateful_ir_ncode(struct ir_remote* remote,
                                       char* commands,
                                       const char* splitter){

    if (!remote->bits || remote->bits % 8)
    {
        log_error("Config file is not valid. Invalid bits number");
        return NULL;
    }
    if(!remote->codes)
    {
        log_error("No codes found. Config file is not valid");
        return NULL;
    }

    char* token, *p;
    int count, i;
    struct ir_ncode* ncode;
    char buf[strlen(commands) + 1];
    strcpy(buf, commands);


    token = strtok_r(buf, splitter, &p);

    if(token == NULL)
    {
        log_debug("Using default code");
        ncode = ss_get_defaults(remote);
    }
    else
    {
        count = 0;
        while(token != NULL)
        {
            count++;
            token = strtok_r(NULL, splitter, &p);
        }

        char* keys[count];
        i = 0;
        strcpy(buf, commands);
        token = strtok_r(buf, splitter, &p);
        while(token != NULL){
            keys[i] = token;
            token = strtok_r(NULL, splitter, &p);
            i++;
        }
        log_debug("Using parametric commands");
        ncode = ss_get_dynamics(remote, keys, count);
    }
    log_debug("Created: %s", ncode->name);
    return ncode;
}

void free_stateful_ir_ncode(struct ir_ncode* ncode){
    if (ncode == NULL)
    {
        return;
    }
    if (ncode->signals != NULL)
    {
        free(ncode->signals);
        ncode->signals= NULL;
    }
    if (ncode->name != NULL)
    {
        free(ncode->name);
        ncode->name= NULL;
    }
    free(ncode);
}

char* get_stateful_last_name(struct ir_remote* remote){
    if(remote->last_sent_code == NULL)
        return NULL;
    return remote->last_sent_code->name;
}