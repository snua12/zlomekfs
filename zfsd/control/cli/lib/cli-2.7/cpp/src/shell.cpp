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

#include <stdio.h>
#include <string.h>

#include "cli/shell.h"
#include "cli/io_device.h"
#include "cli/cli.h"
#include "cli/menu.h"
#include "cli/command_line.h"
#include "cli/endl.h"
#include "cli/param.h"
#include "cli/traces.h"
#include "cli/assert.h"
#include "cli/debug.h"
#include "consistency.h"
#include "constraints.h"
#include "command_line_edition.h"
#include "command_line_history.h"

CLI_NS_USE(cli)


static const unsigned int HELP_MARGIN = 2;
static const unsigned int HELP_OFFSET = 15;
static const unsigned int HISTORY_PAGE = 5;
static const unsigned int HISTORY_STACK_SIZE = 100;

//! @brief Shell trace class singleton redirection.
#define TRACE_SHELL GetShellTraceClass()
//! @brief Shell trace class singleton.
static const TraceClass& GetShellTraceClass(void)
{
    static const TraceClass cli_ShellTraceClass("CLI_SHELL", Help()
        .AddHelp(Help::LANG_EN, "Shell traces")
        .AddHelp(Help::LANG_FR, "Traces du shell"));
    return cli_ShellTraceClass;
}


Shell::Shell(const Cli& CLI_Cli)
  : m_pcliCli(& CLI_Cli), m_pcliInput(NULL),
    m_eLang(Help::LANG_EN), m_bBeep(true),
    m_qMenus(MAX_MENU_PER_CLI),
    m_cliCmdLine(* new CmdLineEdition()),
    m_cliHistory(* new CmdLineHistory(HISTORY_STACK_SIZE)),
    m_eThreadSafeCmd(THREAD_SAFE_NONE)
{
    EnsureCommonDevices();
    EnsureTraces();

    // Members initialization.
    for (int i=0; i<STREAM_TYPES_COUNT; i++)
    {
        m_artStream[i].pcliStream = NULL;
        m_artStream[i].bEnable = true;
    }

    if (! m_qMenus.AddHead(& CLI_Cli))
    {
        GetTraces().Trace(INTERNAL_ERROR) << "Could not set initial shell status." << endl;
    }
    CLI_Cli.SetShell(*this);

    GetTraces().Trace(TRACE_SHELL) << "New shell for CLI '" << GetCli().GetKeyword() << "'." << endl;
}

Shell::~Shell(void)
{
    GetTraces().Trace(TRACE_SHELL) << "Shell deleted for CLI '" << GetCli().GetKeyword() << "'." << endl;

    if (m_pcliInput != NULL)
    {
        m_pcliInput->FreeInstance(__CALL_INFO__);
        m_pcliInput = NULL;
    }

    for (int i=0; i<STREAM_TYPES_COUNT; i++)
    {
        if (m_artStream[i].pcliStream != NULL)
        {
            m_artStream[i].pcliStream->FreeInstance(__CALL_INFO__);
            m_artStream[i].pcliStream = NULL;
        }
    }

    delete & m_cliCmdLine;
    delete & m_cliHistory;
}

const Cli& Shell::GetCli(void) const
{
    CLI_ASSERT(m_pcliCli != NULL);
    return *m_pcliCli;
}

const IODevice& Shell::GetInput(void) const
{
    if (m_pcliInput != NULL)
    {
        return *m_pcliInput;
    }
    else
    {
        return IODevice::GetStdIn();
    }
}

const OutputDevice& Shell::GetStream(const STREAM_TYPE E_StreamType) const
{
    CLI_ASSERT((E_StreamType >= 0) && (E_StreamType < STREAM_TYPES_COUNT));
    if ((E_StreamType >= 0) && (E_StreamType < STREAM_TYPES_COUNT))
    {
        if (m_artStream[E_StreamType].bEnable)
        {
            if (m_artStream[E_StreamType].pcliStream != NULL)
            {
                return *m_artStream[E_StreamType].pcliStream;
            }
        }
    }

    // Default to null device.
    return OutputDevice::GetNullDevice();
}

