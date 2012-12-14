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

#include "cli/string_device.h"

#include "cli_StringDevice.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_StringDevice__1_1StringDevice(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("StringDevice.__StringDevice()") << cli::endl;
    NativeObject::REF i_StringDeviceRef = 0;
    if (cli::StringDevice* const pcli_StringDevice = new cli::StringDevice(0, true)) // UI_OutputMaxLen not taken in account by STL implementation.
    {
        NativeObject::Use(*pcli_StringDevice);
        i_StringDeviceRef = NativeObject::GetNativeRef(*pcli_StringDevice);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("StringDevice.__StringDevice()", i_StringDeviceRef) << cli::endl;
    return i_StringDeviceRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_StringDevice__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::StringDevice* const pcli_StringDevice = NativeObject::GetNativeObject<const cli::StringDevice*>(I_NativeStringDeviceRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_StringDevice);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("StringDevice.__finalize(I_NativeStringDeviceRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeStringDeviceRef", I_NativeStringDeviceRef) << cli::endl;

        NativeObject::Free(*pcli_StringDevice); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("StringDevice.__finalize()") << cli::endl;
    }
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_StringDevice__1_1getString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_String;
    if (const cli::StringDevice* const pcli_StringDevice = NativeObject::GetNativeObject<const cli::StringDevice*>(I_NativeStringDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::Begin("StringDevice.__getString(I_NativeStringDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::ParamInt("I_NativeStringDeviceRef", I_NativeStringDeviceRef) << cli::endl;

        str_String = (const char*) pcli_StringDevice->GetString();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::EndStr("StringDevice.__getString()", str_String.c_str()) << cli::endl;
    }
    return NativeExec::Native2Java(str_String);
}

extern "C" JNIEXPORT void JNICALL Java_cli_StringDevice__1_1reset(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (cli::StringDevice* const pcli_StringDevice = NativeObject::GetNativeObject<cli::StringDevice*>(I_NativeStringDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::Begin("StringDevice.__reset(I_NativeStringDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::ParamInt("I_NativeStringDeviceRef", I_NativeStringDeviceRef) << cli::endl;

        pcli_StringDevice->Reset();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_StringDevice) << NativeTraces::EndVoid("StringDevice.__reset()") << cli::endl;
    }
}
