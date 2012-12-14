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

#include "cli/io_device.h"

#include "constraints.h"
#include "command_line_edition.h"


CLI_NS_USE(cli)


CmdLineEdition::CmdLineEdition(const CmdLineEdition& CLI_CmdLine)
  : m_tkLeft(CLI_CmdLine.m_tkRight),
    m_tkRight(CLI_CmdLine.m_tkLeft),
    m_bInsertMode(CLI_CmdLine.m_bInsertMode)
{
}

CmdLineEdition::CmdLineEdition(void)
  : m_tkLeft(MAX_CMD_LINE_LENGTH), m_tkRight(MAX_CMD_LINE_LENGTH), m_bInsertMode(true)
{
}

CmdLineEdition::~CmdLineEdition(void)
{
}

CmdLineEdition& CmdLineEdition::operator=(const CmdLineEdition& CLI_CmdLine)
{
    m_tkLeft = CLI_CmdLine.m_tkLeft;
    m_tkRight = CLI_CmdLine.m_tkRight;
    m_bInsertMode = CLI_CmdLine.m_bInsertMode;

    return *this;
}

void CmdLineEdition::Set(const tk::String& TK_Left, const tk::String& TK_Right)
{
    // Simply set both parts of the command line.
    m_tkLeft = TK_Left;
    m_tkRight = TK_Right;
}

void CmdLineEdition::Reset(void)
{
    // Simply reset both parts of the command line.
    m_tkLeft.Reset();
    m_tkRight.Reset();
}

void CmdLineEdition::SetInsertMode(const bool B_InsertMode)
{
    m_bInsertMode = B_InsertMode;
}

const bool CmdLineEdition::GetInsertMode(void) const
{
    return m_bInsertMode;
}

void CmdLineEdition::Put(const OutputDevice& CLI_OutputDevice, const char C_Char)
{
    // First of all, append the left part of the command line.
    const char str_String[] = { C_Char, '\0' };
    Put(CLI_OutputDevice, tk::String(10, str_String));
}

void CmdLineEdition::Put(const OutputDevice& CLI_OutputDevice, const tk::String& TK_String)
{
    // First of all, append the left part of the command line.
    unsigned int ui_Len0 = m_tkLeft.GetLength();
    m_tkLeft.Append(TK_String);
    if (m_tkLeft.GetLength() > ui_Len0)
    {
        const unsigned int ui_CharCount = m_tkLeft.GetLength() - ui_Len0;

        // Print out the expanded characters.
        CLI_OutputDevice << m_tkLeft.SubString(ui_Len0, (int) ui_CharCount);

        if (m_bInsertMode)
        {
            // Refresh the right part of the line.

            // Print out the right part of the line.
            CLI_OutputDevice << m_tkRight;
            // Move the cursor to the left.
            for (unsigned int ui=m_tkRight.GetLength(); ui>0; ui--)
            {
                CLI_OutputDevice.PutString("\b");
            }
        }
        else
        {
            // Remove the first characters of the right part of the command line.
            if (m_tkRight.GetLength() > ui_CharCount)
            {
                m_tkRight = m_tkRight.SubString(ui_CharCount, (int) (m_tkRight.GetLength() - ui_CharCount));
            }
            else
            {
                m_tkRight.Reset();
            }
        }
    }
}

void CmdLineEdition::CleanAll(const OutputDevice& CLI_OutputDevice)
{
    Delete(CLI_OutputDevice, (int) m_tkRight.GetLength());
    Delete(CLI_OutputDevice, - (int) m_tkLeft.GetLength());
}

void CmdLineEdition::Delete(const OutputDevice& CLI_OutputDevice, const int I_Count)
{
    if (I_Count > 0)
    {
        // Delete forward

        // Find out the pattern to keep, reduce the right part of the command line.
        const unsigned int ui_CharCount = (((unsigned int) I_Count < m_tkRight.GetLength()) ? (unsigned int) I_Count : m_tkRight.GetLength());
        m_tkRight = m_tkRight.SubString(ui_CharCount, (int) (m_tkRight.GetLength() - ui_CharCount));
        // Print over the kept right part of the command line.
        CLI_OutputDevice << m_tkRight;
        // Blank the useless characters at the end of the line.
        for (unsigned int ui_Blank = ui_CharCount; ui_Blank > 0; ui_Blank --)
            CLI_OutputDevice << " ";
        // Move back the cursor.
        for (unsigned int ui_Back = m_tkRight.GetLength() + ui_CharCount; ui_Back > 0; ui_Back --)
            CLI_OutputDevice << "\b";
    }
    else if (I_Count < 0)
    {
        // Delete backward.

        // Fin out the pattern to keep, reduce the left part of the commane line.
        const unsigned int ui_CharCount = (((unsigned int) -I_Count < m_tkLeft.GetLength()) ? (unsigned int) -I_Count : m_tkLeft.GetLength());
        m_tkLeft = m_tkLeft.SubString(0, (int) (m_tkLeft.GetLength() - ui_CharCount));
        // Move back the cursor.
        for (unsigned int ui_Back1 = ui_CharCount; ui_Back1 > 0; ui_Back1 --)
            CLI_OutputDevice << "\b";
        // Print over the right part of the command line.
        CLI_OutputDevice << m_tkRight;
        // Blank the useless characters at the end of the line.
        for (unsigned int ui_Blank = ui_CharCount; ui_Blank > 0; ui_Blank --)
            CLI_OutputDevice << " ";
        // Move back the cursor.
        for (unsigned int ui_Back2 = m_tkRight.GetLength() + ui_CharCount; ui_Back2 > 0; ui_Back2 --)
            CLI_OutputDevice << "\b";
    }
}

