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


/** Traces management class. */
public final class Traces
{
    /** Static class. No constructor. */
    private Traces() {
    }

    /** Traces output stream accessor.
        @return Traces output device, or null-device if an error occured. */
    public static final OutputDevice.Interface getStream() {
        int i_TracesStream = __getStream();

        // Check whether this is an already known output device on Java side.
        Object j_TracesStream = NativeObject.getObject(i_TracesStream);
        if (j_TracesStream != null) {
            if (j_TracesStream instanceof OutputDevice.Interface) {
                return (OutputDevice.Interface) j_TracesStream;
            } else {
                return OutputDevice.getNullDevice();
            }
        }

        // Otherwise, check with common devices.
        if (i_TracesStream == OutputDevice.getStdOut().getNativeRef()) {
            return OutputDevice.getStdOut();
        }
        if (i_TracesStream == OutputDevice.getStdErr().getNativeRef()) {
            return OutputDevice.getStdErr();
        }
        if (i_TracesStream == OutputDevice.getNullDevice().getNativeRef()) {
            return OutputDevice.getNullDevice();
        }
        if (i_TracesStream == IODevice.getStdIn().getNativeRef()) {
            return IODevice.getStdIn();
        }
        if (i_TracesStream == IODevice.getNullDevice().getNativeRef()) {
            return IODevice.getNullDevice();
        }

        // Default return.
        return OutputDevice.getNullDevice();
    }
    private static final native int __getStream();

    /** Stack overflow protection.
        @param CLI_AvoidTraces Device which output is about to be traced.
        @return true if traces are safe for this output device, false if no trace should be set. */
    public static final boolean isSafe(cli.OutputDevice.Interface CLI_AvoidTraces) {
        return (! Traces.getStream().wouldOutput(CLI_AvoidTraces));
    }

    /** Stream positionning (if not already set).
        @param CLI_Stream Stream reference.
        @return true: success, false: failure.

        When SetStream() has not been called previously, then it takes CLI_Stream in account immediately for tracing.
        Otherwise it stacks the stream (for consistency concerns) and waits for the previous ones to be released possibly. */
    public static final boolean setStream(OutputDevice.Interface CLI_Stream) {
        return __setStream(CLI_Stream.getNativeRef());
    }
    private static final native boolean __setStream(int I_NativeOutputDeviceRef);

    /** Restores the initial traces output stream.
        @param CLI_Stream Output device to be used.
        @return true: success, false: failure. */
    public static final boolean unsetStream(OutputDevice.Interface CLI_Stream) {
        return __unsetStream(CLI_Stream.getNativeRef());
    }
    private static final native boolean __unsetStream(int I_NativeOutputDeviceRef);

    /** Modifies the traces filter.
        @param CLI_Class    Trace class object of the filter modification.
        @param B_ShowTraces true if the traces should be displayed, false otherwise. */
    public static final void setFilter(TraceClass CLI_Class, boolean B_ShowTraces) {
        __setFilter(CLI_Class.getNativeRef(), B_ShowTraces);
    }
    private static final native void __setFilter(int I_NativeTraceClassRef, boolean B_ShowTraces);

    /** Modifies the traces filter for all trace classes.
        @param B_ShowTraces true if the traces should be displayed, false otherwise. */
    public static final void setAllFilter(boolean B_ShowTraces) {
        __setAllFilter(B_ShowTraces);
    }
    private static final native void __setAllFilter(boolean B_ShowTraces);

    /** Sends a trace to the trace system.
        @param CLI_TraceClass   Corresponding trace class.
        @param STR_Text         Text of the trace. */
    public static final void trace(TraceClass CLI_TraceClass, String STR_Text) {
        __trace(CLI_TraceClass.getNativeRef(), STR_Text);
    }
    private static final native void __trace(int I_TraceClassNativeRef, String STR_Text);

    /** Safe trace routine.
        @param CLI_TraceClass   Corresponding trace class.
        @param CLI_AvoidTraces  Avoid stream from being sent traces.
        @param STR_Text         Text of the trace.

        Prevents output from infinite loops. */
    public static final void safeTrace(TraceClass CLI_TraceClass, OutputDevice.Interface CLI_AvoidTraces, String STR_Text) {
        if (isSafe(CLI_AvoidTraces)) {
            trace(CLI_TraceClass, STR_Text);
        }
    }

    /** Safe trace routine.
        @param CLI_TraceClass           Corresponding trace class.
        @param I_AvoidTracesDeviceRef   Reference of device to avoid from being sent traces.
        @param STR_Text                 Text of the trace.

        Prevents output from infinite loops. */
    public static final void safeTrace(TraceClass CLI_TraceClass, int I_AvoidTracesDeviceRef, String STR_Text) {
        NativeObject cli_Device = NativeObject.getObject(I_AvoidTracesDeviceRef);
        if ((cli_Device != null) && (cli_Device instanceof OutputDevice.Interface)) {
            safeTrace(CLI_TraceClass, (OutputDevice.Interface) cli_Device, STR_Text);
        }
    }
}
