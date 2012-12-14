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
//! @brief OutputDevice, IODevice and IOEndl classes defintion.

#ifndef _CLI_IO_DEVICE_H_
#define _CLI_IO_DEVICE_H_

#include "cli/namespace.h"
#include "cli/object.h"
#include "cli/debug.h"
#include "cli/tk.h"
#include "cli/resource_string.h"


CLI_NS_BEGIN(cli)

    //! @brief End of line for input/output devices.
    //! @see endl
    class IOEndl : public Object
    {
    public:
        //! @brief Default constructor.
        IOEndl(void) {}

    private:
        //! @brief No copy constructor.
        IOEndl(const IOEndl&);
        //! @brief No assignment operator.
        IOEndl& operator=(const IOEndl&);
    };

    //! The common IOEndl object.
    //! endl can be passed to OutputDevice to print carriage returns.
    //!
    //! It is better to use endl rather to print "\n" or "\r\n"
    //! since this should depend on the OutputDevice.
    extern const IOEndl endl;


    //! @brief Generic output device.
    //! @see endl
    class OutputDevice : public Object
    {
    private:
        //! @brief No default constructor.
        OutputDevice(void);
        //! @brief No copy constructor.
        OutputDevice(const OutputDevice&);

    protected:
        //! @brief Constructor.
        OutputDevice(
            const char* const STR_DbgName,  //!< Debug name. Useful for traces only.
            const bool B_AutoDelete         //!< Auto-deletion flag.
            );

        //! @brief Destructor.
        virtual ~OutputDevice(void);

    private:
        //! @brief No assignment operator.
        OutputDevice& operator=(const OutputDevice&);

    public:
        //! @brief Debug name accessor.
        //! @return Debug name.
        const tk::String GetDebugName(void) const;

    public:
        //! @brief Ensures instance validity.
        //! @return Total number of instance users after this call.
        const int UseInstance(
            const CallInfo& CLI_CallInfo    //!< Call information.
            );

        //! @brief Releases the instance.
        //! @return Total number of instance users after this call.
        //!
        //! If the auto-deletion flag has been set during construction,
        //! the object is auto-deleted when the number of users reaches 0 on this call.
        const int FreeInstance(
            const CallInfo& CLI_CallInfo    //!< Call information.
            );

        //! @brief Instance user count access.
        const int GetInstanceUsers(void) const;

    public:
        //! @brief Checks the device is opened.
        //!
        //! Opens the device if not already opened.
        //! Acquire a lock on the open state in any case.
        const bool OpenUp(
            const CallInfo& CLI_CallInfo    //!< Call information.
            );

        //! @brief Indicates the device the client does not need the device to opened anymore.
        //!
        //! Releases the lock on the open state.
        //! When no more user need the device to be opened, it is closed straight forward.
        const bool CloseDown(
            const CallInfo& CLI_CallInfo    //!< Call information.
            );

        //! @brief Open state user count access.
        const int GetOpenUsers(void) const;

    protected:
        //! @brief Device opening handler.
        //! @note Devices have to be prepared to be called several times through this method.
        //!       They should do the opening once only
        //!       (unless they have been closed between OpendDevice() calls),
        //!       and indicate no failure thereafter.
        virtual const bool OpenDevice(void) = 0;

        //! @brief Device closure handler.
        //! @note Devices have to be prepared to be called several times through this method.
        //!       They should do the closure once only
        //!       (unless they have been opened between CloseDevice() calls),
        //!       and indicate no failure thereafter.
        virtual const bool CloseDevice(void) = 0;

    public:
        #ifndef CLI_NO_STL
        //! @brief Output operator.
        //! @return The output device itself.
        //! @author [contrib: Oleg Smolsky, 2010, based on CLI 2.5]
        const OutputDevice& operator<<(
            const std::string& STR_Out  //!< Output string object.
            ) const;
        #endif

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const tk::String& STR_Out  //!< Output string object.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const char* const STR_Out   //!< Output null terminated string.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const int I_Out             //!< Integer number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const unsigned int UI_Out   //!< Unsigned integer number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const char C_Out            //!< Single character.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const unsigned char UC_Out  //!< Unsigned char number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const short S_Out           //!< Short number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const unsigned short US_Out //!< Unsigned short number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const long L_Out            //!< Long number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const unsigned long UL_Out  //!< Unsigned long number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const float F_Out           //!< Float number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const double D_Out          //!< Double number.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            void* const PV_Out          //!< Void address.
            ) const;

        //! @brief Output operator.
        //! @return The output device itself.
        const OutputDevice& operator<<(
            const IOEndl& CLI_IOEndl    //!< Carriage return.
            ) const;

    public:
        //! @brief Last error accessor.
        //! @return Last error resource string.
        const ResourceString GetLastError(void) const;

    public:
        //! @brief Null device.
        static OutputDevice& GetNullDevice(void);

        //! @brief Standard output device.
        static OutputDevice& GetStdOut(void);

        //! @brief Standard error device.
        static OutputDevice& GetStdErr(void);

    public:
        //! @brief Screen information.
        class ScreenInfo : public Object
        {
        public:
            enum {
                UNKNOWN = -1,           //!< Unknown value constant for either width or height.
                DEFAULT_WIDTH = 80,     //!< Default width constant.
                DEFAULT_HEIGHT = 20     //!< Default height constant.
            } _RegularValues;

        private:
            //! No default constructor.
            ScreenInfo(void);

        public:
            //! @brief Constructor.
            ScreenInfo(
                const int I_Width,      //!< Width of screen. Can be UNKNOWN.
                const int I_Height,     //!< Height of screen. Can be UNKNOWN.
                const bool B_TrueCls,   //!< True when an efficient CleanScreen() operation is implemented.
                const bool B_WrapLines  //!< True when the line automatically goes down when the cursor reached the right end of the screen.
                ) : Object(),
                    m_iWidth(I_Width), m_iHeight(I_Height),
                    m_bTrueCls(B_TrueCls), m_bWrapLines(B_WrapLines)
            {}
            //! @brief Copy constructor.
            ScreenInfo(
                const ScreenInfo& CLI_Info      //!< Dimension object to copy.
                ) : Object(),
                    m_iWidth(CLI_Info.GetWidth()), m_iHeight(CLI_Info.GetHeight()),
                    m_bTrueCls(CLI_Info.GetbTrueCls()), m_bWrapLines(CLI_Info.GetbWrapLines())
            {}
            //! @brief Destructor.
            ~ScreenInfo(void) {}
        public:
            //! @brief Assignment operator.
            ScreenInfo& operator=(
                const ScreenInfo& CLI_ScreenInfo    //!< Screen information to copy.
                )
            {
                m_iWidth = CLI_ScreenInfo.m_iWidth;
                m_iHeight = CLI_ScreenInfo.m_iHeight;
                m_bTrueCls = CLI_ScreenInfo.m_bTrueCls;
                m_bWrapLines = CLI_ScreenInfo.m_bWrapLines;
                return *this;
            }
        public:
            //! @brief Screen width accessor.
            //! @return Screen width if known, UNKNOWN otherwise.
            const int GetWidth(void) const { return ((m_iWidth > 0) ? m_iWidth : UNKNOWN); }
            //! @brief Safe screen width accessor.
            //! @return Screen width if known, default value otherwise.
            const unsigned int GetSafeWidth(void) const { return ((m_iWidth > 0) ? m_iWidth : DEFAULT_WIDTH); }
            //! @brief Screen height accessor.
            //! @return Screen height if known, UNKNOWN otherwise.
            const int GetHeight(void) const { return ((m_iHeight > 0) ? m_iHeight : UNKNOWN); }
            //! @brief Safe screen height accessor.
            //! @return Screen height if known, default value otherwise.
            const unsigned int GetSafeHeight(void) const { return ((m_iHeight > 0) ? m_iHeight : DEFAULT_HEIGHT); }
            //! @brief True CleanScreen() characteristic accessor.
            //! @return True CleanScreen() characteristic.
            const bool GetbTrueCls(void) const { return m_bTrueCls; }
            //! @brief Line wrapping characteristic accessor.
            //! @return Line wrapping characteristic.
            const bool GetbWrapLines(void) const { return m_bWrapLines; }
        private:
            int m_iWidth;       //!< Page width.
            int m_iHeight;      //!< Page height.
            bool m_bTrueCls;    //!< True CleanScreen() implementation.
            bool m_bWrapLines;  //!< Line wrapping.
        };

    public:
        //! @brief Output handler.
        virtual void PutString(
            const char* const STR_Out   //!< Output string.
            ) const = 0;

        //! @brief Beep handler.
        virtual void Beep(void) const;

        //! @brief Clean screen.
        virtual void CleanScreen(void) const;

        //! @brief Screen info accessor.
        //! @return Screen info with possible ScreenInfo::UNKNOWN values.
        virtual const ScreenInfo GetScreenInfo(void) const;

        //! @brief Stack overflow protection.
        //! @return true if the callee device would redirect characters to the given device for output.
        //!
        //! Determines whether the current device would output the given device in any way.
        //! Default implementation checks whether CLI_Device is the self device.
        virtual const bool WouldOutput(
            const OutputDevice& CLI_Device  //!< Other device that the callee device should check it would output characters to.
            ) const;

    private:
        //! Debug name. Useful for traces only.
        const tk::String m_strDebugName;

        //! Instance lock count.
        int m_iInstanceLock;

        //! Open state lock count.
        int m_iOpenLock;

    protected:
        //! Last error.
        mutable ResourceString m_cliLastError;
    };


    //! @brief Input characters.
    typedef enum _KEY
    {
        NULL_KEY = '\0',    //!< Null key.

        BREAK = 3,          //!< Break (Ctrl+C).
        LOGOUT = 4,         //!< Logout (Ctrl+D).
        ENTER = 13,         //!< Enter.
        ESCAPE = 27,        //!< Escape.
        SPACE = 32,         //!< Space.
        BACKSPACE = 8,      //!< Backspace (changed from '\\b' to 8 in version 2.7 for ASCII compliance).
        DELETE = 127,       //!< Delete key (changed from 128 to 127 in version 2.7 for ASCII compliance).
        CLS = 501,          //!< Clean screen key (changed from 129 to 501 in order to avoid overlap with printable ASCII characters).
        INSERT = 502,       //!< Insert key (changed from 500 to 502 in order to avoid overlap with printable ASCII characters).

        TAB = '\t',
        KEY_0 = '0', KEY_1 = '1', KEY_2 = '2', KEY_3 = '3', KEY_4 = '4', KEY_5 = '5',
        KEY_6 = '6', KEY_7 = '7', KEY_8 = '8', KEY_9 = '9',
        KEY_a = 'a', KEY_aacute = 'á', KEY_agrave = 'à', KEY_auml = 'ä', KEY_acirc = 'â',
        KEY_b = 'b', KEY_c = 'c', KEY_ccedil = 'ç', KEY_d = 'd',
        KEY_e = 'e', KEY_eacute = 'é', KEY_egrave = 'è', KEY_euml = 'ë', KEY_ecirc = 'ê',
        KEY_f = 'f', KEY_g = 'g', KEY_h = 'h',
        KEY_i = 'i', KEY_iacute = 'í', KEY_igrave = 'ì', KEY_iuml = 'ï', KEY_icirc = 'î',
        KEY_j = 'j', KEY_k = 'k', KEY_l = 'l', KEY_m = 'm', KEY_n = 'n',
        KEY_o = 'o', KEY_oacute = 'ó', KEY_ograve = 'ò', KEY_ouml = 'ö', KEY_ocirc = 'ô',
        KEY_p = 'p', KEY_q = 'q', KEY_r = 'r', KEY_s = 's', KEY_t = 't',
        KEY_u = 'u', KEY_uacute = 'ú', KEY_ugrave = 'ù', KEY_uuml = 'ü', KEY_ucirc = 'û',
        KEY_v = 'v', KEY_w = 'w', KEY_x = 'x', KEY_y = 'y', KEY_z = 'z',

        KEY_A = 'A', KEY_B = 'B', KEY_C = 'C', KEY_D = 'D', KEY_E = 'E', KEY_F = 'F',
        KEY_G = 'G', KEY_H = 'H', KEY_I = 'I', KEY_J = 'J', KEY_K = 'K', KEY_L = 'L',
        KEY_M = 'M', KEY_N = 'N', KEY_O = 'O', KEY_P = 'P', KEY_Q = 'Q', KEY_R = 'R',
        KEY_S = 'S', KEY_T = 'T', KEY_U = 'U', KEY_V = 'V', KEY_W = 'W', KEY_X = 'X',
        KEY_Y = 'Y', KEY_Z = 'Z',

        PLUS = '+',
        MINUS = '-',
        STAR = '*',
        SLASH = '/',
        LOWER_THAN = '<',
        GREATER_THAN = '>',
        EQUAL = '=',
        PERCENT = '%',

        UNDERSCORE = '_',
        AROBASE = '@',
        SHARP = '#',
        AMPERCENT = '&',
        DOLLAR = '$',
        BACKSLASH = '\\',
        PIPE = '|',
        TILDE = '~',
        SQUARE = '²',
        EURO = '€',
        POUND = '£',
        MICRO = 'µ',
        PARAGRAPH = '§',
        DEGREE = '°',
        COPYRIGHT = '©',

        QUESTION = '?',
        EXCLAMATION = '!',
        COLUMN = ':',
        DOT = '.',
        COMA = ',',
        SEMI_COLUMN = ';',
        QUOTE = '\'',
        DOUBLE_QUOTE = '"',
        BACK_QUOTE = '`',

        OPENING_BRACE = '(',
        CLOSING_BRACE = ')',
        OPENING_CURLY_BRACE = '{',
        CLOSING_CURLY_BRACE = '}',
        OPENING_BRACKET = '[',
        CLOSING_BRACKET = ']',

        KEY_UP = 1001,              //!< Up arrow key.
        KEY_DOWN = 1002,            //!< Down arrow key.
        KEY_LEFT = 1003,            //!< Left arrow key.
        KEY_RIGHT = 1004,           //!< Right arrow key.
        PAGE_UP = 1005,             //!< Page up arrow key.
        PAGE_DOWN = 1006,           //!< Page down arrow key.
        PAGE_LEFT = 1007,           //!< Page left arrow key.
        PAGE_RIGHT = 1008,          //!< Page right arrow key.

        KEY_BEGIN = 1020,           //!< Begin key.
        KEY_END = 1021,             //!< End key.

        COPY = 2001,                //!< Copy.
        CUT = 2002,                 //!< Cut.
        PASTE = 2003,               //!< Paste.

        UNDO = 2004,                //!< Undo.
        REDO = 2005,                //!< Redo.
        PREVIOUS = 2006,            //!< Previous key.
        NEXT = 2007,                //!< Forward key.

        F1 = 0x0f000001,
        F2 = 0x0f000002,
        F3 = 0x0f000003,
        F4 = 0x0f000004,
        F5 = 0x0f000005,
        F6 = 0x0f000006,
        F7 = 0x0f000007,
        F8 = 0x0f000008,
        F9 = 0x0f000009,
        F10 = 0x0f00000a,
        F11 = 0x0f00000b,
        F12 = 0x0f00000c,
    } KEY;


    //! @brief Generic input/output device.
    //! @see endl
    class IODevice : public OutputDevice
    {
    private:
        //! @brief No default constructor.
        IODevice(void);
        //! @brief No copy constructor.
        IODevice(const IODevice&);

    public:
        //! @brief Constructor.
        IODevice(
            const char* const STR_DbgName,  //!< Debug name.
            const bool B_AutoDelete         //!< Auto-deletion flag.
            );

        //! @brief Destructor.
        virtual ~IODevice(void);

    private:
        //! @brief No assignment operator.
        IODevice& operator=(const IODevice&);

    public:
        //! @brief Input key capture handler.
        virtual const KEY GetKey(void) const = 0;

        //! @brief Input location.
        virtual const ResourceString GetLocation(void) const;

        //! @brief Stack overflow protection.
        //! @return true if the callee device would redirect input to the given device for reading.
        //!
        //! Determines whether the current device would input the given device in any ways.
        //! Default implementation checks whether CLI_Device is the self device.
        virtual const bool WouldInput(
            const IODevice& CLI_Device  //!< Other device that the callee device should check it would input characters from.
            ) const;

    public:
        //! @brief Null device.
        static IODevice& GetNullDevice(void);

        //! @brief Standard input device.
        static IODevice& GetStdIn(void);

    protected:
        //! @brief Common char translation.
        const KEY GetKey(const int I_Char) const;
    };

CLI_NS_END(cli)

#endif // _CLI_IO_DEVICE_H_

