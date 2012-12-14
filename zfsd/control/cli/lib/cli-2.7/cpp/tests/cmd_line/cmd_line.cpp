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

#include <iostream>
#include <stdarg.h>

#include "cli/io_device.h"
#include "cli/string_device.h"
#include "cli/menu.h"
#include "cli/cli.h"

#include "cli/command_line.h"
#include "command_line_edition.h"
#include "command_line_history.h"


// Forward declarations.
    // CommandLine
static const bool TestCmdLineAnalysis(void);
    // CmdLineEdition
static const bool CheckAnalysis(
    const char* const STR_FileName, const unsigned int UI_Line,
    const char* const STR_CmdLine, const bool B_Res,
    const char* const STR_Error);
static const bool TestCmdLineEdition(void);
static const bool CheckEdition(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineEdition& CLI_CmdLine, const char* const STR_Left, const char* const STR_Right,
    cli::StringDevice& CLI_Out, const char* const STR_Out);
static const bool CheckEditionWords(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineEdition& CLI_CmdLine, const char* const STR_Left, const char* const STR_Right);
    // CmdLineHistory
static const bool TestCmdLineHistory(void);
static const bool CheckHistory(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineHistory& CLI_History, const unsigned int UI_Count, ...);
static const bool CheckNavigation(
    const char* const STR_FileName, const unsigned int UI_Line,
    cli::CmdLineHistory& CLI_History, const int I_Navigation,
    const bool B_Res, const cli::CmdLineEdition& CLI_Line);
static void CmdLineError(
    const char* const STR_FileName, const unsigned int UI_Line);


int main(void)
{
    if (! TestCmdLineAnalysis())
    {
        return -1;
    }

    if (! TestCmdLineEdition())
    {
        return -1;
    }

    if (! TestCmdLineHistory())
    {
        return -1;
    }

    return 0;
}

//! @brief CommandLine unit test.
static const bool TestCmdLineAnalysis(void)
{
    // Regular behaviour
    if (! CheckAnalysis(__FILE__, __LINE__, "a", false, "Syntax error next to 'a'"))
        return false;
    if (! CheckAnalysis(__FILE__, __LINE__, "help\n", true, ""))
        return false;
    // Too long command line: CLI_MAX_CMD_LINE_LENGTH = 256 // Not managed by the CommandLine class but by the CommandLineEdition class.
    //  if (! CheckAnalysis(__FILE__, __LINE__, "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdefXXX",
    //                                           false, "?"))
    //      return false;
    // Too many words: CLI_MAX_CMD_LINE_WORD_COUNT = 32
    if (! CheckAnalysis(__FILE__, __LINE__, "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30 31 32 33",
                                            false, "Too many words in command line"))
        return false;
    // Too long word: CLI_MAX_WORD_LENGTH = 16
    if (! CheckAnalysis(__FILE__, __LINE__, "a 0123456789abcdefX",
                                            false, "Too long word '0123456789...'"))
        return false;

    return true;
}

static const bool CheckAnalysis(
    const char* const STR_FileName, const unsigned int UI_Line,
    const char* const STR_CmdLine, const bool B_Res,
    const char* const STR_Error)
{
    if ((STR_CmdLine != NULL) && (STR_Error != NULL))
    {
        cli::CommandLine cli_CmdLine;
        cli::Menu cli_Menu("test", cli::Help());
        class my_Cli : public cli::Cli {
        public:
            my_Cli(const char* const STR_Name, const cli::Help& CLI_Help) : cli::Cli(STR_Name, CLI_Help) {}
        } cli_Cli("test", cli::Help());
        cli_Menu.SetCli(cli_Cli);

        const bool b_Res = cli_CmdLine.Parse(cli_Menu, cli::tk::String((unsigned int) strlen(STR_CmdLine), STR_CmdLine), true);
        if (b_Res != B_Res)
        {
            CmdLineError(STR_FileName, UI_Line);
            std::cerr << "Incorrect result" << std::endl;
            std::cerr << cli_CmdLine.GetLastError().GetString(cli::ResourceString::LANG_EN) << std::endl;
            return false;
        }

        if (cli_CmdLine.GetLastError().GetString(cli::ResourceString::LANG_EN) != STR_Error)
        {
            CmdLineError(STR_FileName, UI_Line);
            std::cerr
                << "Unexpected error '" << cli_CmdLine.GetLastError().GetString(cli::ResourceString::LANG_EN) << "' "
                << "instead of '" << STR_Error << "'" << std::endl;
            return false;
        }

        return true;
    }

    return false;
}

