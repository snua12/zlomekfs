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

#include "cli/menu.h"

#include "cli_MenuRef.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_MenuRef__1_1MenuRef(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeMenuRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("MenuRef.__MenuRef(I_NativeMenuRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeMenuRef", I_NativeMenuRef) << cli::endl;
    NativeObject::REF i_MenuRefRef = 0;
    if (const cli::Menu* const pcli_Menu = NativeObject::GetNativeObject<const cli::Menu*>(I_NativeMenuRef))
    {
        if (cli::MenuRef* const pcli_MenuRef = new cli::MenuRef(*pcli_Menu))
        {
            NativeObject::Use(*pcli_MenuRef);
            i_MenuRefRef = NativeObject::GetNativeRef(*pcli_MenuRef);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("MenuRef.__MenuRef()", i_MenuRefRef) << cli::endl;
    return i_MenuRefRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_MenuRef__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeMenuRefRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("MenuRef.__finalize(I_NativeMenuRefRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeMenuRefRef", I_NativeMenuRefRef) << cli::endl;
    if (const cli::MenuRef* const pcli_MenuRef = NativeObject::GetNativeObject<const cli::MenuRef*>(I_NativeMenuRefRef))
    {
        NativeObject::Free(*pcli_MenuRef);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("MenuRef.__finalize()") << cli::endl;
}
