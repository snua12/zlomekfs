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
//! @brief UI class definition.

#ifndef _CLI_UI_H_
#define _CLI_UI_H_

#include "cli/namespace.h"
#include "cli/tk.h"
#include "cli/non_blocking_io_device.h"


CLI_NS_BEGIN(cli)

    // Forward declarations
    class Shell;


    CLI_NS_BEGIN(ui)

        //! @brief Generic user interface class.
        class UI : public NonBlockingKeyReceiver
        {
        private:
            //! @brief No copy constructor.
            UI(const UI&);

        protected:
            //! @brief Default constructor.
            UI(void);

        public:
            //! @brief Destructor.
            virtual ~UI(void);

        private:
            //! @brief No assignment operator.
            UI& operator=(const UI&);

        public:
            //! @brief Runs within the context of a running shell.
            //! @return true for a regular output, false for an error or a cancellation.
            const bool Run(
                Shell& CLI_Shell        //!< Shell context.
                );

        private:
            //! @brief Method called when execution starts.
            void Start(Shell& CLI_Shell);
        protected:
            //! @brief Method to call by child classes in order to finish execution.
            void Finish(
                const bool B_ExecResult //!< Execution result, true for success, false otherwise.
                );
        protected:
            //! @brief Handler called when data reset is required.
            virtual void Reset(void) = 0;
            //! @brief Handler called when default value is required to be restored.
            virtual void ResetToDefault(void) = 0;

        protected:
            //! @brief Attached shell retrieval.
            //! @return Shell reference.
            Shell& GetShell(void) const;

        public:
            // NonBlockingKeyReceiver interface implementation.
            void OnNonBlockingKey(NonBlockingIODevice& CLI_Source, const KEY E_KeyCode);
            //! @brief Key reception handler.
            virtual void OnKey(
                const KEY E_KeyCode     //!< Key received.
                ) = 0;

        private:
            //! Attached Shell.
            Shell* m_pcliShell;
            //! Internal flag for execution management.
            bool m_bKeepRunning;
            //! Execution result.
            bool m_bExecResult;
        };

    CLI_NS_END(ui)

CLI_NS_END(cli)

#endif // _CLI_UI_H_
