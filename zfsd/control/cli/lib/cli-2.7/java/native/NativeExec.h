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

#ifndef _CLI_NATIVE_EXEC_H_
#define _CLI_NATIVE_EXEC_H_

#include <jni.h>
#if 0
#elif (defined _LINUX)
#include <pthread.h>
typedef pthread_t ThreadHandle;
typedef pthread_mutex_t* MutexHandle;
#elif ((defined _WINDOWS) || (defined _CYGWIN))
#include <windows.h>
#undef DELETE
typedef DWORD ThreadHandle;
typedef HANDLE MutexHandle;
#else
#error "No such environment"
#endif

#include <string>
#include <vector>

#include "cli/tk.h"

#include "NativeObject.h"


//! @brief JNI executions toolkit.
class NativeExec
{
public:
    //! @brief Only one instance singleton.
    static NativeExec& GetInstance(void);

private:
    //! @brief Private constructor. Use the singleton to retrieve the only one instance.
    NativeExec(void);

    //! @brief Destructor.
    virtual ~NativeExec(void);

public:
    //! @brief Register the JNI environment reference for the current thread.
    void RegJNIEnv(
        JNIEnv* const PJ_Env    //!< JNI environment reference.
        );

    //! @brief Retrieve the JNI environment reference previously registered for the current thread.
    //! @return JNI environment reference.
    JNIEnv* const GetJNIEnv(void);

public:
    //! @brief Java 2 native string conversion.
    //! @return Native string object.
    //! @note No problem of ressource management once the string is converted.
    static const std::string Java2Native(
        jstring PJ_String                   //!< Java string to be converted.
        );

    //! @brief Native 2 Java string conversion.
    //! @return Java String newly allocated.
    static jstring Native2Java(
        const std::string& STR_String   //!< Native string to be converted.
        );

    //! @brief Native 2 Java object list conversion.
    //! @return Java object references array newly allocated.
    static jintArray Native2Java(
        const std::vector<NativeObject::REF>& STD_ObjectList    //!< List of objects references to be converted.
        );

private:
    //! Map of JNI environment references indexed by thread indentifiers.
    cli::tk::Map<ThreadHandle, JNIEnv*> m_tkThreadEnvMap;

    //! Thread-safe mutex;
    MutexHandle m_hMutex;
};

#endif // _CLI_NATIVE_TRACES_H_
