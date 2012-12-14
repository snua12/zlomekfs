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

#include <stdlib.h> // atoi
#include <string.h> // strlen
#ifndef CLI_WIN_NETWORK
    #include <errno.h> // errno
    #define socket_errno errno
    #include <sys/types.h> // socket, bind, listen
    #include <sys/socket.h> // socket, bind, listen
    #include <netinet/in.h> // sockaddr_in
    #include <netdb.h> // gethostbyname
#else
    #include <winsock2.h> // Windows sockets
    #define socket_errno WSAGetLastError()
    #define EADDRINUSE WSAEADDRINUSE
    #define close closesocket
    #define in_addr_t in_addr
    // Disable disturbing macros.
    #undef DELETE
#endif
#include <stdio.h> // close

#include "cli/common.h"
#include "cli/telnet.h"
#include "cli/file_device.h"


int main(int I_ArgCount, char* ARSTR_Args[])
{
    static const cli::TraceClass CLI_TELNET_CLIENT("CLI_TELNET_CLIENT", cli::Help());
    static const cli::TraceClass CLI_TELNET_IN("CLI_TELNET_IN", cli::Help());
    class _Trace { public:
        _Trace() {
            cli::GetTraces().SetStream(cli::OutputDevice::GetStdErr());
            //  cli::GetTraces().Declare(CLI_TELNET_CLIENT);
            //  cli::GetTraces().SetFilter(CLI_TELNET_CLIENT, true);
            //  cli::GetTraces().Declare(CLI_TELNET_IN);
            //  cli::GetTraces().SetFilter(CLI_TELNET_IN, false);
        }
        ~_Trace() {
            cli::GetTraces().UnsetStream(cli::OutputDevice::GetStdErr());
        }
    } guard;

    unsigned long ul_Port = 0;
    if ((I_ArgCount < 3) || (ARSTR_Args[0] == NULL))
    {
        cli::OutputDevice::GetStdErr() << "USAGE: telnet-client <port> <file>" << cli::endl;
        cli::OutputDevice::GetStdErr() << "   port: TCP port to connect to" << cli::endl;
        cli::OutputDevice::GetStdErr() << "   file: test file" << cli::endl;
        return -1;
    }
    ul_Port = atoi(ARSTR_Args[1]);
    if (ul_Port == 0)
    {
        cli::OutputDevice::GetStdErr() << "Invalid port " << ARSTR_Args[0] << cli::endl;
        return -1;
    }
    const char* str_TestFile = ARSTR_Args[2];
    if ((str_TestFile == NULL) || (strlen(str_TestFile) <= 0))
    {
        cli::OutputDevice::GetStdErr() << "Invalid test file '" << str_TestFile << "'" << cli::endl;
        return -1;
    }

    const int i_ClientSocket = socket(AF_INET, SOCK_STREAM, 0);
    cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "i_ClientSocket = " << i_ClientSocket << cli::endl;
    #ifdef CLI_WIN_NETWORK
    // Winsock needs to be initialized.
    if (i_ClientSocket < 0)
    {
        WORD wVersionRequested = MAKEWORD(2, 2);
        WSADATA wsaData;
        int err = WSAStartup(wVersionRequested, & wsaData);
        if (err == 0)
        {
            ((int&) i_ClientSocket) = socket(AF_INET, SOCK_STREAM, 0);
        }
    }
    #endif
    if (i_ClientSocket < 0)
    {
        cli::OutputDevice::GetStdErr() << "socket() failed" << cli::endl;
        cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "errno = " << socket_errno << cli::endl;
        return -1;
    }

    struct sockaddr_in s_SockAddr;
    s_SockAddr.sin_family = AF_INET;
    s_SockAddr.sin_port = htons((unsigned short) ul_Port);
    if (const struct hostent* const ps_Host = gethostbyname("localhost"))
    {
#ifndef CLI_WIN_NETWORK
        s_SockAddr.sin_addr.s_addr = * (in_addr_t*) ps_Host->h_addr;
#else
        s_SockAddr.sin_addr.s_addr = ((in_addr_t*) ps_Host->h_addr)->S_un.S_addr;
#endif
    }
    else
    {
        cli::OutputDevice::GetStdErr() << "Could not resolve 'localhost'" << cli::endl;
        cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "errno = " << socket_errno << cli::endl;
        return false;
    }

    if (connect(i_ClientSocket, (struct sockaddr*) & s_SockAddr, sizeof(s_SockAddr)) < 0)
    {
        close(i_ClientSocket);
        cli::OutputDevice::GetStdErr() << "connect() failed" << cli::endl;
        cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "errno = " << socket_errno << cli::endl;
        return -1;
    }
    cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "connect() successful" << cli::endl;

    cli::TelnetConnection cli_TelnetConnection(NULL, i_ClientSocket, cli::ResourceString::LANG_EN, false);
    if (cli_TelnetConnection.OpenUp(__CALL_INFO__))
    {
        cli::InputFileDevice cli_TestFile(str_TestFile, cli::OutputDevice::GetStdOut(), false);
        cli_TestFile.EnableSpecialCharacters(true);
        if (cli_TestFile.OpenUp(__CALL_INFO__))
        {
            cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "Pushing '" << str_TestFile << "'" << cli::endl;
            while (1)
            {
                const cli::KEY e_Key = cli_TestFile.GetKey();
                if (e_Key == cli::NULL_KEY) break;
                cli_TelnetConnection << (char) e_Key;
            }
            cli_TestFile.CloseDown(__CALL_INFO__);
            cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "Push of '" << str_TestFile << "' done" << cli::endl;

            cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "Reading from socket" << cli::endl;
            while (1)
            {
                const cli::KEY e_Key = cli_TelnetConnection.GetKey();
                if (e_Key == cli::NULL_KEY)
                {
                    break;
                }
                switch (e_Key)
                {
                    case cli::ENTER:
                        cli::OutputDevice::GetStdOut() << cli::endl;
                        break;
                    default:
                        cli::OutputDevice::GetStdOut() << (char) e_Key;
                        break;
                }
                /*char str_Buffer[1];
                const int i_Len = recv(i_ClientSocket, str_Buffer, 1, 0);
                int e_Key = str_Buffer[0];
                if (i_Len <= 0)
                {
                    cli::OutputDevice::GetStdErr() << "recv() failed (" << i_Len << ")" << cli::endl;
                    cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "errno = " << socket_errno << cli::endl;
                    break;
                }
                cli::GetTraces().Trace(CLI_TELNET_CLIENT) << (char) e_Key << "(" << (int) e_Key << ")" << cli::endl;
                cli::OutputDevice::GetStdOut() << (char) e_Key;*/
            }
            cli_TelnetConnection.CloseDown(__CALL_INFO__);
            cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "Read from socket done" << cli::endl;
        }
    }

    cli::GetTraces().Trace(CLI_TELNET_CLIENT) << "Connection is done" << cli::endl;
    return 0;
}
