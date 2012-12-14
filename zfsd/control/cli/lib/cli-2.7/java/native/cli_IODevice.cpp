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

#include "cli/io_device.h"

#include "cli_IODevice.h"

#include "NativeDevice.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


// IODevice.Native implementation.

//extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice_Native__1_1getKey(
extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice__1_1Native_1_1getKey(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::KEY e_Key = cli::NULL_KEY;
    if (const cli::IODevice* const pcli_IODevice = NativeObject::GetNativeObject<const cli::IODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::Begin("IODevice.Native.__getKey(I_NativeDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        e_Key = pcli_IODevice->GetKey();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::EndInt("IODevice.Native.__getKey()", e_Key) << cli::endl;
    }
    return e_Key;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice_Native__1_1getLocation(
extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice__1_1Native_1_1getLocation(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    NativeObject::REF i_LocationRef = 0;
    if (const cli::IODevice* const pcli_IODevice = NativeObject::GetNativeObject<const cli::IODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::Begin("IODevice.Native.__getLocation(I_NativeDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        if (const cli::ResourceString* const pcli_Location = new cli::ResourceString(pcli_IODevice->GetLocation()))
        {
            NativeObject::CreateFromNative(*pcli_Location);
            i_LocationRef = NativeObject::GetNativeRef(*pcli_Location);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_IODevice) << NativeTraces::EndInt("IODevice.Native.__getLocation()", i_LocationRef) << cli::endl;
    }
    return i_LocationRef;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice_Native__1_1wouldInput(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_IODevice__1_1Native_1_1wouldInput(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIODeviceRef, jint I_NativeIODevice2Ref)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    // Do not trace! for consistency reasons.
    bool b_Res = false;
    if (const cli::IODevice* const pcli_IODevice1 = NativeObject::GetNativeObject<const cli::IODevice*>(I_NativeIODeviceRef))
    {
        if (const cli::IODevice* const pcli_IODevice2 = NativeObject::GetNativeObject<const cli::IODevice*>(I_NativeIODevice2Ref))
        {
            b_Res = pcli_IODevice1->WouldInput(*pcli_IODevice2);
        }
    }
    return b_Res;
}


// IODevice.Java implementation.

//extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice_Java__1_1Java(
extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice__1_1Java_1_1Java(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_DbgName)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IODevice.Java.__Java(PJ_DbgName)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_DbgName", NativeExec::Java2Native(PJ_DbgName).c_str()) << cli::endl;

    NativeObject::REF i_IODeviceRef = 0;
    if (cli::IODevice* const pcli_IODevice = new NativeDevice<cli::IODevice>(NativeExec::Java2Native(PJ_DbgName).c_str()))
    {
        NativeObject::Use(*pcli_IODevice);
        i_IODeviceRef = NativeObject::GetNativeRef(*pcli_IODevice);
    }

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("IODevice.Java.__Java()", i_IODeviceRef) << cli::endl;
    return i_IODeviceRef;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_IODevice_Java__1_1finalize(
extern "C" JNIEXPORT void JNICALL Java_cli_IODevice__1_1Java_1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeIODeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (cli::IODevice* const pcli_IODevice = NativeObject::GetNativeObject<cli::IODevice*>(I_NativeIODeviceRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_IODevice);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IODevice.Java.__finalize(I_NativeOutputDeviceRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeIODeviceRef", I_NativeIODeviceRef) << cli::endl;

        NativeObject::Free(*pcli_IODevice); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("IODevice.Java.__finalize()") << cli::endl;
    }
}


// IODevice static methods implementation.

extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice__1_1getNullDevice(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IODevice.__getNullDevice()") << cli::endl;
    const cli::IODevice& cli_NullDevice = cli::IODevice::GetNullDevice();
    const NativeObject::REF i_NullDeviceRef = NativeObject::GetNativeRef(cli_NullDevice);
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("IODevice.__getNullDevice()", i_NullDeviceRef) << cli::endl;
    return i_NullDeviceRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_IODevice__1_1getStdIn(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("IODevice.__getStdIn()") << cli::endl;
    const cli::IODevice& cli_StdInDevice = cli::IODevice::GetStdIn();
    const NativeObject::REF i_StdInDeviceRef = NativeObject::GetNativeRef(cli_StdInDevice);
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("IODevice.__getStdIn()", i_StdInDeviceRef) << cli::endl;
    return i_StdInDeviceRef;
}
