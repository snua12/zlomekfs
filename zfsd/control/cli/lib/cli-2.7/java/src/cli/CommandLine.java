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

import java.util.Iterator;
import java.util.Vector;


/** Command line class.
    Describes a list of elements after analysis from a text command line. */
public final class CommandLine extends NativeObject
{
    /** Constructor. */
    public CommandLine() {
        super(__CommandLine());
    }
    private static final native int __CommandLine();

    /** Create a new command line from native instance.
        @param I_NativeCmdLineRef   Native object reference. */
    private static final void createFromNative(int I_NativeCmdLineRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("CommandLine.createFromNative(I_NativeCmdLineRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeCmdLineRef", new Integer(I_NativeCmdLineRef).toString()));

        NativeObject.createdFromNative(new CommandLine(I_NativeCmdLineRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("CommandLine.createFromNative()"));
    }
    /** Create a command line from its native reference.
        @param I_NativeCmdLineRef   Native object reference. */
    private CommandLine(int I_NativeCmdLineRef) {
        // Register the native reference.
        super(I_NativeCmdLineRef);

        // Then fill in element references.
        m_jElements = new Vector<Element>();
        final int i_Count = __getElementCount(I_NativeCmdLineRef);
        for (int i=0; i<i_Count; i++) {
            NativeObject cli_Object = NativeObject.getObject(__getElementAt(I_NativeCmdLineRef, i));
            if (cli_Object instanceof Element) {
                Element cli_Element = (Element) cli_Object;
                Traces.trace(NativeTraces.CLASS, NativeTraces.value("cli_Element", cli_Element.getKeyword()));
                m_jElements.add(cli_Element);
            }
        }
    }

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeCmdLineRef);

    /** Let the native library notify java when command lines are not used anymore.
        See createFromNative().
        @param I_NativeCmdLineRef   Native object reference. */
    private static final void deleteFromNative(int I_NativeCmdLineRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("CommandLine.deleteFromNative(I_NativeCmdLineRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeCmdLineRef", new Integer(I_NativeCmdLineRef).toString()));

        // Forget the command line references.
        NativeObject.deletedFromNative(NativeObject.getObject(I_NativeCmdLineRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("CommandLine.deleteFromNative()"));
    }

    /** Retrieves an iterator over the elements of the command line.
        @return Iterator over the elements of the command line. */
    public Iterator<Element> iterator() {
        return m_jElements.iterator();
    }
    private static final native int __getElementCount(int I_NativeCmdLineRef);
    private static final native int __getElementAt(int I_NativeCmdLineRef, int I_Position);

    private Vector<Element> m_jElements;
}
