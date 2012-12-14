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

#include <stdio.h>
#include <math.h>

#include "cli/io_device.h"
#include "cli/cli.h"
#include "cli/string_device.h"
#include "cli/traces.h"
#include "cli/assert.h"
#include "constraints.h"

CLI_NS_USE(cli)


#ifndef CLI_NO_NAMESPACE
const IOEndl cli::endl;
#else
const IOEndl endl;
#endif

//! @brief Input/Output device instance creation/deletion trace class singleton redirection.
#define TRACE_IO_DEVICE_INSTANCES GetIODeviceInstancesTraceClass()
//! @brief Input/Output device instance creation/deletion trace class singleton.
static const TraceClass& GetIODeviceInstancesTraceClass(void)
{
    static const TraceClass cli_IODeviceInstancesTraceClass("CLI_IO_DEVICE_INSTANCES", Help()
        .AddHelp(Help::LANG_EN, "IO device instance management")
        .AddHelp(Help::LANG_FR, "Gestion des intances de périphériques d'entrée/sortie"));
    return cli_IODeviceInstancesTraceClass;
}
//! @brief Input/Output device opening/closure trace class singleton redirection.
#define TRACE_IO_DEVICE_OPENING GetIODeviceOpeningTraceClass()
//! @brief Input/Output device opening/closure trace class singleton.
static const TraceClass& GetIODeviceOpeningTraceClass(void)
{
    static const TraceClass cli_IODeviceOpeningTraceClass("CLI_IO_DEVICE_OPENING", Help()
        .AddHelp(Help::LANG_EN, "IO device opening management")
        .AddHelp(Help::LANG_FR, "Gestion de l'ouverture des périphériques d'entrée/sortie"));
    return cli_IODeviceOpeningTraceClass;
}


OutputDevice::OutputDevice(
        const char* const STR_DbgName,
        const bool B_AutoDelete)
  : m_strDebugName(MAX_DEVICE_NAME_LENGTH, STR_DbgName),
    m_iInstanceLock(B_AutoDelete ? 0 : 1), m_iOpenLock(0),
    m_cliLastError()
{
    // Please, no traces in constructor for consistency reasons.
}

OutputDevice::~OutputDevice(void)
{
}

const tk::String OutputDevice::GetDebugName(void) const
{
    StringDevice cli_DebugName(MAX_DEVICE_NAME_LENGTH, false);
    cli_DebugName << m_strDebugName << "/" << (void*) this;
    return cli_DebugName.GetString();
}

const int OutputDevice::UseInstance(const CallInfo& CLI_CallInfo)
{
    GetTraces().SafeTrace(TRACE_IO_DEVICE_INSTANCES, *this)
        << "One more user for instance " << GetDebugName() << ", "
        << "user count: " << m_iInstanceLock << " -> " << m_iInstanceLock + 1 << ", "
        << "from " << CLI_CallInfo.GetFunction() << " "
        << "at " << CLI_CallInfo.GetFile() << ":" << CLI_CallInfo.GetLine() << endl;
    m_iInstanceLock ++;
    return m_iInstanceLock;
}

const int OutputDevice::FreeInstance(const CallInfo& CLI_CallInfo)
{
    GetTraces().SafeTrace(TRACE_IO_DEVICE_INSTANCES, *this)
        << "One less user for instance " << GetDebugName() << ", "
        << "user count: " << m_iInstanceLock << " -> " << m_iInstanceLock - 1 << ", "
        << "from " << CLI_CallInfo.GetFunction() << " "
        << "at " << CLI_CallInfo.GetFile() << ":" << CLI_CallInfo.GetLine() << endl;
    if (m_iInstanceLock == 1)
    {
        GetTraces().SafeTrace(TRACE_IO_DEVICE_INSTANCES, *this)
            << "Deleting the device " << GetDebugName() << endl;
        delete this;
        return 0;
    }
    else
    {
        m_iInstanceLock --;
        CLI_ASSERT(m_iInstanceLock > 0);
        return m_iInstanceLock;
    }
}

