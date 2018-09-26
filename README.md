# LIRC-STATEFUL (AC Support)
### New Functions are:	
```
stateful_create [options] <directory>
irsend [options] stateful_send <remote> <code> [<code> ... ]
irsend [options] stateful_get <remote> ""
```

### Modified Files:
```
daemons/lircd.cpp

lib/Makefile.am
lib/list.c
lib/list.h
lib/stateful_checksum_alg.c
lib/stateful_checksum_alg.h
lib/stateful_constants.h
lib/stateful_send.c
lib/stateful_send.h
lib/stateful_utils.c
lib/stateful_utils.h
lib/ir_remote_types.h
lib/dump_config.c
lib/config_file.c


tools/Makefile.am
tools/irsend.cpp
tools/irsend.cpp
```

### At the moment, only NEC or similar protocols are supported.

[SEE LIRC MANUAL FOR FULL INFO](http://www.lirc.org/html/index.html)
