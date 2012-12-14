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

#ifndef _CLI_NATIVE_OBJECT_H_
#define _CLI_NATIVE_OBJECT_H_

#include <string>
#include <map>

#include "cli/object.h"
#include "cli/io_device.h"


//! @brief Generic native object.
class NativeObject
{
public:
    //! Object reference type.
    typedef int REF;

    //! @brief Object to reference conversion.
    //! @return Reference of the native object if found.
    static const REF GetNativeRef(
        const cli::Object& CLI_Object   //!< Native object.
        );

    //! @brief Reference to object conversion.
    //! @return Pointer to the object referenced. NULL if an error occured.
    static cli::Object* const GetNativeObject(
        const REF I_NativeObjectRef     //!< Native object reference.
        );

    //! @brief Reference to object conversion (template version).
    //! @return Pointer to the object referenced. NULL if an error occured.
    template <class T> static T GetNativeObject(
        const REF I_NativeObjectRef     //!< Native object reference.
        )
    {
        return dynamic_cast<T>(GetNativeObject(I_NativeObjectRef));
    }

    //! @brief Retrieves the Java object reference from its native reference.
    //! @return Java object if found.
    static const jobject GetJavaObject(
        const REF I_NativeObjectRef,    //!< Native object reference.
        const bool B_Trace              //!< true if traces can be output, false otherwise.
        );

public:
    //! @brief Declares the given object to be used from Java.
    template <class T> static void Use(
        const T& T_Object               //!< Object in use.
        )
    {
        if (cli::OutputDevice* const pcli_OutputDevice = dynamic_cast<cli::OutputDevice*>(const_cast<T*>(& T_Object)))
        {
            pcli_OutputDevice->UseInstance(__CALL_INFO__);
        }
        Use(GetNativeRef(T_Object));
    }

    //! @brief Declares the given object to be not used anymore from Java.
    template <class T> static void Free(
        const T& T_Object               //!< Object which reference is released.
        )
    {
        const bool b_Delete = Free(GetNativeRef(T_Object));
        if (cli::OutputDevice* const pcli_OutputDevice = dynamic_cast<cli::OutputDevice*>(const_cast<T*>(& T_Object)))
        {
            pcli_OutputDevice->FreeInstance(__CALL_INFO__);
        }
        else if (b_Delete)
        {
            delete & T_Object;
        }
    }

    //! @brief Declares a given object's destruction to be delegated to another one's destruction.
    template <class T1, class T2> static void Delegate(
        const T1& T_What,               //!< Object which deletion is now delegated.
        const T2& T_Who                 //!< Object the deletion of the previous object is delegated to.
        )
    {
        Delegate(GetNativeRef(T_What), GetNativeRef(T_Who));
    }

public:
    //! @brief Tells Java a new object has been created from the native side.
    static const bool CreateFromNative(
        const cli::Object& CLI_Object       //!< CLI element to create from native source.
        );

    //! @brief Tells Java an object has been deleted from the native side.
    static const bool DeleteFromNative(
        const cli::Object& CLI_Object       //!< CLI element to delete from native source.
        );

private:
    //! @brief Declares the given object to be used from Java.
    static void Use(
        const REF I_ObjectRef               //!< Object in use.
        );

    //! @brief Declares the given object to be not used anymore from Java.
    //! @return true if the object should be deleted.
    static const bool Free(
        const REF I_ObjectRef               //!< Object which reference is released.
        );

    //! @brief Declares a given object's destruction to be delegated to another one's destruction.
    static void Delegate(
        const REF I_WhatRef,                //!< Object which deletion is now delegeted.
        const REF I_WhoRef                  //!< Object the deletion of the previous object is delegated to.
        );

public:
    //! @brief Retrieves the Java class name of a native object.
    static const std::string GetJavaClassName(
        const cli::Object& CLI_Object       //!< CLI element.
        );

private:
    //! Object information structure.
    typedef struct _ObjectInfo
    {
        int iTokens;        //!< Token number. When this number falls down 0, perform deletion.
        bool bAutoDelete;   //!< Auto-deletion flag. When not set, do not delete the object on unregistration.
    } ObjectInfo;

    //! Objet map typedef.
    typedef std::map<REF, struct _ObjectInfo> ObjectMap;

    //! Object map.
    static ObjectMap m_mapTokens;
};

#endif // _CLI_NATIVE_OBJECT_H_
