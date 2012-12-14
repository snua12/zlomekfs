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

#include "cli/ui_password.h"

#include "ui_test.h"


// cli::ui::Password unit-tests.
const bool CheckGetPassword(void)
{
    class Test {
    public:
        static const bool Password(
                const char* const STR_File, const unsigned int UI_Line,
                const bool B_DisplayStars,
                const char* const STR_Input, const unsigned int UI_MinLength, const unsigned int UI_MaxLength,
                const bool B_ExpectedResult, const char* const STR_StringOutput, const char* const STR_DeviceOutput)
        {
            my_Cli cli_Cli("test", cli::Help());
            cli::Shell cli_Shell(cli_Cli);
            cli_Shell.SetStream(cli::WELCOME_STREAM, cli::OutputDevice::GetNullDevice());
            cli_Shell.SetStream(cli::PROMPT_STREAM, cli::OutputDevice::GetNullDevice());
            my_MTDevice cli_MTDevice;
            ShellGuard cli_ShellGuard(cli_Shell, cli_MTDevice);

            cli::ui::Password cli_Password(B_DisplayStars, UI_MinLength, UI_MaxLength);
            cli_MTDevice.SetInputString(STR_Input);
            const bool b_Result = cli_Password.Run(cli_Shell);

            if (b_Result != B_ExpectedResult) {
                UIError(STR_File, UI_Line);
                std::cerr << "UI::GetPassword() returned " << b_Result << " (" << B_ExpectedResult << " was expected)" << std::endl;
                return false;
            }
            if (strcmp((const char* const) cli_Password.GetPassword(), STR_StringOutput) != 0) {
                UIError(STR_File, UI_Line);
                std::cerr << "String output '" << cli_Password.GetPassword() << "' does not match '" << STR_StringOutput << "'" << std::endl;
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

    // Test basic character inputs (no star).
    if (! Test::Password(__FILE__, __LINE__, false, "012345\n", 0, 10, true, "012345", Out().endl())) return false;
    // Test basic character inputs (star).
    if (! Test::Password(__FILE__, __LINE__, true, "012345\n", 0, 10, true, "012345", Out().txt("******").endl())) return false;
    // Test maximum password length.
    if (! Test::Password(__FILE__, __LINE__, true, "0123456789\n", 0, 10, true, "0123456789", Out().txt("**********").endl())) return false;
    if (! Test::Password(__FILE__, __LINE__, true, "0123456789a\n", 0, 10, true, "0123456789", Out().txt("**********").beep().endl())) return false;
    if (! Test::Password(__FILE__, __LINE__, true, "012345\b6789a\n", 0, 10, true, "012346789a", Out().txt("******").bsp(1).txt("*****").endl())) return false;
    if (! Test::Password(__FILE__, __LINE__, true, "\b0123456789a\n", 0, 10, true, "0123456789", Out().beep().txt("**********").beep().endl())) return false;
    // Test minimum password length.
    if (! Test::Password(__FILE__, __LINE__, true, "0\n", 5, 10, false, "", Out().txt("*").beep().bsp(1).endl())) return false;

    // Test LEFT/RIGHT moves the cursor in the line (insert mode).
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef%lx%r\n", 0, 10, true, "abcdexf", Out().txt("******").left(1).txt("**\b").right("*").endl())) return false;
    // Test LEFT/RIGHT moves the cursor in the line (replace mode).
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef%i%lx%r\n", 0, 10, true, "abcdex", Out().txt("******").left(1).txt("*").beep().endl())) return false;
    // Test UP/DOWN/PUP/PDOWN does nothing.
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef%u%d\n", 0, 10, true, "abcdef", Out().txt("******").endl())) return false;
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef%U%D\n", 0, 10, true, "abcdef", Out().txt("******").endl())) return false;
    // Test HOME/END keys.
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef%H%E\n", 0, 10, true, "abcdef", Out().txt("******").left(6).right("******").endl())) return false;

    // Test ESCAPE breaks the current input.
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef\b\b%]", 0, 10, false, "", Out().txt("******").bsp(1).bsp(1).bsp(4).endl())) return false;
    // Test CTRL+C breaks the current input.
    if (! Test::Password(__FILE__, __LINE__, true, "abcdef\b\b%!", 0, 10, false, "", Out().txt("******").bsp(1).bsp(1).bsp(4).endl())) return false;

    return true;
}