void CmdLineEdition::PrintCmdLine(const OutputDevice& CLI_OutputDevice) const
{
    CLI_OutputDevice << m_tkLeft;
    CLI_OutputDevice << m_tkRight;
    for (unsigned int ui = m_tkRight.GetLength(); ui > 0; ui --)
    {
        CLI_OutputDevice << '\b';
    }
}

void CmdLineEdition::MoveCursor(const OutputDevice& CLI_OutputDevice, const int I_Count)
{
    if (I_Count > 0)
    {
        // Move forward

        // Find out the pattern to skip.
        const unsigned int ui_CharCount = (((unsigned int) I_Count < m_tkRight.GetLength()) ? (unsigned int) I_Count : m_tkRight.GetLength());
        const tk::String tk_Skipped = m_tkRight.SubString(0, (int) ui_CharCount);
        // Append the left part of the command line.
        if (m_tkLeft.Append(tk_Skipped))
        {
            // Reduce the right part of the command line.
            m_tkRight = m_tkRight.SubString(ui_CharCount, (int) (m_tkRight.GetLength() - ui_CharCount));
            // Print over the skipped part of the command line.
            CLI_OutputDevice << tk_Skipped;
        }
    }
    else if (I_Count < 0)
    {
        // Move backward.

        // Find out the pattern to skip.
        const unsigned int ui_CharCount = (((unsigned int) -I_Count < m_tkLeft.GetLength()) ? (unsigned int) -I_Count : m_tkLeft.GetLength());
        const tk::String tk_Skipped = m_tkLeft.SubString(m_tkLeft.GetLength() - ui_CharCount, (int) ui_CharCount);
        // Append the right part of the command line.
        tk::String tk_Right(tk_Skipped);
        if (tk_Right.Append(m_tkRight) && m_tkRight.Reset() && m_tkRight.Append(tk_Right))
        {
            // Reduce the left part of the command line.
            m_tkLeft = m_tkLeft.SubString(0, (int) (m_tkLeft.GetLength() - ui_CharCount));
            // Backward the cursor.
            for (unsigned int ui=ui_CharCount; ui>0; ui--)
            {
                CLI_OutputDevice << "\b";
            }
        }
    }
}

void CmdLineEdition::NextLine(const OutputDevice& CLI_OutputDevice)
{
    CLI_OutputDevice << m_tkRight << endl;
}

void CmdLineEdition::Home(const OutputDevice& CLI_OutputDevice)
{
    MoveCursor(CLI_OutputDevice, - (int) m_tkLeft.GetLength());
}

void CmdLineEdition::End(const OutputDevice& CLI_OutputDevice)
{
    MoveCursor(CLI_OutputDevice, (int) m_tkRight.GetLength());
}

const tk::String CmdLineEdition::GetLine(void) const
{
    return tk::String::Concat(MAX_CMD_LINE_LENGTH, m_tkLeft, m_tkRight);
}

const tk::String CmdLineEdition::GetLeft(void) const
{
    return m_tkLeft;
}

const tk::String CmdLineEdition::GetRight(void) const
{
    return m_tkRight;
}

const tk::String CmdLineEdition::GetNextWord(void) const
{
    unsigned int ui_Len = 0;

    // Skip blank characters.
    for (   ui_Len = 0;
            (ui_Len <= m_tkRight.GetLength()) && (m_tkRight[ui_Len] == ' ');
            ui_Len ++)
    {
    }

    // Skip characters until the next blank character.
    for (   ;
            (ui_Len <= m_tkRight.GetLength()) && (m_tkRight[ui_Len] != ' ');
            ui_Len ++)
    {
    }

    // Return the next word.
    if (ui_Len > m_tkRight.GetLength())
    {
        ui_Len = m_tkRight.GetLength();
    }
    return m_tkRight.SubString(0, (int) ui_Len);
}

const tk::String CmdLineEdition::GetPrevWord(void) const
{
    unsigned int ui_Len = 0;

    // Skip blank characters.
    for (   ui_Len = 0;
            (ui_Len <= m_tkLeft.GetLength()) && (m_tkLeft[m_tkLeft.GetLength() - 1 - ui_Len] == ' ');
            ui_Len ++)
    {
    }

    // Skip characters until the next blank character.
    for (   ;
            (ui_Len <= m_tkLeft.GetLength()) && (m_tkLeft[m_tkLeft.GetLength() - 1 - ui_Len] != ' ');
            ui_Len ++)
    {
    }

    // Return the previous word.
    if (ui_Len > m_tkLeft.GetLength())
    {
        ui_Len = m_tkLeft.GetLength();
    }
    return m_tkLeft.SubString(m_tkLeft.GetLength() - ui_Len, (int) ui_Len);
}

