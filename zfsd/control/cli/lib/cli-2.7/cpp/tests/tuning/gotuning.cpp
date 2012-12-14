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
#include "cli/common.h"
#include "cli/console.h"
#include "cli/traces.h"

cli::Cli* const GetCli(void)
{
    // Find the empty CLI.
    cli::Cli::List cli_CLIs(1);
    if ((cli::Cli::FindFromName(cli_CLIs, ".*") <= 0) || (cli_CLIs.IsEmpty()))
    {
        cli::OutputDevice::GetStdErr() << "No CLI found" << cli::endl;
    }
    else
    {
        if (const cli::Cli* const pcli_Cli = cli_CLIs.GetHead())
        {
            //! @warning Very bad use of const_cast here !
            //!         However, I pretty well know how the library works.
            //!         That's the reason why I'm affording it to shorten the stuff.
            return const_cast<cli::Cli*>(pcli_Cli);
        }
    }

    return NULL;
}

cli::Shell* const GetShell(void)
{
    if (const cli::Cli* const pcli_Cli = GetCli())
    {
        if (cli::Shell* const pcli_Shell = & pcli_Cli->GetShell())
        {
            return pcli_Shell;
        }
    }

    return NULL;
}

void TuneWelcomeMessage(const cli::ResourceString& CLI_WelcomeMessage)
{
    if (cli::Shell* const pcli_Shell = GetShell())
    {
        pcli_Shell->SetWelcomeMessage(CLI_WelcomeMessage);

        // Same as Shell::PromptWelcomeMessage() / but output in the output stream since the welcome stream is shut down.
            const cli::tk::String str_WelcomeMessage = CLI_WelcomeMessage.GetString(pcli_Shell->GetLang());
            if (! str_WelcomeMessage.IsEmpty())
            {
                pcli_Shell->GetStream(cli::OUTPUT_STREAM) << str_WelcomeMessage;
                // Addition of an extra end of line, because the CLI command line cannot specify one.
                pcli_Shell->GetStream(cli::OUTPUT_STREAM) << cli::endl;
            }
            else if (GetCli() != NULL)
            {
                pcli_Shell->GetStream(cli::OUTPUT_STREAM)
                << "---------------------------------------------------" << cli::endl
                << " Welcome to " << GetCli()->GetKeyword() << "!" << cli::endl
                << cli::endl
                << " " << GetCli()->GetKeyword() << " is a command line interface" << cli::endl
                << " using the CLI library" << cli::endl
                << "   (c) Alexis Royer http://alexis.royer.free.fr/CLI/" << cli::endl
                << " Type 'help' at any time" << cli::endl
                << " or press '?' or TAB to get completion or help." << cli::endl
                << "---------------------------------------------------" << cli::endl;
            }
        // End of Shell::PromptWelcomeMessage()
    }
}

void TuneByeMessage(const cli::ResourceString& CLI_ByeMessage)
{
    if (cli::Shell* const pcli_Shell = GetShell())
    {
        pcli_Shell->SetByeMessage(CLI_ByeMessage);

        // Same as Shell::PromptByeMessage() / but output in the output stream since the welcome stream is shut down.
            const cli::tk::String str_ByeMessage = CLI_ByeMessage.GetString(pcli_Shell->GetLang());
            if (! str_ByeMessage.IsEmpty())
            {
                pcli_Shell->GetStream(cli::OUTPUT_STREAM) << str_ByeMessage;
                // Addition of an extra end of line, because the CLI command line cannot specify one.
                pcli_Shell->GetStream(cli::OUTPUT_STREAM) << cli::endl;
            }
            else
            {
                pcli_Shell->GetStream(cli::OUTPUT_STREAM) << "Bye!" << cli::endl;
            }
        // End of Shell::PromptByeMessage()
    }
}

void TunePrompt(const cli::ResourceString& CLI_Prompt)
{
    if (cli::Shell* const pcli_Shell = GetShell())
    {
        pcli_Shell->SetPrompt(CLI_Prompt);
    }
}

void TuneLang(const cli::ResourceString::LANG E_Lang)
{
    if (cli::Shell* const pcli_Shell = GetShell())
    {
        pcli_Shell->SetLang(E_Lang);
    }
}

void TuneBeep(const bool B_Enable)
{
    if (cli::Shell* const pcli_Shell = GetShell())
    {
        pcli_Shell->SetBeep(B_Enable);
        if (pcli_Shell->GetBeep())
        {
            pcli_Shell->GetStream(cli::OUTPUT_STREAM) << "Echo is on" << cli::endl;
        }
        else
        {
            pcli_Shell->GetStream(cli::OUTPUT_STREAM) << "Echo is off" << cli::endl;
        }
    }
}

void TuneConfigMenu(const bool B_Enable)
{
    if (cli::Cli* const pcli_Cli = GetCli())
    {
        pcli_Cli->EnableConfigMenu(B_Enable);
    }
}