const bool Shell::SetStream(const STREAM_TYPE E_StreamType, OutputDevice& CLI_Stream)
{
    // ALL_STREAMS management.
    if (E_StreamType == ALL_STREAMS)
    {
        for (int i = 0; i < STREAM_TYPES_COUNT; i++)
        {
            if (! SetStream((STREAM_TYPE) i, CLI_Stream))
            {
                return false;
            }
        }

        return true;
    }
    if ((E_StreamType < 0) || (E_StreamType >= STREAM_TYPES_COUNT))
    {
        CLI_ASSERT(false);
        return false;
    }
    // End of ALL_STREAMS management.

    // Free previous reference.
    if (OutputDevice* const pcli_Stream = m_artStream[E_StreamType].pcliStream)
    {
        bool b_Res = true;

        // Unreference the device right now.
        m_artStream[E_StreamType].pcliStream = NULL;
        if (IsRunning())
        {
            if (! pcli_Stream->CloseDown(__CALL_INFO__))
            {
                PrintError(ResourceString(), pcli_Stream->GetLastError());
                b_Res = false;
            }
        }
        pcli_Stream->FreeInstance(__CALL_INFO__);

        if (! b_Res)
        {
            // Abort on error.
            return false;
        }
    }

    // Store next reference.
    {
        CLI_Stream.UseInstance(__CALL_INFO__);
        if (IsRunning())
        {
            if (! CLI_Stream.OpenUp(__CALL_INFO__))
            {
                // Store nothing on error.
                PrintError(ResourceString(), CLI_Stream.GetLastError());
                CLI_Stream.FreeInstance(__CALL_INFO__);
                return false;
            }
        }
        // Do not store the reference until opening is done.
        m_artStream[E_StreamType].pcliStream = & CLI_Stream;
    }

    return true;
}

const bool Shell::StreamEnabled(const STREAM_TYPE E_StreamType) const
{
    if ((E_StreamType < 0) || (E_StreamType >= STREAM_TYPES_COUNT))
    {
        CLI_ASSERT(false);
        return false;
    }
    else
    {
        return m_artStream[E_StreamType].bEnable;
    }
}

const bool Shell::EnableStream(const STREAM_TYPE E_StreamType, const bool B_Enable)
{
    // ALL_STREAMS management.
    if (E_StreamType == ALL_STREAMS)
    {
        for (int i = 0; i < STREAM_TYPES_COUNT; i++)
        {
            if (! EnableStream((STREAM_TYPE) i, B_Enable))
            {
                return false;
            }
        }

        return true;
    }
    if ((E_StreamType < 0) || (E_StreamType >= STREAM_TYPES_COUNT))
    {
        CLI_ASSERT(false);
        return false;
    }
    // End of ALL_STREAMS management.

    m_artStream[E_StreamType].bEnable = B_Enable;
    return true;
}

void Shell::SetWelcomeMessage(const ResourceString& CLI_WelcomeMessage)
{
    m_cliWelcomeMessage = CLI_WelcomeMessage;
}

void Shell::SetByeMessage(const ResourceString& CLI_ByeMessage)
{
    m_cliByeMessage = CLI_ByeMessage;
}

void Shell::SetPrompt(const ResourceString& CLI_Prompt)
{
    m_cliNoDefaultPrompt = CLI_Prompt;
}

void Shell::SetLang(const ResourceString::LANG E_Lang)
{
    m_eLang = E_Lang;
}

const ResourceString::LANG Shell::GetLang(void) const
{
    return m_eLang;
}

void Shell::SetBeep(const bool B_Enable)
{
    m_bBeep = B_Enable;
}

const bool Shell::GetBeep(void) const
{
    return m_bBeep;
}

void Shell::Run(IODevice& CLI_IODevice)
{
    if (StartExecution(CLI_IODevice))
    {
        if (NonBlockingIODevice* const pcli_NonBlockingIODevice = dynamic_cast<NonBlockingIODevice*>(m_pcliInput))
        {
            // Simply attach the shell and wait for characters to be input.
            pcli_NonBlockingIODevice->AttachKeyReceiver(*this);
        }
        else
        {
            MainLoop();
            FinishExecution();
        }
    }
    else
    {
        FinishExecution();
    }
}

const bool Shell::StartExecution(IODevice& CLI_IODevice)
{
    GetTraces().Trace(TRACE_SHELL) << "Shell for CLI '" << GetCli().GetKeyword() << "' starts running." << endl;
    // Open devices.
    if (OpenDevices(CLI_IODevice))
    {
        // Check the CLI is the first menu.
        if ((m_pcliCli != NULL) && (m_qMenus.GetHead() == m_pcliCli))
        {
            // Enter the shell.
            PromptWelcomeMessage();
            PromptMenu();

            return true;
        }
    }

    return false;
}

void Shell::MainLoop(void)
{
    // While there are menus in the menu stack, it means we are still waiting for command lines.
    while (! m_qMenus.IsEmpty())
    {
        bool b_QuitShell = true;

        if ((m_eThreadSafeCmd != THREAD_SAFE_QUIT)      // Thread safe processing.
            && (m_pcliInput != NULL))
        {
            // Get an input key.
            const KEY e_Key = m_pcliInput->GetKey();

            if ((m_eThreadSafeCmd != THREAD_SAFE_QUIT)  // Thread safe processing.
                && (e_Key != NULL_KEY))                 // End of input.
            {
                // Process the input key.
                OnKey(e_Key);
                b_QuitShell = false;
            }
        }

        if (b_QuitShell)
        {
            Quit();
        }
    }
}

