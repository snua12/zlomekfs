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

#include "cli/cli.h"
#include "cli/string_device.h"

#include "cli_Cli.h"

#include "NativeMenu.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_Cli__1_1Cli(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_Name, jint I_NativeHelpRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__Cli(PJ_Name, I_NativeHelpRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_Name", NativeExec::Java2Native(PJ_Name).c_str()) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CliRef", I_NativeHelpRef) << cli::endl;
    NativeObject::REF i_CliRef = 0;
    if (const cli::Help* const pcli_Help = NativeObject::GetNativeObject<const cli::Help*>(I_NativeHelpRef))
    {
        if (cli::Cli* const pcli_Cli = new NativeMenu<cli::Cli>(NativeExec::Java2Native(PJ_Name).c_str(), *pcli_Help))
        {
            NativeObject::Use(*pcli_Cli);
            i_CliRef = NativeObject::GetNativeRef(*pcli_Cli);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Cli.__Cli()", i_CliRef) << cli::endl;
    return i_CliRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Cli__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__finalize(I_NativeCliRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    if (const cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<const cli::Cli*>(I_NativeCliRef))
    {
        NativeObject::Free(*pcli_Cli);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Cli.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jintArray JNICALL Java_cli_Cli__1_1findFromName(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_RegExp)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__findFromName(PJ_RegExp)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_RegExp", NativeExec::Java2Native(PJ_RegExp).c_str()) << cli::endl;

    // Retrieve CLI references.
    cli::Cli::List cli_CliList(0); // UI_MaxCount not taken in account by tk STL implementation
    cli::Cli::FindFromName(cli_CliList, NativeExec::Java2Native(PJ_RegExp).c_str());

    // Convert from cli::Cli* to NativeObject::REF references.
    std::vector<NativeObject::REF> std_CliList;
    for (   cli::Cli::List::Iterator it = cli_CliList.GetIterator();
            cli_CliList.IsValid(it);
            cli_CliList.MoveNext(it))
    {
        if (const cli::Cli* const pcli_Cli = cli_CliList.GetAt(it))
        {
            cli::StringDevice cli_ValueName(0, false); cli_ValueName << "std_CliList[" << std_CliList.size() << "]";
            const NativeObject::REF i_CliRef = NativeObject::GetNativeRef(*pcli_Cli);
            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValueInt(cli_ValueName.GetString(), i_CliRef) << cli::endl;
            std_CliList.push_back(i_CliRef);
        }
    }

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Cli.__findFromName()") << cli::endl;
    return NativeExec::Native2Java(std_CliList);
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_Cli__1_1getName(JNIEnv* PJ_Env, jclass PJ_Class, jint I_NativeCliRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__getName(I_NativeCliRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    std::string str_Name;
    if (const cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<const cli::Cli*>(I_NativeCliRef))
    {
        str_Name = (const char*) pcli_Cli->GetName();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("Cli.__getName()", str_Name.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Name);
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Cli__1_1addMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef, jint I_NativeMenuRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__addMenu(I_NativeCliRef, I_NativeMenuRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeMenuRef", I_NativeMenuRef) << cli::endl;
    bool b_Res = false;
    if (cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<cli::Cli*>(I_NativeCliRef))
    {
        if (cli::Menu* const pcli_Menu = NativeObject::GetNativeObject<cli::Menu*>(I_NativeMenuRef))
        {
            pcli_Cli->AddMenu(pcli_Menu);
            NativeObject::Delegate(*pcli_Menu, *pcli_Cli);
            b_Res = true;
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Cli.__addMenu()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Cli__1_1getMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef, jstring PJ_MenuName)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__getMenu(I_NativeCliRef, I_NativeMenuRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_MenuName", NativeExec::Java2Native(PJ_MenuName).c_str()) << cli::endl;
    NativeObject::REF i_MenuRef = 0;
    if (const cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<const cli::Cli*>(I_NativeCliRef))
    {
        if (const cli::Menu* const pcli_Menu = pcli_Cli->GetMenu(NativeExec::Java2Native(PJ_MenuName).c_str()))
        {
            i_MenuRef = NativeObject::GetNativeRef(*pcli_Menu);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Cli.__getMenu()", i_MenuRef) << cli::endl;
    return i_MenuRef;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Cli__1_1isConfigMenuEnabled(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__isConfigMenuEnabled(I_NativeCliRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    bool b_IsConfigMenuEnabled = false;
    if (cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<cli::Cli*>(I_NativeCliRef))
    {
        b_IsConfigMenuEnabled = pcli_Cli->IsConfigMenuEnabled();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Cli.__isConfigMenuEnabled()", b_IsConfigMenuEnabled) << cli::endl;
    return b_IsConfigMenuEnabled;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Cli__1_1enableConfigMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef, jboolean B_Enable)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Cli.__enableConfigMenu(I_NativeCliRef, B_Enable)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_Enable", B_Enable) << cli::endl;
    bool b_Res = false;
    if (cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<cli::Cli*>(I_NativeCliRef))
    {
        b_Res = pcli_Cli->EnableConfigMenu(B_Enable);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Cli.__enableConfigMenu()", b_Res) << cli::endl;
    return b_Res;
}
