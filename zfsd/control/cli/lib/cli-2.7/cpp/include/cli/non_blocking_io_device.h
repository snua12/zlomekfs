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


//! @file
//! @author Alexis Royer
//! @brief NonBlockingDevice class definition.


#ifndef _CLI_NON_BLOCKING_DEVICE_H_
#define _CLI_NON_BLOCKING_DEVICE_H_

#include "cli/namespace.h"
#include "cli/object.h"
#include "cli/io_device.h"
#include "cli/tk.h"


CLI_NS_BEGIN(cli)

    // Forward declarations.
    class NonBlockingKeyReceiver;
    class Shell;


    //! @brief Non-blocking input device.
    class NonBlockingIODevice : public IODevice
    {
    private:
        //! @brief No default constructor.
        NonBlockingIODevice(void);
        //! @brief No copy constructor.
        NonBlockingIODevice(const NonBlockingIODevice&);

    public:
        //! @brief Main constructor.
        NonBlockingIODevice(
            const char* const STR_DbgName,  //!< Debug name.
            const bool B_AutoDelete         //!< Auto-deletion flag.
            );

        //! @brief Destructor.
        virtual ~NonBlockingIODevice(void);

    private:
        //! @brief No assignment operator.
        NonBlockingIODevice& operator=(const NonBlockingIODevice&);

    public:
        //! @brief IODevice non-blocking implementation.
        virtual const KEY GetKey(void) const;

    public:
        //! @brief Key receiver registration.
        //! @warning Should be called by key receivers only.
        void AttachKeyReceiver(
            NonBlockingKeyReceiver& CLI_KeyReceiver     //!< Key receiver to register.
            );

        //! @brief Key receiver unregistration.
        //! @warning Should be called by key receivers only.
        void DetachKeyReceiver(
            NonBlockingKeyReceiver& CLI_KeyReceiver     //!< Key receiver to unregister.
            );

        //! @brief Returns the current key receiver.
        //! @return Current key receiver if any, NULL otherwise.
        const NonBlockingKeyReceiver* const GetKeyReceiver(void) const;

        //! @brief Returns the registered shell (if any).
        //! @return Registered shell if any, NULL otherwise.
        const Shell* const GetShell(void) const;

    public:
        //! @brief Handler to call when a key is received.
        virtual void OnKey(
            const KEY E_Key //!< Input key.
            ) const;

        //! @brief When a blocking call requires keys to be entered before returning, this method makes the thread waits smartly depending on the integration context.
        //! @warning This kind of wait implementation may cause nested peek message loops, but if you are using cli::ui features, you might need to implement such loop.
        //! @return false when the caller should not wait for keys anymore, true otherwise.
        //!
        //! If a key has been entered during the waiting time, this method MUST call OnKey(), then SHALL stop waiting and return right away.
        virtual const bool WaitForKeys(
            const unsigned int UI_Milli //!< Number of milliseconds to wait.
            ) const;

    private:
        //! Key receivers stack.
        tk::Queue<NonBlockingKeyReceiver*> m_cliKeyReceivers;
    };

    //! @brief Non-blocking key receiver interface.
    class NonBlockingKeyReceiver : public Object
    {
    protected:
        //! @brief Default constructor is not public.
        NonBlockingKeyReceiver(void);
    private:
        //! @brief No copy constructor.
        NonBlockingKeyReceiver(const NonBlockingKeyReceiver&);
    public:
        //! @brief Destructor.
        virtual ~NonBlockingKeyReceiver(void) = 0;

    public:
        //! @brief Hook called by non-blocking devices on character input.
        virtual void OnNonBlockingKey(
            NonBlockingIODevice& CLI_Source,    //!< Input non-blocking device.
            const KEY E_KeyCode                 //!< Input key.
            ) = 0;
    };

CLI_NS_END(cli)

#endif // _CLI_NON_BLOCKING_DEVICE_H_