void Shell::OnNonBlockingKey(NonBlockingIODevice& CLI_Source, const KEY E_KeyCode)
{
    bool b_QuitShell = true;

    if ((! m_qMenus.IsEmpty())                      // Menu stack control.
                                                    // If there are menus in the menu stack,
                                                    // it means we are still waiting for command lines.
        && (m_eThreadSafeCmd != THREAD_SAFE_QUIT)   // Thread safe processing (should be useless in non-blocking execution mode).
        && (E_KeyCode != NULL_KEY)                  // End of input.
        && (& CLI_Source == m_pcliInput))           // Input source control.
    {
        // Process the input key.
        OnKey(E_KeyCode);
        b_QuitShell = false;
    }

    // Possibly quit the shell depending on the input.
    if (b_QuitShell)
    {
        // Let's quit the shell.
        Quit();
    }

    // Eventually finish execution if needed.
    if (m_qMenus.IsEmpty()                          // Menu stack control.
                                                    // If there are no menus left in the menu stack,
                                                    // it means the thing is done.
        || (m_eThreadSafeCmd == THREAD_SAFE_QUIT))  // Thread safe processing (should be useless in non-blocking execution mode).
    {
        // Finish execution.
        FinishExecution();
    }
}

const bool Shell::FinishExecution(void)
{
    bool b_Res = true;

    if (NonBlockingIODevice* const pcli_NonBlockingInput = dynamic_cast<NonBlockingIODevice*>(m_pcliInput))
    {
        // Detach the shell of the non-blocking device.
        pcli_NonBlockingInput->DetachKeyReceiver(*this);
    }

    // Leave the shell.
    PromptByeMessage();

    // Restore the CLI reference.
    if (! m_qMenus.AddTail(m_pcliCli))
    {
        GetTraces().Trace(INTERNAL_ERROR) << "Could not restore a valid status on shell exit." << endl;
        b_Res = false;
    }

    // Close devices.
    if (! CloseDevices())
    {
        b_Res = false;
    }
    GetTraces().Trace(TRACE_SHELL) << "Shell for CLI '" << GetCli().GetKeyword() << "' end of run." << endl;

    return b_Res;
}

const bool Shell::IsRunning(void) const
{
    return (m_pcliInput != NULL);
}

const unsigned int Shell::GetHelpMargin(void) const
{
    return HELP_MARGIN;
}

const unsigned int Shell::GetHelpOffset(void) const
{
    return HELP_OFFSET;
}

const bool Shell::OpenDevices(IODevice& CLI_IODevice)
{
    GetTraces().Trace(TRACE_SHELL) << "Shell for CLI '" << GetCli().GetName() << "' opening devices." << endl;

    bool b_Res = true;

    // First of all, set references.
    {
        // Input.
        if (m_pcliInput != NULL)
        {
            m_pcliInput->FreeInstance(__CALL_INFO__);
            m_pcliInput = NULL;
        }
        m_pcliInput = & CLI_IODevice;
        m_pcliInput->UseInstance(__CALL_INFO__);

        // Output.
        for (int i=0; i<STREAM_TYPES_COUNT; i++)
        {
            if (m_artStream[i].pcliStream == NULL)
            {
                m_artStream[i].pcliStream = & CLI_IODevice;
                m_artStream[i].pcliStream->UseInstance(__CALL_INFO__);
            }
            //! @note Enables all output streams by default.
            m_artStream[i].bEnable = true;
        }
    }

    // Then open up devices.
    {
        // Input.
        if (m_pcliInput == NULL)
        {
            CLI_ASSERT(false);
            b_Res = false;
        }
        else if (! m_pcliInput->OpenUp(__CALL_INFO__))
        {
            PrintError(ResourceString(), m_pcliInput->GetLastError());
            b_Res = false;
        }

        // Output.
        for (int i=0; i<STREAM_TYPES_COUNT; i++)
        {
            if (m_artStream[i].pcliStream == NULL)
            {
                CLI_ASSERT(false);
                b_Res = false;
            }
            else if (! m_artStream[i].pcliStream->OpenUp(__CALL_INFO__))
            {
                PrintError(ResourceString(), m_artStream[i].pcliStream->GetLastError());
                b_Res = false;
            }
        }
    }

    // Trace device management.
    if (! GetTraces().SetStream(const_cast<OutputDevice&>(GetStream(ERROR_STREAM))))
    {
        b_Res = false;
    }

    return b_Res;
}

