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

#include "cli_NativeTraces.h"

#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jstring JNICALL Java_cli_NativeTraces__1_1begin(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_Method)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_Trace = (const char*) NativeTraces::Begin(
        NativeExec::Java2Native(PJ_Method).c_str()
    );
    return NativeExec::Native2Java(str_Trace);
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_NativeTraces__1_1param(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_ParamName, jstring PJ_ParamValue)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_Trace = (const char*) NativeTraces::ParamStr(
        NativeExec::Java2Native(PJ_ParamName).c_str(),
        NativeExec::Java2Native(PJ_ParamValue).c_str()
    );
    return NativeExec::Native2Java(str_Trace);
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_NativeTraces__1_1value(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_VarName, jstring PJ_VarValue)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_Trace = (const char*) NativeTraces::ValueStr(
        NativeExec::Java2Native(PJ_VarName).c_str(),
        NativeExec::Java2Native(PJ_VarValue).c_str()
    );
    return NativeExec::Native2Java(str_Trace);
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_NativeTraces__1_1end__Ljava_lang_String_2(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_Method)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_Trace = (const char*) NativeTraces::EndVoid(
        NativeExec::Java2Native(PJ_Method).c_str()
    );
    return NativeExec::Native2Java(str_Trace);
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_NativeTraces__1_1end__Ljava_lang_String_2Ljava_lang_String_2(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_Method, jstring PJ_Result)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    std::string str_Trace = (const char*) NativeTraces::EndStr(
        NativeExec::Java2Native(PJ_Method).c_str(),
        NativeExec::Java2Native(PJ_Result).c_str()
    );
    return NativeExec::Native2Java(str_Trace);
}
