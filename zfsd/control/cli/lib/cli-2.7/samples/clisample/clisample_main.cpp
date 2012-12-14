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

#include "cli/pch.h"

#include <stdlib.h>
#include <string.h> // strcmp

#include "cli/common.h"
#include "cli/shell.h"
#include "cli/console.h"
#include "cli/telnet.h"


//! @brief Usage display.
void PrintHelp(const cli::OutputDevice& CLI_Stream)
{
    CLI_Stream << "USAGE:" << cli::endl;
    CLI_Stream << "   clisample        : run in a console" << cli::endl;
    CLI_Stream << "   clisample [port] : run as a tcp server on the given port" << cli::endl;
}

int main(int I_Args, char** ARSTR_Args)
{
    unsigned long ul_TcpPort = 0;
    if (I_Args >= 2)
    {
        // Display help.
        if (   (strcmp(ARSTR_Args[1], "-help") == 0)
                ||  (strcmp(ARSTR_Args[1], "-h") == 0)
                ||  (strcmp(ARSTR_Args[1], "--help") == 0)
                ||  (strcmp(ARSTR_Args[1], "-?") == 0))
        {
            PrintHelp(cli::OutputDevice::GetStdOut());
            return 0;
        }

        // Parse TCP port.
        if (const unsigned long ul_Tmp = atoi(ARSTR_Args[1]))
        {
            ul_TcpPort = ul_Tmp;
        }
        else
        {
            cli::OutputDevice::GetStdErr() << "Unknown option '" << ARSTR_Args[1] << "'" << cli::endl;
            PrintHelp(cli::OutputDevice::GetStdErr());
            return -1;
        }
    }

    // Look for a CLI to launch.
    cli::Cli::List cli_List(10);
    cli::Cli::FindFromName(cli_List, ".*");
    if (cli_List.IsEmpty())
    {
        cli::OutputDevice::GetStdErr() << "No CLI found" << cli::endl;
        return -1;
    }
    else if (cli_List.GetCount() > 1)
    {
        cli::OutputDevice::GetStdErr() << "Several CLI found" << cli::endl;
    }

    // Launch it.
    if (ul_TcpPort != 0)
    {
        // Run as a telnet server.
        class MyTelnetServer : public cli::TelnetServer
        {
        private:
            const cli::Cli& m_cliCli;
        public:
            MyTelnetServer(const cli::Cli& CLI_Cli, const unsigned long UL_TcpPort)
              : TelnetServer(1, UL_TcpPort, cli::ResourceString::LANG_EN), m_cliCli(CLI_Cli) // because the CLI is allocated once only, allow only one client.
            {
            }
            virtual cli::Shell* const OnNewConnection(const cli::TelnetConnection& CLI_NewConnection)
            {
                return new cli::Shell(m_cliCli);
            }
            virtual void OnCloseConnection(cli::Shell* const PCLI_Shell, const cli::TelnetConnection& CLI_ClosedConnection)
            {
                if (PCLI_Shell != NULL)
                {
                    delete PCLI_Shell;
                }
            }
        };
        MyTelnetServer cli_TelnetServer(*cli_List.GetHead(), ul_TcpPort);
        cli::OutputDevice::GetStdOut() << "Starting server on port " << ul_TcpPort << cli::endl;
        cli_TelnetServer.StartServer();
    }
    else
    {
        // Create a shell.
        cli::Shell cli_Shell(*cli_List.GetHead());

        // Enable following lines if you wish to disable streams.
        // You can also redirect to something else.
        //  cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
        //  cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
        //  cli_Shell.SetStream(cli::ECHO_STREAM, cli::OutputDevice::GetNullDevice());
        //  cli_Shell.SetStream(cli::OUTPUT_STREAM, cli::OutputDevice::GetNullDevice());
        //  cli_Shell.SetStream(cli::ERROR_STREAM, cli::OutputDevice::GetNullDevice());

        // Run in a console.
        cli::Console cli_Console(false);
        cli_Shell.Run(cli_Console);
    }

    // Successful return.
    return 0;
}
