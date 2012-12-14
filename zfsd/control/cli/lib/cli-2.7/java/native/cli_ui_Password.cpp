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

#include "cli/ui_password.h"

#include "cli_ui_Password.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Password__1_1Password(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jboolean B_DisplayStars, jint I_MinPasswordLength, jint I_MaxPasswordLength)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Password.__Password(B_DisplayStars, I_MinPasswordLength, I_MaxPasswordLength)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_DisplayStars", B_DisplayStars) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MinPasswordLength", I_MinPasswordLength) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MaxPasswordLength", I_MaxPasswordLength) << cli::endl;
    NativeObject::REF i_PasswordRef = 0;
    if (cli::ui::Password* const pcli_Password = new cli::ui::Password(B_DisplayStars, I_MinPasswordLength, I_MaxPasswordLength))
    {
        NativeObject::Use(*pcli_Password);
        i_PasswordRef = NativeObject::GetNativeRef(*pcli_Password);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Password.__Password()", i_PasswordRef) << cli::endl;
    return i_PasswordRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_Password__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativePasswordRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Password.__finalize(I_NativePasswordRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativePasswordRef", I_NativePasswordRef) << cli::endl;
    if (const cli::ui::Password* const pcli_Password = NativeObject::GetNativeObject<const cli::ui::Password*>(I_NativePasswordRef))
    {
        NativeObject::Free(*pcli_Password);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Password.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_ui_Password__1_1getPassword(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativePasswordRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Password.__getPassword(I_NativePasswordRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativePasswordRef", I_NativePasswordRef) << cli::endl;
    std::string str_Password;
    if (const cli::ui::Password* const pcli_Password = NativeObject::GetNativeObject<const cli::ui::Password*>(I_NativePasswordRef))
    {
        str_Password = (const char*) pcli_Password->GetPassword();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("ui.Password.__getPassword()", str_Password.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Password);
}
