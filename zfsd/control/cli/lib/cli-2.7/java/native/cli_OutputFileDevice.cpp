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

#include "cli/file_device.h"

#include "cli_OutputFileDevice.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_OutputFileDevice__1_1OutputFileDevice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_OutputFileName)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputFileDevice.__OutputFileDevice(PJ_OutputFileName)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_OutputFileName", NativeExec::Java2Native(PJ_OutputFileName).c_str()) << cli::endl;
    NativeObject::REF i_FileDeviceRef = 0;
    if (cli::OutputFileDevice* const pcli_FileDevice = new cli::OutputFileDevice(NativeExec::Java2Native(PJ_OutputFileName).c_str(), true))
    {
        NativeObject::Use(*pcli_FileDevice);
        i_FileDeviceRef = NativeObject::GetNativeRef(*pcli_FileDevice);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("OutputFileDevice.__OutputFileDevice()", i_FileDeviceRef) << cli::endl;
    return i_FileDeviceRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_OutputFileDevice__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeFileDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    if (const cli::OutputFileDevice* const pcli_FileDevice = NativeObject::GetNativeObject<const cli::OutputFileDevice*>(I_NativeFileDeviceRef))
    {
        // If b_SafeTrace is true, it means the current trace stream is not pcli_IODevice nor it would output pcli_IODevice.
        // Whether pcli_IODevice is about to be destroyed, if b_SafeTrace is true, there is no problem for tracing even after possible destruction.
        const bool b_SafeTrace = cli::GetTraces().IsSafe(*pcli_FileDevice);

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("OutputFileDevice.__finalize(I_NativeFileDeviceRef)") << cli::endl;
        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeFileDeviceRef", I_NativeFileDeviceRef) << cli::endl;

        NativeObject::Free(*pcli_FileDevice); // <- possible destruction.

        if (b_SafeTrace) cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("OutputFileDevice.__finalize()") << cli::endl;
    }
}
