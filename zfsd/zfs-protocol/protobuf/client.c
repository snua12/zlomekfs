/**
 *  \file client.c
 * 
 *  \author Ales Snuparek
 *  \brief Proof of concept implementation of zlomekFS protocol client
 *
 */

/* Copyright (C) 2003, 2004, 2012 Josef Zlomek, Ales Snuparek

   This file is part of ZFS.

   ZFS is free software; you can redistribute it and/or modify it under the
   terms of the GNU General Public License as published by the Free Software
   Foundation; either version 2, or (at your option) any later version.

   ZFS is distributed in the hope that it will be useful, but WITHOUT ANY
   WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
   FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
   details.

   You should have received a copy of the GNU General Public License along
   with ZFS; see the file COPYING.  If not, write to the Free Software
   Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA; or
   download it from http://www.gnu.org/licenses/gpl.html */

#include <stdio.h>
#include <google/protobuf-c/protobuf-c-rpc.h>
#include "zfs.pb-c.h"

static void handle_query_response(const Zlomekfs__PingRes * result,
								  void *closure_data)
{
	if (result == NULL)
	{
		printf("Error processing request.\n");
	}

	printf("Response len=%u value=%s\n", result->buffer->len,
		   result->buffer->buf.data);

	*(protobuf_c_boolean *) closure_data = 1;

}

int main()
{
	ProtobufCService *service;
	ProtobufC_RPC_Client *client;
	ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
	const char *port = "127.0.0.1:1275";

	service =
		protobuf_c_rpc_client_new(address_type, port,
								  &zlomekfs__zfsd__descriptor, NULL);
	if (service == NULL)
	{
		return 1;
	}

	client = (ProtobufC_RPC_Client *) service;

	fprintf(stderr, "Connecting... ");
	while (!protobuf_c_rpc_client_is_connected(client))
		protobuf_c_dispatch_run(protobuf_c_dispatch_default());
	fprintf(stderr, "done.\n");

	Zlomekfs__PingArgs query = ZLOMEKFS__PING_ARGS__INIT;
	static Zlomekfs__DataBuffer data_buffer = ZLOMEKFS__DATA_BUFFER__INIT;
	static ProtobufCBinaryData bd_hello = { 5, (uint8_t *) "hAllo" };
	data_buffer.len = 6;
	data_buffer.buf = bd_hello;
	query.buffer = &data_buffer;

	protobuf_c_boolean is_done = 0;
	zlomekfs__zfsd__ping(service, &query, handle_query_response, &is_done);
	while (!is_done)
		protobuf_c_dispatch_run(protobuf_c_dispatch_default());

	return 0;


}
