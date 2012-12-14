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


/** Trace class description. */
public class TraceClass extends NativeObject
{
    /** Copy constructor.
        @param CLI_Class Source class object. */
    public TraceClass(TraceClass CLI_Class) {
        super(__TraceClass(CLI_Class.getClassName(), CLI_Class.getHelp().getNativeRef()));
    }
    /** Constructor with a trace class name and a corresponding help given.
        @param STR_ClassName Trace class name.
        @param CLI_Help Corresponding help objet. */
    public TraceClass(String STR_ClassName, Help CLI_Help) {
        super(__TraceClass(STR_ClassName, CLI_Help.getNativeRef()));
    }
    private static final native int __TraceClass(String STR_ClassName, int I_NativeHelpRef);

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeTraceClassRef);

    /** Class name accessor.
        @return Class name. null if an error occured. */
    public String getClassName() {
        return __getClassName(getNativeRef());
    }
    private static final native String __getClassName(int I_NativeTraceClassRef);

    /** Help string accessor.
        @return Help string. null if an error occured. */
    public Help getHelp() {
        Help cli_Help = (Help) NativeObject.getObject(__getHelp(this.getNativeRef()));
        // This trace class instance will not be deleted by the native code.
        NativeObject.forget(cli_Help);
        return cli_Help;
    }
    private static final native int __getHelp(int I_NativeTraceClassRef);
}
