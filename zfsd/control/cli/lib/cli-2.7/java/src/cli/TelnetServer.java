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

package cli;


/** Telnet server class.

    Virtual object that shall be overridden for shell (and cli) instance creations. */
public abstract class TelnetServer extends NativeObject
{
    /** Constructor.
        @param I_MaxConnections Maximum number of connections managed at the same time.
        @param I_TcpPort TCP port to listen onto.
        @param E_Lang Debugging language. */
    public TelnetServer(int I_MaxConnections, int I_TcpPort, int E_Lang) {
        super(__TelnetServer(I_MaxConnections, I_TcpPort, E_Lang));
    }
    private static final native int __TelnetServer(int I_MaxConnections, int I_TcpPort, int E_Lang);

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeServerRef);

    /** Starts the server.
        Warning: blocking call. */
    public void startServer() {
        try {
            __startServer(this.getNativeRef());
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
        }
    }
    private static final native void __startServer(int I_NativeServerRef);

    /** Stops the server */
    public void stopServer() {
        try {
            __stopServer(this.getNativeRef());
        } catch (Exception e) {
            cli.OutputDevice.getStdErr().printStackTrace(e);
        }
    }
    private static final native void __stopServer(int I_NativeServerRef);

    /** Shell (and cli) creation handler.
        @param CLI_NewConnection New telnet connection to create a shell for.
        @return Shell created for the given connection. */
    protected abstract Shell onNewConnection(TelnetConnection CLI_NewConnection);
    private final int __onNewConnection(int I_NativeConnectionRef) {
        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.begin("TelnetServer.__onNewConnection(I_NativeConnectionRef)"));
        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.param("I_NativeConnectionRef", new Integer(I_NativeConnectionRef).toString()));

        int i_NativeShellRef = 0;
        NativeObject cli_Connection = NativeObject.getObject(I_NativeConnectionRef);
        if ((cli_Connection != null) && (cli_Connection instanceof TelnetConnection)) {
            Shell cli_Shell = onNewConnection((TelnetConnection) cli_Connection);
            if (cli_Shell != null) {
                i_NativeShellRef = cli_Shell.getNativeRef();
            }
        }

        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.end("TelnetServer.__onNewConnection()", new Integer(i_NativeShellRef).toString()));
        return i_NativeShellRef;
    }
    private final native int __foo(int I_Bar);

    /** Shell (and cli) release handler.
        @param CLI_Shell Shell (and cli) to be released.
        @param CLI_ConnectionClosed Telnet connection being closed. */
    protected abstract void onCloseConnection(Shell CLI_Shell, TelnetConnection CLI_ConnectionClosed);
    private final void __onCloseConnection(int I_NativeShellRef, int I_NativeConnectionRef) {
        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.begin("TelnetServer.__onCloseConnection(I_NativeShellRef, I_NativeConnectionRef)"));
        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.param("I_NativeShellRef", new Integer(I_NativeShellRef).toString()));
        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.param("I_NativeConnectionRef", new Integer(I_NativeConnectionRef).toString()));

        NativeObject cli_Shell = NativeObject.getObject(I_NativeShellRef);
        NativeObject cli_ConnectionClosed = NativeObject.getObject(I_NativeConnectionRef);
        if (    (cli_Shell != null) && (cli_Shell instanceof Shell)
                && (cli_ConnectionClosed != null) && (cli_ConnectionClosed instanceof TelnetConnection)) {
            onCloseConnection((Shell) cli_Shell, (TelnetConnection) cli_ConnectionClosed);
        }

        Traces.safeTrace(NativeTraces.CLASS, I_NativeConnectionRef, NativeTraces.end("TelnetServer.__onCloseConnection()"));
    }
}
