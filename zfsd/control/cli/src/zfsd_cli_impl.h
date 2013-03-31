/**
 *  \file zfsd_cli_impl.h
 * 
 *  \author Ales Snuparek (based on Alexis Royer tutorial)
 *  \brief Implementation of zlomekFS CLI interface
 *
 */

/*
    Copyright (c) 2006-2011, Alexis Royer, http://alexis.royer.free.fr/CLI

    All rights reserved.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
        * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
        * Neither the name of the CLI library project nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
    CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
    EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
    LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
    NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


#ifndef ZFSD_CLI_IMPL_H
#define ZFSD_CLI_IMPL_H

#include "system.h"
#include <sys/types.h>
#include <signal.h>
#include "log.h"
#include "syplog.h"
#include "control.h"
#include "volume.h"
#include "file.h"
#include "fh.h"


static void sayHello(const cli::OutputDevice& CLI_Out) { CLI_Out << "Hello!" << cli::endl; }
static void sayBye(const cli::OutputDevice& CLI_Out) { CLI_Out << "Bye." << cli::endl; }

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const string & s)
{
	out << s.str;
} 

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const node & nod)
{
	out << "id: " << nod->id;
	out << ", name: " << nod->name;
	out << ", host_name: " << nod->host_name;
	out << ", port: " << nod->port;
	out << ", last_connect: " << nod->last_connect;
	out << ", fd: " << nod->fd;
	out << ", generation: " << nod->generation;
	out << ", marked: " << nod->marked;

	/* Tables for mapping between ZFS IDs and node IDs.  */
	// htab_t map_uid_to_node;
	// htab_t map_uid_to_zfs;
	// htab_t map_gid_to_node;
	// htab_t map_gid_to_zfs;
}


static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const volume & vol)
{
	out << "id: " << vol->id;
	// node master;
	// htab_t slaves;
	out << ", name: " << vol->name;
	out << ", mount: " << vol->mountpoint;
	out << ", delete_p: " << vol->delete_p;
	out << ", marked: " << vol->marked;
	out << ", is_copy: " << vol->is_copy;
	out << ", n_locked_fhs: " << vol->n_locked_fhs;
	out << ", local_path: " << vol->local_path;
	//*CLI_Out << ", size_limit: " << vol->size_limit;
	out << ", last_conflict_ino: " << vol->last_conflict_ino;
	// internal_dentry root_dentry
	// virtual_dir root_vd
	//  hfile_t metadata
	// hfile_t fh_mapping

}

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const zfs_config_node & c)
{
	out << "node_id: " << c.node_id << cli::endl;
	out << "node_name: " << c.node_name << cli::endl;
	out << "host_name: " << c.host_name << cli::endl;
	out << "host_port: " << c.host_port << cli::endl;
}

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const zfs_configuration & c)
{
//	thread config_reader_data;
//	semaphore config_sem;
	out << "mlock_zfsd: " << c.mlock_zfsd << cli::endl;
	out << "local_config_path: " << c.local_config_path << cli::endl;
	out << "mountpoint: " << c.mountpoint << cli::endl;
	out << "default_node_uid: " << c.default_node_uid << cli::endl;
	out << "default_node_gid: " << c.default_node_gid << cli::endl;

	out << "this_node:" << cli::endl;
	out << c.this_node;
	out << "config_node:" << cli::endl;
	out << c.config_node;
	//zfs_config_metadata metadata;
	//zfs_config_threads threads;

#ifdef ENABLE_CLI
	//zfs_config_cli cli;
#endif

#ifdef ENABLE_VERSIONS
	//zfs_config_versions versions;
#endif

#ifdef HAVE_DOKAN
	//zfs_config_dokan dokan;
#endif
}

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const internal_fd_data_t * fd)
{
	//pthread_mutex_t mutex;
	out << "fd: " << fd->fd;
	out << ", generation: " << fd->generation;
	//fibnode heap_node;
	out << cli::endl;
}

