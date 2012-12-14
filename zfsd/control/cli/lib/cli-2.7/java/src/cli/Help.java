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


/** Help class.
    Useful for all CLI element objects. */
public final class Help extends ResourceString
{
    /** Default constructor. */
    public Help() {
        super(__Help());
    }
    /** Copy constructor.
        @param CLI_Help Source object. */
    public Help(Help CLI_Help) {
        super(__Help(CLI_Help.getNativeRef()));
    }
    private static final native int __Help();
    private static final native int __Help(int I_NativeHelpRef);

    /** Creation from native code.
        Useful for help members of other classes.
        @param I_NativeHelpRef Native instance reference. */
    protected static void createFromNative(int I_NativeHelpRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Help.createFromNative(I_NativeHelpRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeHelpRef", new Integer(I_NativeHelpRef).toString()));

        NativeObject.createdFromNative(new Help(I_NativeHelpRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Help.createFromNative()"));
    }
    private Help(int I_NativeHelpRef) {
        super(I_NativeHelpRef);
    }

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeHelpRef);

    /** Destruction from native code.
        See createFromNative(). */
    protected static void deleteFromNative(int I_NativeHelpRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("Help.deleteFromNative(I_NativeHelpRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeHelpRef", new Integer(I_NativeHelpRef).toString()));

        NativeObject.deletedFromNative(NativeObject.getObject(I_NativeHelpRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("Help.deleteFromNative()"));
    }

    /** Help addition for a given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @param J_Help   Help string.
        @return The help instance itself. */
    public final Help addHelp(int E_Lang, String J_Help) {
        __addHelp(this.getNativeRef(), E_Lang, J_Help);
        return this;
    }
    private static final native boolean __addHelp(int I_NativeHelpRef, int E_Lang, String J_Help);

    /** States whether the help object has a help resource for the given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @return true if the help object contains resource for the given language, false otherwise. */
    public final boolean hasHelp(int E_Lang) {
        return __hasHelp(this.getNativeRef(), E_Lang);
    }
    private static final native boolean __hasHelp(int I_NativeHelpRef, int E_Lang);

    /** Retrieves help resource for the given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @return Help resource. */
    public final String getHelp(int E_Lang) {
        return __getHelp(this.getNativeRef(), E_Lang);
    }
    private static final native String __getHelp(int I_NativeHelpRef, int E_Lang);
}
