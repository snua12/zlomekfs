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

#include "cli/string_device.h"
#include "cli/ui_less.h"

#include "ui_test.h"


const bool CheckLess(void)
{
    class Test {
    public:
        static const bool Less(
                const char* const STR_File, const unsigned int UI_Line,
                const char* const STR_Text, const char* const STR_InputKeys, const bool B_WrapLines,
                const bool B_ExpectedResult, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::Less cli_Less(10, 1024);
            cli_Less.GetText() << STR_Text;
            cli_MTDevice.SetInputString(STR_InputKeys);
            cli_MTDevice.SetbWrapLines(B_WrapLines);
            const bool b_Result = cli_Less.Run(cli_Shell);
            cli_MTDevice.SetbWrapLines(false);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::Less() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
                return false;
            }
            const cli::tk::String cli_OutputString = cli_MTDevice.GetOutputString();
            if (strcmp((const char* const) cli_OutputString, STR_DeviceOutput) != 0) {
                UIError(STR_File, UI_Line);
                std::cerr << "Device output '" << cli_OutputString << "' does not match '" << STR_DeviceOutput << "'" << std::endl;
                return false;
            }
            return true;
        }
    };

    // Build test data.
    static const char* const WAIT_LESS = ":";
    static const bool WRAP_LINES = true;
    static const bool NO_WRAP_LINES = false;

    // Test empty text + beep.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        out.endl().endl().endl().endl().txt(WAIT_LESS);
        in << "%u"; out.beep();
        in << "%d"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);

    // Test single line.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "abc"; out.txt("abc").endl();
        out.endl().endl().endl().txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    // Test long line.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "abcdef"; out.txt("abcde").endl().txt("f").endl();
        out.endl().endl().txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "abcdef"; out.txt("abcde").txt("f").endl();
        out.endl().endl().txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);
    // Test very long line + beep.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaabbbbbcccccddddd"; out.txt("aaaaa").endl().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%u"; out.beep();
        in << "%d"; out.cls().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        txt << "eeeee"; out.txt("eeeee").endl();
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("ccccc").endl().txt("ddddd").endl().txt("eeeee").endl();
        txt << "fffff"; out.txt("fffff").endl();
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("ddddd").endl().txt("eeeee").endl().txt("fffff").endl();
        txt << "gg"; out.txt("gg").endl();
        out.txt(WAIT_LESS);
        in << "%d"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaabbbbbcccccddddd"; out.txt("aaaaa").txt("bbbbb").txt("ccccc").txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%u"; out.beep();
        in << "%d"; out.cls().txt("bbbbb").txt("ccccc").txt("ddddd");
        txt << "eeeee"; out.txt("eeeee");
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("ccccc").txt("ddddd").txt("eeeee");
        txt << "fffff"; out.txt("fffff");
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("ddddd").txt("eeeee").txt("fffff");
        txt << "gg"; out.txt("gg").endl();
        out.txt(WAIT_LESS);
        in << "%d"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    // Test 1 + half page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.cls().txt("ccccc").endl().txt("ddddd").endl();
        txt << "eeeee" << cli::endl; out.txt("eeeee").endl();
        txt << "ff" << cli::endl; out.txt("ff").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%D"; out.cls().txt("ccccc").txt("ddddd");
        txt << "eeeee" << cli::endl; out.txt("eeeee");
        txt << "ff" << cli::endl; out.txt("ff").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);
    // Test exact two page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%D"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);
    // Test 2+ page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("22+").endl().txt("333+").endl().txt("4444+").endl();
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%D"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        out.txt(WAIT_LESS);
        in << "%d"; out.cls().txt("22+").endl().txt("333+").endl().txt("4444+");
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    // Test end.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl;
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl;
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%D"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    // Test quit.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        txt << "void" << cli::endl;
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        txt << "void" << cli::endl;
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    // Test Home.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%E"; out.cls().txt("ddddd").endl();
        txt << "eeeee" << cli::endl; out.txt("eeeee").endl();
        txt << "fffff" << cli::endl; out.txt("fffff").endl();
        txt << "gg"; out.txt("gg").endl();
        out.txt(WAIT_LESS);
        in << "%H"; out.cls().txt("aaaaa").endl().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%E"; out.cls().txt("ddddd");
        txt << "eeeee" << cli::endl; out.txt("eeeee");
        txt << "fffff" << cli::endl; out.txt("fffff");
        txt << "gg"; out.txt("gg").endl();
        out.txt(WAIT_LESS);
        in << "%H"; out.cls().txt("aaaaa").txt("bbbbb").txt("ccccc").txt("ddddd");
        out.txt(WAIT_LESS);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    // Test 1 + half page up.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%E"; out.cls().txt("ccccc").endl().txt("ddddd").endl();
        txt << "eeeee" << cli::endl; out.txt("eeeee").endl();
        txt << "ff" << cli::endl; out.txt("ff").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("aaaaa").endl().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%E"; out.cls().txt("ccccc").txt("ddddd");
        txt << "eeeee" << cli::endl; out.txt("eeeee");
        txt << "ff" << cli::endl; out.txt("ff").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("aaaaa").txt("bbbbb").txt("ccccc").txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);
    // Test exact two page up.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("aaaaa").endl().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("aaaaa").txt("bbbbb").txt("ccccc").txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);
    // Test 2+ page up.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl;
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl().txt("1+").endl();
        out.txt(WAIT_LESS);
        in << "%u"; out.cls().txt("aaaaa").endl().txt("bbbbb").endl().txt("ccccc").endl().txt("ddddd").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        out.cls();
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%E"; out.cls();
        txt << "1+" << cli::endl;
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        txt << "++" << cli::endl; out.txt("++").endl();
        out.txt(WAIT_LESS);
        in << "%U"; out.cls().txt("bbbbb").txt("ccccc").txt("ddddd").txt("1+").endl();
        out.txt(WAIT_LESS);
        in << "%u"; out.cls().txt("aaaaa").txt("bbbbb").txt("ccccc").txt("ddddd");
        out.txt(WAIT_LESS);
        in << "%U"; out.beep();
        in << "q"; out.bsp((unsigned int) strlen(WAIT_LESS));
        if (! Test::Less(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, true, out)) return false;
    } while(0);

    return true;
}
