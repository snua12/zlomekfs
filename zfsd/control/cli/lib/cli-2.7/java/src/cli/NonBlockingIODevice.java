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


/** Non-blocking input device. */
public abstract class NonBlockingIODevice {

    /** Non-blocking key receiver interface. */
    public interface KeyReceiver {
        /** Hook called by non-blocking devices on character input.
            @param CLI_Source Input non-blocking device.
            @param E_KeyCode Input key. */
        public abstract void onNonBlockingKey(NonBlockingIODevice.Interface CLI_Source, int E_KeyCode);
    }

    /** Generic input/output device interface. */
    public interface Interface extends IODevice.Interface {
        /** Key receiver registration.
            @param CLI_KeyReceiver Key receiver to register.

            !!! Warning !!! Should be called by key receivers only. */
        public void attachKeyReceiver(KeyReceiver CLI_KeyReceiver);

        /**  Key receiver unregistration.
            @param CLI_KeyReceiver Key receiver to unregister.

            !!! Warning !!! Should be called by key receivers only. */
        public void detachKeyReceiver(KeyReceiver CLI_KeyReceiver);

        /** Returns the current key receiver.
            @return Current key receiver if any, null otherwise. */
        public KeyReceiver getKeyReceiver();

        /** Returns the registered shell (if any).
            @return Registered shell if any, null otherwise. */
        public Shell getShell();

        /** Handler to call when a key is received.
            @param E_Key Input key. */
        public void onKey(int E_Key);

        /** When a blocking call requires keys to be entered before returning, this method makes the thread waits smartly depending on the integration context.
            @param I_Milli Number of milliseconds to wait.
            @return false when the caller should not wait for keys anymore, true otherwise.

            If a key has been entered during the waiting time, this method MUST call onKey(), then SHALL stop waiting and return right away.

            !!! Warning !!! This kind of wait implementation may cause nested peek message loops, but if you are using cli.ui features, you might need to implement such loop. */
        public boolean waitForKeys(int I_Milli);
    }

    // Common behaviours of non-blocking intput/output devices, whatever their location of implementation. */
    private static final native void __Common__attachKeyReceiver(int I_NativeDeviceRef, int I_NativeKeyReceiverRef);
    private static final native void __Common__detachKeyReceiver(int I_NativeDeviceRef, int I_NativeKeyReceiverRef);
    private static final native int __Common__getKeyReceiver(int I_NativeDeviceRef);
    private static final native int __Common__getShell(int I_NativeDeviceRef);
    private static final native void __Common__onKey(int I_NativeDeviceRef, int E_Key);

    /** Native-implemented input/output devices. */
    public static abstract class Native extends IODevice.Native implements NonBlockingIODevice.Interface {
        /** Constructor.
            @param I_NativeRef Native instance reference. */
        public Native(int I_NativeRef) {
            super(I_NativeRef);
        }

        // IODevice.Interface native input/output device implementation.
        public void attachKeyReceiver(KeyReceiver CLI_KeyReceiver) {
            if (CLI_KeyReceiver instanceof NativeObject) {
                NonBlockingIODevice.__Common__attachKeyReceiver(this.getNativeRef(), ((NativeObject) CLI_KeyReceiver).getNativeRef());
            }
        }

        public void detachKeyReceiver(KeyReceiver CLI_KeyReceiver) {
            if (CLI_KeyReceiver instanceof NativeObject) {
                NonBlockingIODevice.__Common__detachKeyReceiver(this.getNativeRef(), ((NativeObject) CLI_KeyReceiver).getNativeRef());
            }
        }

        public KeyReceiver getKeyReceiver() {
            NativeObject cli_KeyReceiver = NativeObject.getObject(NonBlockingIODevice.__Common__getKeyReceiver(this.getNativeRef()));
            if (cli_KeyReceiver instanceof KeyReceiver) {
                return (KeyReceiver) cli_KeyReceiver;
            } else {
                return null;
            }
        }

        public Shell getShell() {
            NativeObject cli_Shell = NativeObject.getObject(NonBlockingIODevice.__Common__getShell(this.getNativeRef()));
            if (cli_Shell instanceof Shell) {
                return (Shell) cli_Shell;
            } else {
                return null;
            }
        }

        public void onKey(int E_Key) {
            NonBlockingIODevice.__Common__onKey(this.getNativeRef(), E_Key);
        }

        public boolean waitForKeys(int I_Milli) {
            return NonBlockingIODevice.__Native__waitForKeys(this.getNativeRef(), I_Milli);
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Native class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Native prefix.
    private static final native boolean __Native__waitForKeys(int I_NativeDeviceRef, int I_Milli);

    /** Java-implemented non blocking input/output devices. */
    public static abstract class Java extends IODevice.Java implements NonBlockingIODevice.Interface {
        /** Constructor.
            @param STR_DbgName Debug name. */
        public Java(String STR_DbgName) {
            super(NonBlockingIODevice.__Java__Java(STR_DbgName));
        }

        /** Destructor. */
        protected void finalize() throws Throwable {
            if (getbDoFinalize()) {
                NonBlockingIODevice.__Java__finalize(this.getNativeRef());
                dontFinalize(); // finalize once.
            }
            super.finalize();
        }

        // NonBlockingIODevice.Interface Java input/output device implementation.

        public int getKey() {
            return NonBlockingIODevice.__Java__getKey(this.getNativeRef());
        }

        public void attachKeyReceiver(KeyReceiver CLI_KeyReceiver) {
            if (CLI_KeyReceiver instanceof NativeObject) {
                NonBlockingIODevice.__Common__attachKeyReceiver(this.getNativeRef(), ((NativeObject) CLI_KeyReceiver).getNativeRef());
            }
        }

        public void detachKeyReceiver(KeyReceiver CLI_KeyReceiver) {
            if (CLI_KeyReceiver instanceof NativeObject) {
                NonBlockingIODevice.__Common__detachKeyReceiver(this.getNativeRef(), ((NativeObject) CLI_KeyReceiver).getNativeRef());
            }
        }

        public KeyReceiver getKeyReceiver() {
            NativeObject cli_KeyReceiver = NativeObject.getObject(NonBlockingIODevice.__Common__getKeyReceiver(this.getNativeRef()));
            if (cli_KeyReceiver instanceof KeyReceiver) {
                return (KeyReceiver) cli_KeyReceiver;
            } else {
                return null;
            }
        }

        public Shell getShell() {
            NativeObject cli_Shell = NativeObject.getObject(NonBlockingIODevice.__Common__getShell(this.getNativeRef()));
            if (cli_Shell instanceof Shell) {
                return (Shell) cli_Shell;
            } else {
                return null;
            }
        }

        public void onKey(int E_Key) {
            NonBlockingIODevice.__Common__onKey(this.getNativeRef(), E_Key);
        }

        public abstract boolean waitForKeys(int I_Milli);
        private final boolean __waitForKeys(int I_Milli) {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("NonBlockingIODevice.Java.__waitForKeys()"));
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.param("I_Milli", new Integer(I_Milli).toString()));

            boolean b_Res = waitForKeys(I_Milli);

            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("NonBlockingIODevice.Java.__waitForKeys()", new Boolean(b_Res).toString()));
            return b_Res;
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Native class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Native prefix.
    private static final native int __Java__Java(String STR_DbgName);
    private static final native void __Java__finalize(int I_NativeDeviceRef);
    private static final native int __Java__getKey(int I_NativeDeviceRef);
}
