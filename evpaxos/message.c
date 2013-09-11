/*
	Copyright (C) 2013 University of Lugano

	This file is part of LibPaxos.

	LibPaxos is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Libpaxos is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with LibPaxos.  If not, see <http://www.gnu.org/licenses/>.
*/


#include "message.h"
#include "xdr.h"
#include <string.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/rpc.h>
#include <event2/bufferevent.h>


static int
send_message(struct bufferevent* bev, paxos_message* msg)
{
	XDR xdr;
	uint32_t size;
	char buffer[PAXOS_MAX_VALUE_SIZE];
	xdrmem_create(&xdr, buffer, PAXOS_MAX_VALUE_SIZE, XDR_ENCODE);
	if (!xdr_paxos_message(&xdr, msg)) {
		paxos_log_error("Error while encoding paxos message");
		xdr_destroy(&xdr);
		return 0;
	}
	size = htonl(xdr_getpos(&xdr));
	bufferevent_write(bev, &size, sizeof(uint32_t));
	bufferevent_write(bev, buffer, xdr_getpos(&xdr));
	xdr_destroy(&xdr);
	return 1;
}

void
send_paxos_prepare(struct bufferevent* bev, paxos_prepare* p)
{
	paxos_message msg = {
		.type = PAXOS_PREPARE,
		.paxos_message_u.prepare = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send prepare for iid %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_promise(struct bufferevent* bev, paxos_promise* p)
{
	paxos_message msg = {
		.type = PAXOS_PROMISE,
		.paxos_message_u.promise = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send promise for iid %d ballot %d", p->iid, p->ballot);
}

void 
send_paxos_accept(struct bufferevent* bev, paxos_accept* p)
{
	paxos_message msg = {
		.type = PAXOS_ACCEPT,
		.paxos_message_u.accept = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send accept for iid %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_accepted(struct bufferevent* bev, paxos_accepted* p)
{	
	paxos_message msg = {
		.type = PAXOS_ACCEPTED,
		.paxos_message_u.accepted = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send accepted for inst %d ballot %d", p->iid, p->ballot);
}

void
send_paxos_repeat(struct bufferevent* bev, paxos_repeat* p)
{
	paxos_message msg = {
		.type = PAXOS_REPEAT,
		.paxos_message_u.repeat = *p };
	send_message(bev, &msg);
	paxos_log_debug("Send repeat for inst %d-%d", p->from, p->to);
}

void
paxos_submit(struct bufferevent* bev, char* data, int size)
{
	paxos_message msg = {
		.type = PAXOS_CLIENT_VALUE,
		.paxos_message_u.client_value.value.value_len = size,
		.paxos_message_u.client_value.value.value_val = data };
	send_message(bev, &msg);
}

static int
decode_paxos_message(struct evbuffer* in, paxos_message* out, size_t size)
{
	XDR xdr;
	char* buffer = (char*)evbuffer_pullup(in, size);
	xdrmem_create(&xdr, buffer, size, XDR_DECODE);
	memset(out, 0, sizeof(paxos_message));
	int rv = xdr_paxos_message(&xdr, out);
	if (!rv) paxos_log_error("Error while decoding paxos message!");
	xdr_destroy(&xdr);
	return rv;
}

int
recv_paxos_message(struct evbuffer* in, paxos_message* out)
{
	uint32_t msg_size;
	size_t buffer_len = evbuffer_get_length(in);
	
	if (buffer_len <= sizeof(uint32_t))
		return 0;
	
	evbuffer_copyout(in, &msg_size, sizeof(uint32_t));
	msg_size = ntohl(msg_size);
	
	if (buffer_len < (msg_size + sizeof(uint32_t)))
		return 0;
	
	if (msg_size > PAXOS_MAX_VALUE_SIZE) {
		evbuffer_drain(in, msg_size);
		paxos_log_error("Discarding message of size %ld. Maximum is %d",
			msg_size, PAXOS_MAX_VALUE_SIZE);
		return 0;
	}
	
	evbuffer_drain(in, sizeof(uint32_t));
	int rv = decode_paxos_message(in, out, msg_size);
	evbuffer_drain(in, msg_size);

	return rv;
}