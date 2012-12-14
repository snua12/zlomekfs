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

import java.util.Map;
import java.util.Hashtable;


/** Java object corresponding to a native object class.
    When a Java instance corresponds to a native instance, it should inherit from this class.

    NativeObject allows automatic native object destruction when the garbage collector frees the Java instance.

    NativeObject also offers a service that allows Java instances to be still referenced
    even though they would normally be destroyed by the garbage collector on Java side.
    However, since the object is still useful on native side, it should be remembered.
    A 'forget' service is also provided to balance the 'remember' service. */
public abstract class NativeObject {

    static {
        // Ensure 'cli' library initializations.
        Cli.load();
    }

    /** Constructor.
        @param I_NativeRef Native instance referenced corresponding to this Java instance. */
    protected NativeObject(int I_NativeRef) {
        m_iNativeRef = I_NativeRef;
        m_bDoFinalize = true;
        m_bForgotten = false;
        NativeObject.remember(this);
    }

    /** To be called by native created objects.
        @param CLI_Object Object created. */
    protected static final void createdFromNative(NativeObject CLI_Object) {
        if (CLI_Object != null) {
            // Object already remembered from the NativeObject constructor (above).
            //  NativeObject.remember(CLI_Object);
        }
    }

    /** Destructor. */
    protected void finalize() throws Throwable {
        forget(this);
    }

    /** To be called by native-deleted objects.
        @param CLI_Object Object deleted. */
    protected static final void deletedFromNative(NativeObject CLI_Object) {
        if (CLI_Object != null) {
            Traces.trace(NativeTraces.CLASS, NativeTraces.value("CLI_Object", CLI_Object.toString()));
            // Do not finalize native-deleted objects.
            CLI_Object.dontFinalize();
            // Also forget the native reference.
            NativeObject.forget(CLI_Object);
        }
    }

    /** Native reference accessor.
        @return The native reference of the instance. */
    public final int getNativeRef() {
        return m_iNativeRef;
    }

    /** Accessor to be used in order to know whether the native instance instance should be destroyed or not.
        @return true if the native instance should be destroyed on finalization, false otherwise. */
    public final boolean getbDoFinalize() {
        return m_bDoFinalize;
    }
    /** Call this method for Java objects which native objects should not be destroyed from Java code
        on finalization. **/
    public final void dontFinalize() {
        m_bDoFinalize = false;
    }

    private final int m_iNativeRef;
    private boolean m_bDoFinalize;
    private boolean m_bForgotten;

    // --- Static implementation. ---

    /** Java instances to be remembered for native object consistency
        should be referenced with this method.
        @param CLI_NativeObject Instance to remember. */
    public static final void remember(NativeObject CLI_NativeObject) {
        if (CLI_NativeObject != null) {
            if (! m_jObjMap.containsKey(CLI_NativeObject.getNativeRef())) {
                m_jObjMap.put(CLI_NativeObject.getNativeRef(), CLI_NativeObject);
            } else {
                throw new InternalError(
                    "Cannot remember twice the same reference. "
                    + "Conflicting objects "
                    + m_jObjMap.get(CLI_NativeObject.getNativeRef()) + " "
                    + CLI_NativeObject + ".");
            }
        } else {
            throw new InternalError("Cannot remember null object.");
        }
    }

    /** Java instances to be forgotten (native object can now be destroyed)
        should be dereferenced with this method.
        @param CLI_NativeObject Instance to forget. */
    public static final void forget(NativeObject CLI_NativeObject) {
        if (CLI_NativeObject != null) {
            if (! CLI_NativeObject.m_bForgotten) {
                CLI_NativeObject.m_bForgotten = true;
                m_jObjMap.remove(CLI_NativeObject.getNativeRef());
            }
        }
    }

    /** Find the Java instance from its native instance reference.
        @param I_NativeRef Native reference corresponding to the searched Java instance.
        @return The corresponding native instance if found, null otherwise. */
    public static NativeObject getObject(int I_NativeRef) {
        return m_jObjMap.get(new Integer(I_NativeRef));
    }

    private static final Map<Integer, NativeObject> m_jObjMap = new Hashtable<Integer, NativeObject>();
}