const int OutputDevice::GetInstanceUsers(void) const
{
    return m_iInstanceLock;
}

const bool OutputDevice::OpenUp(const CallInfo& CLI_CallInfo)
{
    GetTraces().SafeTrace(TRACE_IO_DEVICE_OPENING, *this)
        << "One more user for instance " << GetDebugName() << ", "
        << "user count: " << m_iOpenLock << " -> " << m_iOpenLock + 1 << ", "
        << "from " << CLI_CallInfo.GetFunction() << " "
        << "at " << CLI_CallInfo.GetFile() << ":" << CLI_CallInfo.GetLine() << endl;

    m_iOpenLock ++;

    if (m_iOpenLock == 1)
    {
        GetTraces().SafeTrace(TRACE_IO_DEVICE_OPENING, *this)
            << "Opening the device " << GetDebugName() << endl;
        if (! OpenDevice())
        {
            return false;
        }
    }

    return true;
}

const bool OutputDevice::CloseDown(const CallInfo& CLI_CallInfo)
{
    bool b_Res = true;

    if (m_iOpenLock > 0)
    {
        GetTraces().SafeTrace(TRACE_IO_DEVICE_OPENING, *this)
            << "One less user for instance " << GetDebugName() << ", "
            << "user count: " << m_iOpenLock << " -> " << m_iOpenLock - 1 << ", "
            << "from " << CLI_CallInfo.GetFunction() << " "
            << "at " << CLI_CallInfo.GetFile() << ":" << CLI_CallInfo.GetLine() << endl;

        if (m_iOpenLock == 1)
        {
            GetTraces().SafeTrace(TRACE_IO_DEVICE_OPENING, *this)
                << "Closing the device " << GetDebugName() << endl;
            b_Res = CloseDevice();
        }

        m_iOpenLock --;
    }
    else
    {
        GetTraces().SafeTrace(TRACE_IO_DEVICE_OPENING, *this)
            << "No more closing down for instance " << GetDebugName() << ", "
            << "user count = " << m_iOpenLock << ", "
            << "from " << CLI_CallInfo.GetFunction() << " "
            << "at " << CLI_CallInfo.GetFile() << ":" << CLI_CallInfo.GetLine() << endl;
    }

    return b_Res;
}

const int OutputDevice::GetOpenUsers(void) const
{
    return m_iOpenLock;
}

// [contrib: Oleg Smolsky, 2010, based on CLI 2.5]
#ifndef CLI_NO_STL
const OutputDevice& OutputDevice::operator <<(const std::string& STR_Out) const
{
    PutString(STR_Out.c_str());
    return *this;
}
#endif

const OutputDevice& OutputDevice::operator <<(const tk::String& STR_Out) const
{
    PutString(STR_Out);
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const char* const STR_Out) const
{
    if (STR_Out != NULL)
    {
        PutString(STR_Out);
    }
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const unsigned char UC_Out) const
{
    return this->operator<<((unsigned int) UC_Out);
}

const OutputDevice& OutputDevice::operator <<(const char C_Out) const
{
    char arc_String[] = { C_Out, '\0' };
    PutString(arc_String);
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const short S_Out) const
{
    return this->operator<<((int) S_Out);
}

const OutputDevice& OutputDevice::operator <<(const unsigned short US_Out) const
{
    return this->operator<<((unsigned int) US_Out);
}

const OutputDevice& OutputDevice::operator <<(const long L_Out) const
{
    return this->operator <<((int) L_Out);
}

const OutputDevice& OutputDevice::operator <<(const unsigned long UL_Out) const
{
    return this->operator <<((unsigned int) UL_Out);
}

