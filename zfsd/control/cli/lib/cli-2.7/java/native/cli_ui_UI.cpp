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

#include "cli/ui.h"
#include "cli/shell.h"

#include "cli_ui_UI.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jboolean JNICALL Java_cli_ui_UI__1_1run(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeUIRef, jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.UI.__run(I_NativeUIRef, I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeUIRef", I_NativeUIRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    bool b_Res = false;
    if (cli::ui::UI* const pcli_UI = NativeObject::GetNativeObject<cli::ui::UI*>(I_NativeUIRef))
    {
        if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
        {
            b_Res = pcli_UI->Run(*pcli_Shell);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("ui.Line.__run()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_UI__1_1onNonBlockingKey(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeUIRef, jint I_NativeSourceDeviceRef, jint E_KeyCode)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.UI.__onNonBlockingKey(I_NativeUIRef, I_NativeSourceDeviceRef, E_KeyCode)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeUIRef", I_NativeUIRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeSourceDeviceRef", I_NativeSourceDeviceRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_KeyCode", E_KeyCode) << cli::endl;
    if (cli::ui::UI* const pcli_UI = NativeObject::GetNativeObject<cli::ui::UI*>(I_NativeUIRef))
    {
        if (cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<cli::NonBlockingIODevice*>(I_NativeSourceDeviceRef))
        {
            pcli_UI->OnNonBlockingKey(*pcli_NonBlockingIODevice, (cli::KEY) E_KeyCode);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Line.__onNonBlockingKey()") << cli::endl;
}

