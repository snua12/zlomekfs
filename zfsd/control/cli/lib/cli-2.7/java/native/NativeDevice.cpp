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

#include "NativeDevice.h"
#include "NativeObject.h"
#include "NativeExec.h"
#include "NativeTraces.h"


const bool __NativeDevice__OpenDevice(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__OpenDevice()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // Java device opening.
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_OpenMethodID = pj_Env->GetMethodID(pj_MenuClass, "__openDevice", "()Z"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_OpenMethodID", pj_OpenMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_OpenMethodID);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndBool("__NativeDevice__OpenDevice()", b_Res) << cli::endl;
    return b_Res;
}

const bool __NativeDevice__CloseDevice(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__CloseDevice()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        // Java device closure.
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_CloseMethodID = pj_Env->GetMethodID(pj_MenuClass, "__closeDevice", "()Z"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_CloseMethodID", pj_CloseMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_CloseMethodID);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndBool("__NativeDevice__CloseDevice()", b_Res) << cli::endl;
    return b_Res;
}

void __NativeDevice__PutString(const cli::OutputDevice& CLI_Device, const char* const STR_Out)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__PutString()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamStr("STR_Out", STR_Out) << cli::endl;

    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_PutStringMethodID = pj_Env->GetMethodID(pj_MenuClass, "__putString", "(Ljava/lang/String;)V"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_PutStringMethodID", pj_PutStringMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    if (const jstring pj_OutString = NativeExec::Native2Java(std::string(STR_Out)))
                    {
                        pj_Env->CallVoidMethod(pj_Object, pj_PutStringMethodID, pj_OutString);
                        // No need to delete pj_OutString at this point.
                        // The Java garbage collector should deal with that.
                    }
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndVoid("__NativeDevice__PutString()") << cli::endl;
}

void __NativeDevice__Beep(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__Beep()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_BeepMethodID = pj_Env->GetMethodID(pj_MenuClass, "__beep", "()V"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_BeepMethodID", pj_BeepMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    pj_Env->CallVoidMethod(pj_Object, pj_BeepMethodID);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndVoid("__NativeDevice__Beep()") << cli::endl;
}

void __NativeDevice__CleanScreen(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__CleanScreen()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_CleanScreenMethodID = pj_Env->GetMethodID(pj_MenuClass, "__cleanScreen", "()V"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_CleanScreenMethodID", pj_CleanScreenMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    pj_Env->CallVoidMethod(pj_Object, pj_CleanScreenMethodID);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndVoid("__NativeDevice__CleanScreen()") << cli::endl;
}

const cli::OutputDevice::ScreenInfo __NativeDevice__GetScreenInfo(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__GetScreenInfo()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    cli::OutputDevice::ScreenInfo cli_ScreenInfo(-1, -1, false, false);
    if (NativeObject::CreateFromNative(cli_ScreenInfo))
    {
        if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
            if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
                if (const jmethodID pj_GetScreenInfoMethodID = pj_Env->GetMethodID(pj_MenuClass, "__getScreenInfo", "(I)V"))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_GetScreenInfoMethodID", pj_GetScreenInfoMethodID) << cli::endl;
                    if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                    {
                        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                        pj_Env->CallVoidMethod(pj_Object, pj_GetScreenInfoMethodID, NativeObject::GetNativeRef(cli_ScreenInfo));
                    }
                }
            }
        }
        NativeObject::DeleteFromNative(cli_ScreenInfo);
    }

    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueInt("cli_ScreenInfo.GetWidth()", cli_ScreenInfo.GetWidth()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueInt("cli_ScreenInfo.GetSafeWidth()", cli_ScreenInfo.GetSafeWidth()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueInt("cli_ScreenInfo.GetHeight()", cli_ScreenInfo.GetHeight()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueInt("cli_ScreenInfo.GetSafeHeight()", cli_ScreenInfo.GetSafeHeight()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueBool("cli_ScreenInfo.GetbTrueCls()", cli_ScreenInfo.GetbTrueCls()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValueBool("cli_ScreenInfo.GetbWrapLines()", cli_ScreenInfo.GetbWrapLines()) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndVoid("__NativeDevice__GetScreenInfo()") << cli::endl;
    return cli_ScreenInfo;
}

const bool __NativeDevice__WouldOutput(const cli::OutputDevice& CLI_Device1, const cli::OutputDevice& CLI_Device2)
{
    // Do not trace! for consistency reasons.
    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device1).c_str()))
        {
            if (const jmethodID pj_WouldOutputMethodID = pj_Env->GetMethodID(pj_MenuClass, "__wouldOutput", "(I)Z"))
            {
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device1), false))
                {
                    b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_WouldOutputMethodID, (jint) NativeObject::GetNativeRef(CLI_Device2));
                }
            }
        }
    }
    return b_Res;
}

