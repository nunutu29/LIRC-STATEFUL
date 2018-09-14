//
// Created by Johnny on 06/09/2018.
//

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "stateful_utils.h"
#include "lirc_log.h"

static const logchannel_t logchannel = LOG_LIB;

bool ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) return false;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix >  len_str) return false;
    return strncmp(str + len_str - len_suffix, suffix, len_suffix) == 0;
}

bool starts_with(const char *str, const char *prefix)
{
    return strncmp(prefix, str, strlen(prefix)) == 0;
}

bool equals_to(const char* str1, const char * str2, bool case_sensitive)
{
    if(case_sensitive)
        return strcmp(str1, str2) == 0;
    else {
        const unsigned char *us1 = (const unsigned char *)str1;
        const unsigned char *us2 = (const unsigned char *)str2;

        while(tolower(*us1) == tolower(*us2)){
            if(*us1 == '\0')
            {
                return true;
            }
            us1++;
            us2++;
        }
        return tolower(*us1) - tolower(*us2) == 0;
    }
}

char* trim(char* str)
{
    char* end;

    // Trim leading space
    while(isspace((unsigned char)*str)) str++;

    if(*str == 0)  // All spaces?
        return str;

    // Trim trailing space
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;

    // Write new null terminator character
    end[1] = '\0';

    return str;
}

int to_integer(const char *s, int def)
{
    if (s == NULL || *s == '\0' || isspace(*s))
    {
        return def;
    }
    char * p;
    long d = strtol (s, &p, 0);
    if (s != p && *p == '\0')
        return (int)d;
    return def;
}

int roundUpDown(int numToRound, int multiple)
{
    int result;

    if (!multiple) return numToRound;
    result = numToRound + multiple / 2;
    return result - (result % multiple);
}

bool is_numeric (const char * s)
{
    while (*s) if (isdigit(*s++) == 0) return false;
    return true;
}

int* get_info_from_command_name(const char* name){
    if(!name){
        return NULL;
    }
    int *info = (int*)malloc(sizeof(int) * 3);
    if(!info){
        log_error("Out of memory");
        return NULL;
    }
    char *str, *token;
    size_t first, second;

    str = strrchr(name, '[');
    first = str - name;
    str = strrchr(name, ']');
    second = str - name;

    char buffer[second - first];
    strncpy(buffer, &name[first + 1], second - first - 1);;
    buffer[second - first - 1] = '\0';

    token = strtok(buffer, "_");
    int i = 0;
    while(token && i < 3){
        info[i++] = to_integer(token, 0);
        token = strtok(NULL, "_");
    }
    return info;
}

char* long2bin(long x) {
    char* ret;
    int i = 0;
    if(x == 0){
        ret = (char*)malloc(sizeof(char)*2);
        ret[i++] = '0';
    } else {
        ret = (char*)malloc(sizeof(char));
        while(x != 0){
            ret[i++] = (char)(x % 2 ? '1' : '0');
            x /= 2;
            ret = (char*)realloc(ret, (i + 1) * sizeof(char));
        }
    }
    ret[i] = 0;
    reverse_str(ret);
    return ret;
}

long bin2long(char *x) {
    return strtol(x, NULL, 2);
}

void reverse_str(char *str) {
    char *p1, *p2;

    if (! str || ! *str) return;

    for (p1 = str, p2 = str + strlen(str) - 1; p2 > p1; ++p1, --p2)
    {
        *p1 ^= *p2;
        *p2 ^= *p1;
        *p1 ^= *p2;
    }
}
