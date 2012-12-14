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


#include <stdlib.h> // atoi

#include "cli/pch.h"
#include "cli/common.h"
#include "cli/telnet.h"
#include "empty.h"


class TestServer : public cli::TelnetServer
{
public:
    TestServer(const unsigned long UL_Port) : TelnetServer(2, UL_Port, cli::ResourceString::LANG_EN) {}
protected:
    virtual cli::Shell* const OnNewConnection(const cli::TelnetConnection& CLI_NewConnection)
    {
        if (const cli::Cli* const pcli_Cli = new EmptyCli())
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

int main(int I_ArgCount, char* ARSTR_Args[])
{
    unsigned long ul_Port = 0;
    if ((I_ArgCount < 2) || (ARSTR_Args[0] == NULL))
    {
        cli::OutputDevice::GetStdErr() << "USAGE: telnet <port>" << cli::endl;
        cli::OutputDevice::GetStdErr() << "   port: TCP port to listen onto" << cli::endl;
        return -1;
    }
    else
    {
        ul_Port = atoi(ARSTR_Args[1]);
        if (ul_Port == 0)
        {
            cli::OutputDevice::GetStdErr() << "Invalid port " << ARSTR_Args[0] << cli::endl;
            return -1;
        }
    }

    TestServer cli_Server(ul_Port);
    cli_Server.StartServer();
    return 0;
}
