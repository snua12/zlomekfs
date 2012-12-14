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

#include "cli/non_blocking_io_device.h"
#include "cli/shell.h"

#include "cli_NonBlockingIODevice.h"

#include "NativeObject.h"
#include "NativeTraces.h"
#include "NativeExec.h"
#include "NativeDevice.h"


// NonBlockingIODevice.Common implementation.

//extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice_Common__1_1attachKeyReceiver(
extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice__1_1Common_1_1attachKeyReceiver(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef, jint I_NativeKeyReceiverRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Common.__attachKeyReceiver(I_NativeDeviceRef, I_NativeKeyReceiverRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeKeyReceiverRef", I_NativeKeyReceiverRef) << cli::endl;

        if (cli::NonBlockingKeyReceiver* const pcli_NonBlockingKeyReceiver = NativeObject::GetNativeObject<cli::NonBlockingKeyReceiver*>(I_NativeKeyReceiverRef))
        {
            pcli_NonBlockingIODevice->AttachKeyReceiver(*pcli_NonBlockingKeyReceiver);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndVoid("NonBlockingIODevice.Common.__attachKeyReceiver()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice_Common__1_1detachKeyReceiver(
extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice__1_1Common_1_1detachKeyReceiver(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef, jint I_NativeKeyReceiverRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Common.__detachKeyReceiver(I_NativeDeviceRef, I_NativeKeyReceiverRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeKeyReceiverRef", I_NativeKeyReceiverRef) << cli::endl;

        if (cli::NonBlockingKeyReceiver* const pcli_NonBlockingKeyReceiver = NativeObject::GetNativeObject<cli::NonBlockingKeyReceiver*>(I_NativeKeyReceiverRef))
        {
            pcli_NonBlockingIODevice->DetachKeyReceiver(*pcli_NonBlockingKeyReceiver);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndVoid("NonBlockingIODevice.Common.__detachKeyReceiver()") << cli::endl;
    }
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice_Common__1_1getKeyReceiver(
extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice__1_1Common_1_1getKeyReceiver(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    NativeObject::REF i_KeyReceiverRef = 0;
    if (const cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<const cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Common.__getKeyReceiver(I_NativeDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        if (const cli::NonBlockingKeyReceiver* const pcli_KeyReceiver = pcli_NonBlockingIODevice->GetKeyReceiver())
        {
            i_KeyReceiverRef = NativeObject::GetNativeRef(*pcli_KeyReceiver);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndInt("NonBlockingIODevice.Common.__getKeyReceiver()", i_KeyReceiverRef) << cli::endl;
    }
    return i_KeyReceiverRef;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice_Common__1_1getShell(
extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice__1_1Common_1_1getShell(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    NativeObject::REF i_ShellRef = 0;
    if (const cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<const cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Common.__getShell(I_NativeDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        if (const cli::Shell* const pcli_Shell = pcli_NonBlockingIODevice->GetShell())
        {
            i_ShellRef = NativeObject::GetNativeRef(*pcli_Shell);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndInt("NonBlockingIODevice.Common.__getShell()", i_ShellRef) << cli::endl;
    }
    return i_ShellRef;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice_Common__1_1onKey(
extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice__1_1Common_1_1onKey(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef, jint E_Key)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<const cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Common.__onKey(I_NativeDeviceRef, E_Key)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("E_Key", E_Key) << cli::endl;

        pcli_NonBlockingIODevice->OnKey((cli::KEY) E_Key);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndVoid("NonBlockingIODevice.Common.__onKey()") << cli::endl;
    }
}


// NonBlockingIODevice.Native implementation.

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_NonBlockingIODevice_Native__1_1waitForKeys(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_NonBlockingIODevice__1_1Native_1_1waitForKeys(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef, jint I_Milli)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (const cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<const cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::Begin("NonBlockingIODevice.Native.__waitForKeys(I_NativeDeviceRef, I_Milli)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::ParamInt("I_Milli", I_Milli) << cli::endl;

        if (I_Milli >= 0)
        {
            b_Res = pcli_NonBlockingIODevice->WaitForKeys(I_Milli);
        }

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_NonBlockingIODevice) << NativeTraces::EndBool("NonBlockingIODevice.Native.__waitForKeys()", b_Res) << cli::endl;
    }
    return b_Res;
}


// NonBlockingIODevice.Java implementation.

//extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice_Java__1_1NonBlockingIODevice(
extern "C" JNIEXPORT jint JNICALL Java_cli_NonBlockingIODevice__1_1Java_1_1Java(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_DbgName)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("NonBlockingIODevice.Java.__Java(PJ_DbgName)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_DbgName", NativeExec::Java2Native(PJ_DbgName).c_str()) << cli::endl;

    NativeObject::REF i_NonBlockingIODeviceRef = 0;
    if (cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = new NativeDevice<cli::NonBlockingIODevice>(NativeExec::Java2Native(PJ_DbgName).c_str()))
    {
        NativeObject::Use(*pcli_NonBlockingIODevice);
        i_NonBlockingIODeviceRef = NativeObject::GetNativeRef(*pcli_NonBlockingIODevice);
    }

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("NonBlockingIODevice.Java.__Java()", i_NonBlockingIODeviceRef) << cli::endl;
    return i_NonBlockingIODeviceRef;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice_Java__1_1finalize(
extern "C" JNIEXPORT void JNICALL Java_cli_NonBlockingIODevice__1_1Java_1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (cli::NonBlockingIODevice* const pcli_NonBlockingIODevice = NativeObject::GetNativeObject<cli::NonBlockingIODevice*>(I_NativeDeviceRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_NonBlockingIODevice);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("NonBlockingIODevice.Java.__finalize(I_NativeDeviceRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;

        NativeObject::Free(*pcli_NonBlockingIODevice); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("NonBlockingIODevice.Java.__finalize()") << cli::endl;
    }
}
