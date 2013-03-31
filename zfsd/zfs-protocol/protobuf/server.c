/**
 *  \file server.c
 * 
 *  \author Ales Snuparek
 *  \brief Proof of concept implementation of zlomekFS protocol server
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

#define ZFS_OK 0

static void zlomekfs__ping(Zlomekfs__Zfsd_Service * service,
						   const Zlomekfs__PingArgs * input,
						   Zlomekfs__PingRes_Closure closure,
						   void *closure_data)
{
	(void)service;

	if (input == NULL)
	{
		closure(NULL, closure_data);
		return;
	}


	static Zlomekfs__DataBuffer data_buffer = ZLOMEKFS__DATA_BUFFER__INIT;
	static ProtobufCBinaryData bd_hello = { 5, (uint8_t *) "hello" };
	data_buffer.len = 6;
	data_buffer.buf = bd_hello;

	Zlomekfs__ZfsError zfs_error = ZLOMEKFS__ZFS_ERROR__INIT;
	zfs_error.error = ZFS_OK;

	Zlomekfs__PingRes result = ZLOMEKFS__PING_RES__INIT;
	result.result = &zfs_error;
	result.buffer = input->buffer;

	closure(&result, closure_data);
}

static void zlomekfs__root(Zlomekfs__Zfsd_Service * service,
						   const Zlomekfs__RootArgs * input,
						   Zlomekfs__RootRes_Closure closure,
						   void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}

static void zlomekfs__volume_root(Zlomekfs__Zfsd_Service * service,
								  const Zlomekfs__VolumeRootArgs * input,
								  Zlomekfs__VolumeRootRes_Closure closure,
								  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}

static void zlomekfs__getattr(Zlomekfs__Zfsd_Service * service,
							  const Zlomekfs__GetattrArgs * input,
							  Zlomekfs__GetattrRes_Closure closure,
							  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__setattr(Zlomekfs__Zfsd_Service * service,
							  const Zlomekfs__SetattrArgs * input,
							  Zlomekfs__SetattrRes_Closure closure,
							  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__lookup(Zlomekfs__Zfsd_Service * service,
							 const Zlomekfs__LookupArgs * input,
							 Zlomekfs__LookupRes_Closure closure,
							 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__create(Zlomekfs__Zfsd_Service * service,
							 const Zlomekfs__CreateArgs * input,
							 Zlomekfs__CreateRes_Closure closure,
							 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__open(Zlomekfs__Zfsd_Service * service,
						   const Zlomekfs__OpenArgs * input,
						   Zlomekfs__OpenRes_Closure closure,
						   void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__close(Zlomekfs__Zfsd_Service * service,
							const Zlomekfs__CloseArgs * input,
							Zlomekfs__CloseRes_Closure closure,
							void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__readdir(Zlomekfs__Zfsd_Service * service,
							  const Zlomekfs__ReaddirArgs * input,
							  Zlomekfs__ReaddirRes_Closure closure,
							  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__mkdir(Zlomekfs__Zfsd_Service * service,
							const Zlomekfs__MkdirArgs * input,
							Zlomekfs__MkdirRes_Closure closure,
							void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__rmdir(Zlomekfs__Zfsd_Service * service,
							const Zlomekfs__RmdirArgs * input,
							Zlomekfs__RmdirRes_Closure closure,
							void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__rename(Zlomekfs__Zfsd_Service * service,
							 const Zlomekfs__RenameArgs * input,
							 Zlomekfs__RenameRes_Closure closure,
							 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__link(Zlomekfs__Zfsd_Service * service,
						   const Zlomekfs__LinkArgs * input,
						   Zlomekfs__LinkRes_Closure closure,
						   void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__unlink(Zlomekfs__Zfsd_Service * service,
							 const Zlomekfs__UnlinkArgs * input,
							 Zlomekfs__UnlinkRes_Closure closure,
							 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__read(Zlomekfs__Zfsd_Service * service,
						   const Zlomekfs__ReadArgs * input,
						   Zlomekfs__ReadRes_Closure closure,
						   void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__write(Zlomekfs__Zfsd_Service * service,
							const Zlomekfs__WriteArgs * input,
							Zlomekfs__WriteRes_Closure closure,
							void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__readlink(Zlomekfs__Zfsd_Service * service,
							   const Zlomekfs__ReadlinkArgs * input,
							   Zlomekfs__ReadLinkRes_Closure closure,
							   void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__symlink(Zlomekfs__Zfsd_Service * service,
							  const Zlomekfs__SymlinkArgs * input,
							  Zlomekfs__SymlinkRes_Closure closure,
							  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__mknod(Zlomekfs__Zfsd_Service * service,
							const Zlomekfs__MknodArgs * input,
							Zlomekfs__MknodRes_Closure closure,
							void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__auth_stage1(Zlomekfs__Zfsd_Service * service,
								  const Zlomekfs__AuthStage1Args * input,
								  Zlomekfs__AuthStage1Res_Closure closure,
								  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__auth_stage2(Zlomekfs__Zfsd_Service * service,
								  const Zlomekfs__AuthStage2Args * input,
								  Zlomekfs__AuthStage2Res_Closure closure,
								  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__md5sum(Zlomekfs__Zfsd_Service * service,
							 const Zlomekfs__Md5sumArgs * input,
							 Zlomekfs__Md5sumRes_Closure closure,
							 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__file_info(Zlomekfs__Zfsd_Service * service,
								const Zlomekfs__FileInfoArgs * input,
								Zlomekfs__FileInfoRes_Closure closure,
								void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__reread_config(Zlomekfs__Zfsd_Service * service,
									const Zlomekfs__RereadConfigArgs * input,
									Zlomekfs__RereadConfigRes_Closure closure,
									void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__reintegrate(Zlomekfs__Zfsd_Service * service,
								  const Zlomekfs__ReintegrateArgs * input,
								  Zlomekfs__ReintegrateRes_Closure closure,
								  void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__reintegrate_add(Zlomekfs__Zfsd_Service * service,
									  const Zlomekfs__ReintegrateAddArgs *
									  input,
									  Zlomekfs__ReintegrateAddRes_Closure
									  closure, void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__reintegrate_del(Zlomekfs__Zfsd_Service * service,
									  const Zlomekfs__ReintegrateDelArgs *
									  input,
									  Zlomekfs__ReintegrateDelRes_Closure
									  closure, void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__reintegrate_ver(Zlomekfs__Zfsd_Service * service,
									  const Zlomekfs__ReintegrateVerArgs *
									  input,
									  Zlomekfs__ReintegrateVerRes_Closure
									  closure, void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}


static void zlomekfs__invalidate(Zlomekfs__Zfsd_Service * service,
								 const Zlomekfs__InvalidateArgs * input,
								 Zlomekfs__InvalidateRes_Closure closure,
								 void *closure_data)
{
	(void)service;
	(void)input;
	(void)closure;
	(void)closure_data;
}

static Zlomekfs__Zfsd_Service the_zlomekfs_service =
ZLOMEKFS__ZFSD__INIT(zlomekfs__);

int main()
{
	ProtobufC_RPC_Server *server;
	ProtobufC_RPC_AddressType address_type = PROTOBUF_C_RPC_ADDRESS_TCP;
	const char *port = "1275";

	server =
		protobuf_c_rpc_server_new(address_type, port,
								  (ProtobufCService *) & the_zlomekfs_service,
								  NULL);
	for (;;)
		protobuf_c_dispatch_run(protobuf_c_dispatch_default());

	return 0;
}

