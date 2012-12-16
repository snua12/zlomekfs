/**
 *  \file zfsd_cli_impl.h
 * 
 *  \author Ales Snuparek (based on Alexis Royer tutorial)
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

#include <sys/types.h>
#include <signal.h>
#include "log.h"
#include "syplog.h"


static void sayHello(const cli::OutputDevice& CLI_Out) { CLI_Out << "Hello!" << cli::endl; }
static void sayBye(const cli::OutputDevice& CLI_Out) { CLI_Out << "Bye." << cli::endl; }
static void zlomekfs_terminate() { pid_t pid = getpid();kill(pid, SIGTERM); }
static void zlomekfs_get_log_level(const cli::OutputDevice& CLI_Out) { CLI_Out << get_log_level(&syplogger) << cli::endl; }
static void zlomekfs_set_log_level(const cli::OutputDevice& CLI_Out, uint32_t log_level) { set_log_level(&syplogger, log_level); CLI_Out << "OK" << cli::endl; }

#endif // ZFSD_CLI_IMPL_H
