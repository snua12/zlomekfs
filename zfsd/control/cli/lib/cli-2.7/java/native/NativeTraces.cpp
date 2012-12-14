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

#include "cli/string_device.h"

#include "NativeTraces.h"


int NativeTraces::m_iJniStackSize = -1;

const cli::tk::String NativeTraces::Begin(const char* const STR_Method)
{
    m_iJniStackSize ++;
    cli::StringDevice cli_Begin(0, false);
    cli_Begin << GetIndent() << ">> " << STR_Method;
    return cli_Begin.GetString();
}

const cli::tk::String NativeTraces::ParamStr(const char* const STR_ParamName, const char* const STR_Value)
{
    cli::StringDevice cli_Param(0, false);
    cli_Param << GetIndent() << " " << STR_ParamName << " = " << STR_Value;
    return cli_Param.GetString();
}
const cli::tk::String NativeTraces::ParamPtr(const char* const STR_ParamName, void* const PV_Value)
{
    cli::StringDevice cli_Param(0, false);
    cli_Param << GetIndent() << " " << STR_ParamName << " = " << PV_Value;
    return cli_Param.GetString();
}
const cli::tk::String NativeTraces::ParamInt(const char* const STR_ParamName, const int I_Value)
{
    cli::StringDevice cli_Param(0, false);
    cli_Param << GetIndent() << " " << STR_ParamName << " = " << I_Value;
    return cli_Param.GetString();
}
const cli::tk::String NativeTraces::ParamBool(const char* const STR_ParamName, const bool B_Value)
{
    cli::StringDevice cli_Param(0, false);
    cli_Param << GetIndent() << " " << STR_ParamName << " = " << B_Value;
    return cli_Param.GetString();
}
const cli::tk::String NativeTraces::ParamFloat(const char* const STR_ParamName, const double D_Value)
{
    cli::StringDevice cli_Param(0, false);
    cli_Param << GetIndent() << " " << STR_ParamName << " = " << D_Value;
    return cli_Param.GetString();
}

const cli::tk::String NativeTraces::ValueStr(const char* const STR_ValueName, const char* const STR_Value)
{
    cli::StringDevice cli_Value(0, false);
    cli_Value << GetIndent() << "  -> " << STR_ValueName << " = " << STR_Value;
    return cli_Value.GetString();
}
const cli::tk::String NativeTraces::ValuePtr(const char* const STR_ValueName, void* const PV_Value)
{
    cli::StringDevice cli_Value(0, false);
    cli_Value << GetIndent() << "  -> " << STR_ValueName << " = " << PV_Value;
    return cli_Value.GetString();
}
const cli::tk::String NativeTraces::ValueInt(const char* const STR_ValueName, const int I_Value)
{
    cli::StringDevice cli_Value(0, false);
    cli_Value << GetIndent() << "  -> " << STR_ValueName << " = " << I_Value;
    return cli_Value.GetString();
}
const cli::tk::String NativeTraces::ValueBool(const char* const STR_ValueName, const bool B_Value)
{
    cli::StringDevice cli_Value(0, false);
    cli_Value << GetIndent() << "  -> " << STR_ValueName << " = " << B_Value ;
    return cli_Value.GetString();
}
const cli::tk::String NativeTraces::ValueFloat(const char* const STR_ValueName, const double D_Value)
{
    cli::StringDevice cli_Value(0, false);
    cli_Value << GetIndent() << "  -> " << STR_ValueName << " = " << D_Value ;
    return cli_Value.GetString();
}

const cli::tk::String NativeTraces::EndVoid(const char* const STR_Method)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method;
    m_iJniStackSize --;
    return cli_End.GetString();
}
const cli::tk::String NativeTraces::EndStr(const char* const STR_Method, const char* const STR_Value)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method << " : " << STR_Value;
    m_iJniStackSize --;
    return cli_End.GetString();
}
const cli::tk::String NativeTraces::EndPtr(const char* const STR_Method, void* const PV_Value)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method << " : " << PV_Value;
    m_iJniStackSize --;
    return cli_End.GetString();
}
const cli::tk::String NativeTraces::EndInt(const char* const STR_Method, const int I_Value)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method << " : " << I_Value;
    m_iJniStackSize --;
    return cli_End.GetString();
}
const cli::tk::String NativeTraces::EndBool(const char* const STR_Method, const bool B_Value)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method << " : " << B_Value;
    m_iJniStackSize --;
    return cli_End.GetString();
}
const cli::tk::String NativeTraces::EndFloat(const char* const STR_Method, const double D_Value)
{
    cli::StringDevice cli_End(0, false);
    cli_End << GetIndent() << "<< " << STR_Method << " : " << D_Value;
    m_iJniStackSize --;
    return cli_End.GetString();
}

const cli::tk::String NativeTraces::Instance(const int I_NativeObjectRef, const int I_Tokens, const bool B_AutoDelete)
{
    cli::StringDevice cli_Trace(0, false);
    cli_Trace << GetIndent();
    cli_Trace << "[object " << I_NativeObjectRef << "] ";
    cli_Trace << "tokens = " << I_Tokens << ", ";
    cli_Trace << "auto-delete: " << (B_AutoDelete ? "yes" : "no");
    if ((I_Tokens <= 0) && (B_AutoDelete))
    {
        cli_Trace << " -> deletion";
    }
    return cli_Trace.GetString();
}

const cli::TraceClass& NativeTraces::GetTraceClass(void)
{
    static const cli::TraceClass cli_JniTraceClass("CLI_JNI", cli::Help()
        .AddHelp(cli::Help::LANG_EN, "CLI JNI traces")
        .AddHelp(cli::Help::LANG_FR, "Traces CLI d'exécution JNI"));
    return cli_JniTraceClass;
}

const cli::tk::String NativeTraces::GetIndent(void)
{
    cli::StringDevice cli_Indent(0, false);
    for (int i=0; i<m_iJniStackSize; i++)
    {
        cli_Indent << "    ";
    }
    return cli_Indent.GetString();
}