//! @brief CmdLineEdition unit test.
static const bool TestCmdLineEdition(void)
{
    cli::CmdLineEdition cli_CmdLine;
    cli::StringDevice cli_Out(256, false);

    // Append the command line.
    cli_CmdLine.Put(cli_Out, '0');
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0", "", cli_Out, "0")) return false;
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "1"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "1")) return false;

    // Move the cursor within the command line.
    cli_CmdLine.Home(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "01", cli_Out, "\b\b")) return false;
    cli_CmdLine.MoveCursor(cli_Out, 1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0", "1", cli_Out, "0")) return false;
    cli_CmdLine.MoveCursor(cli_Out, 10);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "1")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0", "1", cli_Out, "\b")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -2);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "01", cli_Out, "\b")) return false;
    cli_CmdLine.End(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "01")) return false;
    // Next Line.
    cli_CmdLine.Home(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "01", cli_Out, "\b\b")) return false;
    cli_CmdLine.NextLine(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "01", cli_Out, "01\n")) return false;
    cli_CmdLine.MoveCursor(cli_Out, 1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0", "1", cli_Out, "0")) return false;
    cli_CmdLine.NextLine(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0", "1", cli_Out, "1\n")) return false;
    cli_CmdLine.End(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "1")) return false;
    cli_CmdLine.NextLine(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "\n")) return false;

    // Insert characters.
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "89"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0189", "", cli_Out, "89")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -2);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "89", cli_Out, "\b\b")) return false;
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "234567"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01234567", "89", cli_Out, "23456789\b\b")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -3);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01234", "56789", cli_Out, "\b\b\b")) return false;

    // Delete characters.
    cli_CmdLine.Delete(cli_Out, 1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01234", "6789", cli_Out, "6789 \b\b\b\b\b")) return false;
    cli_CmdLine.Delete(cli_Out, -1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0123", "6789", cli_Out, "\b6789 \b\b\b\b\b")) return false;
    cli_CmdLine.Delete(cli_Out, 2);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "0123", "89", cli_Out, "89  \b\b\b\b")) return false;
    cli_CmdLine.Delete(cli_Out, -2);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "89", cli_Out, "\b\b89  \b\b\b\b")) return false;
    cli_CmdLine.Delete(cli_Out, 3);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "01", "", cli_Out, "  \b\b")) return false;
    cli_CmdLine.Delete(cli_Out, -3);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "", cli_Out, "\b\b  \b\b")) return false;
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "abcdef"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abcdef", "", cli_Out, "abcdef")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abcde", "f", cli_Out, "\b")) return false;
    cli_CmdLine.CleanAll(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "", cli_Out, " \b\b\b\b\b\b     \b\b\b\b\b")) return false;

    // Insert mode.
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "abcde"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abcde", "", cli_Out, "abcde")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -2);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abc", "de", cli_Out, "\b\b")) return false;
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "01"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abc01", "de", cli_Out, "01de\b\b")) return false;
    cli_CmdLine.SetInsertMode(false);
    cli_CmdLine.Put(cli_Out, '2');
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abc012", "e", cli_Out, "2")) return false;
    cli_CmdLine.SetInsertMode(true);
    cli_CmdLine.Put(cli_Out, '3');
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abc0123", "e", cli_Out, "3e\b")) return false;
    cli_CmdLine.SetInsertMode(false);
    cli_CmdLine.Put(cli_Out, cli::tk::String(10, "45"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "abc012345", "", cli_Out, "45")) return false;

    // Set & Reset & Display.
    cli_CmdLine.Set(cli::tk::String(10, "012345"), cli::tk::String(10, "abcdef"));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "012345", "abcdef", cli_Out, "")) return false;
    cli_CmdLine.PrintCmdLine(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "012345", "abcdef", cli_Out, "012345abcdef\b\b\b\b\b\b")) return false;
    cli_CmdLine.Reset();
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "", cli_Out, "")) return false;

    // Word analysis.
    cli_CmdLine.Put(cli_Out, cli::tk::String(256, "This is just a sample sentence."));
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a sample sentence.", "", cli_Out, "This is just a sample sentence.")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "sentence.", "")) return false;
    cli_CmdLine.MoveCursor(cli_Out, -3);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a sample senten", "ce.", cli_Out, "\b\b\b")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "senten", "ce.")) return false;
    cli_CmdLine.MoveCursor(cli_Out, - (int) cli_CmdLine.GetPrevWord().GetLength());
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a sample ", "sentence.", cli_Out, "\b\b\b\b\b\b")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "sample ", "sentence.")) return false;
    cli_CmdLine.MoveCursor(cli_Out, - 1);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a sample", " sentence.", cli_Out, "\b")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "sample", " sentence.")) return false;
    cli_CmdLine.MoveCursor(cli_Out, - (int) cli_CmdLine.GetPrevWord().GetLength());
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a ", "sample sentence.", cli_Out, "\b\b\b\b\b\b")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "a ", "sample")) return false;
    cli_CmdLine.MoveCursor(cli_Out, (int) cli_CmdLine.GetNextWord().GetLength());
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "This is just a sample", " sentence.", cli_Out, "sample")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "sample", " sentence.")) return false;
    cli_CmdLine.Home(cli_Out);
        if (! CheckEdition(__FILE__, __LINE__, cli_CmdLine, "", "This is just a sample sentence.", cli_Out, "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b")) return false;
        if (! CheckEditionWords(__FILE__, __LINE__, cli_CmdLine, "", "This")) return false;

    return true;
}

