//
// Created by Johnny on 06/09/2018.
//

#ifndef LIRC_STATEFUL_UTILS_H
#define LIRC_STATEFUL_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <limits.h>

/**
 * Checks whatever "str" ends with "suffix"
 * @param str : Pointer to the main string
 * @param suffix : Pointer to the suffix
 * @return true if str ends with suffix, false otherwise
 */
bool ends_with(const char *str, const char *suffix);
/**
 * Checks whatever "str" starts with "prefix"
 * @param str : Pointer to the main string
 * @param suffix : Pointer to the prefix
 * @return true if str starts with prefix, false otherwise
 */
bool starts_with(const char *str, const char *prefix);
/**
 * Checks if str1 equals str2
 * @param str1
 * @param str2
 * @param case_sensitive : indicates if the comparing must be case sensitive
 * @return true if str1 equals str2, false otherwise
 */
bool equals_to(const char* str1, const char * str2, bool case_sensitive);
/**
 * Trims the string pointer by str. str will point to trimmed string
 * No free needed
 * @param str
 * @return
 */
char* trim(char* str);
/**
 * Converts the string pointed by s to integer
 * @param s
 * @param def: in case s is not a valid integer, def will be returned
 * @return
 */
int to_integer(const char *s, int def);
/**
 * rounds the number to ne nearest number that is a multiple of "multiple"
 * @param numToRound
 * @param multiple
 * @return
 */
int roundUpDown(int numToRound, int multiple);
/**
 * checks if the string pointed by s is numeric or not
 * @param s
 * @return
 */
bool is_numeric (const char * s);

/**
 * Creates an array[3] from a stateful ir_ncode->name
 * 0 -> byte
 * 1 -> from bit
 * 2 -> to bit
 * Must free
 * @param name
 * @return
 */
int* get_info_from_command_name(const char* name);

/**
 * Creates a binary array representing x
 * Must free
 * @param x
 * @return
 */
char* long2bin(long x);

/**
 * Converts a binary array to long
 * @param x
 * @return
 */
long bin2long(char *x);
/**
 * Reverse a string
 * @param str
 */
void reverse_str(char *str);

#ifdef __cplusplus
}
#endif

#endif //LIRC_STATEFUL_UTILS_H
