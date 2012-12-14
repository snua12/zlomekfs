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

#include <deque>
#include <iostream>

#include "cli/menu.h"
#include "cli/shell.h"
#include "cli/param.h"
#include "cli/endl.h"
#include "cli/debug.h"

#include "NativeMenu.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


const bool __NativeMenu__Execute(const cli::Menu& CLI_Menu, const cli::CommandLine& CLI_CmdLine)
{
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("__NativeMenu__Execute(CLI_CmdLine)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_Menu", NativeObject::GetNativeRef(CLI_Menu)) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_CmdLine", NativeObject::GetNativeRef(CLI_CmdLine)) << cli::endl;

    bool b_Res = false;

    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // For each parameter, create a corresponding Java object.
        std::deque<const cli::Param*> q_Params;
        const cli::Element* pcli_Element = NULL;
        for (   cli::CommandLineIterator it(CLI_CmdLine);
                it.StepIt() && (pcli_Element = *it); )
        {
            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValueStr("word", (const char*) pcli_Element->GetKeyword()) << cli::endl;
            if (const cli::Param* const pcli_Param = dynamic_cast<const cli::Param*>(pcli_Element))
            {
                if (pcli_Param->GetCloned())
                {
                    if (NativeObject::CreateFromNative(*pcli_Param))
                    {
                        q_Params.push_back(pcli_Param);
                    }
                }
            }
        }

        // Command line object interfacing.
        if (NativeObject::CreateFromNative(CLI_CmdLine))
        {
            // Java menu execution.
            if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Menu).c_str()))
            {
                cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
                if (const jmethodID pj_ExecuteMethodID = pj_Env->GetMethodID(pj_MenuClass, "__execute", "(I)Z"))
                {
                    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_ExecuteMethodID", pj_ExecuteMethodID) << cli::endl;
                    if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Menu), true))
                    {
                        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                        b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_ExecuteMethodID, (jint) NativeObject::GetNativeRef(CLI_CmdLine));
                        // Display error when Java code did not execute the command.
                        if (! b_Res)
                        {
                            std::string str_CommandLine;
                            const cli::Element* pcli_Element = NULL;
                            for (   cli::CommandLineIterator it(CLI_CmdLine);
                                    it.StepIt() && (pcli_Element = *it); )
                            {
                                if (! dynamic_cast<const cli::Endl*>(pcli_Element))
                                {
                                    if (! str_CommandLine.empty())
                                    {
                                        str_CommandLine += " ";
                                    }
                                    str_CommandLine += pcli_Element->GetKeyword();
                                }
                            }
                            // Display unless the command is a well known command.
                            if (true
                                && (str_CommandLine != "cli-config")
                                && (str_CommandLine != "exit")
                                && (str_CommandLine != "help")
                                && (str_CommandLine != "pwm")
                                && (str_CommandLine != "quit")
                                && (str_CommandLine != "traces"))
                            {
                                CLI_Menu.GetShell().GetStream(cli::ERROR_STREAM)
                                    << "Java failed while executing command: "
                                    << "'" << str_CommandLine.c_str() << "'"
                                    << cli::endl;
                            }
                        }
                    }
                }
            }

            NativeObject::DeleteFromNative(CLI_CmdLine);
        }

        // For each parameter, release the corresponding Java object.
        while (! q_Params.empty())
        {
            if (const cli::Param* const pcli_Param = q_Params.back())
            {
                NativeObject::DeleteFromNative(*pcli_Param);
            }
            q_Params.pop_back();
        }
    }

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("__NativeMenu__Execute()", b_Res) << cli::endl;
    return b_Res;
}

const bool __NativeMenu__OnError(const cli::Menu& CLI_Menu, const cli::ResourceString& CLI_Location, const cli::ResourceString& CLI_ErrorMessage)
{
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("__NativeMenu__OnError()") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_Menu", NativeObject::GetNativeRef(CLI_Menu)) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_Location", NativeObject::GetNativeRef(CLI_Location)) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_ErrorMessage", NativeObject::GetNativeRef(CLI_ErrorMessage)) << cli::endl;

    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // Java menu execution.
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Menu).c_str()))
        {
            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_OnErrorMethodID = pj_Env->GetMethodID(pj_MenuClass, "__onError", "(II)Z"))
            {
                cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_OnErrorMethodID", pj_OnErrorMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Menu), true))
                {
                    if (NativeObject::CreateFromNative(CLI_Location))
                    {
                        if (NativeObject::CreateFromNative(CLI_ErrorMessage))
                        {
                            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                            b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_OnErrorMethodID, (jint) NativeObject::GetNativeRef(CLI_Location), (jint) NativeObject::GetNativeRef(CLI_ErrorMessage));

                            NativeObject::DeleteFromNative(CLI_Location);
                        }
                        NativeObject::DeleteFromNative(CLI_ErrorMessage);
                    }
                }
            }
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("__NativeMenu__OnError()", b_Res) << cli::endl;
    return b_Res;
}

const bool __NativeMenu__OnExit(const cli::Menu& CLI_Menu)
{
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("__NativeMenu__OnExit()") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_Menu", NativeObject::GetNativeRef(CLI_Menu)) << cli::endl;

    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // Java menu execution.
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Menu).c_str()))
        {
            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_OnExitMethodID = pj_Env->GetMethodID(pj_MenuClass, "__onExit", "()V"))
            {
                cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_OnExitMethodID", pj_OnExitMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Menu), true))
                {
                    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    pj_Env->CallVoidMethod(pj_Object, pj_OnExitMethodID);
                    b_Res = true;
                }
            }
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("__NativeMenu__OnExit()", b_Res) << cli::endl;
    return b_Res;
}

const std::string __NativeMenu__OnPrompt(const cli::Menu& CLI_Menu)
{
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("__NativeMenu__OnPrompt()") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_Menu", NativeObject::GetNativeRef(CLI_Menu)) << cli::endl;

    std::string str_Prompt;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // Java menu execution.
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Menu).c_str()))
        {
            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_OnPromptMethodID = pj_Env->GetMethodID(pj_MenuClass, "__onPrompt", "()Ljava/lang/String;"))
            {
                cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_OnPromptMethodID", pj_OnPromptMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Menu), true))
                {
                    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    if (const jobject pj_Prompt = pj_Env->CallObjectMethod(pj_Object, pj_OnPromptMethodID))
                    {
                        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Prompt", pj_Prompt) << cli::endl;
                        str_Prompt = NativeExec::Java2Native((const jstring) pj_Prompt);
                    }
                }
            }
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("__NativeMenu__OnExit()", str_Prompt.c_str()) << cli::endl;
    return str_Prompt;
}
