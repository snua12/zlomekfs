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
#include "cli/ui.h"
#include "cli/shell.h"


CLI_NS_BEGIN(cli)

    CLI_NS_BEGIN(ui)

        UI::UI(void)
          : m_pcliShell(NULL),
            m_bKeepRunning(false), m_bExecResult(false)
        {
        }

        UI::~UI(void)
        {
        }

        const bool UI::Run(Shell& CLI_Shell)
        {
            for (Start(CLI_Shell); m_bKeepRunning; )
            {
                if (const NonBlockingIODevice* const pcli_NonBlockingIODevice = dynamic_cast<const NonBlockingIODevice*>(& GetShell().GetInput()))
                {
                    if (! pcli_NonBlockingIODevice->WaitForKeys(100))
                    {
                        if (m_bKeepRunning)
                        {
                            Finish(false);
                        }
                    }
                }
                else
                {
                    const KEY e_KeyCode = GetShell().GetInput().GetKey();
                    OnKey(e_KeyCode);
                }
            }

            return m_bExecResult;
        }

        void UI::Start(Shell& CLI_Shell)
        {
            m_pcliShell = & CLI_Shell;

            IODevice& cli_InputDevice = const_cast<IODevice&>(GetShell().GetInput()); // const cast for UseInstance() and AttachKeyReceiver() methods below
            cli_InputDevice.UseInstance(__CALL_INFO__);

            if (NonBlockingIODevice* const pcli_NonBlockingIODevice = dynamic_cast<NonBlockingIODevice*>(& cli_InputDevice))
            {
                pcli_NonBlockingIODevice->AttachKeyReceiver(*this);
            }

            m_bKeepRunning = true;

            m_bExecResult = false;
            Reset();
            ResetToDefault();
        }

        void UI::Finish(const bool B_ExecResult)
        {
            m_bExecResult = B_ExecResult;
            if (! m_bExecResult)
            {
                ResetToDefault();
                GetShell().GetStream(ECHO_STREAM) << endl;
            }

            m_bKeepRunning = false;

            IODevice& cli_InputDevice = const_cast<IODevice&>(GetShell().GetInput()); // const cast for FreeInstance() and DetachKeyReceiver() methods below
            if (NonBlockingIODevice* const pcli_NonBlockingIODevice = dynamic_cast<NonBlockingIODevice*>(& cli_InputDevice))
            {
                pcli_NonBlockingIODevice->DetachKeyReceiver(*this);
            }

            cli_InputDevice.FreeInstance(__CALL_INFO__);

            m_pcliShell = NULL;
        }

        Shell& UI::GetShell(void) const
        {
            if (m_pcliShell != NULL)
            {
                return * m_pcliShell;
            }
            else
            {
                class _Cli : public Cli { public: _Cli(void) : Cli("", Help()) {} };
                static _Cli cli_Cli;
                static Shell cli_Shell(cli_Cli);
                return cli_Shell;
            }
        }

        void UI::OnNonBlockingKey(NonBlockingIODevice& CLI_Source, const KEY E_KeyCode)
        {
            OnKey(E_KeyCode);
        }

    CLI_NS_END(ui)

CLI_NS_END(cli)

