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

package cli.test;


public class TestSample {

    public static void main(String ARJ_Args[]) {
        // Load the class.
        cli.Cli cli_Cli = null;
        try {
            String str_ClassName = ARJ_Args[0];
            try {
                Class j_CliClass = Class.forName(str_ClassName);
                cli_Cli = (cli.Cli) j_CliClass.newInstance();
            } catch (Exception e) {
                e.printStackTrace();
                return;
            }
        } catch (Exception e) {
            System.err.println("USAGE");
            System.err.println("   TestSample <class name> <input file> <output file>");
            return;
        }

        // Create I/O devices.
        cli.IODevice.Interface cli_Input = null;
        try {
            // Check an input file name is given.
            String str_InputFileName = ARJ_Args[1];

            // Create a corresponding output device.
            cli.OutputDevice.Interface cli_Output = null;
            try {
                // Check an output file name is given.
                String str_OutputFileName = ARJ_Args[2];
                // Create the output file device.
                cli_Output = new cli.OutputFileDevice(str_OutputFileName);
            } catch (Exception e) {
                // Redirect output to stdout.
                cli_Output = cli.OutputDevice.getStdOut();
            }

            // Create the input file device.
            cli_Input = new cli.InputFileDevice(str_InputFileName, cli_Output).enableSpecialCharacters(true);
        } catch (Exception e) {
            // Directly use the console.
            cli_Input = new cli.Console();
        }

        try {
            // Create a shell.
            cli.Shell cli_Shell = new cli.Shell(cli_Cli);

            // Redirect only echo, prompt, output and error streams.
            cli_Shell.setStream(cli.Shell.WELCOME_STREAM, cli.OutputDevice.getNullDevice());

            // Launch the shell.
            cli_Shell.run(cli_Input);
        } catch (Exception e) {
            cli_Input.printStackTrace(e);
        }
    }

}
