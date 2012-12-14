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

#include "cli/telnet.h"
#include "cli/shell.h"

#include "cli_TelnetServer.h"

#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


//! @brief Class implementing native C++ objects matching with cli.TelnetServer derived class.
class NativeTelnetServer : public cli::TelnetServer
{
public:
    //! @brief Constructor.
    NativeTelnetServer(
            const unsigned int UI_MaxConnections, const unsigned long UL_TcpPort, const cli::ResourceString::LANG E_Lang)
      : cli::TelnetServer(UI_MaxConnections, UL_TcpPort, E_Lang)
    {
    }

    //! @brief Destructor.
    virtual ~NativeTelnetServer(void)
    {
    }

    // cli::TelnetServer implementation.

    virtual cli::Shell* const OnNewConnection(const cli::TelnetConnection& CLI_NewConnection)
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("NativeTelnetServer::OnNewConnection(CLI_NewConnection)") << cli::endl;
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_NewConnection", NativeObject::GetNativeRef(CLI_NewConnection)) << cli::endl;

        // Command line object interfacing.
        NativeObject::REF i_ShellRef = 0;
        cli::Shell* pcli_Shell = NULL;
        if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
        {
            if (NativeObject::CreateFromNative(CLI_NewConnection))
            {
                // Java handler call.
                if (const jclass pj_ServerClass = pj_Env->FindClass(NativeObject::GetJavaClassName(*this).c_str()))
                {
                    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_ServerClass", pj_ServerClass) << cli::endl;
                    if (const jmethodID pj_OnNewConnectionMethodID = pj_Env->GetMethodID(pj_ServerClass, "__onNewConnection", "(I)I"))
                    {
                        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_OnNewConnectionMethodID", pj_OnNewConnectionMethodID) << cli::endl;
                        if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(*this), true))
                        {
                            cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                            i_ShellRef = pj_Env->CallIntMethod(pj_Object, pj_OnNewConnectionMethodID, (jint) NativeObject::GetNativeRef(CLI_NewConnection));
                            pcli_Shell = NativeObject::GetNativeObject<cli::Shell*>(i_ShellRef);
                        }
                    }
                }
            }
        }

        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("i_ShellRef", i_ShellRef) << cli::endl;
        return pcli_Shell;
    }

    virtual void OnCloseConnection(cli::Shell* const PCLI_Shell, const cli::TelnetConnection& CLI_ConnectionClosed)
    {
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("NativeTelnetServer::OnCloseConnection(PCLI_Shell, CLI_NewConnection)") << cli::endl;
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("PCLI_Shell", (PCLI_Shell != NULL) ? NativeObject::GetNativeRef(*PCLI_Shell) : 0) << cli::endl;
        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("CLI_ConnectionClosed", NativeObject::GetNativeRef(CLI_ConnectionClosed)) << cli::endl;

        if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
        {
            // Java handler call.
            if (const jclass pj_ServerClass = pj_Env->FindClass(NativeObject::GetJavaClassName(*this).c_str()))
            {
                cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_ServerClass", pj_ServerClass) << cli::endl;
                if (const jmethodID pj_OnCloseConnectionMethodID = pj_Env->GetMethodID(pj_ServerClass, "__onCloseConnection", "(II)V"))
                {
                    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_OnCloseConnectionMethodID", pj_OnCloseConnectionMethodID) << cli::endl;
                    if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(*this), true))
                    {
                        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                        if (PCLI_Shell != NULL)
                        {
                            pj_Env->CallVoidMethod(
                                pj_Object, pj_OnCloseConnectionMethodID,
                                (jint) NativeObject::GetNativeRef(*PCLI_Shell), (jint) NativeObject::GetNativeRef(CLI_ConnectionClosed)
                            );
                        }
                    }
                }
            }

            // Make Java forget the telnet connection instance.
            NativeObject::DeleteFromNative(CLI_ConnectionClosed);
        }

        cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("NativeTelnetServer::OnCloseConnection()") << cli::endl;
    }
};


extern "C" JNIEXPORT jint JNICALL Java_cli_TelnetServer__1_1TelnetServer(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_MaxConnections, jint I_TcpPort, jint E_Lang)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("TelnetServer.__TelnetServer(I_MaxConnections, I_TcpPort, E_Lang)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_MaxConnections", I_MaxConnections) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_TcpPort", I_TcpPort) << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("E_Lang", E_Lang) << cli::endl;
    NativeObject::REF i_ServerRef = 0;
    if (cli::TelnetServer* const pcli_Server = new NativeTelnetServer(I_MaxConnections, I_TcpPort, (cli::ResourceString::LANG) E_Lang))
    {
        NativeObject::Use(*pcli_Server);
        i_ServerRef = NativeObject::GetNativeRef(*pcli_Server);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndInt("TelnetServer.__TelnetServer()", i_ServerRef) << cli::endl;
    return i_ServerRef;
}

extern "C" JNIEXPORT void JNICALL Java_cli_TelnetServer__1_1finalize(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeServerRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("TelnetServer.__finalize(I_NativeServerRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeServerRef", I_NativeServerRef) << cli::endl;
    if (const cli::TelnetServer* const pcli_Server = NativeObject::GetNativeObject<const cli::TelnetServer*>(I_NativeServerRef))
    {
        NativeObject::Free(*pcli_Server);
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("TelnetServer.__finalize()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_TelnetServer__1_1startServer(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeServerRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("TelnetServer.__startServer(I_NativeServerRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeServerRef", I_NativeServerRef) << cli::endl;
    if (NativeTelnetServer* const pcli_Server = NativeObject::GetNativeObject<NativeTelnetServer*>(I_NativeServerRef))
    {
        pcli_Server->StartServer();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("TelnetServer.__startServer()") << cli::endl;
}

extern "C" JNIEXPORT void JNICALL Java_cli_TelnetServer__1_1stopServer(
        JNIEnv* PJ_Env, jclass PJ_Class,
        jint I_NativeServerRef)
{
    NativeExec::GetInstance().RegJNIEnv(PJ_Env);

    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::Begin("TelnetServer.__stopServer(I_NativeServerRef)") << cli::endl;
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::ParamInt("I_NativeServerRef", I_NativeServerRef) << cli::endl;
    if (NativeTelnetServer* const pcli_Server = NativeObject::GetNativeObject<NativeTelnetServer*>(I_NativeServerRef))
    {
        pcli_Server->StopServer();
    }
    cli::GetTraces().Trace(TRACE_JNI) << NativeTraces::EndVoid("TelnetServer.__stopServer()") << cli::endl;
}
