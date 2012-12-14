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

#include <stdlib.h> // atoi

#include "cli/cli.h"
#include "cli/menu.h"
#include "cli/keyword.h"
#include "cli/param_int.h"
#include "cli/endl.h"
#include "cli/syntax_tag.h"
#include "cli/shell.h"
#include "cli/console.h"
#include "cli/telnet.h"


class _Cli : public cli::Cli
{
public:
    _Cli(const char* const STR_Name, const cli::Help& CLI_Help)
      : Cli(STR_Name, CLI_Help)
    {
    }

    virtual ~_Cli(void)
    {
    }
};

_Cli cli_TestCli("test-cli", cli::Help().AddHelp(cli::Help::LANG_EN, "Test CLI"));
 // show
 cli::Keyword& cli_Show = dynamic_cast<cli::Keyword&>(cli_TestCli.AddElement(new cli::Keyword("show", cli::Help().AddHelp(cli::Help::LANG_EN, "Show parameters"))));
  // all
  cli::Keyword& cli_All = dynamic_cast<cli::Keyword&>(cli_Show.AddElement(new cli::Keyword("all", cli::Help().AddHelp(cli::Help::LANG_EN, "Show all parameters"))));
   // endl
   cli::Endl& cli_Endl1 = dynamic_cast<cli::Endl&>(cli_All.AddElement(new cli::Endl(cli::Help().AddHelp(cli::Help::LANG_EN, "Show all parameters"))));
  cli::SyntaxTag& cli_Tag = dynamic_cast<cli::SyntaxTag&>(cli_Show.AddElement(new cli::SyntaxTag(false)));
   // param
   cli::Keyword& cli_Param = dynamic_cast<cli::Keyword&>(cli_Tag.AddElement(new cli::Keyword("param", cli::Help().AddHelp(cli::Help::LANG_EN, "Parameter <id>"))));
    // <id>
    cli::ParamInt& cli_ParamValue = dynamic_cast<cli::ParamInt&>(cli_Param.AddElement(new cli::ParamInt(cli::Help().AddHelp(cli::Help::LANG_EN, "Parameter id"))));
     // endl
     cli::Endl& cli_Endl2 = dynamic_cast<cli::Endl&>(cli_ParamValue.AddElement(new cli::Endl(cli::Help().AddHelp(cli::Help::LANG_EN, "Show given parameter"))));
     //ref
     cli::SyntaxRef& cli_Ref = dynamic_cast<cli::SyntaxRef&>(cli_ParamValue.AddElement(new cli::SyntaxRef(cli_Tag)));
 // my-cli::Menu
 cli::Keyword& cli_MyMenu = dynamic_cast<cli::Keyword&>(cli_TestCli.AddElement(new cli::Keyword("my-cli::Menu", cli::Help().AddHelp(cli::Help::LANG_EN, "Personal cli::Menu"))));
  // endl
  cli::Endl& cli_Endl3 = dynamic_cast<cli::Endl&>(cli_MyMenu.AddElement(new cli::Endl(cli::Help().AddHelp(cli::Help::LANG_EN, "Enter personal cli::Menu"))));

// my-menu
cli::Menu& cli_MyMenuMenu = dynamic_cast<cli::Menu&>(cli_Endl3.SetMenu(new cli::Menu("my-menu", cli::Help().AddHelp(cli::Help::LANG_EN, "Personal Menu"))));


int main(int I_ArgsCount, char* ARSTR_Args[])
{
    if (I_ArgsCount <= 1)
    {
        cli::Shell cli_Shell(cli_TestCli);
        cli::Console cli_Console(false);
        cli_Shell.Run(cli_Console);
    }
    else
    {
        cli::OutputDevice::GetStdOut() << "Running telnet server on port " << ARSTR_Args[1] << cli::endl;
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
        MyTelnetServer cli_Server(cli_TestCli, atoi(ARSTR_Args[1]));
        cli_Server.StartServer();
    }
    return 0;
}