static const cli::OutputDevice& operator<<(const cli::OutputDevice& out, const internal_fh fh)
{
#if 0
	//pthread_mutex_t mutex;
	out << "fd: " << fd->fd;
	out << ", generation: " << fd->generation;
	//fibnode heap_node;
#endif
	//pthread_mutex_t mutex;
	//pthread_cond_t cond;

	//zfs_fh local_fh;
	out << ", ndentries: " << fh->ndentries;
	//fattr attr;
	//metadata meta;
	//varray subdentries;
	//internal_cap cap;
	//interval_tree updated;
	//interval_tree modified;
	//journal_t journal;

	out << ", interval_tree_users: " << fh->interval_tree_users;
	out << ", level: " << fh->level;
	out << ", users: " << fh->users;
	out << ", id2assign: " << fh->id2assign;
	out << ", id2run: " << fh->id2run;
	out << ", fd: " << fh->fd;
	out << ", generation: " << fh->generation;

	/*! Flags, see IFH_* below.  */
	//unsigned int flags;

	out << ", reintegrating_sid: " << fh->reintegrating_sid;
	out << ", reintegrating_generation: " << fh->reintegrating_generation;

#ifdef ENABLE_VERSIONS
	/*! Version file description. Set to -1 if version file is not open.  */
	int version_fd;

	/*! Complete path of version file. Valid only when version file is open. */
	char *version_path;

	/*! File attributes before modification occurred.  */
	fattr version_orig_attr;

	/*! File was truncated before opening.  */
	bool file_truncated;

	/* File size when the file was opened.  */
	uint64_t marked_size;

	/*! Version file content intervals.  */
	interval_tree versioned;

	/*! Number of users of version interval tree.  */
	unsigned int version_interval_tree_users;

	/* List of intervals for open version file.  */
	version_item *version_list;
	unsigned int version_list_length;
#endif


	out << cli::endl;
}

static void zlomekfs_terminate()
{
	pid_t pid = getpid();
	kill(pid, SIGTERM);
}

static void zlomekfs_get_log_level(const cli::OutputDevice& CLI_Out)
{
	CLI_Out << get_log_level(&syplogger) << cli::endl;
}

static void zlomekfs_set_log_level(const cli::OutputDevice& CLI_Out, uint32_t log_level)
{
	set_log_level(&syplogger, log_level); CLI_Out << "OK" << cli::endl;
}

static void zlomekfs_get_connection_speed(const cli::OutputDevice& CLI_Out)
{
	CLI_Out <<  "Connection speed: " << connection_speed_to_str(zfs_control_get_connection_speed());
	CLI_Out << ", forced: " << zfs_control_get_connection_forced() << cli::endl;
}

static void zlomekfs_force_connection_speed(const cli::OutputDevice& CLI_Out, bool force)
{
	zfs_control_set_connection_forced(force);
	zlomekfs_get_connection_speed(CLI_Out);
}

static void zlomekfs_set_connection_speed(const cli::OutputDevice& CLI_Out, connection_speed speed)
{
	zfs_control_set_connection_speed(speed);
	CLI_Out << "Connection speed updated ";
	zlomekfs_force_connection_speed(CLI_Out, true);
}

static void zlomekfs_print_volume(volume vol, void * data)
{
	const cli::OutputDevice * CLI_Out = (cli::OutputDevice *) data;
	*CLI_Out << vol;
	*CLI_Out << cli::endl;
}

static void zlomekfs_print_volumes(const cli::OutputDevice& CLI_Out)
{
	CLI_Out << "Volumes list: " << cli::endl;
	for_each_volumes(zlomekfs_print_volume, (void *) &CLI_Out);
}


static void zlomekfs_print_node(node nod, void * data)
{
	const cli::OutputDevice * CLI_Out = (cli::OutputDevice *) data;
	*CLI_Out << nod;
	*CLI_Out << cli::endl;
}

static void zlomekfs_print_nodes(const cli::OutputDevice& CLI_Out)
{
	CLI_Out << "Volumes list: " << cli::endl;
	for_each_nodes(zlomekfs_print_node, (void *) &CLI_Out);
}

static void zlomekfs_print_zfs_config(const cli::OutputDevice& CLI_Out)
{
	
	CLI_Out << "zfs_config:" << cli::endl;
	CLI_Out << zfs_config;
	CLI_Out << cli::endl;
}

static void zlomekfs_print_internal_fd(const internal_fd_data_t * fd, void * data)
{
	if (fd->fd == -1) return;

	const cli::OutputDevice * CLI_Out = (cli::OutputDevice *) data;
	*CLI_Out << fd;
	*CLI_Out << cli::endl;
}

static void zlomekfs_print_internal_fds(const cli::OutputDevice& CLI_Out)
{
	CLI_Out << "internal_fds:" << cli::endl;
	for_each_internal_fd(zlomekfs_print_internal_fd, (void *) &CLI_Out);
}

static void zlomekfs_print_internal_fh(const internal_fh fh, void * data)
{
	const cli::OutputDevice * CLI_Out = (cli::OutputDevice *) data;
	*CLI_Out << fh;
	*CLI_Out << cli::endl;
}

static void zlomekfs_print_internal_fhs(const cli::OutputDevice& CLI_Out)
{
	CLI_Out << "internal_fh:" << cli::endl;
	for_each_internal_fh(zlomekfs_print_internal_fh, (void *) &CLI_Out);
}

#endif // ZFSD_CLI_IMPL_H
