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

#ifndef _CLI_NATIVE_MENU_H_
#define _CLI_NATIVE_MENU_H_


#include <string>
#include <jni.h>

#include "cli/help.h"
#include "cli/command_line.h"


//! @brief NativeMenu::Execute method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeMenu__Execute(
    const cli::Menu& CLI_Menu, const cli::CommandLine& CLI_CmdLine);

//! @brief NativeMenu::OnError method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeMenu__OnError(
    const cli::Menu& CLI_Menu, const cli::ResourceString& CLI_Location, const cli::ResourceString& CLI_ErrorMessage);

//! @brief NativeMenu::OnExit method implementation.
//!
//! Makes the connection with the Java side.
const bool __NativeMenu__OnExit(
    const cli::Menu& CLI_Menu);

//! @brief NativeMenu::OnPrompt method implementation.
//!
//! Makes the connection with the Java side.
const std::string __NativeMenu__OnPrompt(
    const cli::Menu& CLI_Menu);


//! @brief Template class implementing native C++ objects matching with cli.Cli and cli.Menu derived class.
template <
    class TMenu //!< Either cli::Cli or cli::Menu.
> class NativeMenu : public TMenu
{
public:
    //! @brief Constructor.
    NativeMenu(
            const char* const STR_Name,     //!< Name of the menu (regular cli::Cli or cli::Menu constructor parameter).
            const cli::Help& CLI_Help       //!< Help of the menu (regular cli::Cli or cli::Menu constructor parameter).
            )
      : TMenu(STR_Name, CLI_Help)
    {
    }

    //! @brief Destructor.
    virtual ~NativeMenu(void)
    {
    }

// cli::Menu & cli::Cli interfaces implementation.
public:
    virtual const bool Execute(const cli::CommandLine& CLI_CmdLine) const
    {
        return __NativeMenu__Execute(*this, CLI_CmdLine);
    }

    //! @warning This handler is available for cli::Cli derived classes only. This handler shall not be called for regular menus.
    virtual const bool OnError(const cli::ResourceString& CLI_Location, const cli::ResourceString& CLI_ErrorMessage) const
    {
        return __NativeMenu__OnError(*this, CLI_Location, CLI_ErrorMessage);
    }

    virtual void OnExit(void) const
    {
        __NativeMenu__OnExit(*this);
    }

    virtual const cli::tk::String OnPrompt(void) const
    {
        const std::string std_Prompt = __NativeMenu__OnPrompt(*this);
        return cli::tk::String(std_Prompt.size(), std_Prompt.c_str());
    }
};

#endif // _CLI_NATIVE_MENU_H_
