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


/** Resource string class. */
public class ResourceString extends NativeObject
{
    /** Default constructor. */
    public ResourceString() {
        super(__ResourceString());
    }
    /** Copy constructor.
        @param CLI_String   Source object. */
    public ResourceString(ResourceString CLI_String) {
        super(__ResourceString(CLI_String.getNativeRef()));
    }
    private static final native int __ResourceString();
    private static final native int __ResourceString(int I_NativeStringRef);

    /** Creation from native code.
        Useful for help members of other classes.
        @param I_NativeStringRef Native instance reference. */
    protected static void createFromNative(int I_NativeStringRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("ResourceString.createFromNative(I_NativeStringRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeStringRef", new Integer(I_NativeStringRef).toString()));

        NativeObject.createdFromNative(new ResourceString(I_NativeStringRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("ResourceString.createFromNative()"));
    }
    /** Constructor for createFromNative() or child classes only. */
    protected ResourceString(int I_NativeStringRef) {
        super(I_NativeStringRef);
    }

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeStringRef);

    /** Destruction from native code.
        See createFromNative(). */
    protected static void deleteFromNative(int I_NativeStringRef) {
        Traces.trace(NativeTraces.CLASS, NativeTraces.begin("ResourceString.deleteFromNative(I_NativeStringRef)"));
        Traces.trace(NativeTraces.CLASS, NativeTraces.param("I_NativeStringRef", new Integer(I_NativeStringRef).toString()));

        NativeObject.deletedFromNative(NativeObject.getObject(I_NativeStringRef));

        Traces.trace(NativeTraces.CLASS, NativeTraces.end("ResourceString.deleteFromNative()"));
    }

    /** English language constant. */
    public static final int LANG_EN = 0;
    /** French language constant. */
    public static final int LANG_FR = 1;
    /** Number of languages managed by the library. */
    public static final int LANG_COUNT = 2;

    /** String addition for a given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @param J_String String of the given language.
        @return The resource string instance itself. */
    public final ResourceString setString(int E_Lang, String J_String) {
        __setString(this.getNativeRef(), E_Lang, J_String);
        return this;
    }
    private static final native boolean __setString(int I_NativeStringRef, int E_Lang, String J_String);

    /** States whether the resource string object has a resource for the given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @return true if the resource string object contains resource for the given language, false otherwise. */
    public final boolean hasString(int E_Lang) {
        return __hasString(this.getNativeRef(), E_Lang);
    }
    private static final native boolean __hasString(int I_NativeStringRef, int E_Lang);

    /** Retrieves the string attached for the given language.
        @param E_Lang   Language identifier (LANG_EN, LANG_FR...)
        @return String. */
    public final String getString(int E_Lang) {
        return __getString(this.getNativeRef(), E_Lang);
    }
    private static final native String __getString(int I_NativeStringRef, int E_Lang);
}
