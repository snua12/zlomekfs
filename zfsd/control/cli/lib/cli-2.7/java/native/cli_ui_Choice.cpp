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

#include "cli/ui_choice.h"

#include "cli_ui_Choice.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Choice__1_1beginChoiceList(
        JNIEnv* PJ_Env, jclass PJ_Class)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__beginChoiceList()") << cli::endl;
    NativeObject::REF i_ChoiceListRef = 0;
    // Make a dynamic allocation at this point.
    // It should be deleted in __Choice() below.
    if (cli::tk::Queue<cli::ResourceString>* const ptk_ChoiceList = new cli::tk::Queue<cli::ResourceString>(0)) // UI_MaxCount not taken in account by STL implementation
    {
        i_ChoiceListRef = NativeObject::GetNativeRef(*ptk_ChoiceList);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Choice.__beginChoiceList()", i_ChoiceListRef) << cli::endl;
    return i_ChoiceListRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_Choice__1_1addChoice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeChoiceListRef, jint I_NativeResourceStringRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__addChoice(I_NativeChoiceListRef, I_NativeResourceStringRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeChoiceListRef", I_NativeChoiceListRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeResourceStringRef", I_NativeResourceStringRef) << cli::endl;
    if (cli::tk::Queue<cli::ResourceString>* const ptk_ChoiceList = NativeObject::GetNativeObject<cli::tk::Queue<cli::ResourceString>*>(I_NativeChoiceListRef))
    {
        if (const cli::ResourceString* const pcli_ResourceString = NativeObject::GetNativeObject<const cli::ResourceString*>(I_NativeResourceStringRef))
        {
            ptk_ChoiceList->AddTail(*pcli_ResourceString);
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Choice.__addChoice()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Choice__1_1Choice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_DefaultChoice, jint I_NativeChoiceListRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__Choice(I_DefaultChoice, I_NativeChoiceListRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_DefaultChoice", I_DefaultChoice) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeChoiceListRef", I_NativeChoiceListRef) << cli::endl;
    NativeObject::REF i_ChoiceRef = 0;
    if (const cli::tk::Queue<cli::ResourceString>* const ptk_ChoiceList = NativeObject::GetNativeObject<const cli::tk::Queue<cli::ResourceString>*>(I_NativeChoiceListRef))
    {
        if (cli::ui::Choice* const pcli_Choice = new cli::ui::Choice(I_DefaultChoice, *ptk_ChoiceList))
        {
            NativeObject::Use(*pcli_Choice);
            i_ChoiceRef = NativeObject::GetNativeRef(*pcli_Choice);
        }
        // Delete the  list previously allocated in __beginChoiceList().
        delete ptk_ChoiceList;
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Choice.__Choice()", i_ChoiceRef) << cli::endl;
    return i_ChoiceRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_Choice__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeChoiceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__finalize(I_NativeChoiceRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeChoiceRef", I_NativeChoiceRef) << cli::endl;
    if (const cli::ui::Choice* const pcli_Choice = NativeObject::GetNativeObject<const cli::ui::Choice*>(I_NativeChoiceRef))
    {
        NativeObject::Free(*pcli_Choice);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Choice.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Choice__1_1getChoice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeChoiceRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__getChoice(I_NativeChoiceRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeChoiceRef", I_NativeChoiceRef) << cli::endl;
    int i_Choice = 0;
    if (const cli::ui::Choice* const pcli_Choice = NativeObject::GetNativeObject<const cli::ui::Choice*>(I_NativeChoiceRef))
    {
        i_Choice = pcli_Choice->GetChoice();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Choice.__getChoice()", i_Choice) << cli::endl;
    return i_Choice;
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_ui_Choice__1_1getstrChoice(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeChoiceRef, jint E_Lang)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Choice.__getstrChoice(I_NativeChoiceRef, E_Lang)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeChoiceRef", I_NativeChoiceRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_Lang", E_Lang) << cli::endl;
    std::string str_Choice;
    if (const cli::ui::Choice* const pcli_Choice = NativeObject::GetNativeObject<const cli::ui::Choice*>(I_NativeChoiceRef))
    {
        str_Choice = (const char*) pcli_Choice->GetstrChoice().GetString((cli::ResourceString::LANG) E_Lang);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("ui.Choice.__getstrChoice()", str_Choice.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Choice);
}
