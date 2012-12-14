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

#include "cli_OutputDevice_ScreenInfo.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_ScreenInfo__1_1ScreenInfo(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1ScreenInfo(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_Width, jint I_Height, jboolean B_TrueCls, jboolean B_WrapLines)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__ScreenInfo(I_Width, I_Height, B_TrueCls, B_WrapLines)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_Width", I_Width) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_Height", I_Height) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_TrueCls", B_TrueCls) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_WrapLines", B_WrapLines) << cli::endl;
    NativeObject::REF i_ScreenInfoRef = 0;
    if (cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = new cli::OutputDevice::ScreenInfo(I_Width, I_Height, B_TrueCls, B_WrapLines))
    {
        NativeObject::Use(*pcli_ScreenInfo);
        i_ScreenInfoRef = NativeObject::GetNativeRef(*pcli_ScreenInfo);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.ScreenInfo.__ScreenInfo()", i_ScreenInfoRef) << cli::endl;
    return i_ScreenInfoRef;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_ScreenInfo__1_1finalize(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__finalize(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        NativeObject::Free(*pcli_ScreenInfo);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("OutputDevice.ScreenInfo.__finalize()") << cli::endl;
}

//extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice_ScreenInfo__1_1copy(
extern "C" JNIEXPORT void JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1copy(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef1, jint I_NativeScreenInfoRef2)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__copy(I_NativeScreenInfoRef1, I_NativeScreenInfoRef2)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef1", I_NativeScreenInfoRef1) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef2", I_NativeScreenInfoRef2) << cli::endl;
    if (cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo1 = NativeObject::GetNativeObject<cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef1))
    {
        if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo2 = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef2))
        {
            pcli_ScreenInfo1->operator=(*pcli_ScreenInfo2);
        }
    }
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_ScreenInfo__1_1getWidth(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1getWidth(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__getWidth(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    int i_Width = 0;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        i_Width = pcli_ScreenInfo->GetWidth();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.ScreenInfo.__getWidth()", i_Width) << cli::endl;
    return i_Width;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_ScreenInfo__1_1getSafeWidth(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1getSafeWidth(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__getSafeWidth(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    unsigned int ui_SafeWidth = cli::OutputDevice::ScreenInfo::DEFAULT_WIDTH;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        ui_SafeWidth = pcli_ScreenInfo->GetSafeWidth();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.ScreenInfo.__getSafeWidth()", ui_SafeWidth) << cli::endl;
    return ui_SafeWidth;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_ScreenInfo__1_1getHeight(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1getHeight(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__getHeight(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    int i_Height = 0;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        i_Height = pcli_ScreenInfo->GetHeight();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.ScreenInfo.__getHeight()", i_Height) << cli::endl;
    return i_Height;
}

//extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice_ScreenInfo__1_1getSafeHeight(
extern "C" JNIEXPORT jint JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1getSafeHeight(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__getSafeHeight(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    unsigned int ui_SafeHeight = cli::OutputDevice::ScreenInfo::DEFAULT_HEIGHT;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        ui_SafeHeight = pcli_ScreenInfo->GetSafeHeight();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputDevice.ScreenInfo.__getSafeHeight()", ui_SafeHeight) << cli::endl;
    return ui_SafeHeight;
}

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice_ScreenInfo__1_1hasTrueCls(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1hasTrueCls(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__hasTrueCls(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    bool b_HasTrueCls = false;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        b_HasTrueCls = pcli_ScreenInfo->GetbTrueCls();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("OutputDevice.ScreenInfo.__hasTrueCls()", b_HasTrueCls) << cli::endl;
    return b_HasTrueCls;
}

//extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice_ScreenInfo__1_1getbWrapLines(
extern "C" JNIEXPORT jboolean JNICALL Java_cli_OutputDevice__1_1ScreenInfo_1_1getbWrapLines(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeScreenInfoRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputDevice.ScreenInfo.__getbWrapLines(I_NativeScreenInfoRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeScreenInfoRef", I_NativeScreenInfoRef) << cli::endl;
    bool b_WrapLines = false;
    if (const cli::OutputDevice::ScreenInfo* const pcli_ScreenInfo = NativeObject::GetNativeObject<const cli::OutputDevice::ScreenInfo*>(I_NativeScreenInfoRef))
    {
        b_WrapLines = pcli_ScreenInfo->GetbWrapLines();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("OutputDevice.ScreenInfo.__getbWrapLines()", b_WrapLines) << cli::endl;
    return b_WrapLines;
}
