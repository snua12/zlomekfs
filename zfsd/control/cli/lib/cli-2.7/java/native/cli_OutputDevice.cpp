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

#include "cli_OutputDevice.h"

#include "NativeDevice.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


// OutputDevice.Common implementation.

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Common__1_1putInteger(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Common_1_1putInteger(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef, jint I_Integer)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Common.__putInteger(I_NativeOutputDeviceRef, I_Integer)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_Integer", I_Integer) << cli::endl;

        pcli_OutputDevice->operator<<(I_Integer);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Common.__putInteger()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Common__1_1putFloat(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Common_1_1putFloat(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef, jfloat F_Float)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Common.__putFloat(I_NativeOutputDeviceRef, F_Float)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamFloat("F_Float", F_Float) << cli::endl;

        pcli_OutputDevice->operator<<(F_Float);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Common.__putFloat()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Common__1_1putDouble(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Common_1_1putDouble(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef, jdouble D_Double)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Common.__putDouble(I_NativeOutputDeviceRef, D_Double)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamFloat("D_Double", D_Double) << cli::endl;

        pcli_OutputDevice->operator<<(D_Double);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Common.__putDouble()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Common__1_1endl(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Common_1_1endl(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Common.__endl(I_NativeOutputDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        pcli_OutputDevice->operator<<(cli::endl);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Common.__endl()") << cli::endl;
    }
}


// OutputDevice.Native implementation.

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice_Native__1_1openDevice(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice__1_1Native_1_1openDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Native.__openDevice(I_NativeOutputDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        b_Res = pcli_OutputDevice->OpenUp(__CALL_INFO__);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndBool("OutputDevice.Common.__openDevice()", b_Res) << cli::endl;
    }
    return b_Res;
}

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice_Native__1_1closeDevice(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice__1_1Native_1_1closeDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Native.__closeDevice(I_NativeOutputDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        b_Res = pcli_OutputDevice->CloseDown(__CALL_INFO__);

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndBool("OutputDevice.Common.__closeDevice()", b_Res) << cli::endl;
    }
    return b_Res;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Native__1_1putString(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Native_1_1putString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef, jstring PJ_Text)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Native.__putString(I_NativeOutputDeviceRef, PJ_Text)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamStr("PJ_Text", NativeExec::Java2Native(PJ_Text).c_str()) << cli::endl;

        pcli_OutputDevice->operator<<(NativeExec::Java2Native(PJ_Text).c_str());

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Native.__putString()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Native__1_1beep(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Native_1_1beep(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Native.__beep(I_NativeOutputDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        pcli_OutputDevice->Beep();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Native.__beep()") << cli::endl;
    }
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Native__1_1cleanScreen(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Native_1_1cleanScreen(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputDevice* const pcli_OutputDevice = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::Begin("OutputDevice.Native.__cleanScreen(I_NativeOutputDeviceRef)") << cli::endl;
        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        pcli_OutputDevice->CleanScreen();

        cli::GetTraces().SafeTrace(TRACE_JNI, *pcli_OutputDevice) << NativeTraces::EndVoid("OutputDevice.Native.__cleanScreen()") << cli::endl;
    }
}

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice_Native_1_1wouldOutput(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice__1_1Native_1_1wouldOutput(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef, jint I_NativeOutputDevice2Ref)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    // Do not trace! for consistency reasons.
    bool b_Res = false;
    if (const cli::OutputDevice* const pcli_OutputDevice1 = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        if (const cli::OutputDevice* const pcli_OutputDevice2 = NativeObject::GetNativeObject<const cli::OutputDevice*>(I_NativeOutputDevice2Ref))
        {
            b_Res = pcli_OutputDevice1->WouldOutput(*pcli_OutputDevice2);
        }
    }
    return b_Res;
}


// OutputDevice.Java implementation.

//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_Java__1_1Java(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1Java_1_1Java(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_DbgName)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.Java.__Java(PJ_DbgName)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_DbgName", NativeExec::Java2Native(PJ_DbgName).c_str()) << cli::endl;

    NativeObject::REF i_OutputDeviceRef = 0;
    if (cli::OutputDevice* const pcli_OutputDevice = new NativeDevice<cli::OutputDevice>(NativeExec::Java2Native(PJ_DbgName).c_str()))
    {
        NativeObject::Use(*pcli_OutputDevice);
        i_OutputDeviceRef = NativeObject::GetNativeRef(*pcli_OutputDevice);
    }

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.Java.__Java()", i_OutputDeviceRef) << cli::endl;
    return i_OutputDeviceRef;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_Java__1_1finalize(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1Java_1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (NativeDevice<cli::OutputDevice>* const pcli_OutputDevice = NativeObject::GetNativeObject<NativeDevice<cli::OutputDevice>*>(I_NativeOutputDeviceRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_OutputDevice);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.Java.__finalize(I_NativeOutputDeviceRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeOutputDeviceRef", I_NativeOutputDeviceRef) << cli::endl;

        NativeObject::Free(*pcli_OutputDevice); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("OutputDevice.Java.__finalize()") << cli::endl;
    }
}


// OutputDevice static methods implementation.

extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1getNullDevice(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.__getNullDevice()") << cli::endl;
    const cli::OutputDevice& cli_NullDevice = cli::OutputDevice::GetNullDevice();
    const NativeObject::REF i_NullDeviceRef = NativeObject::GetNativeRef(cli_NullDevice);
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.__getNullDevice()", i_NullDeviceRef) << cli::endl;
    return i_NullDeviceRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1getStdOut(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.__getStdOut()") << cli::endl;
    const cli::OutputDevice& cli_StdOutDevice = cli::OutputDevice::GetStdOut();
    const NativeObject::REF i_StdOutDeviceRef = NativeObject::GetNativeRef(cli_StdOutDevice);
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.__getStdOut()", i_StdOutDeviceRef) << cli::endl;
    return i_StdOutDeviceRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1getStdErr(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.__getStdErr()") << cli::endl;
    const cli::OutputDevice& cli_StdErrDevice = cli::OutputDevice::GetStdErr();
    const NativeObject::REF i_StdErrDeviceRef = NativeObject::GetNativeRef(cli_StdErrDevice);
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.__getStdErr()", i_StdErrDeviceRef) << cli::endl;
    return i_StdErrDeviceRef;
}
