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

#include "cli/menu.h"

#include "cli_Menu.h"

#include "NativeMenu.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_Menu__1_1Menu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_Name, jint I_NativeHelpRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Menu.__Menu(PJ_Name, I_NativeHelpRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_Name", NativeExec::Java2Native(PJ_Name).c_str()) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeHelpRef", I_NativeHelpRef) << cli::endl;
    NativeObject::REF i_MenuRef = 0;
    if (const cli::Help* const pcli_Help = NativeObject::GetNativeObject<const cli::Help*>(I_NativeHelpRef))
    {
        if (cli::Menu* const pcli_Menu = new NativeMenu<cli::Menu>(NativeExec::Java2Native(PJ_Name).c_str(), *pcli_Help))
        {
            NativeObject::Use(*pcli_Menu);
            i_MenuRef = NativeObject::GetNativeRef(*pcli_Menu);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Menu.__Menu()", i_MenuRef) << cli::endl;
    return i_MenuRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Menu__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeMenuRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Menu.__finalize(PJ_Name, I_NativeMenuRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeMenuRef", I_NativeMenuRef) << cli::endl;
    if (const cli::Menu* const pcli_Menu = NativeObject::GetNativeObject<const cli::Menu*>(I_NativeMenuRef))
    {
        NativeObject::Free(*pcli_Menu);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Menu.__finalize()") << cli::endl;
}
