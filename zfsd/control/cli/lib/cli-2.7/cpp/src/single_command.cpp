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

#include "cli/single_command.h"
#include "consistency.h"
#include "constraints.h"

CLI_NS_USE(cli)


SingleCommand::SingleCommand(
        const char* const STR_Command,
        OutputDevice& CLI_Output,
        const bool B_AutoDelete)
  : IODevice(tk::String::Concat(MAX_DEVICE_NAME_LENGTH, "cmd[", STR_Command, "]"), B_AutoDelete), m_cliOutput(CLI_Output),
    m_strCommand(MAX_CMD_LINE_LENGTH, STR_Command), m_iPosition(-1)
{
    EnsureCommonDevices();

    m_cliOutput.UseInstance(__CALL_INFO__);
}

SingleCommand::~SingleCommand(void)
{
    m_cliOutput.FreeInstance(__CALL_INFO__);
}

const tk::String SingleCommand::GetCommand(void) const
{
    return m_strCommand;
}

const bool SingleCommand::OpenDevice(void)
{
    if (! m_cliOutput.OpenUp(__CALL_INFO__))
    {
        m_cliLastError = m_cliOutput.GetLastError();
        return false;
    }

    m_iPosition = 0;
    return true;
}

const bool SingleCommand::CloseDevice(void)
{
    bool b_Res = true;

    m_iPosition = -1;

    if (! m_cliOutput.CloseDown(__CALL_INFO__))
    {
        m_cliLastError = m_cliOutput.GetLastError();
        b_Res = false;
    }

    return b_Res;
}

const KEY SingleCommand::GetKey(void) const
{
    if ((m_iPosition >= 0) && (m_iPosition < (signed) m_strCommand.GetLength()))
    {
        const char c_Char = m_strCommand[m_iPosition];
        m_iPosition ++;
        return IODevice::GetKey(c_Char);
    }
    else if (m_iPosition == (signed) m_strCommand.GetLength())
    {
        m_iPosition ++;
        return IODevice::GetKey('\n');
    }
    else
    {
        m_cliLastError
            .SetString(ResourceString::LANG_EN, "No more characters in command line")
            .SetString(ResourceString::LANG_FR, "Il n'y a plus de caractères dans la ligne de commande");
        return NULL_KEY;
    }
}

void SingleCommand::PutString(const char* const STR_Out) const
{
    if (! m_cliOutput.WouldOutput(*this)) // Avoid infinite loops.
    {
        m_cliOutput.PutString(STR_Out);
    }
}

void SingleCommand::Beep(void) const
{
    if (! m_cliOutput.WouldOutput(*this)) // Avoid infinite loops.
    {
        m_cliOutput.Beep();
    }
}

void SingleCommand::CleanScreen(void) const
{
    if (! m_cliOutput.WouldOutput(*this)) // Avoid infinite loops.
    {
        m_cliOutput.CleanScreen();
    }
}

const bool SingleCommand::WouldOutput(const OutputDevice& CLI_Device) const
{
    return (IODevice::WouldOutput(CLI_Device) || m_cliOutput.WouldOutput(CLI_Device));
}
