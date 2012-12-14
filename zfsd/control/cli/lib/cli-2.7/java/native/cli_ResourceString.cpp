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

#include "cli/resource_string.h"

#include "cli_ResourceString.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ResourceString__1_1ResourceString__(JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__ResourceString()") << cli::endl;
    NativeObject::REF i_StringRef = 0;
    if (cli::ResourceString* const pcli_String = new cli::ResourceString())
    {
        NativeObject::Use(*pcli_String);
        i_StringRef = NativeObject::GetNativeRef(*pcli_String);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ResourceString.__ResourceString()", i_StringRef) << cli::endl;
    return i_StringRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_ResourceString__1_1ResourceString__I(JNIEnv* PJ_Env, jclass PJ_Class, jint I_NativeStringRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__ResourceString(I_NativeStringRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeStringRef", I_NativeStringRef) << cli::endl;
    NativeObject::REF i_StringRef = 0;
    if (const cli::ResourceString* const pcli_Src = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativeStringRef))
    {
        if (cli::ResourceString* const pcli_String = new cli::ResourceString(*pcli_Src))
        {
            NativeObject::Use(*pcli_String);
            i_StringRef = NativeObject::GetNativeRef(*pcli_String);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ResourceString.__ResourceString()", i_StringRef) << cli::endl;
    return i_StringRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ResourceString__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__finalize(I_NativeStringRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeStringRef", I_NativeStringRef) << cli::endl;
    if (const cli::ResourceString* const pcli_String = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativeStringRef))
    {
        NativeObject::Free(*pcli_String);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ResourceString.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_ResourceString__1_1setString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringRef, jint E_Lang, jstring PJ_ResourceString)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__addString(I_NativeStringRef, E_Lang, PJ_ResourceString)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeStringRef", I_NativeStringRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_Lang", E_Lang) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_ResourceString", NativeExec::Java2Native(PJ_ResourceString).c_str()) << cli::endl;
    bool b_Res = false;
    if (cli::ResourceString* const pcli_String = NativeObject::GetNativeObject<cli::ResourceString*>(I_NativeStringRef))
    {
        pcli_String->SetString((cli::ResourceString::LANG) E_Lang, NativeExec::Java2Native(PJ_ResourceString).c_str());
        b_Res = true;
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("ResourceString.__addString()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_ResourceString__1_1hasString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringRef, jint E_Lang)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__hasString()") << cli::endl;
    bool b_Res = false;
    if (const cli::ResourceString* const pcli_String = NativeObject::GetNativeObject<cli::ResourceString*>(I_NativeStringRef))
    {
        b_Res = pcli_String->HasString((cli::ResourceString::LANG) E_Lang);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("ResourceString.__hasString()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_ResourceString__1_1getString(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeStringRef, jint E_Lang)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ResourceString.__getString()") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeStringRef", I_NativeStringRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_Lang", E_Lang) << cli::endl;
    std::string str_ResourceString;
    if (const cli::ResourceString* const pcli_String = NativeObject::GetNativeObject<cli::ResourceString*>(I_NativeStringRef))
    {
        str_ResourceString = (const char*) pcli_String->GetString((cli::ResourceString::LANG) E_Lang);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("ResourceString.__getString()", str_ResourceString.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_ResourceString);
}