const OutputDevice& OutputDevice::operator <<(const int I_Out) const
{
    char str_Out[128];
    snprintf(str_Out, sizeof(str_Out), "%d", I_Out);
    PutString(str_Out);
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const unsigned int UI_Out) const
{
    char str_Out[128];
    snprintf(str_Out, sizeof(str_Out), "%u", UI_Out);
    PutString(str_Out);
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const float F_Out) const
{
    return this->operator <<((double) F_Out);
}

const OutputDevice& OutputDevice::operator <<(const double D_Out) const
{
    // First of all, find out the appropriate format.
    char str_Format[128];
    if ((D_Out != 0.0) && (-1e-6 <= D_Out) && (D_Out <= 1e-6))
    {
        snprintf(str_Format, sizeof(str_Format), "%s", "%f");
    }
    else
    {
        int i_Decimal, i_Out;
        for (   i_Decimal = 6, i_Out = (int) floor(D_Out * 1e6 + 0.5); // floor(D+1/2) should be equivalent to round(D)
                i_Decimal > 1;
                i_Decimal --, i_Out /= 10)
        {
            if ((i_Out % 10) != 0)
            {
                break;
            }
        }
        snprintf(str_Format, sizeof(str_Format), "%s.%d%s", "%", i_Decimal, "f");
    }

    // Format the float number with the computed format.
    char str_Out[128];
    snprintf(str_Out, sizeof(str_Out), str_Format, D_Out);

    // Output the computed string.
    PutString(str_Out);

    // Return the instance itself.
    return *this;
}

const OutputDevice& OutputDevice::operator <<(void* const PV_Out) const
{
    char str_Out[128];
    // %p is not used here, because it has strange behaviours when compiled on different environments.
    snprintf(str_Out, sizeof(str_Out), "0x%08x", (unsigned int) PV_Out);
    PutString(str_Out);
    return *this;
}

const OutputDevice& OutputDevice::operator <<(const IOEndl& CLI_IOEndl) const
{
    tk::UnusedParameter(CLI_IOEndl); // [contrib: Oleg Smolsky, 2010, based on CLI 2.5]
    PutString("\n");
    return *this;
}

const ResourceString OutputDevice::GetLastError(void) const
{
    return m_cliLastError;
}

OutputDevice& OutputDevice::GetNullDevice(void)
{
    class NullDevice : public OutputDevice
    {
    public:
        NullDevice(void) : OutputDevice("null", false) {}
        virtual ~NullDevice(void) {}

    protected:
        virtual const bool OpenDevice(void) { return true; }
        virtual const bool CloseDevice(void) { return true; }
    public:
        virtual void PutString(const char* const STR_Out) const { cli::tk::UnusedParameter(STR_Out); } // [contrib: Oleg Smolsky, 2010, based on CLI 2.5]
    };

    static NullDevice cli_Null;
    return cli_Null;
}

OutputDevice& OutputDevice::GetStdOut(void)
{
    class StdOutDevice : public OutputDevice
    {
    public:
        StdOutDevice(void) : OutputDevice("stdout", false) {}
        virtual ~StdOutDevice(void) {}

    protected:
        virtual const bool OpenDevice(void) { return true; }
        virtual const bool CloseDevice(void) { return true; }
    public:
        virtual void PutString(const char* const STR_Out) const {
            fprintf(stdout, "%s", STR_Out);
            fflush(stdout);
        }
    };

    static StdOutDevice cli_StdOut;
    return cli_StdOut;
}

OutputDevice& OutputDevice::GetStdErr(void)
{
    class StdErrDevice : public OutputDevice
    {
    public:
        StdErrDevice(void) : OutputDevice("stderr", false) {}
        virtual ~StdErrDevice(void) {}

    protected:
        virtual const bool OpenDevice(void) { return true; }
        virtual const bool CloseDevice(void) { return true; }
    public:
        virtual void PutString(const char* const STR_Out) const {
            fprintf(stderr, "%s", STR_Out);
            fflush(stderr);
        }
    };

    static StdErrDevice cli_StdErr;
    return cli_StdErr;
}

void OutputDevice::Beep(void) const
{
    PutString("\a");
}

