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


#include <string.h> // strcmp

#include "cli/pch.h"
#include "cli/common.h"
#include "cli/shell.h"
#include "cli/console.h"
#include "cli/file_device.h"


int main(int I_Args, char** ARSTR_Args)
{
    if (I_Args >= 2)
    {
        if ((strcmp(ARSTR_Args[1], "-h") == 0)
            || (strcmp(ARSTR_Args[1], "-?") == 0)
            || (strcmp(ARSTR_Args[1], "-help") == 0)
            || (strcmp(ARSTR_Args[1], "--help") == 0))
        {
            cli::OutputDevice::GetStdOut() << "USAGE" << cli::endl;
            cli::OutputDevice::GetStdOut() << "   " << ARSTR_Args[0] << cli::endl;
            cli::OutputDevice::GetStdOut() << "       Interactive mode." << cli::endl;
            cli::OutputDevice::GetStdOut() << "   " << ARSTR_Args[0] << " <input file>" << cli::endl;
            cli::OutputDevice::GetStdOut() << "       Output to standard output." << cli::endl;
            cli::OutputDevice::GetStdOut() << "   " << ARSTR_Args[0] << " <input file> <output file>" << cli::endl;
            cli::OutputDevice::GetStdOut() << "       Output to given file." << cli::endl;

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

    // Create a shell.
    cli::Shell cli_Shell(*cli_List.GetHead());

    // Create devices.
    cli::OutputDevice* const pcli_Out = (
        (I_Args >= 3)
        ? dynamic_cast<cli::OutputDevice*>(new cli::OutputFileDevice(ARSTR_Args[2], true))
        : dynamic_cast<cli::OutputDevice*>(new cli::Console(true))
    );
    cli::IODevice* const pcli_In = (
        (I_Args >= 2)
        ? dynamic_cast<cli::IODevice*>(new cli::InputFileDevice(ARSTR_Args[1], *pcli_Out, true))
        : dynamic_cast<cli::IODevice*>(pcli_Out)
    );
    if (cli::InputFileDevice* const pcli_InFile = dynamic_cast<cli::InputFileDevice*>(pcli_In))
    {
        pcli_InFile->EnableSpecialCharacters(true);
    }

    // Redirect only echo, prompt, output and error streams.
    cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());

    // Launch it.
    cli_Shell.Run(*pcli_In);

    // Successful return.
    return 0;
}
