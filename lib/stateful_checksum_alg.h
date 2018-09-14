//
// Created by Johnny on 06/09/2018.
//

#ifndef LIRC_STATEFUL_CHECKSUM_ALG_H
#define LIRC_STATEFUL_CHECKSUM_ALG_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocates a new memory for the new bit array and returns it
 * @param checksum_alg
 * @param bit_array
 * @return
 */
char* checksum(const char* checksum_alg, const char* bit_array);

#ifdef __cplusplus
}
#endif

#endif //LIRC_STATEFUL_CHECKSUM_ALG_H
