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


public class TestTelnet {
    public static void main(String J_Args[]) {
        boolean b_TestSimple = true;
        boolean b_TestUI = true;
        boolean b_MultiConnection = true;
        boolean b_StopServer = true;
        if (J_Args.length > 0) {
            if (! J_Args[0].equals("TestSimple")) { b_TestSimple = false; }
            if (! J_Args[0].equals("TestUI")) { b_TestUI = false; }
            if (! J_Args[0].equals("MultiConnection")) { b_MultiConnection = false; }
            if (! J_Args[0].equals("StopServer")) { b_StopServer = false; }
        }

        if (b_TestSimple) {
            if (! test_Simple()) {
                System.exit(-1);
            }
        }
        if (b_TestUI) {
            if (! test_UI()) {
                System.exit(-1);
            }
        }
        if (b_MultiConnection) {
            if (! test_MultiConnection()) {
                System.exit(-1);
            }
        }
        if (b_StopServer) {
            if (! test_StopServer()) {
                System.exit(-1);
            }
        }
    }

    private static boolean test_Simple() {
        cli.OutputDevice.getStdOut().put("-----------").endl();
        cli.OutputDevice.getStdOut().put("Simple test").endl();
        cli.OutputDevice.getStdOut().put("-----------").endl();

        MyServer cli_Server = new MyServer();
        if (! cli_Server.start()) return false;

        MyClient cli_Client = MyClient.createConnection();
        if (cli_Client == null) return false;
        if (! cli_Client.help()) return false;
        if (! cli_Client.quit()) return false;

        if (! cli_Server.stop()) return false;

        cli.OutputDevice.getStdOut().put("ok").endl().endl();
        return true;
    }

    private static boolean test_UI() {
        cli.OutputDevice.getStdOut().put("-------").endl();
        cli.OutputDevice.getStdOut().put("Test UI").endl();
        cli.OutputDevice.getStdOut().put("-------").endl();

        MyServer cli_Server = new MyServer();
        if (! cli_Server.start()) return false;

        MyClient cli_Client = MyClient.createConnection();
        if (cli_Client == null) return false;
        if (! cli_Client.ui()) return false;
        if (! cli_Client.quit()) return false;

        if (! cli_Server.stop()) return false;

        cli.OutputDevice.getStdOut().put("ok").endl().endl();
        return true;
    }

    private static boolean test_MultiConnection() {
        cli.OutputDevice.getStdOut().put("--------------------").endl();
        cli.OutputDevice.getStdOut().put("Multiple connections").endl();
        cli.OutputDevice.getStdOut().put("--------------------").endl();

        MyServer cli_Server = new MyServer();
        if (! cli_Server.start()) return false;

        MyClient cli_Client1 = MyClient.createConnection();
        if (cli_Client1 == null) return false;
        MyClient cli_Client2 = MyClient.createConnection();
        if (cli_Client2 == null) return false;

        if (! cli_Client2.help()) return false;
        if (! cli_Client1.help()) return false;
        if (! cli_Client2.help()) return false;
        if (! cli_Client1.help()) return false;

        if (! cli_Client1.quit()) return false;
        if (! cli_Client2.quit()) return false;

        if (! cli_Server.stop()) return false;

        cli.OutputDevice.getStdOut().put("ok").endl().endl();
        return true;
    }

    private static boolean test_StopServer() {
        cli.OutputDevice.getStdOut().put("------------").endl();
        cli.OutputDevice.getStdOut().put("Close server").endl();
        cli.OutputDevice.getStdOut().put("------------").endl();

        MyServer cli_Server = new MyServer();
        if (! cli_Server.start()) return false;

        MyClient cli_Client = MyClient.createConnection();
        if (cli_Client == null) return false;
        if (! cli_Client.help()) return false;

        if (! cli_Server.stop()) return false;

        if (! cli_Client.finish()) return false;

        cli.OutputDevice.getStdOut().put("ok").endl().endl();
        return true;
    }
}

class MyCli extends cli.Cli {
    public MyCli(int I_CliNumber) {
        super("MyCli-" + new Integer(I_CliNumber), new cli.Help());
        this.populate();
    }
    public void populate() {
        cli.Keyword cli_UI = (cli.Keyword) addElement(new cli.Keyword("ui-line", new cli.Help()));
        cli_UI.addElement(new cli.Endl(new cli.Help()));
    }
    public boolean execute(cli.CommandLine CLI_CmdLine) {
        try {
            java.util.Iterator<cli.Element> cli_Elements = CLI_CmdLine.iterator();
            cli.Element cli_Element = cli_Elements.next();
            if (cli_Element == null) return false;
            if ((cli_Element instanceof cli.Keyword) && ((cli.Keyword) cli_Element).getKeyword().equals("ui-line")) {
                cli_Element = cli_Elements.next();
                if (cli_Element == null) return false;
                if (cli_Element instanceof cli.Endl) {
                    getShell().getStream(cli.Shell.PROMPT_STREAM).put("Enter line: ");
                    cli.ui.Line cli_Line = new cli.ui.Line("", 0, 20);
                    cli_Line.run(getShell());
                    return true;
                }
            }
        } catch (java.util.NoSuchElementException e1) {
        } catch (Exception e) {
            getErrorStream().printStackTrace(e);
        }
        return false;
    }
}

