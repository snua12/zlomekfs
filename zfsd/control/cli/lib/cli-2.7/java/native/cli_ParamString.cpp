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

#include "cli/param_string.h"

#include "cli_ParamString.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ParamString__1_1ParamString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeHelpRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ParamString.__ParamString(I_NativeHelpRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeHelpRef", I_NativeHelpRef) << cli::endl;
    NativeObject::REF i_ParamRef = 0;
    if (const cli::Help* const pcli_Help = NativeObject::GetNativeObject<const cli::Help*>(I_NativeHelpRef))
    {
        if (cli::ParamString* const pcli_Param = new cli::ParamString(*pcli_Help))
        {
            NativeObject::Use(*pcli_Param);
            i_ParamRef = NativeObject::GetNativeRef(*pcli_Param);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ParamString.__ParamString()", i_ParamRef) << cli::endl;
    return i_ParamRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ParamString__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeParamRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ParamString.__finalize(I_NativeParamRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeParamRef", I_NativeParamRef) << cli::endl;
    if (const cli::ParamString* const pcli_Param = NativeObject::GetNativeObject<const cli::ParamString*>(I_NativeParamRef))
    {
        NativeObject::Free(*pcli_Param);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ParamString.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_ParamString__1_1getValue(JNIEnv* PJ_Env, jclass PJ_Class, jint I_NativeParamRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ParamString.__getValue(I_NativeParamRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeParamRef", I_NativeParamRef) << cli::endl;
    std::string str_Value;
    if (const cli::ParamString* const pcli_Param = NativeObject::GetNativeObject<const cli::ParamString*>(I_NativeParamRef))
    {
        str_Value = *pcli_Param;
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("ParamString.__getValue()", str_Value.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Value);
}
