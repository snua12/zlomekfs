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

#include "cli/element.h"
#include "cli/syntax_node.h"

#include "cli_SyntaxNode.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


extern "C" JNIEXPORT jboolean JNICALL Java_cli_SyntaxNode__1_1addElement(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeSyntaxNodeRef, jint I_NativeElementRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("SyntaxNode.__addElement(I_NativeSyntaxNodeRef, I_NativeElementRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeSyntaxNodeRef", I_NativeSyntaxNodeRef) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeElementRef", I_NativeElementRef) << cli::endl;
    bool b_Res = false;
    if (cli::SyntaxNode* const pcli_SyntaxNode = NativeObject::GetNativeObject<cli::SyntaxNode*>(I_NativeSyntaxNodeRef))
    {
        if (cli::Element* const pcli_Element = NativeObject::GetNativeObject<cli::Element*>(I_NativeElementRef))
        {
            pcli_SyntaxNode->AddElement(pcli_Element);
            NativeObject::Delegate(*pcli_Element, *pcli_SyntaxNode);
            b_Res = true;
        }
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndBool("SyntaxNode.__addElement()", b_Res);
    return b_Res;
}