const bool Shell::CloseDevices(void)
{
    GetTraces().Trace(TRACE_SHELL) << "Shell for CLI '" << GetCli().GetKeyword() << "' closing devices." << endl;

    bool b_Res = true;

    // Trace device management.
    if (! GetTraces().UnsetStream(const_cast<OutputDevice&>(GetStream(ERROR_STREAM))))
    {
        b_Res = false;
    }

    // Close down devices.
    {
        // Output.
        for (int i=0; i<STREAM_TYPES_COUNT; i++)
        {
            if (m_artStream[i].pcliStream == NULL)
            {
                CLI_ASSERT(false);
                b_Res = false;
            }
            else if (! m_artStream[i].pcliStream->CloseDown(__CALL_INFO__))
            {
                PrintError(ResourceString(), m_artStream[i].pcliStream->GetLastError());
                b_Res = false;
            }
        }

        // Input device.
        if (m_pcliInput == NULL)
        {
            CLI_ASSERT(false);
            b_Res = false;
        }
        else if (! m_pcliInput->CloseDown(__CALL_INFO__))
        {
            PrintError(ResourceString(), m_pcliInput->GetLastError());
            b_Res = false;
        }
    }

    // Unset references.
    {
        // Output.
        for (int i=0; i<STREAM_TYPES_COUNT; i++)
        {
            if (m_artStream[i].pcliStream == m_pcliInput)
            {
                m_artStream[i].pcliStream->FreeInstance(__CALL_INFO__);
                m_artStream[i].pcliStream = NULL;
            }
        }

        // Input.
        if (m_pcliInput != NULL)
        {
            m_pcliInput->FreeInstance(__CALL_INFO__);
            m_pcliInput = NULL;
        }
    }

    return b_Res;
}

void Shell::PromptWelcomeMessage(void) const
{
    const tk::String str_WelcomeMessage = m_cliWelcomeMessage.GetString(GetLang());
    if (! str_WelcomeMessage.IsEmpty())
    {
        GetStream(WELCOME_STREAM) << str_WelcomeMessage;
    }
    else
    {
        GetStream(WELCOME_STREAM)
        << "---------------------------------------------------" << endl
        << " Welcome to " << GetCli().GetKeyword() << "!" << endl
        << endl
        << " " << GetCli().GetKeyword() << " is a command line interface" << endl
        << " using the CLI library" << endl
        << "   (c) Alexis Royer http://alexis.royer.free.fr/CLI/" << endl
        << " Type 'help' at any time" << endl
        << " or press '?' or TAB to get completion or help." << endl
        << "---------------------------------------------------" << endl;
    }
}

void Shell::PromptByeMessage(void) const
{
    const tk::String str_ByeMessage = m_cliByeMessage.GetString(GetLang());
    if (! str_ByeMessage.IsEmpty())
    {
        GetStream(WELCOME_STREAM) << str_ByeMessage;
    }
    else
    {
        GetStream(WELCOME_STREAM) << "Bye!" << endl;
    }
}

void Shell::PromptMenu(void) const
{
    // Show the prompt.
    if (! m_qMenus.IsEmpty())
    {
        const tk::String str_NoDefaultPrompt = m_cliNoDefaultPrompt.GetString(GetLang());
        if (! str_NoDefaultPrompt.IsEmpty())
        {
            GetStream(PROMPT_STREAM) << str_NoDefaultPrompt;
        }
        else
        {
            if (const Menu* const pcli_Menu = m_qMenus.GetTail())
            {
                const tk::String cli_Prompt = pcli_Menu->OnPrompt();
                if (! cli_Prompt.IsEmpty())
                {
                    GetStream(PROMPT_STREAM) << cli_Prompt << ">";
                }
                else
                {
                    GetStream(PROMPT_STREAM) << pcli_Menu->GetKeyword() << ">";
                }
            }
        }

        // Eventually echo the current line.
        m_cliCmdLine.PrintCmdLine(GetStream(ECHO_STREAM));
    }
}

void Shell::PrintError(const ResourceString& CLI_Location, const ResourceString& CLI_ErrorMessage) const
{
    // First of all, call the CLI handler.
    if (GetCli().OnError(CLI_Location, CLI_ErrorMessage))
    {
        // Print out the error.
        const tk::String str_Location = CLI_Location.GetString(GetLang());
        const tk::String str_ErrorMessage = CLI_ErrorMessage.GetString(GetLang());
        GetStream(ERROR_STREAM) << str_Location << (str_Location.IsEmpty() ? "" : ": ") << str_ErrorMessage << endl;
    }
}

const Menu* const Shell::GetCurrentMenu(const int I_MenuIndex) const
{
    if (I_MenuIndex < 0)
    {
        return m_qMenus.GetTail();
    }
    else
    {
        int i_MenuIndex = 0;
        for (   tk::Queue<const Menu*>::Iterator it = m_qMenus.GetIterator();
                m_qMenus.IsValid(it);
                m_qMenus.MoveNext(it))
        {
            if (i_MenuIndex == I_MenuIndex)
            {
                return m_qMenus.GetAt(it);
            }
            i_MenuIndex ++;
        }

        return NULL;
    }
}

void Shell::ExitMenu(const bool B_PromptMenu)
{
    const tk::String tk_Left = m_cliCmdLine.GetLeft(), tk_Right = m_cliCmdLine.GetRight();
    m_cliCmdLine.Reset();
    OnExit(B_PromptMenu);
    m_cliCmdLine.Set(tk_Left, tk_Right);
}

