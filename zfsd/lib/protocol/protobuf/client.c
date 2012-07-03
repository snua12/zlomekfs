#include <stdio.h>
#include <google/protobuf-c/protobuf-c-rpc.h>
#include "zfs.pb-c.h"

static void handle_query_response (const Zlomekfs__DataBuffer *result,
			void *closure_data)
{
	if (result == NULL)
	{
		printf ("Error processing request.\n");
	}

	printf("Response len=%u value=%s\n", result->len, result->buf.data);

	* (protobuf_c_boolean *) closure_data = 1;

}

int main()
{
	ProtobufCService *service;
	ProtobufC_RPC_Client *client;
	ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
	const char * port = "127.0.0.1:1275";

	service = protobuf_c_rpc_client_new (address_type, port, &zlomekfs__zlomekfs__descriptor, NULL);
	if (service == NULL)
	{
		return 1;
	}

	client = (ProtobufC_RPC_Client *) service;

	 fprintf (stderr, "Connecting... ");
	 while (!protobuf_c_rpc_client_is_connected (client))
		     protobuf_c_dispatch_run (protobuf_c_dispatch_default ());
	fprintf (stderr, "done.\n");

	Zlomekfs__DataBuffer query = ZLOMEKFS__DATA_BUFFER__INIT;
	static ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
	query.buf = bd_hello;
	query.len = 6;

	protobuf_c_boolean is_done = 0;
	zlomekfs__zlomekfs__ping(service, &query, handle_query_response, &is_done);
	while (!is_done)
		protobuf_c_dispatch_run (protobuf_c_dispatch_default ());

	return 0;


}