void OutputDevice::CleanScreen(void) const
{
    for (int i=0; i<200; i++)
    {
        PutString("\n");
    }
}

const OutputDevice::ScreenInfo OutputDevice::GetScreenInfo(void) const
{
    return ScreenInfo(
        ScreenInfo::UNKNOWN, ScreenInfo::UNKNOWN, // Width and height
        false,  // True Cls
        false   // Line wrapping
    );
}

const bool OutputDevice::WouldOutput(const OutputDevice& CLI_Device) const
{
    return (& CLI_Device == this);
}


IODevice::IODevice(
        const char* const STR_DbgName,
        const bool B_AutoDelete)
  : OutputDevice(STR_DbgName, B_AutoDelete)
{
}

IODevice::~IODevice(void)
{
}

IODevice& IODevice::GetNullDevice(void)
{
    class NullDevice : public IODevice
    {
    public:
        NullDevice(void) : IODevice("null", false) {}
        virtual ~NullDevice(void) {}

    protected:
        virtual const bool OpenDevice(void) { return true; }
        virtual const bool CloseDevice(void) { return true; }
    public:
        virtual void PutString(const char* const STR_Out) const { cli::tk::UnusedParameter(STR_Out); } // [contrib: Oleg Smolsky, 2010, based on CLI 2.5]
        virtual const KEY GetKey(void) const { return NULL_KEY; }
    };

    static NullDevice cli_Null;
    return cli_Null;
}

IODevice& IODevice::GetStdIn(void)
{
    class StdInDevice : public IODevice
    {
    public:
        StdInDevice(void) : IODevice("stdin", false) {
        }
        virtual ~StdInDevice(void) {
        }

    protected:
        virtual const bool OpenDevice(void) {
            OutputDevice::GetStdOut().UseInstance(__CALL_INFO__);
            return OutputDevice::GetStdOut().OpenUp(__CALL_INFO__);
        }
        virtual const bool CloseDevice(void) {
            bool b_Res = OutputDevice::GetStdOut().CloseDown(__CALL_INFO__);
            OutputDevice::GetStdOut().FreeInstance(__CALL_INFO__);
            return b_Res;
        }
    public:
        virtual void PutString(const char* const STR_Out) const {
            OutputDevice::GetStdOut().PutString(STR_Out);
        }
        virtual void Beep(void) const {
            OutputDevice::GetStdOut().Beep();
        }
        virtual const KEY GetKey(void) const {
            const char c_Char = (char) getchar();
                return IODevice::GetKey(c_Char);
            }
    };

    static StdInDevice cli_StdIn;
    return cli_StdIn;
}

