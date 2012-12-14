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


/** Input/output device management.

    See OutputDevice comment. */
public abstract class IODevice {

    /** Generic input/output device interface. */
    public interface Interface extends OutputDevice.Interface {
        /** Input key capture.
            @return Key captured. */
        public int getKey();

        /** Location accessor.
            @return The location as a resource string. */
        public ResourceString getLocation();

        /** Stack overflow protection.
            @param CLI_Device Other device that the callee device should check it would input characters from.
            @return true if the callee device would redirect input to the given device for reading.

            Determines whether the current device would input the given device in any ways.
            Default implementation checks whether CLI_Device is the self device. */
        public boolean wouldInput(IODevice.Interface CLI_Device);
    }

    /** Native-implemented input/output devices. */
    public static abstract class Native extends OutputDevice.Native implements IODevice.Interface {
        /** Constructor.
            @param I_NativeRef Native instance reference. */
        protected Native(int I_NativeRef) {
            super(I_NativeRef);
        }

        // IODevice.Interface native input/output device implementation.

        public final int getKey() {
            return IODevice.__Native__getKey(this.getNativeRef());
        }

        public ResourceString getLocation() {
            ResourceString cli_Location = (ResourceString) NativeObject.getObject(IODevice.__Native__getLocation(this.getNativeRef()));
            NativeObject.forget(cli_Location);
            return cli_Location;
        }

        public boolean wouldInput(IODevice.Interface CLI_Device) {
            return IODevice.__Native__wouldInput(this.getNativeRef(), CLI_Device.getNativeRef());
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Native class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Native prefix.
    private static final native int __Native__getKey(int I_NativeDeviceRef);
    private static final native int __Native__getLocation(int I_NativeDeviceRef);
    private static final native boolean __Native__wouldInput(int I_NativeIODeviceRef, int I_NativeIODevice2Ref);

    /** Java-implemented input/output devices. */
    public static abstract class Java extends OutputDevice.Java implements IODevice.Interface {
        /** Constructor.
            @param J_DbgName Debug name. Useful for traces only. */
        protected Java(String J_DbgName) {
            super(IODevice.__Java__Java(J_DbgName));
        }

        /** Constructor for NonBlockingIODevice classes only.
            @param I_NativeRef Native instance reference. */
        protected Java(int I_NativeRef) {
            super(I_NativeRef);
        }

        /** Destructor. */
        protected void finalize() throws Throwable {
            if (getbDoFinalize()) {
                IODevice.__Java__finalize(this.getNativeRef());
                dontFinalize(); // finalize once.
            }
            super.finalize();
        }

        // IODevice.Interface Java input/output device implementation.

        public abstract int getKey();
        private final int __getKey() {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("IODevice.Java.__getKey()"));
            int e_Key = getKey();
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("IODevice.Java.__getKey()", new Integer(e_Key).toString()));
            return e_Key;
        }

        public abstract ResourceString getLocation();
        private final void __getLocation(int I_NativeResourceStringRef) {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("IODevice.Java.__getLocation(I_NativeResourceStringRef)"));
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.param("I_NativeResourceStringRef", new Integer(I_NativeResourceStringRef).toString()));
            // Retrieve the native resource string instance (output parameter).
            ResourceString cli_NativeLocation = (ResourceString) NativeObject.getObject(I_NativeResourceStringRef);
            if (cli_NativeLocation != null) {
                // Make the call to the Java handler.
                ResourceString cli_JavaLocation = getLocation();
                if (cli_JavaLocation != null) {
                    // Report information from Java to native instance.
                    for (int e_Lang = 0; e_Lang < ResourceString.LANG_COUNT; e_Lang++) {
                        Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.value("e_Lang", new Integer(e_Lang).toString()));
                        String j_Location = cli_JavaLocation.getString(e_Lang);
                        Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.value("j_Location", j_Location));
                        cli_NativeLocation.setString(e_Lang, j_Location);
                    }
                }
            }
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("IODevice.Java.__getLocation()"));
        }

        public boolean wouldInput(IODevice.Interface CLI_Device) {
            return (CLI_Device == this);
        }
        private final boolean __wouldInput(int I_NativeDeviceRef) {
            // Do not trace! for consistency reasons.
            //Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__wouldOutput(CLI_Device)"));
            boolean b_Res = false;
            try {
                IODevice.Interface cli_Device = (IODevice.Interface) NativeObject.getObject(I_NativeDeviceRef);
                if (cli_Device != null) {
                    b_Res = wouldInput(cli_Device);
                }
            } catch (Exception e) {
            }
            //Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__wouldOutput()"));
            return b_Res;
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Java class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Java prefix.
    private static final native int __Java__Java(String J_DbgName);
    private static final native void __Java__finalize(int I_NativeConsoleRef);


    // General static input/output device features.

    /** Null device singleton.
        @return The null output device. */
    public static final IODevice.Interface getNullDevice() {
        class NullDevice extends IODevice.Native {
            public NullDevice() {
                super(__getNullDevice());
            }
        }
        if (m_cliNullDevice == null) {
            m_cliNullDevice = new NullDevice();
        }
        return m_cliNullDevice;
    }
    private static final native int __getNullDevice();
    private static IODevice.Interface m_cliNullDevice = null;

    /** Standard input device singleton.
        @return The standard input device. */
    public static final IODevice.Interface getStdIn() {
        class StdInDevice extends IODevice.Native {
            public StdInDevice() {
                super(__getStdIn());
            }
        }
        if (m_cliStdIn == null) {
            m_cliStdIn = new StdInDevice();
        }
        return m_cliStdIn;
    }
    private static final native int __getStdIn();
    private static IODevice.Interface m_cliStdIn = null;
}
