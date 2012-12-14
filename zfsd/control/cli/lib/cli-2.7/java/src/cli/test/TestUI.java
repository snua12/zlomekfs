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


class MyCli extends cli.Cli {
    public MyCli() { super("MyCli", new cli.Help()); }
    public void populate() {}
    public boolean execute(cli.CommandLine CLI_CmdLine) { return false; }
}

class MyNonBlockingIODevice extends cli.NonBlockingIODevice.Java {
    public MyNonBlockingIODevice() { super("MyNonBlockingIODevice"); m_cliConsole = new cli.Console(); }
    public boolean openDevice() { return m_cliConsole.openDevice(); }
    public boolean closeDevice() { return m_cliConsole.closeDevice(); }
    public cli.OutputDevice.Interface put(String J_Text) { m_cliConsole.put(J_Text); return this; }
    public void beep() { m_cliConsole.beep(); }
    public void cleanScreen() { m_cliConsole.cleanScreen(); }
    public cli.OutputDevice.ScreenInfo getScreenInfo() { return new cli.OutputDevice.ScreenInfo(10, 10, false, false); }
    public boolean wouldOutput(cli.OutputDevice.Interface CLI_Device) { return (super.wouldOutput(CLI_Device) || m_cliConsole.wouldOutput(CLI_Device)); }
    public boolean waitForKeys(int I_Milli) { onKey(m_cliConsole.getKey()); return true; }
    public cli.ResourceString getLocation() { return m_cliConsole.getLocation(); }
    private final cli.Console m_cliConsole;
}

public class TestUI {
    public static void main(String J_Args[]) {
        // Create resources and launch a cli in a non-blocking mode.
        cli.Cli cli_Cli = new MyCli();
        cli.Shell cli_Shell = new cli.Shell(cli_Cli);
        cli_Shell.setStream(cli.Shell.WELCOME_STREAM, cli.OutputDevice.getNullDevice());
        cli_Shell.setStream(cli.Shell.PROMPT_STREAM, cli.OutputDevice.getNullDevice());
        MyNonBlockingIODevice cli_Device = new MyNonBlockingIODevice();
        cli_Shell.run(cli_Device);

        //cli.Traces.setFilter(new cli.TraceClass("CLI_JNI", new cli.Help()), true);

        if (true) {
            cli_Device.put("Please enter a line: ");
            cli.ui.Line cli_Line = new cli.ui.Line("", 0, 10);
            cli_Line.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Line.getLine()).endl();
            cli_Device.endl();
        }

        if (true) {
            cli_Device.put("Please enter a password: ");
            cli.ui.Password cli_Password = new cli.ui.Password(true, 5, 10);
            cli_Password.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Password.getPassword()).endl();
            cli_Device.endl();
        }

        if (true) {
            cli_Device.put("Please enter an integer number: ");
            cli.ui.Int cli_Int = new cli.ui.Int(0, -10, 10);
            cli_Int.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Int.getInt()).endl();
            cli_Device.endl();
        }

        if (true) {
            cli_Device.put("Please enter a float number: ");
            cli.ui.Float cli_Float = new cli.ui.Float(0, -10, 10);
            cli_Float.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Float.getFloat()).endl();
            cli_Device.endl();
        }

        if (true) {
            cli.ui.YesNo cli_YesNo = new cli.ui.YesNo(true);

            cli_Shell.setLang(cli.ResourceString.LANG_EN);
            cli_Device.put("Answer [YES/no]: ");
            cli_YesNo.run(cli_Shell);
            cli_Device.put("User entered: ").put(new Boolean(cli_YesNo.getYesNo()).toString()).put("/").put(cli_YesNo.getstrChoice().getString(cli_Shell.getLang())).endl();

            cli_Shell.setLang(cli.ResourceString.LANG_FR);
            cli_Device.put("Répondez [OUI/non] : ");
            cli_YesNo.run(cli_Shell);
            cli_Device.put("User entered: ").put(new Boolean(cli_YesNo.getYesNo()).toString()).put("/").put(cli_YesNo.getstrChoice().getString(cli_Shell.getLang())).endl();
            cli_Device.endl();
        }

        if (true) {
            java.util.Vector<cli.ResourceString> j_Choices = new java.util.Vector<cli.ResourceString>();
            j_Choices.add(new cli.ResourceString().setString(cli.ResourceString.LANG_EN, "choice#1").setString(cli.ResourceString.LANG_FR, "choix 1"));
            j_Choices.add(new cli.ResourceString().setString(cli.ResourceString.LANG_EN, "choice#2").setString(cli.ResourceString.LANG_FR, "choix 2"));
            cli.ui.Choice cli_Choice = new cli.ui.Choice(0, j_Choices);

            cli_Shell.setLang(cli.ResourceString.LANG_EN);
            cli_Device.put("Please make a choice: ");
            cli_Choice.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Choice.getChoice()).put("/").put(cli_Choice.getstrChoice().getString(cli_Shell.getLang())).endl();

            cli_Shell.setLang(cli.ResourceString.LANG_FR);
            cli_Device.put("Merci de faire un choix : ");
            cli_Choice.run(cli_Shell);
            cli_Device.put("User entered: ").put(cli_Choice.getChoice()).put("/").put(cli_Choice.getstrChoice().getString(cli_Shell.getLang())).endl();
        }

        if (true) {
            cli.ui.More cli_More = new cli.ui.More();
            cli.test.UISampleText.fillText(cli_More.getText(), 10);
            cli_More.run(cli_Shell);
            cli_Device.endl();
        }

        if (true) {
            cli.ui.Less cli_Less = new cli.ui.Less();
            cli.test.UISampleText.fillText(cli_Less.getText(), 10);
            cli_Less.run(cli_Shell);
            cli_Device.endl();
        }

        // Eventually quit the shell.
        cli_Shell.quit();
    }
}
