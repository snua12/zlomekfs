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

#include "cli/traces.h"
#include "cli/io_device.h"

#include "cli_CommandLine.h"

#include "NativeTraces.h"
#include "NativeExec.h"
#include "NativeObject.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_Traces__1_1getStream(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    const cli::OutputDevice& cli_TraceStream = cli::GetTraces().GetStream();
    const NativeObject::REF i_TraceStreamRef = NativeObject::GetNativeRef(cli_TraceStream);
    return i_TraceStreamRef;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Traces__1_1setStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (cli::OutputDevice* const pcli_Stream = NativeObject::GetNativeObject<cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        b_Res = cli::GetTraces().SetStream(*pcli_Stream);
    }
    return b_Res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Traces__1_1unsetStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeOutputDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    bool b_Res = false;
    if (cli::OutputDevice* const pcli_Stream = NativeObject::GetNativeObject<cli::OutputDevice*>(I_NativeOutputDeviceRef))
    {
        b_Res = cli::GetTraces().UnsetStream(*pcli_Stream);
    }
    return b_Res;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Traces__1_1setFilter(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeTraceClassRef, jboolean B_ShowTraces)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::TraceClass* const pcli_TraceClass = NativeObject::GetNativeObject<const cli::TraceClass*>(I_NativeTraceClassRef))
    {
        cli::GetTraces().Declare(*pcli_TraceClass);
        cli::GetTraces().SetFilter(*pcli_TraceClass, B_ShowTraces);
    }
}

extern "C" JNIEXPORT void JNICALL Java_cli_Traces__1_1setAllFilter(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jboolean B_ShowTraces)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().SetAllFilter(B_ShowTraces);
}

extern "C" JNIEXPORT void JNICALL Java_cli_Traces__1_1trace(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_TraceClassNativeRef, jstring PJ_Text)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::TraceClass* const pcli_TraceClass = NativeObject::GetNativeObject<const cli::TraceClass*>(I_TraceClassNativeRef))
    {
        cli::GetTraces().Trace(*pcli_TraceClass) << NativeExec::Java2Native(PJ_Text).c_str() << cli::endl;
    }
}
