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

#ifndef _CLI_NATIVE_NON_BLOCKING_DEVICE_H_
#define _CLI_NATIVE_NON_BLOCKING_DEVICE_H_


#include <string>
#include <jni.h>

#include "cli/io_device.h"
#include "cli/non_blocking_io_device.h"


//! @brief NativeDevice::OpenDevice method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeDevice__OpenDevice(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::CloseDevice method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeDevice__CloseDevice(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::PutString method implementation.
//!
//! Makes the connection with the Java side.
void __NativeDevice__PutString(
        const cli::OutputDevice& CLI_Device, const char* const STR_Out);

//! @brief NativeDevice::Beep method implementation.
//!
//! Makes the connection with the Java side.
void __NativeDevice__Beep(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::CleanScreen method implementation.
//!
//! Makes the connection with the Java side.
void __NativeDevice__CleanScreen(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::CleanScreen method implementation.
//!
//! Makes the connection with the Java side.
const cli::OutputDevice::ScreenInfo __NativeDevice__GetScreenInfo(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::WouldOutput method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeDevice__WouldOutput(
        const cli::OutputDevice& CLI_Device1, const cli::OutputDevice& CLI_Device2);

//! @brief NativeDevice::GetKey method implementation.
//!
//! Makes the connection with the Java side.
const cli::KEY __NativeDevice__GetKey(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::GetLocation method implementation.
//!
//! Makes the connection with the Java side.
const cli::ResourceString __NativeDevice__GetLocation(
        const cli::OutputDevice& CLI_Device);

//! @brief NativeDevice::WouldInput method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeDevice__WouldInput(
        const cli::OutputDevice& CLI_Device1, const cli::OutputDevice& CLI_Device2);

//! @brief NativeDevice::WaitForKeys method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeDevice__WaitForKeys(
        const cli::OutputDevice& CLI_Device,
        const unsigned int UI_Milli);


//! @brief Template class implementing native C++ objects matching with cli.Cli with and cli.Menu derived class.
template <
    class TDevice //!< Either cli::OutputDevice or cli::IODevice or cli::NonBlockingIODevice.
> class NativeDevice : public TDevice
{
private:
    //! @brief No default constructor.
    NativeDevice(void);

    //! @brief No copy constructor.
    NativeDevice(const NativeDevice&);

public:
    //! @brief Constructor.
    //! @note Native output device are always created as auto-destructive.
    NativeDevice(
        const char* const STR_DbgName       //!< Device debug name.
        )
      : TDevice(STR_DbgName, true)
    {
    }

    //! @brief Destructor.
    virtual ~NativeDevice(void)
    {
    }

private:
    //! @brief No assignment operator.
    NativeDevice<TDevice>& operator=(const NativeDevice<TDevice>&);

public:
    // cli::OutputDevice interface implementation.
    virtual const bool OpenDevice(void)
    {
        return __NativeDevice__OpenDevice(*this);
    }

    virtual const bool CloseDevice(void)
    {
        return __NativeDevice__CloseDevice(*this);
    }

    virtual void PutString(const char* const STR_Out) const
    {
        __NativeDevice__PutString(*this, STR_Out);
    }

    virtual void Beep(void) const
    {
        __NativeDevice__Beep(*this);
    }

    virtual void CleanScreen(void) const
    {
        __NativeDevice__CleanScreen(*this);
    }

    virtual const cli::OutputDevice::ScreenInfo GetScreenInfo(void) const
    {
        return __NativeDevice__GetScreenInfo(*this);
    }

    virtual const bool WouldOutput(const cli::OutputDevice& CLI_Device) const
    {
        return __NativeDevice__WouldOutput(*this, CLI_Device);
    }

    // cli::IODevice interface implementation.
    virtual const cli::KEY GetKey(void) const
    {
        return __NativeDevice__GetKey(*this);
    }

    virtual const cli::ResourceString GetLocation(void) const
    {
        return __NativeDevice__GetLocation(*this);
    }

    virtual const bool WouldInput(const cli::IODevice& CLI_Device) const
    {
        return __NativeDevice__WouldInput(*this, CLI_Device);
    }

    // cli::NonBlockingIODevice interface implementation.
    virtual const bool WaitForKeys(const unsigned int UI_Milli) const
    {
        return __NativeDevice__WaitForKeys(*this, UI_Milli);
    }
};

#endif // _CLI_NATIVE_NON_BLOCKING_DEVICE_H_
