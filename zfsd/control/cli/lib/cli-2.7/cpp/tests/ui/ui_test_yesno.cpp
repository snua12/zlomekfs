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

#include "cli/ui_yesno.h"

#include "ui_test.h"


// cli::ui::YesNo unit-tests.
const bool CheckYesNoQuestion(void)
{
    class Test {
    public:
        static const bool YesNo(
                const char* const STR_File, const unsigned int UI_Line,
                const bool B_Default, const char* const STR_Input, const cli::ResourceString::LANG E_Lang,
                const bool B_ExpectedResult, const bool B_YesNoOutput, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetLang(E_Lang);
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::YesNo cli_YesNo(B_Default);
            cli_MTDevice.SetInputString(STR_Input);
            const bool b_Result = cli_YesNo.Run(cli_Shell);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::YesNoQuestion() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
                return false;
            }
            if (cli_YesNo.GetYesNo() != B_YesNoOutput) {
                UIError(STR_File, UI_Line);
                std::cerr << "Yes/no output " << cli_YesNo.GetYesNo() << " does not match " << B_YesNoOutput << std::endl;
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

    static const cli::ResourceString::LANG EN = cli::ResourceString::LANG_EN;
    static const cli::ResourceString::LANG FR = cli::ResourceString::LANG_FR;

    // Test ENTER validates the default choice (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "\n", EN, true, true, Out().txt("Yes").bsp(3).txt("Yes").endl())) return false;
    if (! Test::YesNo(__FILE__, __LINE__, false, "\n", EN, true, false, Out().txt("No").bsp(2).txt("No").endl())) return false;
    // Test ENTER validates the default choice (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "\n", FR, true, true, Out().txt("Oui").bsp(3).txt("Oui").endl())) return false;
    if (! Test::YesNo(__FILE__, __LINE__, false, "\n", FR, true, false, Out().txt("Non").bsp(3).txt("Non").endl())) return false;

    // Test 'y' means yes (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "y\n", EN, true, true, Out().txt("Yes").bsp(3).txt("y").bsp(1).txt("Yes").endl())) return false;
    // Test 'Y' means yes (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "Y\n", EN, true, true, Out().txt("Yes").bsp(3).txt("Y").bsp(1).txt("Yes").endl())) return false;
    // Test 'ye' means yes (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "ye\n", EN, true, true, Out().txt("Yes").bsp(3).txt("ye").bsp(2).txt("Yes").endl())) return false;
    // Test YES means yes (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "YES\n", EN, true, true, Out().txt("Yes").bsp(3).txt("YES").bsp(3).txt("Yes").endl())) return false;
    // Test 'n' means no (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "n\n", EN, true, false, Out().txt("Yes").bsp(3).txt("n").bsp(1).txt("No").endl())) return false;
    // Test 'N' means no (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "N\n", EN, true, false, Out().txt("Yes").bsp(3).txt("N").bsp(1).txt("No").endl())) return false;
    // Test 'no' means no (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "no\n", EN, true, false, Out().txt("Yes").bsp(3).txt("no").bsp(2).txt("No").endl())) return false;
    // Test 'NO' means no (English).
    if (! Test::YesNo(__FILE__, __LINE__, true, "NO\n", EN, true, false, Out().txt("Yes").bsp(3).txt("NO").bsp(2).txt("No").endl())) return false;
    // Test 'o' means yes (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "o\n", FR, true, true, Out().txt("Oui").bsp(3).txt("o").bsp(1).txt("Oui").endl())) return false;
    // Test 'O' means yes (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "O\n", FR, true, true, Out().txt("Oui").bsp(3).txt("O").bsp(1).txt("Oui").endl())) return false;
    // Test 'ou' means yes (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "ou\n", FR, true, true, Out().txt("Oui").bsp(3).txt("ou").bsp(2).txt("Oui").endl())) return false;
    // Test 'OUI' means yes (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "OUI\n", FR, true, true, Out().txt("Oui").bsp(3).txt("OUI").bsp(3).txt("Oui").endl())) return false;
    // Test 'n' means no (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "n\n", FR, true, false, Out().txt("Oui").bsp(3).txt("n").bsp(1).txt("Non").endl())) return false;
    // Test 'N' means no (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "N\n", FR, true, false, Out().txt("Oui").bsp(3).txt("N").bsp(1).txt("Non").endl())) return false;
    // Test 'no' means no (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "no\n", FR, true, false, Out().txt("Oui").bsp(3).txt("no").bsp(2).txt("Non").endl())) return false;
    // Test 'NON' means no (French).
    if (! Test::YesNo(__FILE__, __LINE__, true, "NON\n", FR, true, false, Out().txt("Oui").bsp(3).txt("NON").bsp(3).txt("Non").endl())) return false;

    // Test incorrect text fails and falls back to default answer (English).
    if (! Test::YesNo(__FILE__, __LINE__, false, "yesz\n", EN, false, false, Out().txt("No").bsp(2).txt("yesz").beep().bsp(4).txt("No").endl())) return false;
    // Test incorrect text fails and falls back to default answer (French).
    if (! Test::YesNo(__FILE__, __LINE__, false, "ouiz\n", FR, false, false, Out().txt("Non").bsp(3).txt("ouiz").beep().bsp(4).txt("Non").endl())) return false;

    return true;
}
