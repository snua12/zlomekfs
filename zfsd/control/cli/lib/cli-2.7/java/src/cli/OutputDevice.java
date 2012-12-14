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


/** Output device management.

    Because executions can go in both ways:
        - from the Java wrapper class to the native implementation of a basic output device (SingleCommand, OutputFileDevice...)
        - from native code to a handler of a Java implementation of an output device,
    two distinct classes, OutputDevice.Native and OutputDevice.Java have been defined in order to clearly seperate output devices depending on where their actual implementation is located.
    Inheritance from NativeObject is made through a OutputDevice.Common class, which implements all common behaviours between Native and Java implementations.
    An interface federates them independently of their inheritance from NativeObject.
    Static common behaviours remain in the main class, which is declared abstract in order to constitute a sort of package. */
public abstract class OutputDevice {

    /** Null key. */
    public static final int NULL_KEY = '\0';
    /** Break (Ctrl+C). */
    public static final int BREAK = 3;
    /** Logout (Ctrl+D). */
    public static final int LOGOUT = 4;
    /** Enter. */
    public static final int ENTER = 13;
    /** Escape. */
    public static final int ESCAPE = 27;
    /** Space. */
    public static final int SPACE = 32;
    /** Backspace (changed from '\b' to 8 in version 2.7 for ASCII compliance). */
    public static final int BACKSPACE = 8;
    /** Delete key (changed from 128 to 127 in version 2.7 for ASCII compliance). */
    public static final int DELETE = 127;
    /** Clean screen key (changed from 129 to 501 in order to avoid overlap with printable ASCII characters). */
    public static final int CLS = 501;
    /** Insert key (changed from 500 to 502 in order to avoid overlap with printable ASCII characters). */
    public static final int INSERT = 502;

    public static final int TAB = '\t';
    public static final int KEY_0 = '0';
    public static final int KEY_1 = '1';
    public static final int KEY_2 = '2';
    public static final int KEY_3 = '3';
    public static final int KEY_4 = '4';
    public static final int KEY_5 = '5';
    public static final int KEY_6 = '6';
    public static final int KEY_7 = '7';
    public static final int KEY_8 = '8';
    public static final int KEY_9 = '9';
    public static final int KEY_a = 'a';
    public static final int KEY_aacute = 'á';
    public static final int KEY_agrave = 'à';
    public static final int KEY_auml = 'ä';
    public static final int KEY_acirc = 'â';
    public static final int KEY_b = 'b';
    public static final int KEY_c = 'c';
    public static final int KEY_ccedil = 'ç';
    public static final int KEY_d = 'd';
    public static final int KEY_e = 'e';
    public static final int KEY_eacute = 'é';
    public static final int KEY_egrave = 'è';
    public static final int KEY_euml = 'ë';
    public static final int KEY_ecirc = 'ê';
    public static final int KEY_f = 'f';
    public static final int KEY_g = 'g';
    public static final int KEY_h = 'h';
    public static final int KEY_i = 'i';
    public static final int KEY_iacute = 'í';
    public static final int KEY_igrave = 'ì';
    public static final int KEY_iuml = 'ï';
    public static final int KEY_icirc = 'î';
    public static final int KEY_j = 'j';
    public static final int KEY_k = 'k';
    public static final int KEY_l = 'l';
    public static final int KEY_m = 'm';
    public static final int KEY_n = 'n';
    public static final int KEY_o = 'o';
    public static final int KEY_oacute = 'ó';
    public static final int KEY_ograve = 'ò';
    public static final int KEY_ouml = 'ö';
    public static final int KEY_ocirc = 'ô';
    public static final int KEY_p = 'p';
    public static final int KEY_q = 'q';
    public static final int KEY_r = 'r';
    public static final int KEY_s = 's';
    public static final int KEY_t = 't';
    public static final int KEY_u = 'u';
    public static final int KEY_uacute = 'ú';
    public static final int KEY_ugrave = 'ù';
    public static final int KEY_uuml = 'ü';
    public static final int KEY_ucirc = 'û';
    public static final int KEY_v = 'v';
    public static final int KEY_w = 'w';
    public static final int KEY_x = 'x';
    public static final int KEY_y = 'y';
    public static final int KEY_z = 'z';

