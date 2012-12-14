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

#include "cli/ui_line.h"

#include "cli_ui_Line.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jint JNICALL Java_cli_ui_Line__1_1Line(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jstring PJ_DefaultLine, jint I_MinLineLength, jint I_MaxLineLength)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Line.__Line(PJ_DefaultLine, I_MinLineLength, I_MaxLineLength)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamStr("PJ_DefaultLine", NativeExec::Java2Native(PJ_DefaultLine).c_str()) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MinLineLength", I_MinLineLength) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MaxLineLength", I_MaxLineLength) << cli::endl;
    NativeObject::REF i_LineRef = 0;
    const cli::tk::String tk_DefaultLine(NativeExec::Java2Native(PJ_DefaultLine).size(), NativeExec::Java2Native(PJ_DefaultLine).c_str());
    if (cli::ui::Line* const pcli_Line = new cli::ui::Line(tk_DefaultLine, I_MinLineLength, I_MaxLineLength))
    {
        NativeObject::Use(*pcli_Line);
        i_LineRef = NativeObject::GetNativeRef(*pcli_Line);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("ui.Line.__Line()", i_LineRef) << cli::endl;
    return i_LineRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_ui_Line__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeLineRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Line.__finalize(I_NativeLineRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeLineRef", I_NativeLineRef) << cli::endl;
    if (const cli::ui::Line* const pcli_Line = NativeObject::GetNativeObject<const cli::ui::Line*>(I_NativeLineRef))
    {
        NativeObject::Free(*pcli_Line);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("ui.Line.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT jstring JNICALL Java_cli_ui_Line__1_1getLine(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeLineRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("ui.Line.__getLine(I_NativeLineRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeLineRef", I_NativeLineRef) << cli::endl;
    std::string str_Line;
    if (const cli::ui::Line* const pcli_Line = NativeObject::GetNativeObject<const cli::ui::Line*>(I_NativeLineRef))
    {
        str_Line = (const char*) pcli_Line->GetLine();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndStr("ui.Line.__getLine()", str_Line.c_str()) << cli::endl;
    return NativeExec::Native2Java(str_Line);
}