const KEY IODevice::GetKey(const int I_Char) const
{
    switch (I_Char)
    {
    case 10: case 13:   return ENTER;

    case ' ':   return SPACE;
    case '\t':  return TAB;
    case '\b':  return BACKSPACE;

    case '0':   return KEY_0;
    case '1':   return KEY_1;
    case '2':   return KEY_2;
    case '3':   return KEY_3;
    case '4':   return KEY_4;
    case '5':   return KEY_5;
    case '6':   return KEY_6;
    case '7':   return KEY_7;
    case '8':   return KEY_8;
    case '9':   return KEY_9;

    case 'a':   return KEY_a;
    case 'á':   return KEY_aacute;
    case 'à':   return KEY_agrave;
    case 'ä':   return KEY_auml;
    case 'â':   return KEY_acirc;
    case 'b':   return KEY_b;
    case 'c':   return KEY_c;
    case 'ç':   return KEY_ccedil;
    case 'd':   return KEY_d;
    case 'e':   return KEY_e;
    case 'é':   return KEY_eacute;
    case 'è':   return KEY_egrave;
    case 'ë':   return KEY_euml;
    case 'ê':   return KEY_ecirc;
    case 'f':   return KEY_f;
    case 'g':   return KEY_g;
    case 'h':   return KEY_h;
    case 'i':   return KEY_i;
    case 'í':   return KEY_iacute;
    case 'ì':   return KEY_igrave;
    case 'ï':   return KEY_iuml;
    case 'î':   return KEY_icirc;
    case 'j':   return KEY_j;
    case 'k':   return KEY_k;
    case 'l':   return KEY_l;
    case 'm':   return KEY_m;
    case 'n':   return KEY_n;
    case 'o':   return KEY_o;
    case 'ó':   return KEY_oacute;
    case 'ò':   return KEY_ograve;
    case 'ö':   return KEY_ouml;
    case 'ô':   return KEY_ocirc;
    case 'p':   return KEY_p;
    case 'q':   return KEY_q;
    case 'r':   return KEY_r;
    case 's':   return KEY_s;
    case 't':   return KEY_t;
    case 'u':   return KEY_u;
    case 'ú':   return KEY_uacute;
    case 'ù':   return KEY_ugrave;
    case 'ü':   return KEY_uuml;
    case 'û':   return KEY_ucirc;
    case 'v':   return KEY_v;
    case 'w':   return KEY_w;
    case 'x':   return KEY_x;
    case 'y':   return KEY_y;
    case 'z':   return KEY_z;

    case 'A':   return KEY_A;
    case 'B':   return KEY_B;
    case 'C':   return KEY_C;
    case 'D':   return KEY_D;
    case 'E':   return KEY_E;
    case 'F':   return KEY_F;
    case 'G':   return KEY_G;
    case 'H':   return KEY_H;
    case 'I':   return KEY_I;
    case 'J':   return KEY_J;
    case 'K':   return KEY_K;
    case 'L':   return KEY_L;
    case 'M':   return KEY_M;
    case 'N':   return KEY_N;
    case 'O':   return KEY_O;
    case 'P':   return KEY_P;
    case 'Q':   return KEY_Q;
    case 'R':   return KEY_R;
    case 'S':   return KEY_S;
    case 'T':   return KEY_T;
    case 'U':   return KEY_U;
    case 'V':   return KEY_V;
    case 'W':   return KEY_W;
    case 'X':   return KEY_X;
    case 'Y':   return KEY_Y;
    case 'Z':   return KEY_Z;

    case '+':   return PLUS;
    case '-':   return MINUS;
    case '*':   return STAR;
    case '/':   return SLASH;
    case '<':   return LOWER_THAN;
    case '>':   return GREATER_THAN;
    case '=':   return EQUAL;
    case '%':   return PERCENT;

    case '_':   return UNDERSCORE;
    case '@':   return AROBASE;
    case '#':   return SHARP;
    case '&':   return AMPERCENT;
    case '$':   return DOLLAR;
    case '\\':  return BACKSLASH;
    case '|':   return PIPE;
    case '~':   return TILDE;
    case '²':   return SQUARE;
    case '€':   return EURO;
    case '£':   return POUND;
    case 'µ':   return MICRO;
    case '§':   return PARAGRAPH;
    case '°':   return DEGREE;

    case '?':   return QUESTION;
    case '!':   return EXCLAMATION;
    case ':':   return COLUMN;
    case '.':   return DOT;
    case ',':   return COMA;
    case ';':   return SEMI_COLUMN;
    case '\'':  return QUOTE;
    case '"':   return DOUBLE_QUOTE;

    case '(':   return OPENING_BRACE;
    case ')':   return CLOSING_BRACE;
    case '{':   return OPENING_CURLY_BRACE;
    case '}':   return CLOSING_CURLY_BRACE;
    case '[':   return OPENING_BRACKET;
    case ']':   return CLOSING_BRACKET;

    default:
        // Unrecognized character.
        return NULL_KEY;
    }
}

const ResourceString IODevice::GetLocation(void) const
{
    return ResourceString();
}

const bool IODevice::WouldInput(const IODevice& CLI_Device) const
{
    return (& CLI_Device == this);
}