void Shell::Quit(void)
{
    // Remove all menus of the menu stack to let the shell quit.
    while (! m_qMenus.IsEmpty())
    {
        ExitMenu(false);
    }

    if (dynamic_cast<NonBlockingIODevice*>(m_pcliInput))
    {
        FinishExecution();
    }
}

void Shell::QuitThreadSafe(void)
{
    m_eThreadSafeCmd = THREAD_SAFE_QUIT;
}

void Shell::DisplayHelp(void)
{
    if (! m_qMenus.IsEmpty())
    {
        const tk::String tk_Left = m_cliCmdLine.GetLeft(), tk_Right = m_cliCmdLine.GetRight();
        m_cliCmdLine.Reset();
        OnHelp(
            false,  // Execution within the context of a command line processing. OnExecute() does already prompt the menu.
            false   // Just help, no completion
        );
        m_cliCmdLine.Set(tk_Left, tk_Right);
    }
}

void Shell::PrintWorkingMenu(void)
{
    unsigned int i = 0;
    for (i=0; i<HELP_MARGIN; i++)
    {
        GetStream(OUTPUT_STREAM) << " ";
    }
    for (   tk::Queue<const Menu*>::Iterator it = m_qMenus.GetIterator();
            m_qMenus.IsValid(it);
            m_qMenus.MoveNext(it))
    {
        if (const Menu* const pcli_Menu = m_qMenus.GetAt(it))
        {
            const tk::String cli_Prompt = pcli_Menu->OnPrompt();
            if (! cli_Prompt.IsEmpty())
            {
                GetStream(OUTPUT_STREAM) << "/" << cli_Prompt;
            }
            else
            {
                GetStream(OUTPUT_STREAM) << "/" << pcli_Menu->GetKeyword();
            }
        }
    }
    GetStream(OUTPUT_STREAM) << endl;
}

void Shell::CleanScreen(const bool B_PromptMenu)
{
    m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));
    m_cliCmdLine.Reset();
    GetStream(OUTPUT_STREAM).CleanScreen();
    if (B_PromptMenu)
    {
        PromptMenu();
    }
}

void Shell::Beep(void)
{
    if (m_bBeep)
    {
        GetStream(ERROR_STREAM).Beep();
    }
}

void Shell::EnterMenu(const Menu& CLI_Menu, const bool B_PromptMenu)
{
    if (B_PromptMenu)
    {
        m_cliCmdLine.NextLine(GetStream(ECHO_STREAM)); // Same call as OnExit().
    }
    if (! m_qMenus.AddTail(& CLI_Menu))
    {
        ResourceString cli_Error = ResourceString()
            .SetString(ResourceString::LANG_EN, tk::String::Concat(MAX_RESOURCE_LENGTH, "Too many menus. Cannot enter '", CLI_Menu.GetName(), "'."))
            .SetString(ResourceString::LANG_FR, tk::String::Concat(MAX_RESOURCE_LENGTH, "Trop de menus. Impossible d'entrer dans le menu '", CLI_Menu.GetName(), "'."));
        PrintError(
            (m_pcliInput != NULL) ? m_pcliInput->GetLocation() : ResourceString(),
            cli_Error
        );
    }
    if (B_PromptMenu)
    {
        PromptMenu();
    }
}

