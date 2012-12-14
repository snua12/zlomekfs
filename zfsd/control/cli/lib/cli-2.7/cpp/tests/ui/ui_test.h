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


#ifndef __CLI_TEST_H__
#define __CLI_TEST_H__

#include <iostream>

#include "cli/assert.h"
#include "cli/cli.h"
#include "cli/shell.h"


static void UIError(const char* const STR_FileName, const unsigned int UI_Line)
{
    std::cerr << STR_FileName << ":" << UI_Line << ": test failed" << std::endl;
}

// Tool classes.
class my_Cli : public cli::Cli {
public:
    my_Cli(const char* const STR_Name, const cli::Help& CLI_Help) : cli::Cli(STR_Name, CLI_Help) {}
};

class my_MTDevice : public cli::NonBlockingIODevice {
public:
    my_MTDevice(void) : cli::NonBlockingIODevice("uitest_MTDevice", false), m_tkInput(256), m_tkOutput(1024), m_bWrapLines(false) {}
    virtual const bool OpenDevice(void) { return true; }
    virtual const bool CloseDevice(void) { return true; }
public:
    void SetInputString(const char* const STR_Input) {
        // Clear input.
        m_tkInput.Reset();
        // For each character,
        for (const char* ptc_Input = STR_Input; (ptc_Input != NULL) && (*ptc_Input != '\0'); ptc_Input++) {
            if (*ptc_Input == '%') {
                // Escaped characters
                ptc_Input++;
                switch (*ptc_Input) {
                case '%': m_tkInput.AddTail(cli::PERCENT); break;
                case ']': m_tkInput.AddTail(cli::ESCAPE); break;
                case '!': m_tkInput.AddTail(cli::BREAK); break;
                case 'u': m_tkInput.AddTail(cli::KEY_UP); break;
                case 'U': m_tkInput.AddTail(cli::PAGE_UP); break;
                case 'd': m_tkInput.AddTail(cli::KEY_DOWN); break;
                case 'D': m_tkInput.AddTail(cli::PAGE_DOWN); break;
                case 'l': m_tkInput.AddTail(cli::KEY_LEFT); break;
                case 'L': m_tkInput.AddTail(cli::PAGE_LEFT); break;
                case 'r': m_tkInput.AddTail(cli::KEY_RIGHT); break;
                case 'R': m_tkInput.AddTail(cli::PAGE_RIGHT); break;
                case 'h': case 'H': m_tkInput.AddTail(cli::KEY_BEGIN); break;
                case 'e': case 'E': m_tkInput.AddTail(cli::KEY_END); break;
                case 'i': case 'I': m_tkInput.AddTail(cli::INSERT); break;
                default: CLI_ASSERT(false); break;
                }
            } else {
                // simple key: push in the input queue.
                m_tkInput.AddTail(this->IODevice::GetKey(*ptc_Input));
            }
        }
    }
    virtual const bool WaitForKeys(const unsigned int UI_Milli) const {
        // For each key in the queue.
        while (! m_tkInput.IsEmpty()) {
            OnKey(m_tkInput.RemoveHead());
        }
        // Then indicate no more keys.
        return false;
    }
public:
    virtual void PutString(const char* const STR_Output) const {
        for (const char* pc_Out = STR_Output; (pc_Out != NULL) && (*pc_Out != '\0'); pc_Out ++)
        {
            if (*pc_Out == '\b')
                m_tkOutput.Append("%b");
            else
                m_tkOutput.Append(*pc_Out);
        }
    }
    virtual void Beep(void) const {
        m_tkOutput.Append("!BEEP!");
    }
    virtual void CleanScreen(void) const {
        m_tkOutput.Append("!CLS!");
    }
    void SetbWrapLines(const bool B_WrapLines) {
        m_bWrapLines = B_WrapLines;
    }
    virtual const ScreenInfo GetScreenInfo(void) const {
        return ScreenInfo(
            5, 5,           // Width and height
            false,          // True Cls
            m_bWrapLines    // Line wrapping
        );
    }
    const cli::tk::String GetOutputString(void) {
        const cli::tk::String tk_Output(m_tkOutput);
        m_tkOutput.Reset();
        return tk_Output;
    }
private:
    mutable cli::tk::Queue<cli::KEY> m_tkInput;
    mutable cli::tk::String m_tkOutput;
    bool m_bWrapLines;
};

class Out : public cli::tk::String {
public:
    Out(void) : cli::tk::String(1024) {}

    Out& txt(const char* const STR_Text) {
        for (const char* pc_Out = STR_Text; (pc_Out != NULL) && (*pc_Out != '\0'); pc_Out ++)
            if (*pc_Out == '\b') { Append("%b"); } else { Append(*pc_Out); }
        return *this;
    }
    Out& left(const unsigned int UI_Left) {
        for (unsigned int ui=UI_Left; ui; ui--) Append("%b");
        return *this;
    }
    Out& right(const char* const STR_Right) {
        Append(STR_Right);
        return *this;
    }
    Out& bsp(const unsigned int UI_Backspace) { unsigned int ui;
        for (ui=UI_Backspace; ui; ui--) Append("%b");
        for (ui=UI_Backspace; ui; ui--) Append(" ");
        for (ui=UI_Backspace; ui; ui--) Append("%b");
        return *this;
    }
    Out& del(const unsigned int UI_Delete) { int ui;
        for (ui=UI_Delete; ui; ui--) Append(" ");
        for (ui=UI_Delete; ui; ui--) Append("%b");
        return *this;
    }
    Out& beep(void) { Append("!BEEP!"); return *this; }
    Out& cls(void) { Append("!CLS!"); return *this; }
    Out& endl(void) { Append("\n"); return *this; }
};

class ShellGuard {
public: ShellGuard(cli::Shell& CLI_Shell, cli::IODevice& CLI_IODevice) : m_cliShell(CLI_Shell) { m_cliShell.Run(CLI_IODevice); }
public: ~ShellGuard(void) { m_cliShell.Quit(); }
private: cli::Shell& m_cliShell;
};

#endif // __CLI_TEST_H__
