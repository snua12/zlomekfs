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

package cli.ui;


/** Generic user interface class. */
abstract class UI extends cli.NativeObject implements cli.NonBlockingIODevice.KeyReceiver {

    /** Default constructor. */
    protected UI(int I_NativeRef) {
        super(I_NativeRef);
    }

    /** Runs within the context of a running shell.
        @param CLI_Shell Shell context.
        @return true for a regular output, false for an error or a cancellation. */
    public boolean run(cli.Shell CLI_Shell) {
        return __run(this.getNativeRef(), CLI_Shell.getNativeRef());
    }
    private static final native boolean __run(int I_NativeUIRef, int I_NativeShellRef);

    // NonBlockingKeyReceiver interface implementation.
    public void onNonBlockingKey(cli.NonBlockingIODevice.Interface CLI_Source, int E_KeyCode) {
        __onNonBlockingKey(this.getNativeRef(), CLI_Source.getNativeRef(), E_KeyCode);
    }
    private static final native void __onNonBlockingKey(int I_NativeUIRef, int I_NativeSourceDeviceRef, int E_KeyCode);

}
