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

#include <string>

#include "cli/element.h"
#include "cli/cli.h"
#include "cli/shell.h"
#include "cli/io_device.h"

#include "cli_Element.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jstring JNICALL Java_cli_Element__1_1getKeyword(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getKeyword(I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    std::string str_Keyword;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        str_Keyword = (const char*) pcli_Element->GetKeyword();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("Element.__getKeyword()", str_Keyword.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Keyword);
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Element__1_1getHelp(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getHelp(I_NativeElementRef, I_NativeHelpRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    NativeObject::REF i_HelpRef = 0;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        const cli::Help& cli_Help = pcli_Element->GetHelp();
        NativeObject::CreateFromNative(cli_Help);
        NativeObject::Delegate(cli_Help, *pcli_Element);
        i_HelpRef = NativeObject::GetNativeRef(cli_Help);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Element.__getHelp()", i_HelpRef) << cli::endl;
    return i_HelpRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Element__1_1getCli(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getCli(I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    NativeObject::REF i_CliRef = 0;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        const cli::Cli& cli_Cli = pcli_Element->GetCli();
        i_CliRef = NativeObject::GetNativeRef(cli_Cli);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Element.__getCli()", i_CliRef) << cli::endl;
    return i_CliRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Element__1_1getShell(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getShell(I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    NativeObject::REF i_ShellRef = 0;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        const cli::Shell& cli_Shell = pcli_Element->GetShell();
        i_ShellRef = NativeObject::GetNativeRef(cli_Shell);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Element.__getShell()", i_ShellRef) << cli::endl;
    return i_ShellRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Element__1_1getOutputStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getOutputStream(I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    NativeObject::REF i_OutputDeviceRef = 0;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        const cli::OutputDevice& cli_OutputDevice = pcli_Element->GetOutputStream();
        i_OutputDeviceRef = NativeObject::GetNativeRef(cli_OutputDevice);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Element.__getOutputStream()", i_OutputDeviceRef) << cli::endl;
    return i_OutputDeviceRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Element__1_1getErrorStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Element.__getErrorStream(I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    NativeObject::REF i_ErrorDeviceRef = 0;
    if (const cli::Element* const pcli_Element = NativeObject::GetNativeObject<const cli::Element*>(I_NativeElementRef))
    {
        const cli::OutputDevice& cli_ErrorDevice = pcli_Element->GetErrorStream();
        i_ErrorDeviceRef = NativeObject::GetNativeRef(cli_ErrorDevice);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Element.__getErrorStream()", i_ErrorDeviceRef) << cli::endl;
    return i_ErrorDeviceRef;
}
