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


/** Float user interface class. */
public class Float extends Line {

    /** Constructor.
        @param D_DefaultValue Default value.
        @param D_MinValue Minimum value.
        @param D_MaxValue Maximum value. */
    public Float(double D_DefaultValue, double D_MinValue, double D_MaxValue) {
        super(__Float(D_DefaultValue, D_MinValue, D_MaxValue));
    }
    private static final native int __Float(double D_DefaultValue, double D_MinValue, double D_MaxValue);

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeFloatRef);

    /** Float retrieval.
        @return Float value entered by the user. */
    public double getFloat() {
        return __getFloat(this.getNativeRef());
    }
    private static final native double __getFloat(int I_NativeFloatRef);

}