class MyServer extends cli.TelnetServer {
    public static final int PORT = 9012;

    public MyServer() {
        super(2, PORT, cli.ResourceString.LANG_EN);
        m_pjThread = null;
    }

    private Thread m_pjThread;
    public boolean start() {
        m_pjThread = new Thread() { public void run() {
            startServer();
        }};
        m_pjThread.start();
        return true;
    }
    public boolean stop() {
        stopServer();
        try {
            m_pjThread.join();
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
            return false;
        }
        return true;
    }

    private static int m_iConnectionCount = 0;
    protected cli.Shell onNewConnection(cli.TelnetConnection CLI_NewConnection) {
        cli.Cli cli_Cli = new MyCli(m_iConnectionCount);
        cli.Shell cli_Shell = new cli.Shell(cli_Cli);
        cli_Shell.setStream(cli.Shell.WELCOME_STREAM, cli.OutputDevice.getNullDevice());
        cli_Shell.setStream(cli.Shell.OUTPUT_STREAM, cli.OutputDevice.getNullDevice());
        m_iConnectionCount ++;
        return cli_Shell;
    }
    protected void onCloseConnection(cli.Shell CLI_Shell, cli.TelnetConnection CLI_ConnectionClosed) {
        m_iConnectionCount --;
    }
}

class MyClient extends java.net.Socket {
    public static MyClient createConnection() {
        try {
            MyClient cli_Client = new MyClient();
            if (! cli_Client.isBound()) {
                cli.OutputDevice.getStdErr().put("Client is not bound").endl();
                return null;
            }
            if (! cli_Client.isConnected()) {
                cli.OutputDevice.getStdErr().put("Client is not connected").endl();
                return null;
            }
            if (! cli_Client.getPrompt()) {
                return null;
            }

            return cli_Client;
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
            return null;
        }
    }

    private static int m_iClientCount = 0;
    private final int m_iCliNumber;
    private MyClient() throws java.net.UnknownHostException, java.io.IOException {
        super("localhost", MyServer.PORT);
        m_iCliNumber = m_iClientCount;
        m_iClientCount ++;
    }

    public boolean help() {
        if (! putBytes("help\n")) return false;
        if (! getEcho("help")) return false;
        if (! getPrompt()) return false;
        return true;
    }

    public boolean ui() {
        if (! putBytes("ui-line\n")) return false;
        if (! getEcho("ui-line")) return false;
        if (! putBytes("test line\n")) return false;
        if (! getEcho("Enter line: test line")) return false;
        if (! getPrompt()) return false;
        return true;
    }

    public boolean quit() {
        if (! putBytes("quit\n")) return false;
        if (! getEcho("quit")) return false;
        if (! finish()) return false;
        return true;
    }

    public boolean finish() {
        try {
            int i_Byte = getInputStream().read();
            if (i_Byte != -1) {
                cli.OutputDevice.getStdErr().put("Client is not closed").endl();
                return false;
            }
            close();
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
            return false;
        }
        m_iClientCount --;
        return true;
    }

    private boolean putBytes(String J_Out) {
        try {
            getOutputStream().write(J_Out.getBytes());
            return true;
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
            return false;
        }
    }

    private int getByte() {
        try {
            int i_Byte = getInputStream().read();
            if (i_Byte == -1) {
                return 0;
            }
            String j_Out = new String(); j_Out += (char) i_Byte;
            cli.OutputDevice.getStdOut().put(j_Out);
            return i_Byte;
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
            return 0;
        }
    }

    private boolean getPrompt() {
        // Read buffer until a '>' is found.
        String j_Buffer = new String();
        int i_Byte = 0;
        while (true) {
            i_Byte = getByte();
            if (i_Byte == 0) {
                return false;
            }
            if (i_Byte == '>') {
                // Check the last characters correspond to the correct CLI name.
                String j_Expected = "MyCli-" + new Integer(m_iCliNumber);
                if (! j_Buffer.endsWith(j_Expected)) {
                    cli.OutputDevice.getStdErr().put("Prompt '" + j_Expected + "' not found in '" + j_Buffer + "'").endl();
                    return false;
                }
                return true;
            } else {
                j_Buffer = j_Buffer + (char) i_Byte;
            }
        }
    }

    private boolean getEcho(String J_Command) {
        // Read buffer until a \n is found.
        String j_Buffer = new String();
        int i_Byte = 0;
        while (true) {
            i_Byte = getByte();
            if (i_Byte == 0) {
                return false;
            }
            if (i_Byte == '\n') {
                // Check the last characters correspond to the given command.
                if (! j_Buffer.startsWith(J_Command)) {
                    cli.OutputDevice.getStdErr().put("Echo '" + J_Command + "' not found in '" + j_Buffer + "'").endl();
                    return false;
                }
                return true;
            } else {
                j_Buffer = j_Buffer + (char) i_Byte;
            }
        }
    }
}
