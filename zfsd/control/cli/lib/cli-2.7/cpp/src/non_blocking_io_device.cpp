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

#include "cli/non_blocking_io_device.h"
#include "cli/shell.h"
#include "cli/assert.h"

#include "constraints.h"


CLI_NS_USE(cli)


// NonBlockingIODevice class implementation

NonBlockingIODevice::NonBlockingIODevice(const char* const STR_DbgName, const bool B_AutoDelete)
  : IODevice(STR_DbgName, B_AutoDelete),
    m_cliKeyReceivers(MAX_MT_CONTEXTS)
{
}

NonBlockingIODevice::~NonBlockingIODevice(void)
{
}

const KEY NonBlockingIODevice::GetKey(void) const
{
    // As this device is non-blocking, this method should not be called.
    CLI_ASSERT(false);
    return NULL_KEY;
}

void NonBlockingIODevice::AttachKeyReceiver(NonBlockingKeyReceiver& CLI_KeyReceiver)
{
    m_cliKeyReceivers.AddHead(& CLI_KeyReceiver);
}

void NonBlockingIODevice::DetachKeyReceiver(NonBlockingKeyReceiver& CLI_KeyReceiver)
{
    for (tk::Queue<NonBlockingKeyReceiver*>::Iterator it = m_cliKeyReceivers.GetIterator(); m_cliKeyReceivers.IsValid(it); m_cliKeyReceivers.MoveNext(it))
    {
        if (const NonBlockingKeyReceiver* const pcli_KeyReceiver = m_cliKeyReceivers.GetAt(it))
        {
            if (pcli_KeyReceiver == & CLI_KeyReceiver)
            {
                m_cliKeyReceivers.Remove(it);
                return;
            }
        }
    }

    // If this line is reached, it means that CLI_KeyReceiver has not been found in m_cliKeyReceivers.
    CLI_ASSERT(false);
}

const NonBlockingKeyReceiver* const NonBlockingIODevice::GetKeyReceiver(void) const
{
    if (! m_cliKeyReceivers.IsEmpty())
    {
        if (NonBlockingKeyReceiver* const pcli_KeyReceiver = m_cliKeyReceivers.GetHead())
        {
            return pcli_KeyReceiver;
        }
    }

    return NULL;
}

const Shell* const NonBlockingIODevice::GetShell(void) const
{
    for (   tk::Queue<NonBlockingKeyReceiver*>::Iterator it = m_cliKeyReceivers.GetIterator();
            m_cliKeyReceivers.IsValid(it);
            m_cliKeyReceivers.MoveNext(it))
    {
        if (const Shell* const pcli_Shell = dynamic_cast<const Shell*>(m_cliKeyReceivers.GetAt(it)))
        {
            return pcli_Shell;
        }
    }

    return NULL;
}

void NonBlockingIODevice::OnKey(const KEY E_Key) const
{
    if (! m_cliKeyReceivers.IsEmpty())
    {
        if (NonBlockingKeyReceiver* const pcli_KeyReceiver = m_cliKeyReceivers.GetHead())
        {
            pcli_KeyReceiver->OnNonBlockingKey(const_cast<NonBlockingIODevice&>(*this), E_Key);
        }
    }
    else
    {
        CLI_ASSERT(false);
    }
}

const bool NonBlockingIODevice::WaitForKeys(unsigned int UI_Milli) const
{
    tk::UnusedParameter(UI_Milli);

    // By default, no peek message loop, just return false.
    return false;
}


// NonBlockingKeyReceiver class implementation

NonBlockingKeyReceiver::NonBlockingKeyReceiver(void)
{
}

NonBlockingKeyReceiver::~NonBlockingKeyReceiver(void)
{
}
