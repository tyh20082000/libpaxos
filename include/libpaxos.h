#ifndef _LIBPAXOS_H_
#define _LIBPAXOS_H_

#include "paxos_config.h"
#include <sys/types.h>
#include <stdint.h>
#include <event2/event.h>

/* 
    The maximum size that can be submitted by a client.
    Set MAX_UDP_MSG_SIZE in config file to reflect your network MTU.
    Max packet size minus largest header possible
    (should be accept_ack_batch+accept_ack, around 30 bytes)
*/
#define PAXOS_MAX_VALUE_SIZE (MAX_UDP_MSG_SIZE - 40)

/* 
    Alias for instance identificator and ballot number.
*/
typedef unsigned int ballot_t;
typedef uint32_t iid_t;

/* 
	When starting a learner you must pass a function to be invoked whenever
	a value is delivered.
	This defines the type of such function.
	Example: 
	void my_deliver_fun(char * value, size_t size, iid_t iid, ballot_t ballot, int proposer) {
		...
	}
*/
typedef void (* deliver_function)(char*, size_t, iid_t, ballot_t, int, void*);


/*
    Starts a learner and returns when the initialization is complete.
    Return value is 0 if successful
    f -> A deliver_function invoked when a value is delivered.
         This argument cannot be NULL
         It's called by an internal thread therefore:
         i)  it must be quick
         ii) you must synchronize/lock externally if this function touches data
             shared with some other thread (i.e. the one that calls learner init)
    cif -> A custom_init_function invoked by the internal libevent thread, 
           invoked when the normal learner initialization is completed
           Can be used to add other events to the existing event loop.
           It's ok to pass NULL if you don't need it.
           cif has to return -1 for error and 0 for success
*/
struct learner* learner_init(const char* config_file, deliver_function f,
	void* arg, struct event_base* base);

/*
	Starts an acceptor and returns when the initialization is complete.
	Return value is 0 if successful
	acceptor_id -> Must be in the range [0...(N_OF_ACCEPTORS-1)]
*/
struct acceptor*
acceptor_init(int id, const char* config, struct event_base* b);

/*
	Starts an acceptor that instead of creating a clean DB,
	tries to recover from an existing one.
	Return value is 0 if successful
*/
struct acceptor*
acceptor_init_recover(int id, const char* config, struct event_base* b);

/*
	Shuts down the acceptor in the current process.
	It may take a few seconds to complete since the DB needs to be closed.
*/
//FIXME should delegate close to libevent thread
int
acceptor_exit(struct acceptor* a);

/*
	Starts a proposer with the given ID (which MUST be unique).
	Return value is 0 if successful
	proposer_id -> Must be in the range [0...(MAX_N_OF_PROPOSERS-1)]
*/
struct proposer*
proposer_init(int id, const char* config, struct event_base* b);


#endif /* _LIBPAXOS_H_ */