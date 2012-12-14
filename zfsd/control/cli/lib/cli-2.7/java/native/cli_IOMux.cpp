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

#include "cli/io_mux.h"

#include "cli_IOMux.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_IOMux__1_1IOMux(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IOMux.__IOMux()") << cli::endl;
    NativeObject::REF i_IOMuxRef = 0;
    if (cli::IOMux* const pcli_IOMux = new cli::IOMux(true))
    {
        NativeObject::Use(*pcli_IOMux);
        i_IOMuxRef = NativeObject::GetNativeRef(*pcli_IOMux);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("IOMux.__IOMux()", i_IOMuxRef) << cli::endl;
    return i_IOMuxRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_IOMux__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIOMuxRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::IOMux* const pcli_IOMux = NativeObject::GetNativeObject<const cli::IOMux*>(I_NativeIOMuxRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_IOMux);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IOMux.__finalize(I_NativeIOMuxRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeIOMuxRef", I_NativeIOMuxRef) << cli::endl;

        NativeObject::Free(*pcli_IOMux); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("IOMux.__finalize()") << cli::endl;
    }
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_IOMux__1_1addDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIOMuxRef, jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (cli::IOMux* const pcli_IOMux = NativeObject::GetNativeObject<cli::IOMux*>(I_NativeIOMuxRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::Begin("IOMux.__addDevice(I_NativeIOMuxRef, I_NativeInputRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::ParamInt("I_NativeIOMuxRef", I_NativeIOMuxRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        if (cli::IODevice* const pcli_Device = NativeObject::GetNativeObject<cli::IODevice*>(I_NativeDeviceRef))
        {
            b_Res = pcli_IOMux->AddDevice(pcli_Device);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::EndBool("IOMux.__addDevice()", b_Res) << cli::endl;
    }
    return b_Res;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_IOMux__1_1getCurrentDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIOMuxRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    jint i_CurrentDeviceRef = 0;
    if (const cli::IOMux* const pcli_IOMux = NativeObject::GetNativeObject<const cli::IOMux*>(I_NativeIOMuxRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::Begin("IOMux.__getCurrentDevice(I_NativeIOMuxRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::ParamInt("I_NativeIOMuxRef", I_NativeIOMuxRef) << cli::endl;

        if (const cli::IODevice* const pcli_CurrentDevice = pcli_IOMux->GetCurrentDevice())
        {
            i_CurrentDeviceRef = NativeObject::GetNativeRef(*pcli_CurrentDevice);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::EndInt("IOMux.__getCurrentDevice()", i_CurrentDeviceRef) << cli::endl;
    }
    return i_CurrentDeviceRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_IOMux__1_1switchNextDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIOMuxRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    jint i_NextDeviceRef = 0;
    if (cli::IOMux* const pcli_IOMux = NativeObject::GetNativeObject<cli::IOMux*>(I_NativeIOMuxRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::Begin("IOMux.__switchNextDevice(I_NativeIOMuxRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::ParamInt("I_NativeIOMuxRef", I_NativeIOMuxRef) << cli::endl;

        if (const cli::IODevice* const pcli_NextDevice = pcli_IOMux->SwitchNextDevice())
        {
            i_NextDeviceRef = NativeObject::GetNativeRef(*pcli_NextDevice);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::EndInt("IOMux.__switchNextDevice()", i_NextDeviceRef) << cli::endl;
    }
    return i_NextDeviceRef;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_IOMux__1_1resetDeviceList(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIOMuxRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    jboolean b_Res = false;
    if (cli::IOMux* const pcli_IOMux = NativeObject::GetNativeObject<cli::IOMux*>(I_NativeIOMuxRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::Begin("IOMux.__resetDeviceList(I_NativeIOMuxRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::ParamInt("I_NativeIOMuxRef", I_NativeIOMuxRef) << cli::endl;

        b_Res = pcli_IOMux->ResetDeviceList();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IOMux) << NativeTraces::EndBool("IOMux.__resetDeviceList()", b_Res) << cli::endl;
    }
    return b_Res;
}