    public static final int KEY_A = 'A';
    public static final int KEY_B = 'B';
    public static final int KEY_C = 'C';
    public static final int KEY_D = 'D';
    public static final int KEY_E = 'E';
    public static final int KEY_F = 'F';
    public static final int KEY_G = 'G';
    public static final int KEY_H = 'H';
    public static final int KEY_I = 'I';
    public static final int KEY_J = 'J';
    public static final int KEY_K = 'K';
    public static final int KEY_L = 'L';
    public static final int KEY_M = 'M';
    public static final int KEY_N = 'N';
    public static final int KEY_O = 'O';
    public static final int KEY_P = 'P';
    public static final int KEY_Q = 'Q';
    public static final int KEY_R = 'R';
    public static final int KEY_S = 'S';
    public static final int KEY_T = 'T';
    public static final int KEY_U = 'U';
    public static final int KEY_V = 'V';
    public static final int KEY_W = 'W';
    public static final int KEY_X = 'X';
    public static final int KEY_Y = 'Y';
    public static final int KEY_Z = 'Z';

    public static final int PLUS = '+';
    public static final int MINUS = '-';
    public static final int STAR = '*';
    public static final int SLASH = '/';
    public static final int LOWER_THAN = '<';
    public static final int GREATER_THAN = '>';
    public static final int EQUAL = '=';
    public static final int PERCENT = '%';

    public static final int UNDERSCORE = '_';
    public static final int AROBASE = '@';
    public static final int SHARP = '#';
    public static final int AMPERCENT = '&';
    public static final int DOLLAR = '$';
    public static final int BACKSLASH = '\\';
    public static final int PIPE = '|';
    public static final int TILDE = '~';
    public static final int SQUARE = '²';
    public static final int EURO = '€';
    public static final int POUND = '£';
    public static final int MICRO = 'µ';
    public static final int PARAGRAPH = '§';
    public static final int DEGREE = '°';
    public static final int COPYRIGHT = '©';

    public static final int QUESTION = '?';
    public static final int EXCLAMATION = '!';
    public static final int COLUMN = ':';
    public static final int DOT = '.';
    public static final int COMA = ',';
    public static final int SEMI_COLUMN = ';';
    public static final int QUOTE = '\'';
    public static final int DOUBLE_QUOTE = '"';
    public static final int BACK_QUOTE = '`';

    public static final int OPENING_BRACE = '(';
    public static final int CLOSING_BRACE = ')';
    public static final int OPENING_CURLY_BRACE = '{';
    public static final int CLOSING_CURLY_BRACE = '}';
    public static final int OPENING_BRACKET = '[';
    public static final int CLOSING_BRACKET = ']';

    /** Up arrow key. */
    public static final int KEY_UP = 1001;
    /** Down arrow key. */
    public static final int KEY_DOWN = 1002;
    /** Left arrow key. */
    public static final int KEY_LEFT = 1003;
    /** Right arrow key. */
    public static final int KEY_RIGHT = 1004;
    /** Page up arrow key. */
    public static final int PAGE_UP = 1005;
    /** Page down arrow key. */
    public static final int PAGE_DOWN = 1006;
    /** Page left arrow key. */
    public static final int PAGE_LEFT = 1007;
    /** Page right arrow key. */
    public static final int PAGE_RIGHT = 1008;

    /** Begin key. */
    public static final int KEY_BEGIN = 1020;
    /** End key. */
    public static final int KEY_END = 1021;

    /** Copy. */
    public static final int COPY = 2001;
    /** Cut. */
    public static final int CUT = 2002;
    /** Paste. */
    public static final int PASTE = 2003;

    /** Undo. */
    public static final int UNDO = 2004;
    /** Redo. */
    public static final int REDO = 2005;
    /** Previous key. */
    public static final int PREVIOUS = 2006;
    /** Forward key. */
    public static final int NEXT = 2007;

    public static final int F1 = 0x0f000001;
    public static final int F2 = 0x0f000002;
    public static final int F3 = 0x0f000003;
    public static final int F4 = 0x0f000004;
    public static final int F5 = 0x0f000005;
    public static final int F6 = 0x0f000006;
    public static final int F7 = 0x0f000007;
    public static final int F8 = 0x0f000008;
    public static final int F9 = 0x0f000009;
    public static final int F10 = 0x0f00000a;
    public static final int F11 = 0x0f00000b;
    public static final int F12 = 0x0f00000c;


