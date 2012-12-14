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
//! @brief TelnetServer and TelnetConnection classes definition.

#ifndef _CLI_TELNET_H_
#define _CLI_TELNET_H_

#include "cli/namespace.h"
#include "cli/object.h"
#include "cli/non_blocking_io_device.h"
#include "cli/tk.h"


CLI_NS_BEGIN(cli)

    // Forward declarations.
    class Shell;
    class TelnetConnection;


    //! @brief Telnet server class.
    //!
    //! Virtual object that shall be overridden for shell (and cli) instance creations.
    class TelnetServer : public Object
    {
    private:
        //! @brief No default constructor.
        TelnetServer(void);
        //! @brief No copy constructor.
        TelnetServer(const TelnetServer&);

    public:
        //! @brief Constructor.
        TelnetServer(
            const unsigned int UI_MaxConnections,   //!< Maximum number of connections managed at the same time.
            const unsigned long UL_TcpPort,         //!< TCP port to listen onto.
            const ResourceString::LANG E_Lang       //!< Debugging language.
            );

        //! @brief Destructor.
        virtual ~TelnetServer(void);

    private:
        //! @brief No assignment operator.
        TelnetServer& operator=(const TelnetServer&);

    public:
        //! @brief Starts the server.
        //! @warning Blocking call.
        void StartServer(void);

        //! @brief Stops the server.
        void StopServer(void);

    protected:
        //! @brief Shell (and cli) creation handler.
        //! @return Shell created for the given connection.
        virtual Shell* const OnNewConnection(
            const TelnetConnection& CLI_NewConnection       //!< New telnet connection to create a shell for.
            ) = 0;

        //! @brief Shell (and cli) release handler.
        virtual void OnCloseConnection(
            Shell* const PCLI_Shell,                        //!< Shell (and cli) to be released.
            const TelnetConnection& CLI_ConnectionClosed    //!< Telnet connection being closed.
            ) = 0;

    private:
        //! @brief Makes one run of the main loop.
        //! @return false when an error occured, true otherwise (even for a timeout).
        const bool RunLoop(
            const int I_Milli       //!< Maximum number of milliseconds to wait for.
                                    //!< -1 for infinite waiting.
            );

        //! @brief Accepts a connection.
        const bool AcceptConnection(void);

        //! @brief Closes a connection.
        const bool CloseConnection(
            const int I_ConnectionSocket    //!< Connection socket handler.
            );

        //! @brief Terminates the server execution.
        //! @return true for success, false otherwise.
        const bool TerminateServer(void);

    private:
        //! Server socket.
        int m_iServerSocket;
        //! Listened TCP port.
        const unsigned long m_ulTcpPort;
        //! Debugging language.
        const ResourceString::LANG m_eLang;
        //! Connection information.
        typedef struct {
            int i_Socket;                       //!< Connection socket handler.
            TelnetConnection* pcli_Connection;  //!< Telnet connection instance reference.
            Shell* pcli_Shell;                  //!< Shell instance reference.
        } ConnectionInfo;
        //! Connections registry.
        tk::Map<int, ConnectionInfo> m_tkConnections;
        //! Maximum number of connections at the same time.
        const unsigned int m_uiMaxConnections;

        friend class TelnetConnection; // Let TelnetConnection call RunLoop().
    };


    //! @brief Telnet connection input/output device.
    class TelnetConnection : public NonBlockingIODevice
    {
    private:
        //! @brief No default constructor.
        TelnetConnection(void);
        //! @brief No copy constructor.
        TelnetConnection(const TelnetConnection&);

    public:
        //! @brief Constructor.
        TelnetConnection(
            TelnetServer* const PCLI_Server,    //!< Telnet server instance reference. Can be NULL for stand alone connections.
            const int I_Socket,                 //!< Connection socket handler.
            const ResourceString::LANG E_Lang,  //!< Debugging language.
            const bool B_AutoDelete             //!< Auto-deletion flag.
            );

        //! @brief Destructor.
        virtual ~TelnetConnection(void);

    private:
        //! @brief No assignment operator.
        TelnetConnection& operator=(const TelnetConnection&);

    protected:
        //! @brief Open device handler.
        virtual const bool OpenDevice(void);
        //! @brief Close device handler.
        virtual const bool CloseDevice(void);
    protected:
        //! @brief Characters received from the socket.
        //! @return true for success, false otherwise.
        const bool ReceiveChars(void) const;
        //! @brief Processes input chars and converts them into keys one by one.
        const bool ProcessKeys(void) const;
        //! @brief Checks whether the connection should still be up.
        //! @return true if the connection is still up, false otherwise.
        const bool CheckUp(void) const;
    public:
        //! @brief Character input handler.
        virtual const KEY GetKey(void) const;
        //! @brief Non-blocking IO device OnKey overriding.
        virtual void OnKey(const KEY E_Key) const;
        //! @brief Non-blocking IO device implementation.
        virtual const bool WaitForKeys(const unsigned int UI_Milli) const;
        //! @brief Output handler.
        virtual void PutString(const char* const STR_Out) const;
        //! @brief Beep handler.
        virtual void Beep(void) const;
        //! @brief Clean screen handler.
        virtual void CleanScreen(void) const;

    private:
        //! Telnet server instance reference.
        TelnetServer* const m_pcliServer;
        //! Connection socket handler.
        const int m_iSocket;
        //! Debugging language.
        const ResourceString::LANG m_eLang;
        //! Character input buffer.
        mutable tk::Queue<int> m_qChars;
        //! Waiting key flag.
        mutable bool m_bWaitingForKeys;

        friend class TelnetServer; // Let TelnetServer call ReceiveChars().
    };

CLI_NS_END(cli)

#endif // _CLI_TELNET_H_

