#include <stdio.h>
#include <google/protobuf-c/protobuf-c-rpc.h>
#include "zfs.pb-c.h"

static void zlomekfs__ping(Zlomekfs__Zlomekfs_Service *service,
				const Zlomekfs__DataBuffer *input,
				Zlomekfs__DataBuffer_Closure closure,
				void *closure_data)
{
	(void) service;
	if (input == NULL)
	{
		closure (NULL, closure_data);
	}
	else
	{
		Zlomekfs__DataBuffer result = ZLOMEKFS__DATA_BUFFER__INIT;
		static ProtobufCBinaryData bd_hello = { 5, (uint8_t*)"hello" };
		result.buf = bd_hello;
		result.len = 6;
		closure (&result, closure_data);
	}
}

static Zlomekfs__Zlomekfs_Service the_zlomekfs_service = ZLOMEKFS__ZLOMEKFS__INIT(zlomekfs__);

int main()
{
	ProtobufC_RPC_Server *server;
	ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
	const char *port = "1275";

	server = protobuf_c_rpc_server_new (address_type, port, (ProtobufCService *) &the_zlomekfs_service, NULL);
	for (;;)
		protobuf_c_dispatch_run (protobuf_c_dispatch_default ());

	return 0;
}
