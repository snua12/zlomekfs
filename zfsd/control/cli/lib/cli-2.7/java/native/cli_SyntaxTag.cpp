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

#include "cli/pch.h"

#include "cli/syntax_tag.h"

#include "cli_SyntaxTag.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_SyntaxTag__1_1SyntaxTag(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jboolean B_Hollow)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("SyntaxTag.__SyntaxTag(B_Hollow)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_Hollow", B_Hollow) << cli::endl;
    NativeObject::REF i_TagRef = 0;
    if (cli::SyntaxTag* const pcli_Tag = new cli::SyntaxTag(B_Hollow))
    {
        NativeObject::Use(*pcli_Tag);
        i_TagRef = NativeObject::GetNativeRef(*pcli_Tag);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("SyntaxTag.__SyntaxTag()", i_TagRef) << cli::endl;
    return i_TagRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_SyntaxTag__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeSyntaxTagRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("SyntaxTag.__finalize(I_NativeSyntaxTagRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeSyntaxTagRef", I_NativeSyntaxTagRef) << cli::endl;
    if (const cli::SyntaxTag* const pcli_Tag = NativeObject::GetNativeObject<const cli::SyntaxTag*>(I_NativeSyntaxTagRef))
    {
        NativeObject::Free(*pcli_Tag);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("SyntaxTag.__finalize()") << cli::endl;
}
