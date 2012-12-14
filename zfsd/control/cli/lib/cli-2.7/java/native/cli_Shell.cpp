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

#include "cli/shell.h"
#include "cli/cli.h"
#include "cli/io_device.h"

#include "cli_Shell.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1Shell(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeCliRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__Shell(I_NativeCliRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeCliRef", I_NativeCliRef) << cli::endl;
    NativeObject::REF i_ShellRef = 0;
    if (const cli::Cli* const pcli_Cli = NativeObject::GetNativeObject<const cli::Cli*>(I_NativeCliRef))
    {
        if (cli::Shell* const pcli_Shell = new cli::Shell(*pcli_Cli))
        {
            NativeObject::Use(*pcli_Cli);
            NativeObject::Use(*pcli_Shell);
            i_ShellRef = NativeObject::GetNativeRef(*pcli_Shell);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__Shell()", i_ShellRef) << cli::endl;
    return i_ShellRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__finalize(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        const cli::Cli& cli_Cli = pcli_Shell->GetCli();
        NativeObject::Free(*pcli_Shell);
        NativeObject::Free(cli_Cli);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getCli(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getCli(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    NativeObject::REF i_CliRef = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        const cli::Cli& cli_Cli = pcli_Shell->GetCli();
        i_CliRef = NativeObject::GetNativeRef(cli_Cli);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getCli()", i_CliRef) << cli::endl;
    return i_CliRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getInput(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getInput(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    NativeObject::REF i_InputRef = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        const cli::IODevice& cli_Input = pcli_Shell->GetInput();
        i_InputRef = NativeObject::GetNativeRef(cli_Input);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getInput()", i_InputRef) << cli::endl;
    return i_InputRef;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint E_StreamType)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getStream(I_NativeShellRef, E_StreamType)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_StreamType", E_StreamType) << cli::endl;
    NativeObject::REF i_StreamRef = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        const cli::OutputDevice& cli_Stream = pcli_Shell->GetStream((cli::STREAM_TYPE) E_StreamType);
        i_StreamRef = NativeObject::GetNativeRef(cli_Stream);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getStream()", i_StreamRef) << cli::endl;
    return i_StreamRef;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1setStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint E_StreamType, jint I_NativeDeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setStream(I_NativeShellRef, E_StreamType, I_NativeDeviceRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_StreamType", E_StreamType) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeDeviceRef", I_NativeDeviceRef) << cli::endl;
    bool b_Res = false;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (((E_StreamType >= 0) && (E_StreamType < cli::STREAM_TYPES_COUNT))
            || (E_StreamType == cli::ALL_STREAMS))
        {
            if (cli::OutputDevice* const pcli_Device = NativeObject::GetNativeObject<cli::OutputDevice*>(I_NativeDeviceRef))
            {
                b_Res = pcli_Shell->SetStream((cli::STREAM_TYPE) E_StreamType, *pcli_Device);
            }
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__setStream()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1streamEnabled(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint E_StreamType)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__streamEnabled(I_NativeShellRef, E_StreamType)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_StreamType", E_StreamType) << cli::endl;
    bool b_Res = false;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        b_Res = pcli_Shell->StreamEnabled((cli::STREAM_TYPE) E_StreamType);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__streamEnabled()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1enableStream(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint E_StreamType, jboolean B_Enable)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__enableStream(I_NativeShellRef, E_StreamType, B_Enable)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_StreamType", E_StreamType) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_Enable", B_Enable) << cli::endl;
    bool b_Res = false;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        b_Res = pcli_Shell->EnableStream((cli::STREAM_TYPE) E_StreamType, B_Enable);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__enableStream()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1setWelcomeMessage(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_NativeWelcomeMessageRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setWelcomeMessage(I_NativeShellRef, I_NativeWelcomeMessageRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeWelcomeMessageRef", I_NativeWelcomeMessageRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (const cli::ResourceString* const pcli_WelcomeMessage = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativeWelcomeMessageRef))
        {
            pcli_Shell->SetWelcomeMessage(*pcli_WelcomeMessage);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__setWelcomeMessage()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1setByeMessage(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_NativeByeMessageRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setByeMessage(I_NativeShellRef, I_NativeWelcomeMessageRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeByeMessageRef", I_NativeByeMessageRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (const cli::ResourceString* const pcli_ByeMessage = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativeByeMessageRef))
        {
            pcli_Shell->SetByeMessage(*pcli_ByeMessage);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__setByeMessage()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1setPrompt(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_NativePromptRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setPrompt(I_NativeShellRef, I_NativePromptRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativePromptRef", I_NativePromptRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (const cli::ResourceString* const pcli_Prompt = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativePromptRef))
        {
            pcli_Shell->SetPrompt(*pcli_Prompt);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__setPrompt()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1setLang(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint E_Lang)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setLang(I_NativeShellRef, E_Lang)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_Lang", E_Lang) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if ((E_Lang >= 0) && (E_Lang < cli::ResourceString::LANG_COUNT))
        {
            pcli_Shell->SetLang((cli::ResourceString::LANG) E_Lang);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__setLang()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getLang(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getLang(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::ResourceString::LANG e_Lang = cli::ResourceString::LANG_DEFAULT;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        e_Lang = pcli_Shell->GetLang();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getLang()", e_Lang) << cli::endl;
    return e_Lang;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1setBeep(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jboolean B_Enable)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__setBeep(I_NativeShellRef, B_Enable)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_Enable", B_Enable) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->SetBeep(B_Enable);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__setBeep()") << cli::endl;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1getBeep(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getBeep(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    bool b_Beep = false;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        b_Beep = pcli_Shell->GetBeep();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__getBeep()", b_Beep) << cli::endl;
    return b_Beep;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1run(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_NativeIODeviceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__run(I_NativeShellRef, I_NativeIODeviceRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeIODeviceRef", I_NativeIODeviceRef) << cli::endl;
    bool b_Res = false;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (cli::IODevice* const pcli_IODevice = NativeObject::GetNativeObject<cli::IODevice*>(I_NativeIODeviceRef))
        {
            pcli_Shell->Run(*pcli_IODevice);
            b_Res = true;
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__run()", b_Res) << cli::endl;
    return b_Res;
}

extern "C" JNIEXPORT jboolean JNICALL Java_cli_Shell__1_1isRunning(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__isRunning(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    bool b_IsRunning = false;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        b_IsRunning = pcli_Shell->IsRunning();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("Shell.__isRunning()", b_IsRunning) << cli::endl;
    return b_IsRunning;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getHelpMargin(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getHelpMargin(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    int i_HelpMargin = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        i_HelpMargin = pcli_Shell->GetHelpMargin();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getHelpMargin()", i_HelpMargin) << cli::endl;
    return i_HelpMargin;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getHelpOffset(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getHelpOffset(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    int i_HelpOffset = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        i_HelpOffset = pcli_Shell->GetHelpOffset();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getHelpOffset()", i_HelpOffset) << cli::endl;
    return i_HelpOffset;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_Shell__1_1getCurrentMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_MenuIndex)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__getCurrentMenu(I_NativeShellRef, I_MenuIndex)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MenuIndex", I_MenuIndex) << cli::endl;
    NativeObject::REF i_CurrentMenuRef = 0;
    if (const cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<const cli::Shell*>(I_NativeShellRef))
    {
        if (const cli::Menu* const pcli_CurrentMenu = pcli_Shell->GetCurrentMenu(I_MenuIndex))
        {
            i_CurrentMenuRef = NativeObject::GetNativeRef(*pcli_CurrentMenu);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("Shell.__getCurrentMenu()", i_CurrentMenuRef) << cli::endl;
    return i_CurrentMenuRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1enterMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jint I_NativeMenuRef, jboolean B_PromptMenu)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__enterMenu(I_NativeShellRef, I_NativeMenuRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeMenuRef", I_NativeMenuRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_PromptMenu", B_PromptMenu) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        if (const cli::Menu* const pcli_Menu = NativeObject::GetNativeObject<const cli::Menu*>(I_NativeMenuRef))
        {
            pcli_Shell->EnterMenu(*pcli_Menu, B_PromptMenu);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__enterMenu()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1exitMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jboolean B_PromptMenu)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__exitMenu(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_PromptMenu", B_PromptMenu) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->ExitMenu(B_PromptMenu);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__exitMenu()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1quit(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__quit(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->Quit();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__quit()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1quitThreadSafe(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__quitThreadSafe(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->QuitThreadSafe();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__quitThreadSafe()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1displayHelp(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__displayHelp(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->DisplayHelp();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__displayHelp()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1printWorkingMenu(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__printWorkingMenu(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->PrintWorkingMenu();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__printWorkingMenu()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_Shell__1_1cleanScreen(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeShellRef, jboolean B_PromptMenu)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("Shell.__cleanScreen(I_NativeShellRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeShellRef", I_NativeShellRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamBool("B_PromptMenu", B_PromptMenu) << cli::endl;
    if (cli::Shell* const pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(I_NativeShellRef))
    {
        pcli_Shell->CleanScreen(B_PromptMenu);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("Shell.__cleanScreen()") << cli::endl;
}
