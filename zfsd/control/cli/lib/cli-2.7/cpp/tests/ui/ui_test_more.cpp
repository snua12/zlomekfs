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
#include "cli/ui_more.h"

#include "ui_test.h"


// cli::ui::More unit-tests.
const bool CheckMore(void)
{
    class Test {
    public:
        static const bool More(
                const char* const STR_File, const unsigned int UI_Line,
                const char* const STR_Text, const char* const STR_InputKeys, const bool B_WrapLines, const cli::ResourceString::LANG E_Lang,
                const bool B_ExpectedResult, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetLang(E_Lang);
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::More cli_More(10, 1024);
            cli_More.GetText() << STR_Text;
            cli_MTDevice.SetInputString(STR_InputKeys);
            cli_MTDevice.SetbWrapLines(B_WrapLines);
            const bool b_Result = cli_More.Run(cli_Shell);
            cli_MTDevice.SetbWrapLines(false);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::More() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
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
    static const cli::ResourceString::LANG EN = cli::ResourceString::LANG_EN;
    static const cli::ResourceString::LANG FR = cli::ResourceString::LANG_FR;
    static const char* const WAIT_EN = "--- More ---";
    static const char* const WAIT_FR = "--- Plus ---";
    static const bool WRAP_LINES = true;
    static const bool NO_WRAP_LINES = false;

    // Test empty text
    if (! Test::More(__FILE__, __LINE__, "", "", NO_WRAP_LINES, EN, true, Out())) return false;
    // Test single line
    if (! Test::More(__FILE__, __LINE__, "abc", "", NO_WRAP_LINES, EN, true, Out().txt("abc").endl())) return false;
    // Test long line
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "abcde"; out.txt("abcde").endl();
        txt << "f"; out.txt("f").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "abcde"; out.txt("abcde");
        txt << "f"; out.txt("f").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);
    // Test very long line.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa"; out.txt("aaaaa").endl();
        txt << "bbbbb"; out.txt("bbbbb").endl();
        txt << "ccccc"; out.txt("ccccc").endl();
        txt << "ddddd"; out.txt("ddddd").endl().txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "eeeee"; out.txt("eeeee").endl().txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "fffff"; out.txt("fffff").endl().txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "gg"; out.txt("gg").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa"; out.txt("aaaaa");
        txt << "bbbbb"; out.txt("bbbbb");
        txt << "ccccc"; out.txt("ccccc");
        txt << "ddddd"; out.txt("ddddd").txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "eeeee"; out.txt("eeeee").txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "fffff"; out.txt("fffff").txt(WAIT_EN);
        in << "%d"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "gg"; out.txt("gg").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);

    // Test 1 + half page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "eeeee" << cli::endl; out.txt("eeeee").endl();
        txt << "fffff" << cli::endl; out.txt("fffff").endl();
        txt << "gg" << cli::endl; out.txt("gg").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "eeeee" << cli::endl; out.txt("eeeee");
        txt << "fffff" << cli::endl; out.txt("fffff");
        txt << "gg" << cli::endl; out.txt("gg").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);
    // Test exact two page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);
    // Test 2+ page down.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "++" << cli::endl; out.txt("++").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "++" << cli::endl; out.txt("++").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);

    // Test end.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_EN);
        in << "%E"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+").endl();
        txt << "++" << cli::endl; out.txt("++").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa");
        txt << "bbbbb" << cli::endl; out.txt("bbbbb");
        txt << "ccccc" << cli::endl; out.txt("ccccc");
        txt << "ddddd" << cli::endl; out.txt("ddddd").txt(WAIT_EN);
        in << "%E"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "1+" << cli::endl; out.txt("1+").endl();
        txt << "22+" << cli::endl; out.txt("22+").endl();
        txt << "333+" << cli::endl; out.txt("333+").endl();
        txt << "4444+" << cli::endl; out.txt("4444+");
        txt << "++" << cli::endl; out.txt("++").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);

    // Test quit.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_EN);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "void" << cli::endl;
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, EN, true, out)) return false;
    } while(0);

    // Test French.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "aaaaa" << cli::endl; out.txt("aaaaa").endl();
        txt << "bbbbb" << cli::endl; out.txt("bbbbb").endl();
        txt << "ccccc" << cli::endl; out.txt("ccccc").endl();
        txt << "ddddd" << cli::endl; out.txt("ddddd").endl().txt(WAIT_FR);
        in << "q"; out.bsp((unsigned int) strlen(WAIT_FR));
        txt << "void" << cli::endl;
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), NO_WRAP_LINES, FR, true, out)) return false;
    } while(0);

    // Bug with line wrapping.
    do {
        cli::StringDevice txt(1024, false), in(1024, false); Out out;
        txt << "abcd" << cli::endl; out.txt("abcd").endl();
        txt << "abcde" << cli::endl; out.txt("abcde");
        txt << "abcdef" << cli::endl; out.txt("abcde").txt("f").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "abcdefg" << cli::endl; out.txt("abcde").txt("fg").endl();
        txt << "abcdefgh" << cli::endl; out.txt("abcde").txt("fgh").endl().txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "abcdefghi" << cli::endl; out.txt("abcde").txt("fghi").endl();
        txt << "abcdefghij" << cli::endl; out.txt("abcde").txt("fghij").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN));
        txt << "abcdefghijk" << cli::endl; out.txt("abcde").txt("fghij").txt("k").endl();
        txt << "abcdefghijkl" << cli::endl; out.txt("abcde").txt(WAIT_EN);
        in << " "; out.bsp((unsigned int) strlen(WAIT_EN)).txt("fghij").txt("kl").endl();
        if (! Test::More(__FILE__, __LINE__, txt.GetString(), in.GetString(), WRAP_LINES, EN, true, out)) return false;
    } while(0);

    return true;
}