void Shell::OnKey(const KEY E_KeyCode)
{
    // In general, history navigation is lost.
    // Except for navigation keys, in history, and within the line.
    switch (E_KeyCode)
    {
    case KEY_UP: case KEY_DOWN: case PAGE_UP: case PAGE_DOWN:
    case KEY_BEGIN: case KEY_END: case KEY_LEFT: case KEY_RIGHT:
        break;
    default:
        m_cliHistory.EnableNavigationMemory(false);
        break;
    }

    switch (E_KeyCode)
    {
    case KEY_UP:    OnHistory(1);                           break;
    case KEY_DOWN:  OnHistory(-1);                          break;
    case PAGE_UP:   OnHistory(HISTORY_PAGE);                break;
    case PAGE_DOWN: OnHistory(- (signed int) HISTORY_PAGE); break;
    case KEY_BEGIN: OnKeyBegin();                           break;
    case KEY_END:   OnKeyEnd();                             break;
    case KEY_LEFT:  OnKeyLeft();                            break;
    case KEY_RIGHT: OnKeyRight();                           break;
    case BACKSPACE: OnBackspace();                          break;
    case DELETE:    OnSuppr();                              break;
    case ENTER:     OnExecute();                            break;
    case BREAK:
    case ESCAPE:    OnEscape();                             break;
    case LOGOUT:    OnExit(true);                           break;
    case TAB:       OnHelp(true, true);                     break;
    case QUESTION:
        if (m_cliCmdLine.GetLine().GetChar(m_cliCmdLine.GetLine().GetLength() - 1) == '\\')
            OnPrintableChar(E_KeyCode);
        else
            OnHelp(true, false);
        break;
    case CLS:       CleanScreen(true);                      break;

    case KEY_a: case KEY_aacute: case KEY_agrave: case KEY_auml: case KEY_acirc:
    case KEY_b: case KEY_c: case KEY_ccedil: case KEY_d:
    case KEY_e: case KEY_eacute: case KEY_egrave: case KEY_euml: case KEY_ecirc:
    case KEY_f: case KEY_g: case KEY_h:
    case KEY_i: case KEY_iacute: case KEY_igrave: case KEY_iuml: case KEY_icirc:
    case KEY_j: case KEY_k: case KEY_l: case KEY_m: case KEY_n:
    case KEY_o: case KEY_oacute: case KEY_ograve: case KEY_ouml: case KEY_ocirc:
    case KEY_p: case KEY_q: case KEY_r: case KEY_s: case KEY_t:
    case KEY_u: case KEY_uacute: case KEY_ugrave: case KEY_uuml: case KEY_ucirc:
    case KEY_v: case KEY_w: case KEY_x: case KEY_y: case KEY_z:

    case KEY_A: case KEY_B: case KEY_C: case KEY_D: case KEY_E: case KEY_F:
    case KEY_G: case KEY_H: case KEY_I: case KEY_J: case KEY_K: case KEY_L:
    case KEY_M: case KEY_N: case KEY_O: case KEY_P: case KEY_Q: case KEY_R:
    case KEY_S: case KEY_T: case KEY_U: case KEY_V: case KEY_W: case KEY_X:
    case KEY_Y: case KEY_Z:

    case KEY_0: case KEY_1: case KEY_2: case KEY_3: case KEY_4: case KEY_5:
    case KEY_6: case KEY_7: case KEY_8: case KEY_9:

    case PLUS:
    case MINUS:
    case STAR:
    case SLASH:
    case LOWER_THAN:
    case GREATER_THAN:
    case EQUAL:
    case PERCENT:

    case SPACE:
    case UNDERSCORE:
    case AROBASE:
    case SHARP:
    case AMPERCENT:
    case DOLLAR:
    case BACKSLASH:
    case PIPE:
    case TILDE:
    case SQUARE:
    case EURO:
    case POUND:
    case MICRO:
    case PARAGRAPH:

    case EXCLAMATION:
    case COLUMN:
    case DOT:
    case COMA:
    case SEMI_COLUMN:
    case QUOTE:
    case DOUBLE_QUOTE:

    case OPENING_BRACE:
    case CLOSING_BRACE:
    case OPENING_CURLY_BRACE:
    case CLOSING_CURLY_BRACE:
    case OPENING_BRACKET:
    case CLOSING_BRACKET:
        OnPrintableChar(E_KeyCode);
        break;

    default:
        // Non managed character. Just ignore.
        break;
    }
}

void Shell::OnPrintableChar(const char C_KeyCode)
{
    m_cliCmdLine.Put(GetStream(ECHO_STREAM), C_KeyCode);
}

void Shell::OnKeyBegin(void)
{
    m_cliCmdLine.Home(GetStream(ECHO_STREAM));
}

void Shell::OnKeyEnd(void)
{
    m_cliCmdLine.End(GetStream(ECHO_STREAM));
}

void Shell::OnKeyLeft(void)
{
    if (! m_cliCmdLine.GetLeft().IsEmpty())
    {
        m_cliCmdLine.MoveCursor(GetStream(ECHO_STREAM), -1);
    }
    else
    {
        Beep();
    }
}

void Shell::OnKeyRight(void)
{
    if (! m_cliCmdLine.GetRight().IsEmpty())
    {
        m_cliCmdLine.MoveCursor(GetStream(ECHO_STREAM), 1);
    }
    else
    {
        Beep();
    }
}

void Shell::OnBackspace(void)
{
    if (! m_cliCmdLine.GetLeft().IsEmpty())
    {
        m_cliCmdLine.Delete(GetStream(ECHO_STREAM), -1);
    }
    else
    {
        Beep();
    }
}

void Shell::OnSuppr(void)
{
    if (! m_cliCmdLine.GetRight().IsEmpty())
    {
        m_cliCmdLine.Delete(GetStream(ECHO_STREAM), 1);
    }
    else
    {
        Beep();
    }
}

void Shell::OnEscape(void)
{
    m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));
    m_cliCmdLine.Reset();
    PromptMenu();
}

void Shell::OnExit(const bool B_PromptMenu)
{
    // Reset the current line if needed.
    if (B_PromptMenu)
    {
        m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));
    }

    m_cliCmdLine.Reset();

    if (const Menu* const pcli_Menu = m_qMenus.RemoveTail())
    {
        pcli_Menu->OnExit();
    }
    if (B_PromptMenu)
    {
        PromptMenu();
    }
}

