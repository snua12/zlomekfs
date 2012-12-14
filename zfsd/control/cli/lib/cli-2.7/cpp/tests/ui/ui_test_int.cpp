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

#include "cli/ui_int.h"

#include "ui_test.h"


// cli::ui::Int unit-tests.
const bool CheckGetInt(void)
{
    class Test {
    public:
        static const bool Int(
                const char* const STR_File, const unsigned int UI_Line,
                const int I_Default, const char* const STR_Input, const int I_MinValue, const int I_MaxValue,
                const bool B_ExpectedResult, const int I_IntOutput, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::Int cli_Int(I_Default, I_MinValue, I_MaxValue);
            cli_MTDevice.SetInputString(STR_Input);
            const bool b_Result = cli_Int.Run(cli_Shell);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::GetInt() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
                return false;
            }
            if (cli_Int.GetInt() != I_IntOutput) {
                UIError(STR_File, UI_Line);
                std::cerr << "Int output " << cli_Int.GetInt() << " does not match " << I_IntOutput << std::endl;
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
    if (! Test::Int(__FILE__, __LINE__, 0, "\n", 0, 20, true, 0, Out().txt("0").bsp(1).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 1, "\n", 0, 20, true, 1, Out().txt("1").bsp(1).txt("1").endl())) return false;
    // Test entering a new value discards the default value.
    if (! Test::Int(__FILE__, __LINE__, 0, "1\n", 0, 20, true, 1, Out().txt("0").bsp(1).txt("1").bsp(1).txt("1").endl())) return false;
    // Test final output gives the actual value understood (float truncation).
    if (! Test::Int(__FILE__, __LINE__, 0, "1.5\n", 0, 20, true, 1, Out().txt("0").bsp(1).txt("1.5").bsp(3).txt("1").endl())) return false;
    // Test final output gives the actual value understood (head zero discarded).
    // Test maximum value.
    if (! Test::Int(__FILE__, __LINE__, 0, "0123\n", 0, 20, false, 0, Out().txt("0").bsp(1).txt("0123").beep().bsp(4).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "012\b3\n", 0, 20, true, 13, Out().txt("0").bsp(1).txt("012").bsp(1).txt("3").bsp(3).txt("13").endl())) return false;
    // Test minimum value (positive numbers).
    if (! Test::Int(__FILE__, __LINE__, 0, "01\n", 10, 20, false, 0, Out().txt("0").bsp(1).txt("01").beep().bsp(2).txt("0").endl())) return false;
    // Test minimum value (negative number against 0).
    if (! Test::Int(__FILE__, __LINE__, 0, "-1\n", 0, 20, false, 0, Out().txt("0").bsp(1).txt("-1").beep().bsp(2).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "-1\n", -20, 20, true, -1, Out().txt("0").bsp(1).txt("-1").bsp(2).txt("-1").endl())) return false;
    // Test final output gives the actual value understood (head zero of negative numbers discarded).
    if (! Test::Int(__FILE__, __LINE__, 0, "-01\n", -20, 20, true, -1, Out().txt("0").bsp(1).txt("-01").bsp(3).txt("-1").endl())) return false;
    // Test final output gives the actual value understood (negative float numbers discarded).
    if (! Test::Int(__FILE__, __LINE__, 0, "-1.5\n", -20, 20, true, -1, Out().txt("0").bsp(1).txt("-1.5").bsp(4).txt("-1").endl())) return false;
    // Test minimum value (negative numbers).
    if (! Test::Int(__FILE__, __LINE__, 0, "-0123\n", -20, 20, false, 0, Out().txt("0").bsp(1).txt("-0123").beep().bsp(5).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "-012\b3\n", -20, 20, true, -13, Out().txt("0").bsp(1).txt("-012").bsp(1).txt("3").bsp(4).txt("-13").endl())) return false;

    // Test LEFT/RIGHT moves the cursor in the line (insert mode).
    if (! Test::Int(__FILE__, __LINE__, 100, "%l7%r\n", 0, 2000, true, 1070, Out().txt("100").left(1).txt("70\b").right("0").bsp(4).txt("1070").endl())) return false;
    // Test LEFT/RIGHT moves the cursor in the line (replace mode).
    if (! Test::Int(__FILE__, __LINE__, 100, "%i%l7%r\n", 0, 2000, true, 107, Out().txt("100").left(1).txt("7").beep().bsp(3).txt("107").endl())) return false;
    // Test UP/DOWN increment value and beep when max/min is already set.
    if (! Test::Int(__FILE__, __LINE__, 0, "%u%u\n", 0, 1, true, 1, Out().txt("0").bsp(1).txt("1").beep().bsp(1).txt("1").bsp(1).txt("1").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 1, "%d%d\n", 0, 1, true, 0, Out().txt("1").bsp(1).txt("0").beep().bsp(1).txt("0").bsp(1).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "%u%l%l%d\n", 0, 1, true, 0, Out().txt("0").bsp(1).txt("1").left(1).beep().del(1).txt("0").bsp(1).txt("0").endl())) return false;
    // Test UP/DOWN with out of bounds values beeps and automatically corrects to min/max values.
    if (! Test::Int(__FILE__, __LINE__, 0, "-5%u\n", 0, 10, true, 0, Out().txt("0").bsp(1).txt("-5").beep().bsp(2).txt("0").bsp(1).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "-5%d\n", 0, 10, true, 0, Out().txt("0").bsp(1).txt("-5").beep().bsp(2).txt("0").bsp(1).txt("0").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "20%u\n", 0, 10, true, 10, Out().txt("0").bsp(1).txt("20").beep().bsp(2).txt("10").bsp(2).txt("10").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "20%d\n", 0, 10, true, 10, Out().txt("0").bsp(1).txt("20").beep().bsp(2).txt("10").bsp(2).txt("10").endl())) return false;
    // Test PUP/PDOWN reach max/min values, and beep when max/min values is already set or out of bounds.
    if (! Test::Int(__FILE__, __LINE__, 0, "%U%U\n", 0, 20, true, 20, Out().txt("0").bsp(1).txt("20").beep().bsp(2).txt("20").bsp(2).txt("20").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 20, "%D%D\n", 0, 20, true, 0, Out().txt("20").bsp(2).txt("0").beep().bsp(1).txt("0").bsp(1).txt("0").endl())) return false;
    // Test PUP/PDOWN with out of bounds values beeps and automatically corrects to min/max values.
    if (! Test::Int(__FILE__, __LINE__, 0, "100%U\n", 0, 20, true, 20, Out().txt("0").bsp(1).txt("100").beep().bsp(3).txt("20").bsp(2).txt("20").endl())) return false;
    if (! Test::Int(__FILE__, __LINE__, 0, "-100%D\n", 0, 20, true, 0, Out().txt("0").bsp(1).txt("-100").beep().bsp(4).txt("0").bsp(1).txt("0").endl())) return false;
    // Test HOME/END keys.
    if (! Test::Int(__FILE__, __LINE__, 20, "%H%E\n", 0, 20, true, 20, Out().txt("20").left(2).right("20").bsp(2).txt("20").endl())) return false;
    // Test UP/DOWN do not keep LINE 'current is default' property.
    if (! Test::Int(__FILE__, __LINE__, 0, "%u0\n", 0, 1, false, 0, Out().txt("0").bsp(1).txt("10").beep().bsp(2).txt("0").endl())) return false;

    return true;
}
