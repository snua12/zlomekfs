/**
 *  \file control_zfsd_cli.cpp
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

#include "system.h"
#include "zfs_config.h"
#include <pthread.h>
#include "cli/pch.h"
#include "cli/common.h"


#ifdef ENABLE_CLI_TELNET
#include "cli/telnet.h"
#endif
#ifdef ENABLE_CLI_CONSOLE
#include "cli/console.h"
#endif

#include "cli/common.h"
#include "zfsd_cli.h"
#include "control_zfsd_cli.h"

#ifdef ENABLE_CLI_TELNET

static pthread_t cli_telnet_thread;
static bool cli_telnet_thread_is_running = false;

class TestServer : public cli::TelnetServer
{
public:
    TestServer(const unsigned long UL_Port) : TelnetServer(2, UL_Port, cli::ResourceString::LANG_EN) {}
protected:
    virtual cli::Shell* const OnNewConnection(const cli::TelnetConnection& CLI_NewConnection)
    {
        if (const cli::Cli* const pcli_Cli = new ZfsdCli())
        {
            if (cli::Shell* const pcli_Shell = new cli::Shell(*pcli_Cli))
            {
                pcli_Shell->SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
                return pcli_Shell;
            }
        }
        return NULL;
    }
    virtual void OnCloseConnection(cli::Shell* const PCLI_Shell, const cli::TelnetConnection& CLI_ConnectionClosed)
    {
        if (PCLI_Shell != NULL)
        {
            const cli::Cli* const pcli_Cli = & PCLI_Shell->GetCli();
            delete PCLI_Shell;
            if (pcli_Cli != NULL)
            {
                delete pcli_Cli;
            }
        }
    }
};

static void * zfsd_cli_telnet_main(ATTRIBUTE_UNUSED void * data)
{
	TestServer cli_Server(zfs_config.cli.telnet_port);
	cli_Server.StartServer();
}
#endif

#ifdef ENABLE_CLI_CONSOLE
static pthread_t cli_console_thread;
static bool cli_console_thread_is_running = false;

static void * zfsd_cli_main(ATTRIBUTE_UNUSED void * data) 
{
	ZfsdCli cli_ZfsdCli;
	cli::Shell cli_Shell(cli_ZfsdCli);
	cli::Console cli_Console(false);
	cli_Shell.Run(cli_Console);
	return NULL;
}
#endif

void start_cli_control(void)
{

#ifdef ENABLE_CLI_TELNET
	if (cli_telnet_thread_is_running == false)
	{
		int rv = pthread_create(&cli_telnet_thread, NULL, zfsd_cli_telnet_main, NULL);
		if (rv == 0) cli_telnet_thread_is_running = true;
	}
#endif
#ifdef ENABLE_CLI_CONSOLE
	if (cli_console_thread_is_running == false)
	{
		int rv = pthread_create(&cli_console_thread, NULL, zfsd_cli_main, NULL);
		if (rv == 0) cli_console_thread_is_running = true;
	}
#endif
}

void stop_cli_control(void)
{
#ifdef ENABLE_CLI_TELNET
	if (cli_telnet_thread_is_running == true)
	{
		int rv = pthread_cancel(cli_telnet_thread);
		cli_telnet_thread_is_running = false;
	}
#endif
#ifdef ENABLE_CLI_CONSOLE
	if (cli_console_thread_is_running == true)
	{
		int rv = pthread_cancel(cli_console_thread);
		cli_console_thread_is_running = false;
	}
#endif
}

