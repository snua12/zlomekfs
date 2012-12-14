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

#include "cli/ui_less.h"

#include "cli_ui_Less.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Less__1_1Less(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Less.__Less()") << cli::endl;
    NativeObject::REF i_LessRef = 0;
    if (cli::ui::Less* const pcli_Less = new cli::ui::Less(0, 0)) // UI_MaxLines & UI_MaxLineLength not taken in account by tk STL implementation
    {
        NativeObject::Use(*pcli_Less);
        i_LessRef = NativeObject::GetNativeRef(*pcli_Less);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Less.__Less()", i_LessRef) << cli::endl;
    return i_LessRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_Less__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeLessRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Less.__finalize(I_NativeLessRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeLessRef", I_NativeLessRef) << cli::endl;
    if (const cli::ui::Less* const pcli_Less = NativeObject::GetNativeObject<const cli::ui::Less*>(I_NativeLessRef))
    {
        NativeObject::Free(*pcli_Less);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Less.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Less__1_1getText(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeLessRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Less.__getText(I_NativeLessRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeLessRef", I_NativeLessRef) << cli::endl;
    NativeObject::REF i_TextRef = 0;
    if (cli::ui::Less* const pcli_Less = NativeObject::GetNativeObject<cli::ui::Less*>(I_NativeLessRef))
    {
        i_TextRef = NativeObject::GetNativeRef(pcli_Less->GetText());
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Less.__getText()", i_TextRef) << cli::endl;
    return i_TextRef;
}
