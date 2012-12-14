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
#include "cli/non_blocking_io_device.h"


class NonBlockingTestDevice : public cli::NonBlockingIODevice
{
public:
    // Contruction / destruction.
    NonBlockingTestDevice(const bool B_AutoDelete) : cli::NonBlockingIODevice("NonBlockingTestDevice", B_AutoDelete)
    {
    }
    virtual ~NonBlockingTestDevice(void)
    {
    }

public:
    // OuptutDevice implementation.
    virtual const bool OpenDevice(void)
    {
        return true;
    }
    virtual const bool CloseDevice(void)
    {
        return true;
    }
    virtual void PutString(const char* const STR_Out) const
    {
        // Do not print anything.
        //cli::OutputDevice::GetStdOut().PutString(STR_Out);
    }
public:
    // Pushes a key to the non-blocking device.
    void EnterKey(const cli::KEY E_Key)
    {
        OnKey(E_Key);
    }
};

int main(void)
{
    // Retrieve the CLI
    cli::Cli::List cli_List(10);
    const int i_Clis = cli::Cli::FindFromName(cli_List, ".*");
    if (i_Clis == 0)
    {
        cli::OutputDevice::GetStdErr() << "Error: No CLI found." << cli::endl;
        return -1;
    }
    else if (i_Clis > 1)
    {
        cli::OutputDevice::GetStdErr() << "Warning: Several CLIs found. Executing only the first one." << cli::endl;
    }

    if (const cli::Cli* const pcli_Cli = cli_List.GetHead())
    {
        // Create a shell
        cli::Shell cli_Shell(*pcli_Cli);

        // Create the devices
        NonBlockingTestDevice cli_Device(false);

        // Launch the shell
        cli_Shell.Run(cli_Device);
        // The shell should still be running at this point.
        if (! cli_Shell.IsRunning())
        {
            cli::OutputDevice::GetStdErr() << "The shell should still be running after being launched." << cli::endl;
            return -1;
        }

        // Type 'help'.
        cli_Device.EnterKey(cli::KEY_h);
        cli_Device.EnterKey(cli::KEY_e);
        cli_Device.EnterKey(cli::KEY_l);
        cli_Device.EnterKey(cli::KEY_p);
        cli_Device.EnterKey(cli::ENTER);
        // The shell should still be running at this point.
        if (! cli_Shell.IsRunning())
        {
            cli::OutputDevice::GetStdErr() << "The shell should still be running after the 'help' command." << cli::endl;
            return -1;
        }

        // Type 'exit'.
        cli_Device.EnterKey(cli::KEY_e);
        cli_Device.EnterKey(cli::KEY_x);
        cli_Device.EnterKey(cli::KEY_i);
        cli_Device.EnterKey(cli::KEY_t);
        cli_Device.EnterKey(cli::ENTER);
        // The shell should not be running at this point anymore.
        if (cli_Shell.IsRunning())
        {
            cli::OutputDevice::GetStdErr() << "The shell should not be running anymore after the 'exit' command." << cli::endl;
            return -1;
        }

        return 0;
    }
    else
    {
        cli::OutputDevice::GetStdErr() << "Internal error." << cli::endl;
        return -1;
    }
}
