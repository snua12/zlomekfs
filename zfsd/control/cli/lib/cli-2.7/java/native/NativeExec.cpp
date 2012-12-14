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

#include "cli/io_device.h"
#include "cli/traces.h"
#include "cli/assert.h"

#include "NativeExec.h"


//! @brief JNI execution trace class singleton redirection.
#define TRACE_JNI_EXEC GetJniExecutionTraceClass()
static const cli::TraceClass& GetJniExecutionTraceClass(void)
{
    static const cli::TraceClass cli_JniExecutionTraceClass("CLI_JNI_EXEC", cli::Help()
        .AddHelp(cli::Help::LANG_EN, "Advanced JNI execution traces")
        .AddHelp(cli::Help::LANG_FR, "Traces d'exécution avancées JNI"));
    return cli_JniExecutionTraceClass;
}


NativeExec& NativeExec::GetInstance(void)
{
    static NativeExec cli_NativeExec;
    return cli_NativeExec;
}

NativeExec::NativeExec(void)
  : m_tkThreadEnvMap(0) // UI_MaxCount not taken in account by STL implementation of tk::Map
{
    #if (defined _LINUX)
    m_hMutex = new pthread_mutex_t;
    const int i_MutexRes = pthread_mutex_init(
        m_hMutex,
        NULL            // default mutex attributes
    );
    if (i_MutexRes != 0)
    {
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_init() failed (" << i_MutexRes << ")" << cli::endl;
        CLI_ASSERT(false);
    }
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
    m_hMutex = ::CreateMutex(
        NULL,           // no security attributes
        FALSE,          // initially not owned
        "CliJniExec"    // name of mutex
    );
    if (m_hMutex == NULL)
    {
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "CreateMutex() failed (" << ::GetLastError() << ")" << cli::endl;
        CLI_ASSERT(false);
    }
    #endif // _WINDOWS
}

NativeExec::~NativeExec(void)
{
    #if (defined _LINUX)
    if (m_hMutex != NULL)
    {
        const int i_MutexRes = pthread_mutex_destroy(m_hMutex);
        if (i_MutexRes != 0)
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_destroy() failed (" << i_MutexRes << ")" << cli::endl;
            CLI_ASSERT(false);
        }
        delete m_hMutex;
        m_hMutex = NULL;
    }
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
    if (m_hMutex != NULL)
    {
        if (! ::CloseHandle(m_hMutex))
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "CloseHandle() failed (" << ::GetLastError() << ")" << cli::endl;
            CLI_ASSERT(false);
        }
    }
    #endif // _WINDOWS
}

void NativeExec::RegJNIEnv(JNIEnv* const PJ_Env)
{
    #if (defined _LINUX)
    const ThreadHandle h_ThreadId = pthread_self();

    const int i_LockRes = pthread_mutex_lock(m_hMutex);
    if (i_LockRes == 0)
    {
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
    const ThreadHandle h_ThreadId = ::GetCurrentThreadId();

    const DWORD dw_WaitRes = ::WaitForSingleObject(m_hMutex, INFINITE);
    switch (dw_WaitRes)
    {
    case WAIT_OBJECT_0:
    #endif // _WINDOWS

        if (JNIEnv* const* const ppj_KnownEnv = m_tkThreadEnvMap.GetAt(h_ThreadId))
        {
            if (*ppj_KnownEnv != PJ_Env)
            {
                cli::GetTraces().Trace(TRACE_JNI_EXEC) << "Changing JNI environment from " << *ppj_KnownEnv << " to " << PJ_Env << " for thread " << h_ThreadId << cli::endl;
                m_tkThreadEnvMap.SetAt(h_ThreadId, PJ_Env);
            }
        }
        else
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "Registering JNI environment " << PJ_Env << " for thread " << h_ThreadId << cli::endl;
            m_tkThreadEnvMap.SetAt(h_ThreadId, PJ_Env);
        }

        #if (defined _LINUX)
        const int i_UnlockRes = pthread_mutex_unlock(m_hMutex);
        if (i_UnlockRes != 0)
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_unlock() failed (" << i_UnlockRes << cli::endl;
            CLI_ASSERT(false);
        }
        #endif // _LINUX

        #if ((defined _WINDOWS) || (defined _CYGWIN))
        if (! ::ReleaseMutex(m_hMutex))
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "ReleaseMutex() failed (" << ::GetLastError() << cli::endl;
            CLI_ASSERT(false);
        }
        #endif // _WINDOWS

    #if (defined _LINUX)
    }
    else
    {
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_lock() failed (" << i_LockRes << ")" << cli::endl;
        CLI_ASSERT(false);
    }
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
        break;
    case WAIT_FAILED:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_FAILED (" << ::GetLastError() << ")" << cli::endl;
    case WAIT_ABANDONED:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_ABANDONED (" << ::GetLastError() << ")" << cli::endl;
    case WAIT_TIMEOUT:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_TIMEOUT (" << ::GetLastError() << ")" << cli::endl;
    default:
        CLI_ASSERT(false);
        break;
    }
    #endif // _WINDOWS
}