    /** Generic output device interface. */
    public interface Interface {
        /** Device opening handler.
            Should rather be protected, but this is not possible in a Java interface.
            @return true for success, false otherwise. */
        public boolean openDevice();

        /** Device closure handler.
            Should rather be protected, but this is not possible in a Java interface.
            @return true for success, false otherwise. */
        public boolean closeDevice();

        /** Pushes characters to the output device.
            @param J_Text String to be displayed by the device.
            @return The output device itself. */
        public OutputDevice.Interface put(String J_Text);

        /** Pushes an integer value to be displayed by the output device.
            @param J_Integer Integer value to be displayed by the device.
            @return The output device itself. */
        public OutputDevice.Interface put(Integer J_Integer);

        /** Pushes a float value to be displayed by the output device.
            @param J_Float Float value to be displayed by the device.
            @return The output device itself. */
        public OutputDevice.Interface put(Float J_Float);

        /** Pushes a double value to be displayed by the output device.
            @param J_Double Double value to be displayed by the device.
            @return The output device itself. */
        public OutputDevice.Interface put(Double J_Double);

        /** Pushes an end of line to be displayed by the output device.
            @return The output device itself. */
        public OutputDevice.Interface endl();

        /** Makes the output device beep. */
        public void beep();

        /** Cleans the screen. */
        public void cleanScreen();

        /** Screen info accessor.
            @return Screen info with possible ScreenInfo::UNKNOWN values. */
        public ScreenInfo getScreenInfo();

        /** Stack overflow protection.
            @param CLI_Device Other device that the callee device should check it would output characters to.
            @return true if the callee device would redirect characters to the given device for output.

            Determines whether the current device would output the given device in anyway.
            Default implementation checks whether this is the self device. */
        public boolean wouldOutput(OutputDevice.Interface CLI_Device);

        /** Exception stack trace display.
            @param J_Exception Exception which stack trace to display. */
        public void printStackTrace(Exception J_Exception);

        /** Native object interface compliance.
            @return The native reference of the instance. */
        public int getNativeRef();
    }

    /** Screen information. */
    public final static class ScreenInfo extends NativeObject {

        /** Unknown value constant for either width or height. */
        public static final int UNKNOWN = -1;
        /** Default width constant. */
        public static final int DEFAULT_WIDTH = 80;
        /** Default height constant. */
        public static final int DEFAULT_HEIGHT = 20;

        /** Constructor.
            @param I_Width Width of screen. Can be UNKNOWN.
            @param I_Height Height of screen. Can be UNKNOWN.
            @param B_TrueCls True when an efficient CleanScreen() operation is implemented.
            @param B_WrapLines True when the line automatically goes down when the cursor reached the right end of the screen. */
        public ScreenInfo(int I_Width, int I_Height, boolean B_TrueCls, boolean B_WrapLines) {
            super(OutputDevice.__ScreenInfo__ScreenInfo(I_Width, I_Height, B_TrueCls, B_WrapLines));
        }

        /** Creation from native code.
            Useful for help members of other classes.
            @param I_NativeScreenInfoRef Native instance reference. */
        protected static void createFromNative(int I_NativeScreenInfoRef) {
            Traces.trace(NativeTraces.CLASS, NativeTraces.begin("OutputDevice.ScreenInfo.createFromNative(I_NativeScreenInfoRef)"));
            Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeScreenInfoRef", new Integer(I_NativeScreenInfoRef).toString()));

            NativeObject.createdFromNative(new ScreenInfo(I_NativeScreenInfoRef));

            Traces.trace(NativeTraces.CLASS, NativeTraces.end("OutputDevice.ScreenInfo.createFromNative()"));
        }
        /** Constructor for createFromNative(). */
        protected ScreenInfo(int I_NativeScreenInfoRef) {
            super(I_NativeScreenInfoRef);
        }

        /** Destructor. */
        protected void finalize() throws Throwable {
            if (getbDoFinalize()) {
                OutputDevice.__ScreenInfo__finalize(this.getNativeRef());
                dontFinalize(); // finalize once.
            }
            super.finalize();
        }