void Shell::OnHelp(const bool B_PromptMenu, const bool B_Completion)
{
    if (! m_qMenus.IsEmpty())
    {
        // Parse the command line.
        CommandLine cli_CommandLine;
        if (cli_CommandLine.Parse(*m_qMenus.GetTail(), m_cliCmdLine.GetLeft(), false))
        {
            // Research matching elements.
            Element::List cli_Exact(MAX_WORDS_PER_NODE), cli_Elements(MAX_WORDS_PER_NODE);
            if (! cli_CommandLine.GetLastElement().FindElements(
                    cli_Exact, cli_Elements, cli_CommandLine.GetLastWord()))
            {
                return;
            }

            // Initialize completion so far.
            tk::String str_CompletionSoFar(MAX_WORD_LENGTH);
            // Determine whether help should be printed, and completion activated.
            bool b_PrintHelpList = false;

            if (! B_Completion)
            {
                // Just print help.
                b_PrintHelpList = true;
            }
            else
            {
                // Print help and do completion depending on the possible nodes.
                if (cli_Elements.IsEmpty())
                {
                    // No matching element.
                    Beep();
                }
                else if ((cli_Elements.GetCount() == 1)
                        && (! dynamic_cast<const Endl*>(cli_Elements.GetHead()))
                        && (! dynamic_cast<const Param*>(cli_Elements.GetHead())))
                {
                    // Straight forward completion.
                    m_cliCmdLine.Delete(GetStream(ECHO_STREAM), - cli_CommandLine.GetNumBackspacesForCompletion());
                    m_cliCmdLine.Put(GetStream(ECHO_STREAM), cli_Elements.GetHead()->GetKeyword());
                    m_cliCmdLine.Put(GetStream(ECHO_STREAM), ' ');
                }
                else
                {
                    // Look for completion so far.
                    bool b_InitCompletionSoFar = true;
                    for (   Element::List::Iterator it = cli_Elements.GetIterator();
                            cli_Elements.IsValid(it);
                            cli_Elements.MoveNext(it))
                    {
                        if (dynamic_cast<const Param*>(cli_Elements.GetAt(it))
                            || dynamic_cast<const Endl*>(cli_Elements.GetAt(it)))
                        {
                            // No completion for parameters and end of lines.
                            str_CompletionSoFar.Set("");
                            break;
                        }
                        else if (const Element* const pcli_Element = dynamic_cast<const Element*>(cli_Elements.GetAt(it)))
                        {
                            if (b_InitCompletionSoFar)
                            {
                                // First initialization of str_CompletionSoFar.
                                if (! str_CompletionSoFar.Set(pcli_Element->GetKeyword()))
                                    GetTraces().Trace(INTERNAL_ERROR) << "Shell::OnHelp(): Could not store completion so far" << endl;
                                b_InitCompletionSoFar = false;
                            }
                            else
                            {
                                // Remove last character of str_CompletionSoFar till it matches the current keyword.
                                while ( (! str_CompletionSoFar.IsEmpty())
                                        && (pcli_Element->GetKeyword().SubString(0, str_CompletionSoFar.GetLength()) != str_CompletionSoFar))
                                {
                                    if (! str_CompletionSoFar.Set(str_CompletionSoFar.SubString(0, str_CompletionSoFar.GetLength() - 1)))
                                        GetTraces().Trace(INTERNAL_ERROR) << "Shell::OnHelp(): Could not store completion so far" << endl;
                                }
                            }
                        }
                    }

                    // When no more completion so far available, print help.
                    if ((cli_CommandLine.GetLastWord() == NULL)
                        || (str_CompletionSoFar == cli_CommandLine.GetLastWord()))
                    {
                        b_PrintHelpList = true;
                    }
                }
            }

            // Help display.
            if (b_PrintHelpList)
            {
                // First of all, sort help elements on keywords.
                class _ { public:
                    static const int cmp(const Element* const& PCLI_1, const Element* const& PCLI_2)
                    {
                        if (PCLI_1->GetKeyword() < PCLI_2->GetKeyword())
                            return 1;
                        if (PCLI_1->GetKeyword() > PCLI_2->GetKeyword())
                            return -1;
                        return 0;
                    }
                };
                cli_Elements.Sort(_::cmp);

                if (B_PromptMenu)
                {
                    m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));
                }

                // For each matching keyword...
                for (   Element::List::Iterator it = cli_Elements.GetIterator();
                        cli_Elements.IsValid(it);
                        cli_Elements.MoveNext(it))
                {
                    // Print the corresponding help.
                    PrintHelp(*cli_Elements.GetAt(it));
                }

                if (B_PromptMenu)
                {
                    PromptMenu();
                }
            }
            // Completion so far.
            else if (B_Completion && (! str_CompletionSoFar.IsEmpty()))
            {
                m_cliCmdLine.Delete(GetStream(ECHO_STREAM), - cli_CommandLine.GetNumBackspacesForCompletion());
                m_cliCmdLine.Put(GetStream(ECHO_STREAM), str_CompletionSoFar);
            }
        }
        else
        {
            if (B_PromptMenu)
            {
                m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));
            }
            PrintError(
                (m_pcliInput != NULL) ? m_pcliInput->GetLocation() : ResourceString(),
                cli_CommandLine.GetLastError()
            );
            if (B_PromptMenu)
            {
                PromptMenu();
            }
        }
    }
}

