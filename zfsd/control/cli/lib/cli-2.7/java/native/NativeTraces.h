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

#ifndef _CLI_NATIVE_TRACES_H_
#define _CLI_NATIVE_TRACES_H_

#include "cli/traces.h"
#include "cli/io_device.h"


//! @brief JNI trace class singleton redirection.
#define TRACE_JNI NativeTraces::GetTraceClass()


//! @brief Tool class for native traces.
class NativeTraces
{
public:
    //! @brief Native trace class singleton.
    //! @return The native trace class.
    static const cli::TraceClass& GetTraceClass(void);

public:
    //! @brief Traces the beginning of a native method.
    static const cli::tk::String Begin(
        const char* const STR_Method            //!< Method name.
        );

    //! @brief Traces a parameter of type string.
    static const cli::tk::String ParamStr(
            const char* const STR_ParamName,    //!< Parameter name.
            const char* const STR_Value         //!< Parameter value.
            );
    //! @brief Traces a parameter of type pointer of a native method.
    static const cli::tk::String ParamPtr(
            const char* const STR_ParamName,    //!< Parameter name.
            void* const PV_Value                //!< Parameter value.
            );
    //! @brief Traces a parameter of type integer of a native method.
    static const cli::tk::String ParamInt(
            const char* const STR_ParamName,    //!< Parameter name.
            const int I_Value                   //!< Parameter value.
            );
    //! @brief Traces a parameter of type boolean of a native method.
    static const cli::tk::String ParamBool(
            const char* const STR_ParamName,    //!< Parameter name.
            const bool B_Value                  //!< Parameter value.
            );
    //! @brief Traces a parameter of type float of a native method.
    static const cli::tk::String ParamFloat(
            const char* const STR_ParamName,    //!< Parameter name.
            const double D_Value                //!< Parameter value.
            );

    //! @brief Traces a value of a native variable of type string.
    static const cli::tk::String ValueStr(
            const char* const STR_ValueName,    //!< Variable name.
            const char* const STR_Value         //!< Variable value.
            );
    //! @brief Traces a value of a native variable of type pointer.
    static const cli::tk::String ValuePtr(
            const char* const STR_ValueName,    //!< Variable name.
            void* const PV_Value                //!< Variable value.
            );
    //! @brief Traces a value of a native variable of type integer.
    static const cli::tk::String ValueInt(
            const char* const STR_ValueName,    //!< Variable name.
            const int I_Value                   //!< Variable value.
            );
    //! @brief Traces a value of a native variable of type boolean.
    static const cli::tk::String ValueBool(
            const char* const STR_ValueName,    //!< Variable name.
            const bool B_Value                  //!< Variable value.
            );
    //! @brief Traces a value of a native variable of type float.
    static const cli::tk::String ValueFloat(
            const char* const STR_ValueName,    //!< Variable name.
            const double D_Value                //!< Variable value.
            );

    //! @brief Traces the end of a void native method.
    static const cli::tk::String EndVoid(
            const char* const STR_Method        //!< Method name.
            );
    //! @brief Traces the end of a non-void native method returning a string.
    static const cli::tk::String EndStr(
            const char* const STR_Method,       //!< Method name.
            const char* const STR_Value         //!< Return value.
            );
    //! @brief Traces the end of a non-void native method returning a pointer.
    static const cli::tk::String EndPtr(
            const char* const STR_Method,       //!< Method name.
            void* const PV_Value                //!< Return value.
            );
    //! @brief Traces the end of a non-void native method returning an int.
    static const cli::tk::String EndInt(
            const char* const STR_Method,       //!< Method name.
            const int I_Value                   //!< Return value.
            );
    //! @brief Traces the end of a non-void native method returning a boolean.
    static const cli::tk::String EndBool(
            const char* const STR_Method,       //!< Method name.
            const bool B_Value                  //!< Return value.
            );
    //! @brief Traces the end of a non-void native method returning a float.
    static const cli::tk::String EndFloat(
            const char* const STR_Method,       //!< Method name.
            const double D_Value                //!< Return value.
            );

    //! @brief Traces the status of a native object.
    static const cli::tk::String Instance(
            const int I_NativeObjectRef,        //!< Native object reference.
            const int I_Tokens,                 //!< Number of tokens in use.
            const bool B_AutoDelete             //!< Auto-delete flag.
            );

private:
    //! @brief CLI_JNI traces indentation computation.
    //! @return CLI_JNI indentation.
    static const cli::tk::String GetIndent(void);

    //! Number of JNI functions traced in the stack.
    static int m_iJniStackSize;
};

#endif // _CLI_NATIVE_TRACES_H_