        /** Destruction from native code.
            See createFromNative(). */
        protected static void deleteFromNative(int I_NativeScreenInfoRef) {
            Traces.trace(NativeTraces.CLASS, NativeTraces.begin("OutputDevice.ScreenInfo.deleteFromNative(I_NativeScreenInfoRef)"));
            Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeScreenInfoRef", new Integer(I_NativeScreenInfoRef).toString()));

            NativeObject.deletedFromNative(NativeObject.getObject(I_NativeScreenInfoRef));

            Traces.trace(NativeTraces.CLASS, NativeTraces.end("OutputDevice.ScreenInfo.deleteFromNative()"));
        }

        /** Assignment method.
            @param CLI_ScreenInfo Screen info object to copy. */
        public void copy(ScreenInfo CLI_ScreenInfo) {
            if (CLI_ScreenInfo != null) {
                OutputDevice.__ScreenInfo__copy(this.getNativeRef(), CLI_ScreenInfo.getNativeRef());
            }
        }

        /** Screen width accessor.
            @return Screen width if known, UNKNOWN otherwise. */
        public int getWidth() {
            return OutputDevice.__ScreenInfo__getWidth(this.getNativeRef());
        }

        /** Safe screen width accessor.
            @return Screen width if known, default value otherwise. */
        public int getSafeWidth() {
            return OutputDevice.__ScreenInfo__getSafeWidth(this.getNativeRef());
        }

        /** Screen height accessor.
            @return Screen height if known, UNKNOWN otherwise. */
        public int getHeight() {
            return OutputDevice.__ScreenInfo__getHeight(this.getNativeRef());
        }

        /** Safe screen height accessor.
            @return Screen height if known, default value otherwise. */
        public int getSafeHeight() {
            return OutputDevice.__ScreenInfo__getSafeHeight(this.getNativeRef());
        }

        /** True cleanScreen() characteristic accessor.
            @return True cleanScreen() characteristic. */
        public boolean getbTrueCls() {
            return OutputDevice.__ScreenInfo__getbTrueCls(this.getNativeRef());
        }

        /** Lines wrapping characteristic accessor.
            @return True when the line automatically goes down when the cursor reached the right end of the screen. */
        public boolean getbWrapLines() {
            return OutputDevice.__ScreenInfo__getbWrapLines(this.getNativeRef());
        }
    };
    // JNI seems to have trouble at linking following methods when they are embedded in the nested ScreenInfo class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __ScreenInfo prefix.
    private static final native int __ScreenInfo__ScreenInfo(int I_Width, int I_Height, boolean B_TrueCls, boolean B_WrapLines);
    private static final native void __ScreenInfo__finalize(int I_NativeScreenInfoRef);
    private static final native void __ScreenInfo__copy(int I_NativeScreenInfoRef1, int I_NativeScreenInfoRef2);
    private static final native int __ScreenInfo__getWidth(int I_NativeScreenInfoRef);
    private static final native int __ScreenInfo__getSafeWidth(int I_NativeScreenInfoRef);
    private static final native int __ScreenInfo__getHeight(int I_NativeScreenInfoRef);
    private static final native int __ScreenInfo__getSafeHeight(int I_NativeScreenInfoRef);
    private static final native boolean __ScreenInfo__getbTrueCls(int I_NativeScreenInfoRef);
    private static final native boolean __ScreenInfo__getbWrapLines(int I_NativeScreenInfoRef);

    /** Class containing all common behaviours of output devices, whatever their location of implementation. */
    public static abstract class Common extends NativeObject {
        /** Constructor.
            @param I_NativeRef Native instance reference. */
        protected Common(int I_NativeRef) {
            super(I_NativeRef);
        }

        // OutputDevice.Interface.put common output device implementations.
        // Makes calls to the native implementation whatever kind of output device it is, in order to benefit from the format code in it.

        public final OutputDevice.Interface put(Integer J_Integer) {
            OutputDevice.__Common__putInteger(this.getNativeRef(), J_Integer.intValue());
            return (OutputDevice.Interface) this;
        }

