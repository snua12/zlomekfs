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


/** CLI element class. */
public abstract class Element extends NativeObject
{
    /** Constructor.
        @param I_NativeRef  Native object reference. */
    protected Element(int I_NativeRef) {
        super(I_NativeRef);
    }

    /** Element name accessor.
        @return Element name. */
    public final String getKeyword() {
        return __getKeyword(this.getNativeRef());
    }
    private static final native String __getKeyword(int I_NativeElementRef);

    /** Element help accessor.
        @return Element help. */
    public final Help getHelp() {
        Help cli_Help = (Help) NativeObject.getObject(__getHelp(this.getNativeRef()));
        // This help instance will not be deleted by the native code.
        NativeObject.forget(cli_Help);
        return cli_Help;
    }
    private static final native int __getHelp(int I_NativeElementRef);

    /** Owner CLI accessor.
        @return Owner CLI. */
    public final Cli getCli() {
        return (Cli) NativeObject.getObject(__getCli(this.getNativeRef()));
    }
    private static final native int __getCli(int I_NativeElementRef);

    /** Running shell accessor.
        @return Running shell. */
    public final Shell getShell() {
        return (Shell) NativeObject.getObject(__getShell(this.getNativeRef()));
    }
    private static final native int __getShell(int I_NativeElementRef);

    /** Current output stream accessor.
        @return Current output stream. */
    public final OutputDevice.Interface getOutputStream() {
        return (OutputDevice.Interface) NativeObject.getObject(__getOutputStream(this.getNativeRef()));
    }
    private static final native int __getOutputStream(int I_NativeElementRef);

    /** Current error stream accessor.
        @return Current error stream. */
    public final OutputDevice.Interface getErrorStream() {
        return (OutputDevice.Interface) NativeObject.getObject(__getErrorStream(this.getNativeRef()));
    }
    private static final native int __getErrorStream(int I_NativeElementRef);
}