static const bool CheckEdition(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineEdition& CLI_CmdLine, const char* const STR_Left, const char* const STR_Right,
    cli::StringDevice& CLI_Out, const char* const STR_Out)
{
    bool b_Res = true;

    if ((CLI_CmdLine.GetLeft() != STR_Left) || (CLI_CmdLine.GetRight() != STR_Right)
        || (CLI_CmdLine.GetLine() != cli::tk::String::Concat(256, STR_Left, STR_Right)))
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << " left: '" << CLI_CmdLine.GetLeft() << "' ('" << STR_Left << "' expected)" << std::endl;
        std::cerr << " right: '" << CLI_CmdLine.GetRight() << "' ('" << STR_Right << "' expected)" << std::endl;
        b_Res = false;
    }
    if (CLI_Out.GetString() != STR_Out)
    {
        class Print { public: static const cli::tk::String String(const char* const STR_String) {
            const cli::tk::String tk_String(256, STR_String);
            cli::tk::String tk_Res(256);
            for (unsigned int ui = 0; ui < tk_String.GetLength(); ui ++) {
                switch (tk_String[ui]) {
                case '\b': tk_Res.Append("\\b"); break;
                case '\n': tk_Res.Append("\\n"); break;
                default: tk_Res.Append(tk_String[ui]); break;
                }
            }
            return tk_Res;
        }};
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << " out: '" << Print::String(CLI_Out.GetString()) << "'" << std::endl;
        std::cerr << " expected: '" << Print::String(STR_Out) << "'" << std::endl;
        b_Res = false;
    }

    CLI_Out.Reset();
    return b_Res;
}

static const bool CheckEditionWords(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineEdition& CLI_CmdLine, const char* const STR_Left, const char* const STR_Right)
{
    bool b_Res = true;

    if (CLI_CmdLine.GetPrevWord() != STR_Left)
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << "left: '" << CLI_CmdLine.GetPrevWord() << "' ('" << STR_Left << "' expected)" << std::endl;
        b_Res = false;
    }
    if (CLI_CmdLine.GetNextWord() != STR_Right)
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << "right: '" << CLI_CmdLine.GetNextWord() << "' ('" << STR_Right << "' expected)" << std::endl;
        b_Res = false;
    }

    return b_Res;
}

//! @brief CmdLineHistory unit test.
static const bool TestCmdLineHistory(void)
{
    // Create the history line object.
    cli::CmdLineHistory cli_History(5);

    // Create lines.
    cli::CmdLineEdition cli_EmptyLine;
    cli::CmdLineEdition cli_Line[7];
    cli::CmdLineEdition& cli_CurrentLine = cli_Line[0];
    cli_CurrentLine.Set(cli::tk::String(20, "current line"), cli::tk::String(20));
    for (int i=1; i<7; i++)
    {
        const cli::StringDevice cli_Content(20, false);
        cli_Content << "history " << i;
        (cli_Line[i]).Set(cli_Content.GetString(), cli::tk::String(20));
    }

    // Initial state.
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 1, & cli_EmptyLine)) return false;

    // Set the current line.
    cli_History.SaveCurrentLine(cli_CurrentLine);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 1, & cli_CurrentLine)) return false;

    // Push history 1.
    cli_History.Push(cli_Line[1]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 2, & cli_EmptyLine, & cli_Line[1])) return false;

    // Push history 2.
    cli_History.Push(cli_Line[2]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 3, & cli_EmptyLine, & cli_Line[2], & cli_Line[1])) return false;

    // Push history 3.
    cli_History.Push(cli_Line[3]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 4, & cli_EmptyLine, & cli_Line[3], & cli_Line[2], & cli_Line[1])) return false;

    // Set the current line.
    cli_History.SaveCurrentLine(cli_CurrentLine);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 4, & cli_CurrentLine, & cli_Line[3], & cli_Line[2], & cli_Line[1])) return false;

    // Push the same line.
    cli_History.Push(cli_Line[3]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 4, & cli_CurrentLine, & cli_Line[3], & cli_Line[2], & cli_Line[1])) return false;

    // Push history 4.
    cli_History.Push(cli_Line[4]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 5, & cli_EmptyLine, & cli_Line[4], & cli_Line[3], & cli_Line[2], & cli_Line[1])) return false;

    // Push history 5.
    cli_History.Push(cli_Line[5]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 6, & cli_EmptyLine, & cli_Line[5], & cli_Line[4], & cli_Line[3], & cli_Line[2], & cli_Line[1])) return false;

    // Push history 6.
    cli_History.Push(cli_Line[6]);
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 6, & cli_EmptyLine, & cli_Line[6], & cli_Line[5], & cli_Line[4], & cli_Line[3], & cli_Line[2])) return false;

    // Navigation
    cli_History.SaveCurrentLine(cli_CurrentLine);
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 3, true, cli_Line[4])) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 1, true, cli_Line[3])) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, -2, true, cli_Line[5])) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 0, true, cli_Line[5])) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, -5, false, cli_CurrentLine)) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, -1, false, cli_CurrentLine)) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 10, false, cli_Line[2])) return false;
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 1, false, cli_Line[2])) return false;
    cli_History.EnableNavigationMemory(false);
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 1, true, cli_Line[6])) return false;
    cli_History.EnableNavigationMemory(false);
    cli_History.EnableNavigationMemory(true);
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 2, true, cli_Line[4])) return false;

    // Clear
    cli_History.Clear();
    if (! CheckHistory(__FILE__, __LINE__, cli_History, 1, & cli_EmptyLine)) return false;

    // Navigate again
    if (! CheckNavigation(__FILE__, __LINE__, cli_History, 1, false, cli_EmptyLine)) return false;

    return true;
}