JNIEnv* const NativeExec::GetJNIEnv(void)
{
    JNIEnv* pj_Env = NULL;

    #if (defined _LINUX)
    const ThreadHandle h_ThreadId = pthread_self();

    const int i_LockRes = pthread_mutex_lock(m_hMutex);
    if (i_LockRes == 0)
    {
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
    const ThreadHandle h_ThreadId = ::GetCurrentThreadId();

    const DWORD dw_WaitRes = ::WaitForSingleObject(m_hMutex,INFINITE);
    switch (dw_WaitRes)
    {
    case WAIT_OBJECT_0:
    #endif // _WINDOWS

        if (JNIEnv* const* const ppj_KnownEnv = m_tkThreadEnvMap.GetAt(h_ThreadId))
        {
            pj_Env = *ppj_KnownEnv;
        }
        else
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "No JNI environment for thread " << h_ThreadId << cli::endl;
            CLI_ASSERT(false);
        }

        #if (defined _LINUX)
        const int i_UnlockRes = pthread_mutex_unlock(m_hMutex);
        if (i_UnlockRes != 0)
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_unlock() failed (" << i_UnlockRes << cli::endl;
            CLI_ASSERT(false);
        }
        #endif // _LINUX

        #if ((defined _WINDOWS) || (defined _CYGWIN))
        if (! ::ReleaseMutex(m_hMutex))
        {
            cli::GetTraces().Trace(TRACE_JNI_EXEC) << "ReleaseMutex() failed (" << ::GetLastError() << cli::endl;
            CLI_ASSERT(false);
        }
        #endif // _WINDOWS

    #if (defined _LINUX)
    }
    else
    {
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "pthread_mutex_lock() failed (" << i_LockRes << ")" << cli::endl;
        CLI_ASSERT(false);
    }
    #endif // _LINUX

    #if ((defined _WINDOWS) || (defined _CYGWIN))
        break;
    case WAIT_FAILED:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_FAILED (" << ::GetLastError() << ")" << cli::endl;
    case WAIT_ABANDONED:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_ABANDONED (" << ::GetLastError() << ")" << cli::endl;
    case WAIT_TIMEOUT:
        cli::GetTraces().Trace(TRACE_JNI_EXEC) << "WaitForSingleObject() returned WAIT_TIMEOUT (" << ::GetLastError() << ")" << cli::endl;
    default:
        CLI_ASSERT(false);
        break;
    }
    #endif // _WINDOWS

    return pj_Env;
}

const std::string NativeExec::Java2Native(jstring PJ_String)
{
    std::string std_String;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        if (const char* const str_String = pj_Env->GetStringUTFChars(PJ_String, 0))
        {
            std_String = str_String;
            pj_Env->ReleaseStringUTFChars(PJ_String, str_String);
        }
    }
    return std_String;
}

jstring NativeExec::Native2Java(const std::string& STR_String)
{
    jstring pj_String = NULL;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        pj_String = pj_Env->NewStringUTF(STR_String.c_str());
    }
    return pj_String;
}

jintArray NativeExec::Native2Java(const std::vector<NativeObject::REF>& STD_ObjectList)
{
    jintArray j_Array = NULL;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        if ((j_Array = pj_Env->NewIntArray(STD_ObjectList.size())))
        {
            if (! STD_ObjectList.empty())
            {
                if (jint* const pi_Array = new jint[STD_ObjectList.size()])
                {
                    unsigned int ui = 0;
                    for (std::vector<NativeObject::REF>::const_iterator it = STD_ObjectList.begin(); it != STD_ObjectList.end(); it ++)
                    {
                        pi_Array[ui] = *it;
                        ui ++;
                    }
                    pj_Env->SetIntArrayRegion(j_Array, 0, ui, pi_Array);
                    delete [] pi_Array;
                }
            }
        }
    }
    return j_Array;
}