void Shell::OnExecute(void)
{
    m_cliCmdLine.NextLine(GetStream(ECHO_STREAM));

    if (! m_qMenus.IsEmpty())
    {
        // First of all, store the line in history stack.
        m_cliHistory.Push(m_cliCmdLine);

        // Append "\n" to the line and parse it.
        tk::String str_Line(MAX_CMD_LINE_LENGTH + 1);
        CommandLine cli_CommandLine;
        if (str_Line.Set(m_cliCmdLine.GetLine()) && str_Line.Append("\n")
            && cli_CommandLine.Parse(*m_qMenus.GetTail(), str_Line, true))
        {
            bool b_Executed = false;
            // No need to execute unless there are at least one word and one endl elements in the command line.
            CommandLineIterator it(cli_CommandLine);
            if (it.StepIt() && it.StepIt())
            {
                // Command line execution.
                if (const Menu* const pcli_Menu = m_qMenus.GetTail())
                {
                    if (pcli_Menu->ExecuteReserved(cli_CommandLine))
                        b_Executed = true;
                    if (pcli_Menu->Execute(cli_CommandLine))
                        b_Executed = true;
                }

                // Post-execution.
                if (b_Executed)
                {
                    // Entering a sub-menu.
                    if (const Endl* pcli_Endl = dynamic_cast<const Endl*>(& cli_CommandLine.GetLastElement()))
                    {
                        if (pcli_Endl->GetMenuRef() != NULL)
                        {
                            EnterMenu(pcli_Endl->GetMenuRef()->GetMenu(), false);
                        }
                    }
                }
                else
                {
                    const ResourceString cli_ExecutionError = ResourceString()
                        .SetString(ResourceString::LANG_EN, "Execution error")
                        .SetString(ResourceString::LANG_FR, "Erreur d'exécution");
                    PrintError(
                        (m_pcliInput != NULL) ? m_pcliInput->GetLocation() : ResourceString(),
                        cli_ExecutionError
                    );
                }
            }
        }
        else
        {
            PrintError(
                (m_pcliInput != NULL) ? m_pcliInput->GetLocation() : ResourceString(),
                cli_CommandLine.GetLastError()
            );
        }
        m_cliCmdLine.Reset();
        PromptMenu();
    }
}

void Shell::OnHistory(const int I_Navigation)
{
    if (! m_cliHistory.Navigate(m_cliCmdLine, GetStream(ECHO_STREAM), I_Navigation))
    {
        // Out of stack.
        Beep();
    }
}

void Shell::PrintHelp(const Element& CLI_Element)
{
    // Retrieve basic information.
    tk::String str_Keyword = CLI_Element.GetKeyword();
    tk::String str_Help = CLI_Element.GetHelp().GetString(GetLang());

    // Optional corrections.
    if (dynamic_cast<const Endl*>(& CLI_Element))
    {
        str_Keyword.Set("<cr>");
    }
    else if (const Param* const pcli_Param = dynamic_cast<const Param*>(& CLI_Element))
    {
        if (const Param* const pcli_Clone = pcli_Param->Clone())
        {
            pcli_Clone->SetstrValue("");
            if (! str_Keyword.Set(pcli_Clone->GetKeyword()))
                GetTraces().Trace(INTERNAL_ERROR) << "Shell::PrintHelp(): Could not review str_Keyword for the parameter" << endl;
            if (! pcli_Param->GetstrValue().IsEmpty())
            {
                if (! str_Keyword.Append("("))
                    GetTraces().Trace(INTERNAL_ERROR) << "Shell::PrintHelp(): Could not review str_Keyword for the parameter" << endl;
                if (! str_Keyword.Append(pcli_Param->GetstrValue()))
                    GetTraces().Trace(INTERNAL_ERROR) << "Shell::PrintHelp(): Could not review str_Keyword for the parameter" << endl;
                if (! str_Keyword.Append(")"))
                    GetTraces().Trace(INTERNAL_ERROR) << "Shell::PrintHelp(): Could not review str_Keyword for the parameter" << endl;
            }
            delete pcli_Clone;
        }
    }

    // Display.
    unsigned int i = 0;
    for (i = 0; i < HELP_MARGIN; i++)
    {
        GetStream(OUTPUT_STREAM) << " ";
    }
    GetStream(OUTPUT_STREAM) << str_Keyword << " ";
    for (i = str_Keyword.GetLength(); i < HELP_OFFSET; i++)
    {
        GetStream(OUTPUT_STREAM) << " ";
    }
    GetStream(OUTPUT_STREAM) << str_Help;
    GetStream(OUTPUT_STREAM) << endl;
}

