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

#include "cli/ui_float.h"

#include "ui_test.h"


// cli::ui::Float unit-tests.
const bool CheckGetFloat(void)
{
    class Test {
    public:
        static const bool Float(
                const char* const STR_File, const unsigned int UI_Line,
                const double D_Default, const char* const STR_Input, const double D_MinValue, const double D_MaxValue,
                const bool B_ExpectedResult, const double D_FloatOutput, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::Float cli_Float(D_Default, D_MinValue, D_MaxValue);
            cli_MTDevice.SetInputString(STR_Input);
            const bool b_Result = cli_Float.Run(cli_Shell);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::GetFloat() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
                return false;
            }
            if (cli_Float.GetFloat() != D_FloatOutput) {
                UIError(STR_File, UI_Line);
                std::cerr << "Float output " << cli_Float.GetFloat() << " does not match " << D_FloatOutput << std::endl;
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

    // Test ENTER keeps the default value.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "\n", 0.0, 20.0, true, 0.0, Out().txt("0.0").bsp(3).txt("0.0").endl())) return false;
    if (! Test::Float(__FILE__, __LINE__, 1.0, "\n", 0.0, 20.0, true, 1.0, Out().txt("1.0").bsp(3).txt("1.0").endl())) return false;
    // Test basic float inputs (default is discarded).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "0\n", 0.0, 20.0, true, 0.0, Out().txt("0.0").bsp(3).txt("0").bsp(1).txt("0.0").endl())) return false;
    if (! Test::Float(__FILE__, __LINE__, 0.0, "1.5\n", 0.0, 20.0, true, 1.5, Out().txt("0.0").bsp(3).txt("1.5").bsp(3).txt("1.5").endl())) return false;
    // Test maximum value.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "0123\n", 0.0, 20.0, false, 0.0, Out().txt("0.0").bsp(3).txt("0123").beep().bsp(4).txt("0.0").endl())) return false;
    // Test maximum value (head zero discarded).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "012\b3\n", 0.0, 20.0, true, 13.0, Out().txt("0.0").bsp(3).txt("012").bsp(1).txt("3").bsp(3).txt("13.0").endl())) return false;
    // Test minimum value (head zero discarded).
    if (! Test::Float(__FILE__, __LINE__, 10.0, "01\n", 10.0, 20.0, false, 10.0, Out().txt("10.0").bsp(4).txt("01").beep().bsp(2).txt("10.0").endl())) return false;
    // Test minimum value (negative number against 0).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-1\n", 0.0, 20.0, false, 0.0, Out().txt("0.0").bsp(3).txt("-1").beep().bsp(2).txt("0.0").endl())) return false;
    // Test minimum value (negative number successfull).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-1\n", -20.0, 20.0, true, -1.0, Out().txt("0.0").bsp(3).txt("-1").bsp(2).txt("-1.0").endl())) return false;
    // Test minimum value (negative number successfull, head zero discarded).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-01\n", -20.0, 20.0, true, -1.0, Out().txt("0.0").bsp(3).txt("-01").bsp(3).txt("-1.0").endl())) return false;
    // Test minimum value (negative float number successfull).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-1.5\n", -20.0, 20.0, true, -1.5, Out().txt("0.0").bsp(3).txt("-1.5").bsp(4).txt("-1.5").endl())) return false;
    // Test minimum value (negative number failure).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-0123\n", -20.0, 20.0, false, 0, Out().txt("0.0").bsp(3).txt("-0123").beep().bsp(5).txt("0.0").endl())) return false;
    // Test minimum value (negative float number successfull, with backspace).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-012\b3\n", -20.0, 20.0, true, -13.0, Out().txt("0.0").bsp(3).txt("-012").bsp(1).txt("3").bsp(4).txt("-13.0").endl())) return false;

    // Test LEFT/RIGHT moves the cursor in the line (insert mode).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%l7%r\n", 0.0, 1.0, true, 0.7, Out().txt("0.0").left(1).txt("70\b").right("0").bsp(4).txt("0.7").endl())) return false;
    // Test LEFT/RIGHT moves the cursor in the line (replace mode).
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%i%l7%r\n", 0.0, 1.0, true, 0.7, Out().txt("0.0").left(1).txt("7").beep().bsp(3).txt("0.7").endl())) return false;
    // Test UP/DOWN does nothing.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%u%u\n", 0.0, 1.0, true, 0.0, Out().txt("0.0").bsp(3).txt("0.0").endl())) return false;
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%d%d\n", 0.0, 1.0, true, 0.0, Out().txt("0.0").bsp(3).txt("0.0").endl())) return false;
    // Test PUP/PDOWN reach max/min values, and beep when max/min values is already set or out of bounds.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%U%U\n", 0.0, 1.0, true, 1.0, Out().txt("0.0").bsp(3).txt("1.0").beep().bsp(3).txt("1.0").bsp(3).txt("1.0").endl())) return false;
    if (! Test::Float(__FILE__, __LINE__, 1.0, "%D%D\n", 0.0, 1.0, true, 0.0, Out().txt("1.0").bsp(3).txt("0.0").beep().bsp(3).txt("0.0").bsp(3).txt("0.0").endl())) return false;
    // Test PUP/PDWON with out of bounds values beeps and automatically corrects to min/max values.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "2%U\n", 0.0, 1.0, true, 1.0, Out().txt("0.0").bsp(3).txt("2").beep().bsp(1).txt("1.0").bsp(3).txt("1.0").endl())) return false;
    if (! Test::Float(__FILE__, __LINE__, 0.0, "-1%D\n", 0.0, 1.0, true, 0.0, Out().txt("0.0").bsp(3).txt("-1").beep().bsp(2).txt("0.0").bsp(3).txt("0.0").endl())) return false;
    // Test HOME/END keys.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%H%E\n", 0.0, 1.0, true, 0.0, Out().txt("0.0").left(3).right("0.0").bsp(3).txt("0.0").endl())) return false;
    // Test UP/DOWN do not keep LINE 'current is default' property.
    if (! Test::Float(__FILE__, __LINE__, 0.0, "%U0\n", 0.0, 1.0, true, 1.0, Out().txt("0.0").bsp(3).txt("1.0").txt("0").bsp(4).txt("1.0").endl())) return false;

    return true;
}
