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
#include "cli/cli.h"
#include "cli/file_device.h"
#include "cli/traces.h"
#include "cli/assert.h"

#include "io_device_impl.h"


const cli::Cli& GetCli(void)
{
    cli::Cli::List cli_List(10);
    if ((cli::Cli::FindFromName(cli_List, ".*") > 0)
        && (! cli_List.IsEmpty()))
    {
        // Return first CLI if any.
        return *cli_List.GetHead();
    }
    else
    {
        // Should not happen.
        CLI_ASSERT(false);
        class MyCli : public cli::Cli { public:
            MyCli(void) : cli::Cli("", cli::Help()) {}
        };
        static MyCli cli_MyCli;
        return cli_MyCli;
    }
}

cli::Shell& GetShell(void)
{
    // Ensure IOMux creation.
    GetIOMux();

    static cli::Shell cli_Shell(GetCli());
    return cli_Shell;
}

cli::IOMux& GetIOMux(void)
{
    static cli::IOMux cli_IOMux(false);
    return cli_IOMux;
}

int main(int I_Args, char* ARSTR_Args[])
{
    if (I_Args != 3)
    {
        cli::OutputDevice::GetStdErr() << "USAGE:" << cli::endl;
        cli::OutputDevice::GetStdErr() << "   io_device <input-file> <traces-output-file>" << cli::endl;
        return -1;
    }

    // Enable traces.
    if (cli::OutputDevice* const pcli_TraceDevice = new cli::OutputFileDevice(ARSTR_Args[2], true))
    {
        cli::GetTraces().SetStream(*pcli_TraceDevice);
        cli::GetTraces().Trace(cli::TraceClass("CLI_IO_DEVICE_INSTANCES", cli::Help()));
        cli::GetTraces().SetFilter(cli::TraceClass("CLI_IO_DEVICE_INSTANCES", cli::Help()), true);
        cli::GetTraces().Trace(cli::TraceClass("CLI_IO_DEVICE_OPENING", cli::Help()));
        cli::GetTraces().SetFilter(cli::TraceClass("CLI_IO_DEVICE_OPENING", cli::Help()), true);

        // Input.
        GetIOMux().AddDevice(new cli::InputFileDevice(ARSTR_Args[1], cli::OutputDevice::GetStdOut(), true));

        // Execute.
        GetShell().Run(GetIOMux());

        // Finish.
        GetIOMux().ResetDeviceList();
        cli::GetTraces().UnsetStream(*pcli_TraceDevice);
    }
    return 0;
}

const bool SetDevice(
        cli::STREAM_TYPE E_Use,
        cli::OutputDevice* const PCLI_Device)
{
    bool b_Res = false;

    // Check input parameters.
    if ((E_Use >= 0) && (E_Use <= cli::STREAM_TYPES_COUNT)
        && (PCLI_Device != NULL))
    {
        // First of all, lock the device instance.
        PCLI_Device->UseInstance(__CALL_INFO__);

        if (E_Use < cli::STREAM_TYPES_COUNT) // STREAM_TYPES_COUNT means 'input'
        {
            // Output streams.
            b_Res = GetShell().SetStream(E_Use, *PCLI_Device);
        }
        else if (cli::IODevice* const pcli_IODevice = dynamic_cast<cli::IODevice*>(PCLI_Device))
        {
            // Input streams.
            b_Res = GetIOMux().AddDevice(pcli_IODevice);
        }
        else
        {
            GetShell().GetStream(cli::ERROR_STREAM) << "Invalid parameters" << cli::endl;
        }

        // Eventually free the device instance.
        PCLI_Device->FreeInstance(__CALL_INFO__);
    }
    else
    {
        GetShell().GetStream(cli::ERROR_STREAM) << "Invalid parameters" << cli::endl;
    }

    return b_Res;
}