        public final OutputDevice.Interface put(Float J_Float) {
            OutputDevice.__Common__putFloat(this.getNativeRef(), J_Float.floatValue());
            return (OutputDevice.Interface) this;
        }

        public final OutputDevice.Interface put(Double J_Double) {
            OutputDevice.__Common__putDouble(this.getNativeRef(), J_Double.doubleValue());
            return (OutputDevice.Interface) this;
        }

        public final OutputDevice.Interface endl() {
            OutputDevice.__Common__endl(this.getNativeRef());
            return (OutputDevice.Interface) this;
        }

        public final void printStackTrace(Exception J_Exception) {
            J_Exception.printStackTrace(
                new java.io.PrintStream(
                    new cli.OutputDevice.OutputStream((OutputDevice.Interface) this)
                )
            );
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Common class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Common prefix.
    private static final native void __Common__putInteger(int I_NativeOutputDeviceRef, int I_Integer);
    private static final native void __Common__putFloat(int I_NativeOutputDeviceRef, float F_Float);
    private static final native void __Common__putDouble(int I_NativeOutputDeviceRef, double D_Double);
    private static final native void __Common__endl(int I_NativeOutputDeviceRef);

    /** Native-implemented output devices. */
    public static abstract class Native extends Common implements OutputDevice.Interface {
        /** Constructor.
            @param I_NativeRef Native instance reference. */
        protected Native(int I_NativeRef) {
            super(I_NativeRef);
        }

        // OutputDevice.Interface native output device implementation.

        public boolean openDevice() {
            return OutputDevice.__Native__openDevice(this.getNativeRef());
        }

        public boolean closeDevice() {
            return OutputDevice.__Native__closeDevice(this.getNativeRef());
        }

        public final OutputDevice.Interface put(String J_Text) {
            OutputDevice.__Native__putString(this.getNativeRef(), J_Text);
            return this;
        }

        public final void beep() {
            OutputDevice.__Native__beep(this.getNativeRef());
        }

        public final void cleanScreen() {
            OutputDevice.__Native__cleanScreen(this.getNativeRef());
        }

        public final ScreenInfo getScreenInfo() {
            int i_ScreenInfoRef = OutputDevice.__Native__getScreenInfo(this.getNativeRef());
            NativeObject cli_ScreenInfo = NativeObject.getObject(i_ScreenInfoRef);
            if (cli_ScreenInfo instanceof ScreenInfo) {
                return (ScreenInfo) cli_ScreenInfo;
            }
            return null;
        }

        public boolean wouldOutput(OutputDevice.Interface CLI_Device) {
            return OutputDevice.__Native__wouldOutput(this.getNativeRef(), CLI_Device.getNativeRef());
        }
    }
    // JNI seems to have trouble at linking following methods when they are embedded in the nested Native class above (at least with java 1.5.0_03).
    // Therefore they are just declared in the scope of the global OutputDevice class with a __Native prefix.
    private static final native boolean __Native__openDevice(int I_NativeOutputDeviceRef);
    private static final native boolean __Native__closeDevice(int I_NativeOutputDeviceRef);
    private static final native void __Native__putString(int I_NativeOutputDeviceRef, String J_Text);
    private static final native void __Native__beep(int I_NativeOutputDeviceRef);
    private static final native void __Native__cleanScreen(int I_NativeOutputDeviceRef);
    private static final native int __Native__getScreenInfo(int I_NativeOutputDeviceRef);
    private static final native boolean __Native__wouldOutput(int I_NativeOutputDeviceRef, int I_NativeOutputDevice2Ref);

    /** Java-implemented output devices. */
    public static abstract class Java extends Common implements OutputDevice.Interface {
        /** Constructor.
            @param J_DbgName Debug name. Useful for traces only. */
        protected Java(String J_DbgName) {
            super(OutputDevice.__Java__Java(J_DbgName));
        }

        /** Constructor for IODevice.Java classes only.
            @param I_NativeRef Native instance reference. */
        protected Java(int I_NativeRef) {
            super(I_NativeRef);
        }

        /** Destructor. */
        protected void finalize() throws Throwable {
            if (getbDoFinalize()) {
                OutputDevice.__Java__finalize(this.getNativeRef());
                dontFinalize(); // finalize once.
            }
            super.finalize();
        }

        // OutputDevice.Interface Java output device implementation.

        public abstract boolean openDevice();
        private final boolean __openDevice() {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__openDevice()"));
            boolean b_Res = openDevice();
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__openDevice()", new Boolean(b_Res).toString()));
            return b_Res;
        }

        public abstract boolean closeDevice();
        private final boolean __closeDevice() {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__closeDevice()"));
            boolean b_Res = closeDevice();
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__closeDevice()", new Boolean(b_Res).toString()));
            return b_Res;
        }

        public abstract OutputDevice.Interface put(String J_Text);
        private final void __putString(String J_Text) {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__putString(J_Text)"));
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.param("J_Text", J_Text));
            put(J_Text);
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__putString()"));
        }

        public abstract void beep();
        private final void __beep() {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__beep()"));
            beep();
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__beep()"));
        }

        public abstract void cleanScreen();
        private final void __cleanScreen() {
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__cleanScreen()"));
            cleanScreen();
            Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.end("OutputDevice.Java.__cleanScreen()"));
        }

