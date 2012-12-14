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


/** Choice user interface class. */
public class Choice extends Line {

    /** Constructor.
        @param I_DefaultChoice Index in the input queue corresponding to the default answer.
        @param J_Choices Input choice list. */
    public Choice(int I_DefaultChoice, java.util.Collection<cli.ResourceString> J_Choices) {
        super(__Choice(I_DefaultChoice, J_Choices));
    }
    private static final int __Choice(int I_DefaultChoice, java.util.Collection<cli.ResourceString> J_Choices) {
        int i_NativeChoiceListRef = __beginChoiceList();
        for (java.util.Iterator<cli.ResourceString> it = J_Choices.iterator(); it.hasNext(); ) {
            cli.ResourceString cli_Choice = it.next();
            if (cli_Choice != null) {
                __addChoice(i_NativeChoiceListRef, cli_Choice.getNativeRef());
            }
        }
        return __Choice(I_DefaultChoice, i_NativeChoiceListRef);
    }
    private static final native int __beginChoiceList();
    private static final native void __addChoice(int I_NativeChoiceListRef, int I_NativeResourceStringRef);
    private static final native int __Choice(int I_DefaultChoice, int I_NativeChoiceListRef);

    /** Contructor for derived classes only.
        @param I_NativeRef Native object reference. */
    protected Choice(int I_NativeRef) {
        super(I_NativeRef);
    }

    /** Destructor. */
    protected void finalize() throws Throwable {
        if (getbDoFinalize()) {
            __finalize(this.getNativeRef());
            dontFinalize(); // finalize once.
        }
        super.finalize();
    }
    private static final native void __finalize(int I_NativeChoiceRef);

    /** Choice retrieval.
        @return Index in the input queue corresponding to the answer entered by the user. -1 if no matching choice found. */
    public int getChoice() {
        return __getChoice(this.getNativeRef());
    }
    private static final native int __getChoice(int I_NativeChoiceRef);

    /** Choice retrieval in its string form.
        @return String of the choice. Empty string if no matching choice found. */
    public cli.ResourceString getstrChoice() {
        cli.ResourceString cli_Choice = new cli.ResourceString();
        for (int e_Lang = 0; e_Lang < cli.ResourceString.LANG_COUNT; e_Lang ++) {
            String j_Choice = __getstrChoice(this.getNativeRef(), e_Lang);
            cli_Choice.setString(e_Lang, j_Choice);
        }
        return cli_Choice;
    }
    private static final native String __getstrChoice(int I_NativeChoiceRef, int E_Lang);

}
