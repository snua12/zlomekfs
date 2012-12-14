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


/** Menu class.
    A menu owns Element instances, which owns next Element instances... terminated by an Endl instance.
    Command lines are defined this way.
    When the shell analyzes a command line, it asks the corresponding menu to execute it through the abstract execute() method.
    The menu class also offers a handler to execute special operations when the menu exits. */
public abstract class Menu extends SyntaxNode {

    /** Constructor from native instance.
        @param I_NativeRef Native instance reference. */
    protected Menu(int I_NativeRef) {
        super(I_NativeRef);
    }
    /** Constructor from Java.
        @param J_Name Name of the menu.
        @param CLI_Help Help attached to the menu. */
    public Menu(String J_Name, Help CLI_Help) {
        super(__Menu(J_Name, CLI_Help.getNativeRef()));
    }
    private static final native int __Menu(String J_Help, int I_NativeHelpRef);

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeMenuRef);

    /** Menu name access.
        @return Name of the menu. */
    public String getName() {
        return getKeyword();
    }

    /** Populate abstract method.
        This method should be overridden by final menu classes
        to describe the elements defining the command lines attached to the menu. */
    public abstract void populate();

    /** Command line execution.
        This method should be overriden by final menu classes.
        @param CLI_CmdLine Command line to execute.
        @return true if the command line has been executed successfully (syntactically speaking),
                false otherwise. */
    public abstract boolean execute(CommandLine CLI_CmdLine);
    private final boolean __execute(int I_NativeCmdLineRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Menu.__execute(I_NativeCmdLineRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeCmdLineRef", new Integer(I_NativeCmdLineRef).toString()));

        boolean b_Res = false;
        try {
            NativeObject cli_CommandLine = NativeObject.getObject(I_NativeCmdLineRef);
            if (cli_CommandLine != null) {
                Traces.trace(NativeTraces.CLASS, NativeTraces.value("cli_CommandLine", cli_CommandLine.toString()));
                if (cli_CommandLine instanceof CommandLine) {
                    b_Res = execute((CommandLine) cli_CommandLine);
                }
            } else {
                getErrorStream().put("Could not find CommandLine reference " + new Integer(I_NativeCmdLineRef)).endl();
            }
        } catch (Exception e) {
            getErrorStream().printStackTrace(e);
        }

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Menu.__execute()", new Boolean(b_Res).toString()));
        return b_Res;
    }

    /** Handler called when the menu exits.
        This method may be overriden by final menu classes. */
    public void onExit() {
    }
    private final void __onExit() {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Menu.__onExit()"));
        onExit();
        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Menu.__onExit()"));
    }

    /** Handler called when then menu displays its prompt.
        @return The prompt to display. An empty string for the default prompt. */
    public String onPrompt() {
        return "";
    }
    private final String __onPrompt() {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Menu.__onPrompt()"));
        String j_Prompt = onPrompt();
        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Menu.__onPrompt()", j_Prompt));
        return j_Prompt;
    }

}