static const bool CheckHistory(
    const char* const STR_FileName, const unsigned int UI_Line,
    const cli::CmdLineHistory& CLI_History, const unsigned int UI_Count, ...)
{
    if (CLI_History.GetCount() != UI_Count)
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << "bad count " << CLI_History.GetCount() << " instead of " << UI_Count << std::endl;
        return false;
    }

    va_list t_Args;
    va_start(t_Args, UI_Count);
    for (unsigned int ui=0; ui<UI_Count; ui++)
    {
        if (const cli::CmdLineEdition* const pcli_CmdLine = va_arg(t_Args, const cli::CmdLineEdition*))
        {
            if (CLI_History.GetLine(ui).GetLine() != pcli_CmdLine->GetLine())
            {
                CmdLineError(STR_FileName, UI_Line);
                std::cerr << "Argument #" << ui << ": "
                    << "incorrect string '" << CLI_History.GetLine(0).GetLine() << "' "
                    << "instead of '" << pcli_CmdLine->GetLine() << "'"
                    << std::endl;
                return false;
            }
        }
        else
        {
            CmdLineError(STR_FileName, UI_Line);
            std::cerr << "Internal error: invalid argument #" << ui << std::endl;
            return false;
        }
    }
    va_end(t_Args);

    // Successful return.
    return true;
}

static const bool CheckNavigation(
    const char* const STR_FileName, const unsigned int UI_Line,
    cli::CmdLineHistory& CLI_History, const int I_Navigation,
    const bool B_Res, const cli::CmdLineEdition& CLI_Line)
{
    cli::CmdLineEdition cli_CmdLine;
    cli_CmdLine.Set(cli::tk::String(20, "current line"), cli::tk::String(20));
    cli::StringDevice cli_Output(256, false);

    // Compute the expected output.
    cli::StringDevice cli_ExpectedOutput(256, false);
    cli_ExpectedOutput << "\b\b\b\b\b\b\b\b\b\b\b\b            \b\b\b\b\b\b\b\b\b\b\b\b"; // Backspace - blank - backspace in order to remove "current line".
    CLI_Line.PrintCmdLine(cli_ExpectedOutput);

    // Execute navigation.
    const bool b_Res = CLI_History.Navigate(cli_CmdLine, cli_Output, I_Navigation);

    // Check result.
    if (b_Res != B_Res)
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr << "Incorrect result" << std::endl;
        return false;
    }

    // Check the current command line.
    if (cli_CmdLine.GetLine() != CLI_Line.GetLine())
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr
            << "Incorrect string '" << cli_CmdLine.GetLine() << "' "
            << "instead of '" << CLI_Line.GetLine() << "'"
            << std::endl;
        return false;
    }

    // Check output.
    if (cli_Output.GetString() != cli_ExpectedOutput.GetString())
    {
        CmdLineError(STR_FileName, UI_Line);
        std::cerr
            << "Incorrect output string '" << cli_Output.GetString() << "' "
            << "instead of '" << cli_ExpectedOutput.GetString() << "'"
            << std::endl;
        return false;
    }

    // Successful return.
    return true;
}

static void CmdLineError(
    const char* const STR_FileName, const unsigned int UI_Line)
{
    std::cerr << STR_FileName << ":" << UI_Line << ": test failed" << std::endl;
}
