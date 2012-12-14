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

#include "cli/command_line.h"

#include "cli_CommandLine.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_CommandLine__1_1CommandLine(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("CommandLine.__CommandLine()") << cli::endl;
    NativeObject::REF i_CmdLineRef = 0;
    if (cli::CommandLine* const pcli_CmdLine = new cli::CommandLine())
    {
        NativeObject::Use(*pcli_CmdLine);
        i_CmdLineRef = NativeObject::GetNativeRef(*pcli_CmdLine);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("CommandLine.__CommandLine()", i_CmdLineRef) << cli::endl;
    return i_CmdLineRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_CommandLine__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCmdLineRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("CommandLine.__finalize(I_NativeCmdLineRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCmdLineRef", I_NativeCmdLineRef) << cli::endl;
    if (const cli::CommandLine* const pcli_CmdLine = NativeObject::GetNativeObject<const cli::CommandLine*>(I_NativeCmdLineRef))
    {
        NativeObject::Free(*pcli_CmdLine);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("CommandLine.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_CommandLine__1_1getElementCount(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCmdLineRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("CommandLine.__getElementCount(I_NativeCmdLineRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCmdLineRef", I_NativeCmdLineRef) << cli::endl;
    int i_Count = 0;
    if (const cli::CommandLine* const pcli_CmdLine = NativeObject::GetNativeObject<const cli::CommandLine*>(I_NativeCmdLineRef))
    {
        for (   cli::CommandLineIterator it(*pcli_CmdLine);
                it.StepIt(); )
        {
            i_Count ++;
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("CommandLine.__getElementCount()", i_Count) << cli::endl;
    return i_Count;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_CommandLine__1_1getElementAt(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCmdLineRef, jint I_Position)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("CommandLine.__getElementAt(I_NativeCmdLineRef, I_Position)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCmdLineRef", I_NativeCmdLineRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_Position", I_Position) << cli::endl;
    NativeObject::REF i_ElementRef = 0;
    if (const cli::CommandLine* const pcli_CmdLine = NativeObject::GetNativeObject<const cli::CommandLine*>(I_NativeCmdLineRef))
    {
        int i_Count = 0;
        for (   cli::CommandLineIterator it(*pcli_CmdLine);
                it.StepIt() && (i_ElementRef == 0);
                )
        {
            if (i_Count >= I_Position)
            {
                if (const cli::Element* const pcli_Element = *it)
                {
                    i_ElementRef = NativeObject::GetNativeRef(*pcli_Element);
                }
            }
            i_Count ++;
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("CommandLine.__getElementAt()", i_ElementRef) << cli::endl;
    return i_ElementRef;
}
