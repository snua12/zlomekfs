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

#include <pthread.h>
#include "cli/pch.h"
#include "cli/common.h"
#include "cli/console.h"

#include "cli/common.h"
#include "zfsd_cli.h"
#include "control_zfsd_cli.h"

#include "system.h"

static pthread_t cli_thread;
static bool cli_thread_is_running = false;

static void * zfsd_cli_main(ATTRIBUTE_UNUSED void * data) 
{
	ZfsdCli cli_ZfsdCli;
	cli::Shell cli_Shell(cli_ZfsdCli);
	cli::Console cli_Console(false);
	cli_Shell.Run(cli_Console);
}

void start_cli_control(void)
{
	if (cli_thread_is_running == true) return;

	int rv = pthread_create(&cli_thread, NULL, zfsd_cli_main, NULL);

	if (rv == 0) cli_thread_is_running = true;
}

void stop_cli_control(void)
{
	if (cli_thread_is_running == false) return;
	pthread_cancel(cli_thread);
	cli_thread_is_running = false;
}