        public abstract ScreenInfo getScreenInfo();
        private final void __getScreenInfo(int I_NativeScreenInfoRef) {
            ScreenInfo cli_ScreenInfo = getScreenInfo();
            if (cli_ScreenInfo != null) {
                NativeObject cli_ScreenInfoOut = NativeObject.getObject(I_NativeScreenInfoRef);
                if (cli_ScreenInfoOut instanceof ScreenInfo) {
                    ((ScreenInfo) cli_ScreenInfoOut).copy(cli_ScreenInfo);
                }
            }
        }

        public boolean wouldOutput(OutputDevice.Interface CLI_Device) {
            return (CLI_Device == this);
        }
        private final boolean __wouldOutput(int I_NativeDeviceRef) {
            // Do not trace! for consistency reasons.
            //Traces.safeTrace(NativeTraces.CLASS, this, NativeTraces.begin("OutputDevice.Java.__wouldOutput(CLI_Device)"));
            boolean b_Res = false;
            try {
                OutputDevice.Interface cli_Device = (OutputDevice.Interface) NativeObject.getObject(I_NativeDeviceRef);
                if (cli_Device != null) {
                    b_Res = wouldOutput(cli_Device);
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


    // General static output device features.

    /** Null device singleton.
        @return The null output device. */
    public static final OutputDevice.Interface getNullDevice() {
        class NullDevice extends OutputDevice.Native {
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
    private static OutputDevice.Interface m_cliNullDevice = null;

    /** Standard output device singleton.
        @return The standard output device. */
    public static final OutputDevice.Interface getStdOut() {
        class StdOutDevice extends OutputDevice.Native {
            public StdOutDevice() {
                super(__getStdOut());
            }
        }
        if (m_cliStdOut == null) {
            m_cliStdOut = new StdOutDevice();
        }
        return m_cliStdOut;
    }
    private static final native int __getStdOut();
    private static OutputDevice.Interface m_cliStdOut = null;

    /** Standard error device singleton.
        @return The standard error device. */
    public static final OutputDevice.Interface getStdErr() {
        class StdErrDevice extends OutputDevice.Native {
            public StdErrDevice() {
                super(__getStdErr());
            }
        }
        if (m_cliStdErr == null) {
            m_cliStdErr = new StdErrDevice();
        }
        return m_cliStdErr;
    }
    private static final native int __getStdErr();
    private static OutputDevice.Interface m_cliStdErr = null;


    // CLI output device <-> Java OutputStream binding.

    /** Java output stream attached to a CLI output device. */
    public static class OutputStream extends java.io.OutputStream {
        /** Constructor.
            @param CLI_OutputDevice Output device this stream should be attached to. */
        public OutputStream(OutputDevice.Interface CLI_OutputDevice) {
            m_cliOutputDevice = CLI_OutputDevice;
        }

        /** Output handler.
            @param b Character to be written. */
        public void write(int b) {
            if (m_cliOutputDevice != null) {
                byte[] arb_Bytes = { (byte) b };
                m_cliOutputDevice.put(new String(arb_Bytes));
            }
        }

        /** CLI output device attached to this java output stream. */
        private final OutputDevice.Interface m_cliOutputDevice;
    }
}