const cli::KEY __NativeDevice__GetKey(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__GetKey()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    cli::KEY e_Key = cli::NULL_KEY;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_GetKeyMethodID = pj_Env->GetMethodID(pj_MenuClass, "__getKey", "()I"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_GetKeyMethodID", pj_GetKeyMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    e_Key = (cli::KEY) pj_Env->CallIntMethod(pj_Object, pj_GetKeyMethodID);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndInt("__NativeDevice__GetKey()", e_Key) << cli::endl;
    return (cli::KEY) e_Key;
}

const cli::ResourceString __NativeDevice__GetLocation(const cli::OutputDevice& CLI_Device)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__GetLocation()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;

    cli::ResourceString cli_Location;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_GetLocationMethodID = pj_Env->GetMethodID(pj_MenuClass, "__getLocation", "(I)V"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_GetLocationMethodID", pj_GetLocationMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    NativeObject::CreateFromNative(cli_Location);
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    pj_Env->CallVoidMethod(pj_Object, pj_GetLocationMethodID, (jint) NativeObject::GetNativeRef(cli_Location));
                    NativeObject::DeleteFromNative(cli_Location);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndVoid("__NativeDevice__GetLocation()") << cli::endl;
    return cli_Location;
}

const bool __NativeDevice__WouldInput(const cli::OutputDevice& CLI_Device1, const cli::OutputDevice& CLI_Device2)
{
    // Do not trace! for consistency reasons.
    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device1).c_str()))
        {
            if (const jmethodID pj_WouldInputMethodID = pj_Env->GetMethodID(pj_MenuClass, "__wouldInput", "(I)Z"))
            {
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device1), false))
                {
                    b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_WouldInputMethodID, (jint) NativeObject::GetNativeRef(CLI_Device2));
                }
            }
        }
    }
    return b_Res;
}

const bool __NativeDevice__WaitForKeys(const cli::OutputDevice& CLI_Device, const unsigned int UI_Milli)
{
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::Begin("__NativeDevice__WaitForKeys()") << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("CLI_Device", NativeObject::GetNativeRef(CLI_Device)) << cli::endl;
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ParamInt("UI_Milli", UI_Milli) << cli::endl;

    bool b_Res = false;
    if (JNIEnv* const pj_Env = NativeExec::GetInstance().GetJNIEnv())
    {
        cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Env", pj_Env) << cli::endl;
        if (const jclass pj_MenuClass = pj_Env->FindClass(NativeObject::GetJavaClassName(CLI_Device).c_str()))
        {
            cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_MenuClass", pj_MenuClass) << cli::endl;
            if (const jmethodID pj_GetWaitForKeysMethodID = pj_Env->GetMethodID(pj_MenuClass, "__waitForKeys", "(I)Z"))
            {
                cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_GetWaitForKeysMethodID", pj_GetWaitForKeysMethodID) << cli::endl;
                if (const jobject pj_Object = NativeObject::GetJavaObject(NativeObject::GetNativeRef(CLI_Device), cli::GetTraces().IsSafe(CLI_Device)))
                {
                    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::ValuePtr("pj_Object", pj_Object) << cli::endl;
                    b_Res = pj_Env->CallBooleanMethod(pj_Object, pj_GetWaitForKeysMethodID, (jint) UI_Milli);
                }
            }
        }
    }
    cli::GetTraces().SafeTrace(TRACE_JNI, CLI_Device) << NativeTraces::EndBool("__NativeDevice__WaitForKeys()", b_Res) << cli::endl;
    return b_Res;
}
