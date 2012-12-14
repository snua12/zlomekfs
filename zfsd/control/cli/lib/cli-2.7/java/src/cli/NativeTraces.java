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


/** Tool class for native traces. */
abstract class NativeTraces {
    /** Native traces. */
    protected final static cli.TraceClass CLASS = new cli.TraceClass("CLI_JNI", new cli.Help());

    /** Traces the entrance within a method.
        @param STR_Method   Method name. */
    public static final String begin(String STR_Method) {
        return __begin(STR_Method);
    }
    private static final native String __begin(String STR_Method);

    /** Traces a parameter value, when entering a method basically.
        @param STR_ParamName    Name of the parameter.
        @param STR_ParamValue   Value of the parameter. */
    public static final String param(String STR_ParamName, String STR_ParamValue) {
        return __param(STR_ParamName, STR_ParamValue);
    }
    private static final native String __param(String STR_ParamName, String STR_ParamValue);

    /** Traces a variable value, within the body of a method basically.
        @param STR_VarName      Name of the variable.
        @param STR_VarValue     Value of the variable. */
    public static final String value(String STR_VarName, String STR_VarValue) {
        return __value(STR_VarName, STR_VarValue);
    }
    private static final native String __value(String STR_VarName, String STR_VarValue);

    /** Traces the output of a void method.
        @param STR_Method   Method name. */
    public static final String end(String STR_Method) {
        return __end(STR_Method);
    }
    /** Traces the output of a method returning a value.
        @param STR_Method   Method name.
        @param STR_Result   Value returned by the method. */
    public static final String end(String STR_Method, String STR_Result) {
        if (STR_Result != null) {
            return __end(STR_Method, STR_Result);
        } else {
            return __end(STR_Method);
        }
    }
    private static final native String __end(String STR_Method);
    private static final native String __end(String STR_Method, String STR_Result);
}
