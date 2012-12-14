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

#include "cli/ui_yesno.h"

#include "cli_ui_YesNo.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ui_YesNo__1_1YesNo(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jboolean B_DefaultAnswer)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.YesNo.__YesNo(B_DefaultAnswer)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_DefaultAnswer", B_DefaultAnswer) << cli::endl;
    NativeObject::REF i_YesNoRef = 0;
    if (cli::ui::YesNo* const pcli_YesNo = new cli::ui::YesNo(B_DefaultAnswer))
    {
        NativeObject::Use(*pcli_YesNo);
        i_YesNoRef = NativeObject::GetNativeRef(*pcli_YesNo);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.YesNo.__YesNo()", i_YesNoRef) << cli::endl;
    return i_YesNoRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_YesNo__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeYesNoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.YesNo.__finalize(I_NativeYesNoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeYesNoRef", I_NativeYesNoRef) << cli::endl;
    if (const cli::ui::YesNo* const pcli_YesNo = NativeObject::GetNativeObject<const cli::ui::YesNo*>(I_NativeYesNoRef))
    {
        NativeObject::Free(*pcli_YesNo);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.YesNo.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_ui_YesNo__1_1getYesNo(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeYesNoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.YesNo.__getYesNo(I_NativeYesNoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeYesNoRef", I_NativeYesNoRef) << cli::endl;
    bool b_YesNo = false;
    if (const cli::ui::YesNo* const pcli_YesNo = NativeObject::GetNativeObject<const cli::ui::YesNo*>(I_NativeYesNoRef))
    {
        b_YesNo = pcli_YesNo->GetYesNo();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("ui.YesNo.__getYesNo()", b_YesNo) << cli::endl;
    return b_YesNo;
}
