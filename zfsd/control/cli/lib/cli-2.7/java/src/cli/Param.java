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


/** Generic parameter element. */
public abstract class Param extends SyntaxNode {

    /** Constructor.
        @param I_NativeRef Native instance reference. */
    protected Param(int I_NativeRef) {
        super(I_NativeRef);
    }

    /** Copy value method.
        @param CLI_Param Source parameter to copy the value from. */
    public final void copyValue(Param CLI_Param) {
        __copyValue(this.getNativeRef(), CLI_Param.getNativeRef());
    }
    private static final native boolean __copyValue(int I_NativeDestParamRef, int I_NativeSrcParamRef);

    /** Determines whether an element matches this parameter.
        @param CLI_Element Element to check the correspondance with this parameter.
        @return true if the element matches this parameter, false if the element does not match this parameter. */
    public boolean matches(Element CLI_Element) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Param.matches(CLI_Element)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("this", new Integer(this.getNativeRef()).toString()));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("CLI_Element", new Integer(CLI_Element.getNativeRef()).toString()));

        boolean b_Res = false;
        if (CLI_Element instanceof Param) {
            Param cli_Param = (Param) CLI_Element;
            if (cli_Param != null) {
                if (cli_Param.getCloned() == this) {
                    this.copyValue(cli_Param);
                    b_Res = true;
                }
            }
        }

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Param.matches()", new Boolean(b_Res).toString()));
        return b_Res;
    }
    /** Source clone parameter accessor.
        @return Source clone parameter instance. */
    protected final cli.Param getCloned() {
        return (cli.Param) NativeObject.getObject(__getCloned(this.getNativeRef()));
    }
    private static final native int __getCloned(int I_NativeParamRef);

}
