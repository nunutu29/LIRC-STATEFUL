//
// Created by Johnny on 06/09/2018.
//

#ifndef LIRC_STATEFUL_SEND_H
#define LIRC_STATEFUL_SEND_H

#include "ir_remote_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
*
* @param remote
* @param keys
* @return ir_ncode* if everything is fine, NULL otherwise
* Use free_ir_ncode() to free this. (ir_remote.h)
*
*  The config file contains multiple sates, and each of one has a certain name.
*  commands must be a string containing one state per button splited by "splitter"
*  i.e.
*  MODE_1 ... ... ...
*  MODE_2 ... ... ...
*  MODE_3 ... ... ...
*  commands should contain only one MODE state
*  Example of commands: "POWER_1 MODE_2 SWING_1 FAN_3"
*
*  If a state is missing the default one will be used.
*  The states doesn't have to be in order. The order is relevant only in
*  the config file
*/
struct ir_ncode* get_stateful_ir_ncode(struct ir_remote* remote,
                                       char* commands,
                                       const char* splitter);

/**
 * dispose ncode created by get_stateful_ir_ncode(...)
 * @param ncode
 */
void free_stateful_ir_ncode(struct ir_ncode* ncode);

/**
 * Get last command combination used to create the state of the remote
 * All commands are divided by , (comma)
 * @param remote
 * @return
 */
char* get_stateful_last_name(struct ir_remote* remote);

#ifdef __cplusplus
}
#endif

#endif //LIRC_STATEFUL_SEND_H
