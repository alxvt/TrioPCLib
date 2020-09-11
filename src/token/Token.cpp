/* ***********************************************************************
* Project: Controller DLL
* Author: smartin
***********************************************************************
*/

#include "StdAfx.h"

/*---------------------------------------------------------------------
-- macros
---------------------------------------------------------------------*/
#ifdef _DEBUG
    #undef THIS_FILE
static char THIS_FILE[]=__FILE__;
    #define new DEBUG_NEW
#endif

#if defined(_DEBUG) && defined(_LONG_COMMAND_TIMEOUTS)
    #define TKN_GET_TOKEN_TABLE_TIMEOUT 1000000
    #define TKN_NORMAL_COMMAND_TIMEOUT  1000000
    #define TKN_LONG_COMMAND_TIMEOUT    6000000
#else
    #define TKN_GET_TOKEN_TABLE_TIMEOUT 10
    #define TKN_NORMAL_COMMAND_TIMEOUT  10
    #define TKN_LONG_COMMAND_TIMEOUT    60
#endif

#define TOKEN_RECEIVE_SLEEP_TIME        1
#define MIN_VERSION_ESCAPE 1.51

#define TOKENTABLE      0
#define MAX_COMMANDS    1000

/*---------------------------------------------------------------------
-- standard includes
---------------------------------------------------------------------*/
#include <float.h>

/*---------------------------------------------------------------------
-- project includes
---------------------------------------------------------------------*/
#include "../Controller/ControllerLib.h"
#include "Token.h"

/*---------------------------------------------------------------------
-- data types
---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
-- function prototypes
---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
-- global variables
---------------------------------------------------------------------*/
extern "C" {
    long g_lFlushBeforeWrite = 0;
    long g_lLastError = 0;
}
const int CToken::m_knMaxNameLength = 32;

/*---------------------------------------------------------------------
-- implementation
---------------------------------------------------------------------*/
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

#ifndef _WIN32
bool CToken::CMap::map_less_than::operator() (const CString& s1, const CString& s2) const
{
    return strcmp(s1, s2) < 0;
}

BOOL CToken::CMap::Lookup(const CString &key, CToken::token_t & rValue) const
{
    map_type::const_iterator it;

    it = _map.find(key);

    if (it == _map.end())
    {
        return FALSE;
    }
    else
    {
        rValue = it->second;
        return TRUE;
    }
}

void CToken::CMap::SetAt(const CString &key, const CToken::token_t& newValue)
{
    std::pair<map_type::iterator, bool> ret;

    ret = _map.insert(std::pair<CString, CToken::token_t>(key, newValue));
    if (ret.second == false)
    {
        fprintf(stderr, "failed to insert <%s,%s> into map\n", (LPCTSTR)key, newValue.Name);
    }
#if 0
    else
    {
        fprintf(stderr, "inserted <%s,%s> into map\n", (LPCTSTR)key, newValue.Name);
    }
#endif
}
#endif

#if !defined(__GNUC__)
#pragma warning (disable : 4355 4996)
#endif
CToken::CToken(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_ControllerClass)
    : m_ControllerCapabilities(this)
    , m_ControllerClass(p_ControllerClass)
    , m_fireEventClass(p_fireEventClass)
    , m_fireEventFunction(p_fireEventFunction)
	, m_protocol_error(FALSE)
{
    ZERO.Type = 'S'; ZERO.Token = 200;
    UNKNOWN.Type = 'S'; UNKNOWN.Token = 0;
    EXTENDED_TOKEN.Type = 'S'; EXTENDED_TOKEN.Token = 0;
}

CToken::~CToken()
{

}

BOOL CToken::AppendToken(char *tokenline, size_t tokenline_size, size_t &tp, const token_t &token) const
{
    // if this is an extended token then store
    if (EXTENDED_TOKEN.Token && (token.Type == 'C') && (token.Token >= EXTENDED_TOKEN.Token))
    {
        if (tp >= tokenline_size - 1)
            return FALSE;
        tokenline[tp++] = EXTENDED_TOKEN.Token;
        tokenline[tp++] = token.Token - EXTENDED_TOKEN.Token;
    }
    // this is not an extended token
    else
    {
        if (tp >= tokenline_size)
            return FALSE;
        tokenline[tp++] = token.Token;
    }

    return TRUE;
}

BOOL CToken::AppendToken_SystemVariable(char *tokenline, size_t tokenline_size, size_t &tp, const token_t &token) const
{
    // sanity check
    if (token.Type != 'V')
        return FALSE;
    if ((tp + 1) >= tokenline_size)
        return FALSE;

    // get the system variable token
    int system_variable_number = token.Token;
    token_t system_variable;
    if (system_variable_number > 255)
    {
        if(!m_TokenMap.Lookup(_T("SYSTEMVARIABLE2"), system_variable))
            return FALSE;
        system_variable_number -= 256;
    }
    else
        if(!m_TokenMap.Lookup(_T("SYSTEMVARIABLE"), system_variable))
            return FALSE;

    // store tokens
    tokenline[tp++] = system_variable.Token;
    tokenline[tp++] = system_variable_number;

    return TRUE;
}

void CToken::AtlTrace(const char *format, ...) const
{
    // create ASCII message
    char debugString[256];
    va_list argList;
    va_start(argList, format);
    _vsnprintf(debugString, sizeof(debugString), format, argList);
    va_end(argList);

#ifdef UNICODE
    wchar_t unicodeDebugString[256];
    AtlA2WHelper(unicodeDebugString, debugString, sizeof(unicodeDebugString));
    m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, wcsdup(unicodeDebugString));
#else
    m_fireEventFunction(m_fireEventClass, EVENT_MESSAGE, 0, strdup(debugString));
#endif
}

BOOL CToken::GetTokenTable()
{
    // local data
    token_t         token = { 0, 0, "", { {0} } };
    unsigned char   TokenTable[] = {TOKENTABLE, (unsigned char)ZERO.Token};
    char            EndOfTable[] = "EOT]\r\n";
    char            EndOfLine[] = "\r\n";
    int             Status = 0,Compare = 0,TokenLen = 0;
    BOOL            Negative = FALSE;
    CTime           timeStart;

    // send the tokentable command
    if (!ControllerFlush(m_ControllerClass, CHANNEL_REMOTE))
        return FALSE;
    if (!Write((char *)TokenTable,sizeof(TokenTable)))
        return FALSE;

    // start the timeout
    timeStart = CTime::GetCurrentTime();

    // parse the response
    size_t   BytesRead = 0;
    do
    {
        // local data
        char    Buffer[100];
        size_t   BytesRead;
        size_t   BytesProcessed;
        BOOL    XFlag = FALSE;

        // read data
        BytesRead = sizeof(Buffer);
        // if we can't read data then fail
        if(!ControllerRead(m_ControllerClass, CHANNEL_REMOTE, Buffer, &BytesRead)) Status = -2;
        // if there was no data to read then just sleep for a while
        else if(!BytesRead) Sleep(TOKEN_RECEIVE_SLEEP_TIME);
        // we read data so reset the timeout
        else timeStart = CTime::GetCurrentTime();

        // process the read data
        for (BytesProcessed = 0;BytesProcessed < BytesRead;BytesProcessed++)
        {
            XFlag = TRUE;
            // process this character
            switch (Status)
            {
            case 0: // start of line
                // reset data
                Compare = 0;
                token.Token = 0;
                token.Type = 0;

                // process this character
                switch (Buffer[BytesProcessed])
                {
                case '[':   token.Type = 0;   Status = 2;    break;  // start/end of table
                case 'C':   token.Type = 'C'; Status = 10;   break;  // command
                case 'S':   token.Type = 'S'; Status = 20;   break;  // special token
                case 'V':   token.Type = 'V'; Status = 30;   break;  // system variable
                case 'E':   token.Type = 'E'; Status = 20;   break;  // token exceptions
                case 'M':   token.Type = 'M'; Status = 40;   break;  // modifier
                case 'T':   token.Type = 'T'; Status = 50;   break;  // constant
                default:    token.Type = 0;   Status = 1;    break;  // unknown
                }
                break;
            case 1: // skip to end of line
                // if this is the end of table
                if (EndOfLine[Compare] == Buffer[BytesProcessed])
                {
                    // if this is the end of the string then done
                    if (!EndOfLine[++Compare])
                        Status = 0;
                }
                // this is not part of the compare so reset the count
                else
                    Compare = 0;
                break;
            case 2: // start/end of table
                // if this is the end of table
                if (EndOfTable[Compare] == Buffer[BytesProcessed])
                {
                    // if this is the end of the string then done
                    if (!EndOfTable[++Compare])
                        Status = -1;
                }
                // if this is the end of the section then reset status
                else if (']' == Buffer[BytesProcessed])
                {
                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                }
                // this is not part of the compare so reset the count
                else
                    Compare = 0;
                break;
            case 10: // command - token number
                // if this is a digit then
                if (isdigit(Buffer[BytesProcessed]))
                    token.Token = (token.Token * 10 + Buffer[BytesProcessed] - '0');
                // this is the start of the token name
                else
                {
                    // store this character
                    token.Name[0] = Buffer[BytesProcessed];

                    // reset data
                    TokenLen = 1;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 11: // command - name
                // if this is not a comma then store as part of name
                if (',' != Buffer[BytesProcessed])
                    token.Name[TokenLen++] = Buffer[BytesProcessed];
                // this is the end of name so get min quotes
                else
                {
                    // terminate token
                    token.Name[TokenLen] = 0;

                    // initialise min quote count
                    token.Command.MinQuoteParameters = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 12: // command - min quote parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.MinQuoteParameters = token.Command.MinQuoteParameters * 10 +
                                                       Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get min numeric parameters
                else
                {
                    // initialise max quote parameter count
                    token.Command.MaxQuoteParameters = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 13: // command - max quote parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.MaxQuoteParameters = token.Command.MaxQuoteParameters * 10 +
                                                       Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get max numeric parameters
                else
                {
                    // initialise min numeric parameter count
                    token.Command.MinNumericParameters = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 14: // command - min numeric parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.MinNumericParameters = token.Command.MinNumericParameters * 10 +
                                                         Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get max numeric parameters
                else
                {
                    // initialise max numeric parameter count
                    token.Command.MaxNumericParameters = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 15: // command - max numeric parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.MaxNumericParameters = token.Command.MaxNumericParameters * 10 +
                                                         Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get modifier set
                else
                {
                    // initialise modifier set
                    token.Command.ModifierSet = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 16: // command - modifier set
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.ModifierSet = token.Command.ModifierSet * 10 +
                                                Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get exception
                else
                {
                    // initialise exception
                    token.Command.Exception = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 17: // command - exception
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Command.ModifierSet = token.Command.ModifierSet * 10 +
                                                Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get exception
                else
                {
                    // initialise flags
                    token.Command.Flags = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 18:
                // process this flag
                switch (Buffer[BytesProcessed])
                {
                case '>':
                    // store the token
                    m_TokenMap.SetAt(CString(token.Name),token);

                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                    break;
                case 'c':   token.Command.Flag.IsCommand = 1;      break;
                case 'o':   token.Command.Flag.IsOperator = 1;     break;
                case 'f':   token.Command.Flag.IsFunction = 1;     break;
                case 'b':   token.Command.Flag.UsesBrackets = 1;   break;
                case 'd':   token.Command.Flag.IsDisabled = 1;     break;
                case 'e':   token.Command.Flag.IsPC = 1;           break;
                }

                // done
                break;
            case 20: // special - token number
                // if this is a digit then
                if (isdigit(Buffer[BytesProcessed]))
                    token.Token = (token.Token * 10 + Buffer[BytesProcessed] - '0');
                // this is the start of the token name
                else
                {
                    // store this character
                    token.Name[0] = Buffer[BytesProcessed];

                    // reset data
                    TokenLen = 1;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 21: // special - name
                // if this is not the end of the line then store as part of name
                if ('\r' != Buffer[BytesProcessed])
                    token.Name[TokenLen++] = Buffer[BytesProcessed];
                // this is the end of name so get min quotes
                else
                {
                    // terminate token
                    token.Name[TokenLen] = 0;

                    // reset status
                    Status++;

                    // store the token
                    m_TokenMap.SetAt(CString(token.Name),token);
                }

                // done
                break;
            case 22: // special - end
                // if this the end of the line then reset status
                if ('\n' == Buffer[BytesProcessed])
                    Status = 0;
                // this is not the end of line so wait for end of line
                else
                {
                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                }

                // done
                break;
            case 30: // variable - token number
                // if this is a digit then
                if (isdigit(Buffer[BytesProcessed]))
                    token.Token = (token.Token * 10 + Buffer[BytesProcessed] - '0');
                // this is the start of the token name
                else
                {
                    // store this character
                    token.Name[0] = Buffer[BytesProcessed];

                    // reset data
                    TokenLen = 1;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 31: // variable - name
                // if this is not the end of the token then store as part of name
                if (',' != Buffer[BytesProcessed])
                    token.Name[TokenLen++] = Buffer[BytesProcessed];
                // this is the end of name so get modifier set
                else
                {
                    // terminate token
                    token.Name[TokenLen] = 0;

                    // reset the modiier set
                    token.SysVar.ModifierSet = 0;

                    // reset status
                    Status++;
                }

                // done
                break;
            case 32: // variable - modifier set
                // if this is a digit then store
                if (isdigit(Buffer[BytesProcessed]))
                    token.SysVar.ModifierSet = token.SysVar.ModifierSet * 10 + Buffer[BytesProcessed] - '0';
                // this is the start of the flags
                else
                {
                    // store this character
                    token.SysVar.Flags = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 33:
                // process this flag
                switch (Buffer[BytesProcessed])
                {
                case '>':
                    // store the token
                    m_TokenMap.SetAt(CString(token.Name),token);

                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                    break;
                case 'w':   token.SysVar.Flag.IsWrite = 1;         break;
                case 'd':   token.SysVar.Flag.IsDisabled = 1;      break;
                case 'e':   token.SysVar.Flag.IsPC = 1;            break;
                }

                // done
                break;
            case 40: // modifier - token number
                // if this is a digit then
                if (isdigit(Buffer[BytesProcessed]))
                    token.Token = (token.Token * 10 + Buffer[BytesProcessed] - '0');
                // this is the start of the token name
                else
                {
                    // store this character
                    token.Name[0] = Buffer[BytesProcessed];

                    // reset data
                    TokenLen = 1;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 41: // modifier - name
                // if this is not the end of the token then store as part of name
                if (',' != Buffer[BytesProcessed])
                    token.Name[TokenLen++] = Buffer[BytesProcessed];
                // this is the end of name so get min numeric parameters
                else
                {
                    // terminate token
                    token.Name[TokenLen] = 0;

                    // reset the modiier set
                    token.Modifier.MinNumericParameters = 0;

                    // reset status
                    Status++;
                }

                // done
                break;
            case 42: // modifier - min numeric parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Modifier.MinNumericParameters = token.Modifier.MinNumericParameters *
                                                          10 + Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get max numeric parameters
                else
                {
                    // initialise max numeric parameter count
                    token.Modifier.MaxNumericParameters = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 43: // modifier - max numeric parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Modifier.MaxNumericParameters = token.Modifier.MaxNumericParameters *
                                                          10 + Buffer[BytesProcessed] - '0';
                }
                // this is the end of name so get modifier set
                else
                {
                    // initialise modifier set
                    token.Modifier.ModifierMask = 0;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 44: // modifier - modifier mask
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Modifier.ModifierMask = token.Modifier.ModifierMask * 10 +
                                                  Buffer[BytesProcessed] - '0';
                }
                // this is the end of the token
                else
                {
                    // store the token
                    m_TokenMap.SetAt(CString(token.Name),token);

                    // set next status
                    Status++;
                }

                // done
                break;
            case 45:
                // if this the end of the line then reset status
                if ('\n' == Buffer[BytesProcessed])
                    Status = 0;
                // this is not the end of line so wait for end of line
                else
                {
                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                }

                // done
                break;
            case 50: // constant - token number
                // if this is a digit then
                if (isdigit(Buffer[BytesProcessed]))
                    token.Token = (token.Token * 10 + Buffer[BytesProcessed] - '0');
                // this is the start of the token name
                else
                {
                    // store this character
                    token.Name[0] = Buffer[BytesProcessed];

                    // reset data
                    TokenLen = 1;

                    // set next status
                    Status++;
                }

                // done
                break;
            case 51: // constant - name
                // if this is not the end of the token then store as part of name
                if (',' != Buffer[BytesProcessed])
                    token.Name[TokenLen++] = Buffer[BytesProcessed];
                // this is the end of name so get min numeric parameters
                else
                {
                    // terminate token
                    token.Name[TokenLen] = 0;

                    // reset the constant value
                    token.Constant.Value = 0.0;
                    Negative = FALSE;

                    // reset status
                    Status++;
                }

                // done
                break;
            case 52: // modifier - min numeric parameters
                // if this is not a comma then store as part of name
                if (isdigit(Buffer[BytesProcessed]))
                {
                    token.Constant.Value = token.Constant.Value * 10.0 +
                                           (Buffer[BytesProcessed] - '0');
                }
                // if this is the negative sign then this is a negative number
                else if ('-' == Buffer[BytesProcessed])
                    Negative = TRUE;
                // this is the end of name so terminate
                else if ('.' != Buffer[BytesProcessed])
                {
                    // terminate the constant value
                    token.Constant.Value /= (Negative ? -10000.0 : 10000.0);

                    // store the token
                    m_TokenMap.SetAt(CString(token.Name),token);

                    // set next status
                    Status++;
                }

                // done
                break;
            case 53:
                // if this the end of the line then reset status
                if ('\n' == Buffer[BytesProcessed])
                    Status = 0;
                // this is not the end of line so wait for end of line
                else
                {
                    // reset the compare count
                    Compare = 0;

                    // reset the status
                    Status = 1;
                }

                // done
                break;
            } // switch (Status)
        } // for(BytesProcessed = 0;BytesProcessed < BytesRead;BytesProcessed++)

        if (XFlag)
            XFlag = FALSE;

    } while (
        (Status >= 0) &&
        ((((CTime::GetCurrentTime() - timeStart).GetTotalSeconds() < TKN_GET_TOKEN_TABLE_TIMEOUT)) || BytesRead)
    );

    // update the value of the end of line token
    if (-1 == Status)
    {
        token_t token;

        if (m_TokenMap.Lookup(_T("ZERO"), token))
            ZERO.Token = token.Token;
        if (m_TokenMap.Lookup(_T("UNKNOWN"),token))
            UNKNOWN.Token = token.Token;
        if (m_TokenMap.Lookup(_T("EXTENDED_TOKEN"),token))
            EXTENDED_TOKEN.Token = token.Token;
    }

    // return success
    return(-1 == Status);
}

#if !defined(__GNUC__)
BOOL CToken::SetVariable(const wchar_t *Variable, double fValue)
{
    CStringA ASCIIVariable(Variable);
    return SetVariable(ASCIIVariable, fValue);
}
#endif

BOOL CToken::SetVariable(const char *Variable, double fValue)
{
    // local data
    token_t token;

    // if the token does not exist then fail
    if(!m_TokenMap.Lookup(CString(Variable),token))
        return FALSE;

    // if this is a system variable then set it
    if ('V' == token.Type)
    {
        // local data
        token_t         aux;
        size_t          tp = 0;
        char            tokenline[100];
        double          fError;

        // store the variable token
        AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

        // store the assign token
        if(!m_TokenMap.Lookup(_T("ASSIGN"),aux))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, aux);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
            return FALSE;

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if(!Write(tokenline,tp))
            return FALSE;

        // get the response
        if (!GetResponse(NULL,&fError))
            return FALSE;

        // if there was an error then fail
        if (fError != 0.0)
            return FALSE;

        // success
        return TRUE;
    }
    // this is not a system variable so check if it is a VR variable
    else
        return FALSE;
}

#if !defined(__GNUC__)
BOOL CToken::GetVariable(const wchar_t *Variable, double *pfValue)
{
    CStringA ASCIIVariable(Variable);
    return GetVariable(ASCIIVariable, pfValue);
}
#endif

BOOL CToken::GetVariable(const char *Variable, double *pfValue)
{
    // local data
    token_t token;
    char    tokenline[100];

    // if the token does not exist then fail
    if(!m_TokenMap.Lookup(CString(Variable), token))
        return FALSE;

    // if this is a system variable then set it
    if ('V' == token.Type)
    {
        // local data
        size_t          tp = 0;
        double          fError;

        // store the SYSTEMVARIABLE token
        AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if (!Write(tokenline,tp))
        {
            AtlTrace("Error writing GetVariable %s\n", Variable);
            return FALSE;
        }

        // get the response
        if (!GetResponse(pfValue,&fError))
        {
            AtlTrace("Error GetResponse GetVariable %s\n", Variable);
            return FALSE;
        }

        // if there was an error then fail
        if (fError != 0.0)
        {
            AtlTrace("Error %f GetVariable %s\n", fError, Variable);
            return FALSE;
        }

        // success
        return TRUE;
    }

    // return failure
    return FALSE;
}

#if !defined(__GNUC__)
BOOL CToken::ArrayCommand(const wchar_t *Command, int Elements, const double afArray[], double *pfReturnValue)
{
    CStringA ASCIICommand(Command);
    return ArrayCommand(ASCIICommand, Elements, afArray, pfReturnValue);
}
#endif

BOOL CToken::ArrayCommand(const char *Command, int Elements, const double afArray[],double *pfReturnValue)
{
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    int             Element;
    double          fError;
    BOOL            OK = TRUE;

    // store the move token
    if (!m_TokenMap.Lookup(CString(Command),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if there are parameters then store them
    if (Elements)
    {
        // store the OPENBRACKETS token
        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store all the parameters
        for (Element = 0; Element < Elements; Element++)
        {
            // if this is not the first axis then store the COMMA token
            if (Element > 0)
            {
                // store the comma token
                if (!m_TokenMap.Lookup(_T("COMMA"),token))
                    return FALSE;
                AppendToken(tokenline, sizeof(tokenline), tp, token);
            }

            // store the number
            if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, afArray[Element]))
                return FALSE;
        } // for (Element = 0; Element < Elements && OK; Element++)

        // store the close brackets token
        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    fError = 0.0;
    OK = GetResponse(pfReturnValue,&fError);

    // return success
    return OK && (fError == 0.0);
}

#if !defined(__GNUC__)
BOOL CToken::Command(const wchar_t *Command, double *ReturnValue)
{
    CStringA ASCIICommand(Command);
    return this->Command(ASCIICommand, ReturnValue);
}
#endif

BOOL CToken::Command(const char *Command, double *ReturnValue)
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError;

    // store the move token
    if(!m_TokenMap.Lookup(CString(Command),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,2))
        return FALSE;

    // get the response
    if(!GetResponse(ReturnValue,&fError))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::ProcessCommand(const wchar_t *Command, const wchar_t *Program, double fProcess, double *pfReturnValue)
{
    CStringA ASCIICommand(Command);
    CStringA ASCIIProgram(Program);
    return ProcessCommand(ASCIICommand, ASCIIProgram, fProcess, pfReturnValue);
}
#endif

BOOL CToken::ProcessCommand(const char *Command, const char *Program, double fProcess, double *pfReturnValue)
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    int             i;
    double          fError;

    // store the move token
    if(!m_TokenMap.Lookup(CString(Command),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program name
    for (i = 0;Program[i];i++)
        tokenline[tp++] = Program[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if the process was specified then use it
    if (-1 != fProcess)
    {
        // store the COMMA token
        if(!m_TokenMap.Lookup(_T("COMMA"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fProcess))
            return FALSE;
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(pfReturnValue,&fError))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::MotionCommand(const wchar_t *Command, int Elements, const double afArray[],int Base, double *pfReturnValue)
{
    CStringA ASCIICommand(Command);
    return MotionCommand(ASCIICommand, Elements, afArray, Base, pfReturnValue);
}
#endif

BOOL CToken::MotionCommand(const char *Command, int Elements, const double afArray[],int Base, double *pfReturnValue) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    int             Element;
    double          fError;

    // store the move token
    if(!m_TokenMap.Lookup(CString(Command),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if there are parameters then store them
    if (Elements)
    {
        // store the OPENBRACKETS token
        if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store all the parameters
        for (Element = 0;Element < Elements;Element++)
        {
            // if this is not the first axis then store the COMMA token
            if (Element)
            {
                // store the comma token
                if(!m_TokenMap.Lookup(_T("COMMA"),token))
                    return FALSE;
                AppendToken(tokenline, sizeof(tokenline), tp, token);
            }

            // store the number
            if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, afArray[Element]))
                return FALSE;
        }

        // store the close brackets token
        if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
    }

    // if the base is specified then append it
    if (-1 != Base)
    {
        // store the modifier token
        if(!m_TokenMap.Lookup(_T("MODIFIER"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the axis token
        if(!m_TokenMap.Lookup(_T("AXIS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Base))
            return FALSE;

        // store the close brackets token
        if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(pfReturnValue,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

BOOL CToken::GetResponse(double *pfValue,double *pfError,BOOL bTimeout)
{
    double value;

    // read the number
	if (!m_ControllerCapabilities.ReadNumber(m_ControllerClass, bTimeout, &value))
	{
		m_protocol_error = TRUE;
		return FALSE;
	}

    // extract the ieee value
    if (pfValue) *pfValue = value;

    // read the number
	if (!m_ControllerCapabilities.ReadNumber(m_ControllerClass, bTimeout, &value))
	{
		m_protocol_error = TRUE;
		return FALSE;
	}

    // extract the ieee value
    g_lLastError = (long)(*pfError = value);

    // return success status
    return TRUE;
}

BOOL CToken::GetCommand(short Channel,short *pnValue)
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError,fReturnValue;

    // store the move token
    if(!m_TokenMap.Lookup(_T("GET"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("HASH"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Channel))
        return FALSE;

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if we have the TRIOTESTVARIAB and it is a SYSTEMVARIABLE (v) then use it
    if (m_TokenMap.Lookup(_T("TRIOPCTESTVARIAB"), token) && (token.Type == 'V'))
    {
        // store the named variable token
        AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);
    }
    else
    {
        // store the named variable token
        if(!m_TokenMap.Lookup(_T("NAMEDVARIABLE"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the variable number
        tokenline[tp++] = 0;
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(&fReturnValue,&fError,FALSE))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // get the named variable
    if(!GetVariable("TRIOPCTESTVARIAB", &fReturnValue))
        return FALSE;

    // store the value
    *pnValue = (short)fReturnValue;

    // return success
    return TRUE;
}

BOOL CToken::GetNamedVariable(int Variable, double *pfValue)
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    unsigned char   chr;
    double          fError, value;

    // store the LOOKUP token
    if(!m_TokenMap.Lookup(_T("LOOKUP"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 0.0))
        return FALSE;

    // store the comma token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the NAMEDVARIABLE token
    if(!m_TokenMap.Lookup(_T("NAMEDVARIABLE"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the variable number
    tokenline[tp++] = (char)Variable;

    // store the end of expression token
    if(!m_TokenMap.Lookup(_T("EOX"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the close brackets token
    if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // read data
    if (!ReadByte(&chr, TKN_NORMAL_COMMAND_TIMEOUT))
        return FALSE;

    // process this format
    switch (chr)
    {
    case 0:
        // read the number
        if (!m_ControllerCapabilities.ReadNumber(m_ControllerClass, TRUE, &value))
            return FALSE;
        break;
    }

    // return the ieee value
    *pfValue = value;

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

BOOL CToken::InputCommand(short nChannel, double *pfValue)
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError;

    // store the INPUT token
    if(!m_TokenMap.Lookup(_T("INPUT"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the HASH token
    if(!m_TokenMap.Lookup(_T("HASH"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, nChannel))
        return FALSE;

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if we have the TRIOTESTVARIAB and it is a SYSTEMVARIABLE (v) then use it
    if (m_TokenMap.Lookup(_T("TRIOPCTESTVARIAB"), token) && (token.Type == 'V'))
    {
        // store the named variable token
        AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);
    }
    else
    {
        // store the named variable token
        if(!m_TokenMap.Lookup(_T("NAMEDVARIABLE"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the variable number
        tokenline[tp++] = 0;
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(pfValue,&fError,FALSE))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // get the named variable
    if(!GetVariable("TRIOPCTESTVARIAB", pfValue))
        return FALSE;

    // return success
    return TRUE;
}

BOOL CToken::KeyCommand(short nChannel, short *pValue) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError,fValue;

    // store the KEY token
    if(!m_TokenMap.Lookup(_T("KEY"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the HASH token
    if(!m_TokenMap.Lookup(_T("HASH"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, nChannel))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(&fValue,&fError,FALSE))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // set the return value
    *pValue = (short)fValue;

    // return success
    return TRUE;
}

BOOL CToken::LinputCommand(short nChannel, short nStartVr) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError;

    // store the INPUT token
    if(!m_TokenMap.Lookup(_T("LINPUT"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the HASH token
    if(!m_TokenMap.Lookup(_T("HASH"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, nChannel))
        return FALSE;

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the named variable token
    if(!m_TokenMap.Lookup(_T("VR"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, nStartVr))
        return FALSE;

    // store the close brackets token
    if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(NULL,&fError,FALSE))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::ExecuteCommand(const wchar_t *Command) 
{
    CStringA ASCIICommand(Command);
    return ExecuteCommand(ASCIICommand);
}
#endif

BOOL CToken::ExecuteCommand(const char *Command) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0,i;
    double          fError;

    // store the EXECUTE token
    if(!m_TokenMap.Lookup(_T("EXECUTE"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the command
    for (i=0;Command[i];i++) tokenline[tp++] = Command[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(NULL,&fError,FALSE))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

BOOL CToken::SetVrCommand(int Variable, double fValue) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError;

    // store the VR token
    if(!m_TokenMap.Lookup(_T("VR"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Variable))
        return FALSE;

    // store the CLOSEBRACKETS token
    if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the ASSIGN token
    if(!m_TokenMap.Lookup(_T("ASSIGN"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // return success
    return TRUE;
}

BOOL CToken::GetNamedVariable(int Variable, int Process, double *pfReturnValue) 
{
    // local data
    token_t token;
    char    tokenline[100];
    size_t          tp = 0;
    unsigned char   chr;
    double          fError, value;

    // store the LOOKUP token
    if(!m_TokenMap.Lookup(_T("LOOKUP"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 2.0))
        return FALSE;

    // store the comma token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Variable))
        return FALSE;

    // store the close brackets token
    if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the MODIFIER token
    if(!m_TokenMap.Lookup(_T("MODIFIER"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PROC token
    if(!m_TokenMap.Lookup(_T("PROC"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Process))
        return FALSE;

    // store the close brackets token
    if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // read data
    if (!ReadByte(&chr, TKN_NORMAL_COMMAND_TIMEOUT))
        return FALSE;

    // process this format
    switch (chr)
    {
    case 0:
        // read the number
        if (!m_ControllerCapabilities.ReadNumber(m_ControllerClass, TRUE, &value))
            return FALSE;
        break;
    }

    // return the ieee value
    *pfReturnValue = value;

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if(fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

BOOL CToken::SetTable(long lStartPosition, long lElements, const double afValues[]) 
{
    if (ControllerCanMemory(m_ControllerClass))
        return ControllerSetTable(m_ControllerClass, lStartPosition, lElements, afValues);
    else if (m_ControllerCapabilities.GetControllerVersion() > 2.01351)
        return SetTableValuesCommand(lStartPosition, lElements, afValues);
    else
        return SetTableCommand(lStartPosition, lElements, afValues);
}

BOOL CToken::SetTableValuesCommand(long lStartPosition, long lElements, const double afValues[]) 
{
	BOOL			rc = FALSE;
    token_t         token;
    char            tokenline[80];
    size_t          tp = 0;
    double          fError;
    long            lElement, lElementSize, lNumberBufferSize;
	size_t			lNumberBufferPosition;
	char *			number_buffer;

	// create a buffer to store all the numbers
	lElementSize = (m_ControllerCapabilities.GetFloatingPointFormat() == CControllerCapabilities::tms320FloatingPointFormat) ? 6 : 10;
	lNumberBufferSize = lElementSize * lElements;
	number_buffer = new char[lNumberBufferSize];
	if (!number_buffer)
		goto done;

	// build the number buffer
    for (lNumberBufferPosition = lElement = 0; lElement < lElements; lElement++)
    {
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, number_buffer, lNumberBufferSize, lNumberBufferPosition, *afValues++))
            goto done;
	}

    // store the TABLEVALUES token
    if(!m_TokenMap.Lookup(_T("TABLEVALUES"),token))
        goto done;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        goto done;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the start position
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, lStartPosition))
        goto done;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"),token))
        goto done;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the stop position
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, lStartPosition + lElements - 1))
        goto done;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"),token))
        goto done;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the format
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 2))
        goto done;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        goto done;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        goto done;

    // get the response
    if (!GetResponse(NULL,&fError))
        goto done;

    // if there was an error then fail
    if (fError != 0.0)
        goto done;

    // send numbers
    if (!Write(number_buffer, lNumberBufferPosition, FALSE))
        goto done;

    // success if we get here
	rc = TRUE;

done:
	if (number_buffer)
		delete[] number_buffer;

    return rc;
}

BOOL CToken::SetTableCommand(long lStartPosition, long lElements, const double afValues[]) 
{
    // local data
    const int float_size = m_ControllerCapabilities.GetFloatingPointFormat() == m_ControllerCapabilities.ieee64FloatingPointFormat ? 8 : 4;
    const int step_size = (80 - 3)/(float_size + 3);
    token_t         token;
    // <TABLE><(><NUMBER>........<EOX><,>...<NUMBER>........<EOX><)><ZERO>
    //            ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
    //            CAN REPEAT UP TO STEP_SIZE TIMES
    char            tokenline[80];
    size_t          tp;
    long            lProcessed,lBlockSize,j;
    double          fError=0.0;

    // process all the variables
    for (lProcessed = 0;lProcessed < lElements;)
    {
        // reset the token pointer
        tp = 0;

        // store the TABLE token
        if(!m_TokenMap.Lookup(_T("TABLE"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, lStartPosition + lProcessed))
            return FALSE;

        // store the comma token
        if (!m_TokenMap.Lookup(_T("COMMA"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // calculate the block size
        lBlockSize = min(step_size,lElements - lProcessed);

        // process the remaining values
        for (j = 0;j < lBlockSize - 2;j++)
        {
            // store the number
            if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, afValues[lProcessed++]))
                return FALSE;

            // store the comma token
            if (!m_TokenMap.Lookup(_T("COMMA"),token))
                return FALSE;
            AppendToken(tokenline, sizeof(tokenline), tp, token);
        }

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, afValues[lProcessed++]))
            return FALSE;

        // store the CLOSEBRACKETS token
        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if (!Write(tokenline,tp))
            return FALSE;

        // get the response
        if (!GetResponse(NULL,&fError))
            return FALSE;

        // if there was an error then fail
        if (fError != 0.0)
            return FALSE;
    }

    // return success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::New(const wchar_t *Program) 
{
    CStringA ASCIIProgram(Program);
    return New(ASCIIProgram);
}
#endif

BOOL CToken::New(const char *Program) 
{
    // local data
    token_t token;
    char    tokenline[100];
    size_t  tp = 0,i;
    double  fError;

    // store the NEW token
    if(!m_TokenMap.Lookup(_T("NEW"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program name
    for (i = 0;Program[i];i++) tokenline[tp++] = Program[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the EOX token
    if(!m_TokenMap.Lookup(_T("EOX"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // ignore the response line
    if (!IgnoreLine(TKN_NORMAL_COMMAND_TIMEOUT)) return(FALSE);

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // return the success status
    return fError == 0.0;
}

#if !defined(__GNUC__)
BOOL CToken::Dir(CStringW &csDir, const wchar_t *szOption) 
{
    CStringA ASCIIDir;
    CStringA ASCIIOption(szOption ? szOption : L"");
    if (!Dir(ASCIIDir, ASCIIOption))
        return FALSE;
    csDir = ASCIIDir;
    return TRUE;
}
#endif

BOOL CToken::Dir(CStringA &csDir, const char *szOption) 
{
    // local data
    token_t token;
    char    tokenline[100];
    size_t  tp = 0;
    double  fError;
    BOOL    bDone = FALSE, bError = FALSE;
    CTime   timeStart = CTime::GetCurrentTime();

    // store the DIR token
    if(!m_TokenMap.Lookup(_T("DIR"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // append the option
    if (szOption && szOption[0])
    {
        // if this option does not fit then fail
        if ((strlen(szOption) + tp + 3) > sizeof(tokenline))
            return FALSE;

        // store option
        if(!m_TokenMap.Lookup(_T("QUOTES"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
        for (size_t i = 0;i < strlen(szOption);i++) tokenline[tp++] = szOption[i];
        AppendToken(tokenline, sizeof(tokenline), tp, token);
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the directory list
    CStringA csLine;
    do
    {
        // local data
        char    chr;

        // read data
        if (!ReadByte(&chr, TKN_NORMAL_COMMAND_TIMEOUT))
            bError = TRUE;

        // end of line?
        if (chr == '\n')
        {
            // end of directory?
            if (!_stricmp(csLine, "OK"))
                bDone = 1;
            else
            {
                csDir += csLine + chr;
                csLine.Empty();
            }
        }
        else if (chr != '\r')
            csLine += chr;
    } while
        (
               !bDone
            && !bError
            && ((CTime::GetCurrentTime() - timeStart).GetTotalSeconds() < TKN_NORMAL_COMMAND_TIMEOUT)
        );

    // if failed then abort
    if (!bDone)
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // return the success status
    return fError == 0.0;
}

#if !defined(__GNUC__)
BOOL CToken::Select(const wchar_t *Program) 
{
    CStringA ASCIIProgram(Program);
    return Select((const char *)ASCIIProgram);
}
#endif

BOOL CToken::Select(const char *Program) 
{
    // local data
    token_t token;
    char    tokenline[100],SelectedProgram[17];
    size_t  tp = 0,i = 0;
    int Status = 0;
    double  fError;
    BOOL    bDone = FALSE;

    // store the NEW token
    if(!m_TokenMap.Lookup(_T("SELECT"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program name
    for (i = 0;Program[i];i++) tokenline[tp++] = Program[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the EOX token
    if(!m_TokenMap.Lookup(_T("EOX"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the directory list
    i = 0;
    do
    {
        // local data
        char    chr;
        CStringA csLine;

        // read data
        if (!ReadByte(&chr, TKN_LONG_COMMAND_TIMEOUT)) Status = -2;

        // process this character
        switch (Status)
        {
        case 0:
            // if this character is valid and fits then store it
            if ((' ' != chr) && (i < (sizeof(SelectedProgram) - 1)))
                SelectedProgram[i++] = chr;
            // terminate the selected program
            else
            {
                // terminate the program
                SelectedProgram[i] = 0;

                // set the next status
                Status = 1;
            }
            break;
        case 1:
            // if this is the end of line then see what we have got
            if ('\n' == chr)
            {
                // if this is not a valid string then it must be the program name
                // so we are done
                if (strcmp("Compiling",SelectedProgram) &&
                    strcmp("Linking",SelectedProgram) &&
                    memcmp("Pass=",SelectedProgram,5))
                    bDone = TRUE;
                // get the next line
                else
                {
                    // reset the status
                    Status = 0;

                    // set the data
                    i=0;
                }
            }
        }
    } while (!bDone && (Status >= 0));

    // if the selected program is not correct then fail
    if (_stricmp(Program, SelectedProgram))
        return FALSE;

    // get the response
    if (!GetResponse(NULL, &fError))
        return FALSE;

    // return the success status
    return fError == 0.0;
}

#if !defined(__GNUC__)
BOOL CToken::InsertLine(const wchar_t *Program, short LineNumber, const wchar_t *Line) 
{
    CStringA ASCIIProgram(Program);
    CStringA ASCIILine(Line);
    return InsertLine(ASCIIProgram, LineNumber, ASCIILine);
}
#endif

BOOL CToken::InsertLine(const char *Program, short nLineNumber, const char *Line) 
{
    // local data
    token_t token;
    char    tokenline[120];
    CStringA csSelectedProgram;
    size_t  tp = 0,i = 0;
    double  fError;

    // get the currently selected program
    if(!GetSelected(csSelectedProgram))
        return FALSE;

    // if this is not the right program then select it
    if (_stricmp(csSelectedProgram, Program))
    {
        // select this program
        if(!Select(Program))
        return FALSE;
    }

    // store the EDPROG token
    if(!m_TokenMap.Lookup(_T("EDPROG"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program line
    for (i = 0;(Line[i]) && (i < 80);i++)
    {
        tokenline[tp++] = Line[i];
    }

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the EDPROG function number
    if (!m_ControllerCapabilities.StoreInteger(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 1))
        return FALSE;

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreInteger(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, nLineNumber))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // ignore the line that comes back
    if (!IgnoreLine(TKN_NORMAL_COMMAND_TIMEOUT))
        return FALSE;

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // return the success status
    return fError == 0.0;
}

BOOL CToken::GetSelected(CStringA &csSelectedProgram) 
{
    // local data
    token_t token;
    char    tokenline[100];
    size_t  tp = 0, Status = 0;
    double  fError;
    BOOL    bDone = FALSE;

    // store the NEW token
    if(!m_TokenMap.Lookup(_T("EDPROG"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the COMMA token
    if(!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 7))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the selected program name
    csSelectedProgram.Empty();
    do
    {
        // local data
        char    chr;

        // read data
        if (!ReadByte(&chr, TKN_NORMAL_COMMAND_TIMEOUT)) Status = -2;

        // process this character
        switch (Status)
        {
        case 0:
            // if this character is valid and fits then store it
            if ('\r' != chr) csSelectedProgram += chr;
            // terminate the selected program
            else Status = 1;
            break;
        case 1:
            // if this is the end of line then we have finished
            if ('\n' == chr) bDone = TRUE;
        }
    } while (!bDone && Status >= 0);
    if (!bDone)
        return FALSE;

    // get the response
    if(!GetResponse(NULL,&fError))
        return FALSE;

    // return the success status
    return fError == 0.0;
}

BOOL CToken::GetTable(long lStartPosition, long lValues, double *fValues) 
{
    if (ControllerCanMemory(m_ControllerClass))
        return ControllerGetTable(m_ControllerClass, lStartPosition, lValues, fValues);
    else
        return GetTableCommand(lStartPosition, lValues, fValues);
}

BOOL CToken::GetTableCommand(long lStartPosition, long lValues, double *fValues) 
{
    enum ETableValueScanState
    {
        eTVSS_Error = -1, eTVSS_Start = 0, eTVSS_RepeatCount,
        eTVSS_Value, eTVSS_InvalidValue
    };

    ETableValueScanState    evState;
    token_t token;
    char    tokenline[100], Buffer[256];
    size_t  tp, nStorePos;
    long    i, lRepeatCount = 0;
    double  fError = 0.0;
    CTime   start;
    size_t  BytesRead = 0,dwPos = 0;

    // reset the token pointer
    tp = 0;

    // store the TABLEVALUES token
    if (!m_TokenMap.Lookup(_T("TABLEVALUES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, lStartPosition))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, lStartPosition + lValues - 1))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 1))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline, tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL, &fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // process all the entries
    nStorePos = 0;
    i = 0;
    start = CTime::GetCurrentTime();
    evState = eTVSS_Start;
    do
    {
        // force ACK
        if (UNKNOWN.Token && ControllerGetPortType(m_ControllerClass) == port_type_ethernet)
            ControllerWrite(m_ControllerClass, CHANNEL_REMOTE, (char *)&UNKNOWN.Token, 1);

        // read data
        BytesRead = sizeof(Buffer);
        if (!ControllerRead(m_ControllerClass, CHANNEL_REMOTE, Buffer, &BytesRead))
            evState = eTVSS_Error;
        else if (!BytesRead)
            Sleep(TOKEN_RECEIVE_SLEEP_TIME);
        else
            start = CTime::GetCurrentTime();

        // process this buffer
        for (dwPos = 0; (dwPos < BytesRead) && (i < lValues) && (evState != eTVSS_Error); dwPos++)
        {
            if (Buffer[dwPos] != '\0')
            {
                // process this status
                switch (evState)
                {
                case eTVSS_Error:
                    break;
                case eTVSS_Start:
                    // if this is the start of a sequence then do it
                    if ('k' == Buffer[dwPos])
                    {
                        nStorePos = 0;
                        evState = eTVSS_RepeatCount;
                    }
                    // if this is a number then store
                    else if (isdigit(Buffer[dwPos]) || ('-' == Buffer[dwPos]) ||
                             ('+' == Buffer[dwPos]) || ('.' == Buffer[dwPos]))
                    {
                        nStorePos = 0;
                        lRepeatCount = 1;
                        tokenline[nStorePos++] = Buffer[dwPos];
                        evState = eTVSS_Value;
                    }
                    else if (Buffer[dwPos] == '#')
                    {
                        nStorePos = 0;
                        lRepeatCount = 1;
                        tokenline[nStorePos++] = Buffer[dwPos];
                        evState = eTVSS_InvalidValue;
                    }
                    else
                    {
                        // this is an error
                        switch (Buffer[dwPos])
                        {
                        case 0:
                        case 10:
                        case 13:
                            break;
                        default:
                            evState = eTVSS_Error;
                        }
                    }
                    break;
                case eTVSS_RepeatCount:
                    // if this is the comma then we are done
                    if (',' == Buffer[dwPos])
                    {
                        // store the repeat count
                        tokenline[nStorePos] = 0;
                        lRepeatCount = atol(tokenline);

                        // if the repeat count is invalid then fail
                        if (lRepeatCount <= 0)
                            evState = eTVSS_Error;
                        else
                        {
                            // repeat count is valid so set next status
                            nStorePos = 0;
                            evState = eTVSS_Value;
                        }
                    }
                    // if this is a digit then store it
                    else if (isdigit(Buffer[dwPos]) && (nStorePos < sizeof(tokenline)))
                        tokenline[nStorePos++] = Buffer[dwPos];
                    else
                        evState = eTVSS_Error;
                    break;
                case eTVSS_Value:
                    // if this is the comma then we are done
                    if (',' == Buffer[dwPos])
                    {
                        // local data
                        double fValue;
                        long j;
                        char *end;

                        // store the value
                        tokenline[nStorePos] = 0;
                        fValue = strtod(tokenline, &end);

                        // if the conversion aborted before the end of string then we have an invalid number
                        if (*end)
                            evState = eTVSS_Error;

                        // store this value
                        for (j = 0; (i < lValues) && (j < lRepeatCount); j++, i++)
                            fValues[i] = fValue;

                        // go back for more
                        evState = eTVSS_Start;
                    }
                    // if this valid character fits then store it
                    else if ((nStorePos < sizeof(tokenline)) && isprint(Buffer[dwPos]))
                        tokenline[nStorePos++] = Buffer[dwPos];
                    else
                        evState = eTVSS_Error;
                    break;
                case eTVSS_InvalidValue:
                    // if this is the comma then we are done
                    if (',' == Buffer[dwPos])
                    {
                        double fValue;
                        long j;

                        fValue = (float)(FLT_MAX / 1000.0f);

                        // store this value
                        for (j = 0; (i < lValues) && (j < lRepeatCount); j++, i++)
                            fValues[i] = fValue;

                        evState = eTVSS_Start;
                    }
                    break;
                } // switch (evState)
            } // if (Buffer[dwPos] != '\0')
        } // for (dwPos = 0; (dwPos < BytesRead) && (i < lValues) && (evState >= eTVSS_Error) ...
    } while (
            (i < lValues) &&
            ((CTime::GetCurrentTime() - start).GetTotalSeconds() < TKN_NORMAL_COMMAND_TIMEOUT) &&
            (evState != eTVSS_Error)
            );

    // if we failed then fail
    if ((evState == eTVSS_Error) || (i != lValues))
        return FALSE;

    // find the end of line
    while ((dwPos < BytesRead) && (Buffer[dwPos] != '\n')) dwPos++;

    // get trailing line end
    return(Buffer[dwPos] == '\n') ? TRUE : IgnoreLine(TKN_NORMAL_COMMAND_TIMEOUT);
}

BOOL CToken::Write(char Buffer[], size_t Length, const BOOL p_enableEscape /*= TRUE*/) const
{
    // local data
    size_t Count,i;
    BOOL bSuccess;

    // flush the communications channel
    // THE FOLLOWING MAKES THE SYSTEM MORE ROBUST BUT IT SLOWS COMMS DOWN **TREMENDOUSLY**. DO WE NEED IT
    // ##################################################################
    if (g_lFlushBeforeWrite) ControllerFlush(m_ControllerClass, CHANNEL_REMOTE);

    // escape comms
    if (p_enableEscape && m_ControllerCapabilities.IsCommunicationsEscaped() && ControllerIsEscapeRemoteChannel(m_ControllerClass))
    {
        // count the occurrences of the ZERO character in the buffer
        for (i = Count = 0;i < Length;i++) Count += (ZERO.Token == (UCHAR)Buffer[i]) || (27 == (UCHAR)Buffer[i]);

        // if there are too many occurrences and the version is correct and the controller interfaces requires them
        // then add the escape characters
        if (Count > 1)
        {
            // local data
            char *pchBuffer;

            // create the buffer
            pchBuffer = new char[Length + Count];
            if (!pchBuffer)
                return FALSE;

            // fill the buffer
            for (i = Count = 0;i < Length - 1;i++,Count++)
            {
                if ((ZERO.Token == (UCHAR)Buffer[i]) || (27 == (UCHAR)Buffer[i]))
                    pchBuffer[Count++] = 27;
                pchBuffer[Count] = Buffer[i];
            }
            pchBuffer[Count++] = ZERO.Token;

            // process this buffer
            bSuccess = ControllerWrite(m_ControllerClass, CHANNEL_REMOTE, pchBuffer, Count);

            // free the buffer
            delete pchBuffer;
        }
        else
            bSuccess = ControllerWrite(m_ControllerClass, CHANNEL_REMOTE, Buffer, Length);
    }
    else
        bSuccess = ControllerWrite(m_ControllerClass, CHANNEL_REMOTE, Buffer, Length);

    // return the success status
    return bSuccess;
}

BOOL CToken::IgnoreLine(int timeout) const
{
    // local data
    BOOL bDone = FALSE, bError = FALSE;
    CTime timeStart = CTime::GetCurrentTime();

    // ignore the line that comes back
    do
    {
        // local data
        char    chr;

        // read data
        if (!ReadByte(&chr, timeout)) bError = TRUE;
        else bDone = ('\n' == chr);
    } while (
            !bDone && !bError &&
            ((CTime::GetCurrentTime() - timeStart).GetTotalSeconds() < timeout)
            );

    return bDone;
}

BOOL CToken::MarkCommand(short Axis, short *Value, short MarkNumber /*= 0*/) 
{
    // local data
    token_t         token;
    char            tokenline[100];
    size_t          tp = 0;
    double          fError,fValue;
    const char      *MarkName = NULL;

    // Select command
    switch (MarkNumber)
    {
    case 0:
        MarkName = "MARK";
        break;
    case 1:
        MarkName = "MARKB";
        break;
    default:
        return FALSE;
    }

    // if the MARK token is a command then process as a command
    if(!m_TokenMap.Lookup(CString(MarkName),token))
        return FALSE;

    if ('C' == token.Type)
    {
        // store
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the modifier token
        if(!m_TokenMap.Lookup(_T("MODIFIER"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the HASH token
        if(!m_TokenMap.Lookup(_T("AXIS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if(!m_TokenMap.Lookup(_T("OPENBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Axis))
            return FALSE;

        // store the CLOSEBRACKETS token
        if(!m_TokenMap.Lookup(_T("CLOSEBRACKETS"),token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if(!Write(tokenline,tp))
            return FALSE;

        // get the response
        if(!GetResponse(&fValue,&fError,FALSE))
            return FALSE;

        // if there was an error then fail
        if(fError != 0.0)
            return FALSE;

        // set the return value
        *Value = (short)fValue;

        // return success
        return TRUE;
    }
    else
    {
        double dValue;
        if (GetVariable(MarkName, &dValue))
        {
            *Value = (short)dValue;
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }
}

BOOL CToken::Scope(BOOL bOn, long lSamplePeriod, long lTableStart, long lTableEnd, const CStringList &ParamList) 
{
    BOOL    OK = TRUE;

    try
    {
        token_t         token;
        size_t          tp = 0;
        char            tokenline[200];
        double          fError;

        // store the SCOPE token
        if (!m_TokenMap.Lookup(_T("SCOPE"), token))
            throw(1);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
            throw(2);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // On / Off
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, bOn ? 1.0f : 0.0f))
            throw(3);

        if (!m_TokenMap.Lookup(_T("COMMA"), token))
            throw(10);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // Sample Period
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)lSamplePeriod))
            throw(11);

        if (!m_TokenMap.Lookup(_T("COMMA"), token))
            throw(12);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // Table Start
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)lTableStart))
            throw(13);

        if (!m_TokenMap.Lookup(_T("COMMA"), token))
            throw(14);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // Table End
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)lTableEnd))
            throw(15);

        if (!m_TokenMap.Lookup(_T("COMMA"), token))
            throw(16);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // Parameters to be scoped
        POSITION    pos = ParamList.GetHeadPosition();
        CStringA    strParam;
        int         nParam = 0;
        while (pos != NULL && (nParam < 8))
        {
            strParam = ParamList.GetNext(pos);

            if (!TokenizeScopeParameter(tokenline, sizeof(tokenline), tp, strParam))
                throw(21);

            // Add comma if more parrameters
            if (pos != NULL)
            {
                if (!m_TokenMap.Lookup(_T("COMMA"), token))
                    throw(22);
                AppendToken(tokenline, sizeof(tokenline), tp, token);
            }

            ++nParam;
        } // while (pos != NULL && OK && (nParam < 8))

        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
            throw(30);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if (!Write((char*)tokenline, tp))
            throw(40);

        // get the response
        if (!GetResponse(NULL, &fError))
            throw(41);
        if (fError != 0.0f)
            throw(42);
    }
    catch (int c)
    {
        CStringA strOut;

        strOut.Format("Caught %d in Scope\n", c);
        OK = FALSE;
        AtlTrace(strOut);
    }

    return OK;
}

BOOL CToken::Scope(BOOL bOn) 
{
    BOOL    OK = TRUE;

    try
    {
        token_t         token;
        size_t          tp = 0;
        char            tokenline[100];
        double          fError;

        // store the SCOPE token
        if (!m_TokenMap.Lookup(_T("SCOPE"), token))
            throw(1);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
            throw(2);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // On / Off
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, bOn ? 1.0f : 0.0f))
            throw(3);

        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
            throw(30);
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

        // send line
        if (!Write((char*)tokenline, tp))
            throw(40);

        // get the response
        if (!GetResponse(NULL, &fError))
            throw(41);
        if (fError != 0.0f)
            throw(42);
    }
    catch (int c)
    {
        CStringA strOut;

        strOut.Format("Caught %d in Scope\n", c);
        OK = FALSE;
        AtlTrace(strOut);
    }

    return OK;
}

BOOL CToken::Trigger() 
{
    BOOL    OK = TRUE;
    token_t token;
    size_t  tp = 0;
    char    tokenline[10];
    double  Error;

    // store the TRIGGER token
    if (m_TokenMap.Lookup(_T("TRIGGER"), token))
    {
        AppendToken(tokenline, sizeof(tokenline), tp, token);
        // terminate the line
        AppendToken(tokenline, sizeof(tokenline), tp, ZERO);
        // send line
        if (Write(tokenline, tp))
        {
            // get the response
            if (GetResponse(NULL, &Error))
            {
                if (Error != 0.0f)
                    OK = FALSE;
            }
            else
                OK = FALSE;
        }
        else
            OK = FALSE;
    } // if (m_TokenMap.Lookup(_T("TRIGGER"), token))
    else
        OK = FALSE;

    return OK;
}

BOOL CToken::TokenizeScopeParameter(char *tokenline, size_t tokenlineLength, size_t &tp, const char *Param) 
{
    BOOL OK = TRUE;
    CString tokenList[5];
    size_t  Length, Index, Count, PendingToken;
    token_t token;

    // syntax is <SYSTEMVARIABLE> [<MODIFIER><(><INTEGER><)>]
    // break parameter string into a token  list delimitered by whitespace
    for (Length = strlen(Param), PendingToken = Index = Count = 0; Index < Length; Index++)
    {
        const char chr = toupper(Param[Index]);

        // if this is a whitespace then the previous token has finished
        if (isspace(chr))
        {
            // terminate the previous token
            Count += PendingToken;
            PendingToken = 0;
        }
        // this is a delimiter
        else if (strchr("()", chr))
        {
            // terminate the previous token
            Count += PendingToken;
            PendingToken = 0;

            // store the delimiter
            if (Count < (sizeof(tokenList)/sizeof(*tokenList)))
                tokenList[Count++] = chr;
        }
        // an ordinary character so just append it to the current token
        else if (Count < (sizeof(tokenList)/sizeof(*tokenList)))
        {
            tokenList[Count] += chr;
            PendingToken = 1;
        }
    }

    // terminate the last token
    Count += PendingToken;

    // the token list should have 1 or 5 elements
    if ((Count != 1) && (Count != 5))
        return FALSE;

    //-- store the variable
    if (!m_TokenMap.Lookup(CString(tokenList[0]), token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, tokenlineLength, tp, token);

    // store the modifier
    if (Count == 5)
    {
        if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
            return FALSE;
        AppendToken(tokenline, tokenlineLength, tp, token);

        if (!m_TokenMap.Lookup(CString(tokenList[1]), token) || ('M' != token.Type))
            return FALSE;
        AppendToken(tokenline, tokenlineLength, tp, token);

        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
            return FALSE;
        AppendToken(tokenline, tokenlineLength, tp, token);

        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, tokenlineLength, tp, _tcstod(tokenList[3], NULL)))
            return FALSE;

        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
            return FALSE;
        AppendToken(tokenline, tokenlineLength, tp, token);
    }

    if (!m_TokenMap.Lookup(_T("EOX"), token))
        return FALSE;
    AppendToken(tokenline, tokenlineLength, tp, token);

    return OK;
}

BOOL CToken::MechatroLink(short Module, short Function, short NumberOfParameters, const double MLParameters[], double *Result)  
{
    BOOL bOk = TRUE;

    try
    {
        token_t token;
        char tokenline[100];
        int Parameter;
        size_t tp = 0;
        double Error;

        // store the MECHATROLINK token
        if (!m_TokenMap.Lookup(_T("MECHATROLINK"), token)) throw 1;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the open brackets
        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token)) throw 2;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the module
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)Module)) throw 3;

        // store the function
        if (!m_TokenMap.Lookup(_T("COMMA"), token)) throw 4;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)Function)) throw 5;

        // store the parameters
        for (Parameter = 0; Parameter < NumberOfParameters; Parameter++)
        {
            if (!m_TokenMap.Lookup(_T("COMMA"), token)) throw 4;
            AppendToken(tokenline, sizeof(tokenline), tp, token);
            if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, (float)MLParameters[Parameter])) throw 5;
        }

        // store the close brackets
        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token)) throw 6;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // done
        if (!m_TokenMap.Lookup(_T("ZERO"), token)) throw 7;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        if (!Write((char *)tokenline, tp)) throw 8;

        // get the response
        if (!GetResponse(Result, &Error, FALSE)) throw 9;
        if (Error != 0.0f) throw 10;
    }
    catch (...)
    {
        bOk = FALSE;
    }

    return bOk;
}

BOOL CToken::GetVr(int Vr, double *Value) 
{
    // if this is a PCI card then do a direct transfer
    if (ControllerCanMemory(m_ControllerClass) && ControllerGetVr(m_ControllerClass, Vr, Value))
    {
        return TRUE;
    }
    else
    {
        // local data
        double  Array[1];

        // process as a normal command
        Array[0] = Vr;
        BOOL retval = ArrayCommand("VR", 1, Array, Value);
        return retval;
    }
}

BOOL CToken::SetVr(int Vr, double Value) 
{
    // if this is a PCI card then do a direct transfer
    if (ControllerCanMemory(m_ControllerClass) && ControllerSetVr(m_ControllerClass, Vr, Value))
    {
        return TRUE;
    }
    else
    {
        return SetVrCommand(Vr, Value);
    }
}

#if !defined(__GNUC__)
BOOL CToken::GetAxisVariable(const wchar_t *Variable, short Axis, double *Value) 
{
    CStringA ASCIIVariable(Variable);
    return GetAxisVariable(ASCIIVariable, Axis, Value);
}
#endif

BOOL CToken::GetAxisVariable(const char *Variable, short Axis, double *Value) 
{
    // local data
    token_t token;
    char tokenline[100];
    size_t tp = 0;
    double Error;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable),token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the AXIS modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("AXIS"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Axis))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(Value,&Error))
        return FALSE;

    // if there was an error then fail
    if (Error != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::SetAxisVariable(const wchar_t *Variable, short Axis, double fValue) 
{
    CStringA ASCIIVariable(Variable);
    return SetAxisVariable(ASCIIVariable, Axis, fValue);
}
#endif

BOOL CToken::SetAxisVariable(const char *Variable, short Axis, double fValue) 
{
    // local data
    token_t token;
    size_t          tp = 0;
    char    tokenline[100];
    double  fError;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable), token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the AXIS modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("AXIS"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Axis))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the ASSIGN token
    if (!m_TokenMap.Lookup(_T("ASSIGN"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::GetProcessVariable(const wchar_t *Variable, short Process, double *Value) 
{
    CStringA ASCIIVariable(Variable);
    return GetProcessVariable(ASCIIVariable, Process, Value);
}
#endif

BOOL CToken::GetProcessVariable(const char *Variable, short Process, double *Value) 
{
    // local data
    token_t token;
    char tokenline[100];
    size_t tp = 0;
    double Error;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable),token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the PROCESS modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("PROC"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Process))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(Value,&Error))
        return FALSE;

    // if there was an error then fail
    if (Error != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::SetProcessVariable(const wchar_t *Variable, short Process, double fValue) 
{
    CStringA ASCIIVariable(Variable);
    return SetProcessVariable(ASCIIVariable, Process, fValue);
}
#endif

BOOL CToken::SetProcessVariable(const char *Variable, short Process, double fValue) 
{
    // local data
    token_t token;
    size_t          tp = 0;
    char    tokenline[100];
    double  fError;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable), token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the PROCESS modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("PROC"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Process))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the ASSIGN token
    if (!m_TokenMap.Lookup(_T("ASSIGN"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::GetSlotVariable(const wchar_t *Variable, short Slot, double *Value) 
{
    CStringA ASCIIVariable(Variable);
    return GetSlotVariable(ASCIIVariable, Slot, Value);
}
#endif

BOOL CToken::GetSlotVariable(const char *Variable, short Slot, double *Value) 
{
    // local data
    token_t token;
    char tokenline[100];
    size_t tp = 0;
    double Error;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable),token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the SLOT modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("SLOT"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Slot))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(Value,&Error))
        return FALSE;

    // if there was an error then fail
    if (Error != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::SetSlotVariable(const wchar_t *Variable, short Slot, double fValue) 
{
    CStringA ASCIIVariable(Variable);
    return SetSlotVariable(ASCIIVariable, Slot, fValue);
}
#endif

BOOL CToken::SetSlotVariable(const char *Variable, short Slot, double fValue) 
{
    // local data
    token_t token;
    size_t          tp = 0;
    char    tokenline[100];
    double  fError;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable), token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the SLOT modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("SLOT"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Slot))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the ASSIGN token
    if (!m_TokenMap.Lookup(_T("ASSIGN"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::GetPortVariable(const wchar_t *Variable, short Port, double *Value) 
{
    CStringA ASCIIVariable(Variable);
    return GetPortVariable(ASCIIVariable, Port, Value);
}
#endif

BOOL CToken::GetPortVariable(const char *Variable, short Port, double *Value) 
{
    // local data
    token_t token;
    char tokenline[100];
    size_t tp = 0;
    double Error;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable),token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the PORT modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("PORT"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Port))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if (!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(Value,&Error))
        return FALSE;

    // if there was an error then fail
    if (Error != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::SetPortVariable(const wchar_t *Variable, short Port, double fValue) 
{
    CStringA ASCIIVariable(Variable);
    return SetPortVariable(ASCIIVariable, Port, fValue);
}
#endif

BOOL CToken::SetPortVariable(const char *Variable, short Port, double fValue) 
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // store the SYSTEMVARIABLE token
    if (!m_TokenMap.Lookup(CString(Variable), token) || ('V' != token.Type))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the PORT modifier token
    if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);
    if (!m_TokenMap.Lookup(_T("PORT"), token) || ('M' != token.Type))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Port))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the ASSIGN token
    if (!m_TokenMap.Lookup(_T("ASSIGN"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the number token
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, fValue))
        return FALSE;

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

// ----------------------------------------

CToken::CControllerCapabilities::CControllerCapabilities(CToken *token)
: m_floatingPointFormat(tms320FloatingPointFormat)
, m_processorType(unknownProcessor)
, m_token(token)
, m_escapeCommunications(TRUE)
, m_ControllerVersion(0.0)
, m_ControllerType(0)
{
}

BOOL CToken::CControllerCapabilities::SetControllerType(int p_controllerType)
{
    CString message;

    m_token->AtlTrace("SetControllerType(%i)", p_controllerType);

    m_ControllerType = p_controllerType % 1000;
    return SetControllerCapabilities();
}

BOOL CToken::CControllerCapabilities::SetControllerVersion(double p_controllerVersion)
{
    CString message;

    m_token->AtlTrace("SetControllerVersion(%f)", p_controllerVersion);

    m_ControllerVersion = p_controllerVersion;
    return SetControllerCapabilities();
}

BOOL CToken::CControllerCapabilities::SetControllerCapabilities()
{
    // if we do not have controller version AND controller type then ignore
    if (!m_ControllerType || (0.0 == m_ControllerVersion)) return TRUE;

    // set capabilities
    if ((m_ControllerType >= 300) || (m_ControllerType == 266))
    {
        // set processor type
        if ((m_ControllerType == 464) || (m_ControllerType == 266))
        {
            m_token->AtlTrace("Selected processor MIPS");
            m_processorType = mips;
        }
        else if (m_ControllerType == 400)
        {
            m_token->AtlTrace("Selected processor SIMULATOR");
            m_processorType = simulator;
        }
        else
        {
            m_token->AtlTrace("Selected processor ARM");
            m_processorType = arm;

            // we must reopen with a larger sndbuf
            ControllerClose(m_token->m_ControllerClass, COMMUNICATIONS_CAPABILITY_REMOTE);
            ControllerSetInterfaceTCP(m_token->m_ControllerClass, -1);
            ControllerOpen(m_token->m_ControllerClass, COMMUNICATIONS_CAPABILITY_REMOTE);
        }

        // set floating point format
        m_floatingPointFormat = ieee64FloatingPointFormat;

        // set escape communications protocol
        m_escapeCommunications = TRUE;
    }
    else
    {
        // set processor type
        m_token->AtlTrace("Selected processor TMS320");
        m_processorType = tms320;
        m_floatingPointFormat = tms320FloatingPointFormat;

        // set escape communications protocol
        m_escapeCommunications = (m_ControllerVersion >= MIN_VERSION_ESCAPE);
    }

    return TRUE;
}

unsigned long CToken::CControllerCapabilities::FloatToTms(float fValue) const
{
    // local data
    union
    {
        float   f;
        long    l;
    } input;
    long output;
    struct tag_ieee
    {
        unsigned long m : 23;
        unsigned long e : 8;
        unsigned long s : 1;
    } *ieee;
    struct tag_tms
    {
        unsigned long m: 23;
        unsigned long s : 1;
        long e : 8;
    } *tms;
    long m;

    // assign the value
    input.f = fValue;

    // assign the pointers
    ieee = (struct tag_ieee *)&input;
    tms = (struct tag_tms *)&output;

    // if ieee is zero then set tms zero
    if (!ieee->e && !ieee->m)
    {
        tms->e = -128; tms->s = tms->m = 0;
    }
    // if ieee is denormalized the set tms zero ##### normalize
    else if (!ieee->e && ieee->m)
    {
        tms->e = -128; tms->s = tms->m = 0;
    }
    // if ieee is infinity the set the largest number possible
    else if (255 == ieee->e && !ieee->m)
    {
        tms->e = 127; tms->s = ieee->s; tms->m = 0x7fffff;
    }
    // if ieee is NaN then set tms zero
    else if (255 == ieee->e && ieee->m)
    {
        tms->e = -128; tms->s = tms->m = 0;
    }
    // if this is a positive then create the tms positive
    else if (!ieee->s)
    {
        tms->e = ieee->e - 127; tms->s = ieee->s; tms->m = ieee->m;
    }
    // this is a negative so create a tms negative
    else
    {
        // create the negative number
        tms->e = ieee->e - 127; tms->s = ieee->s;

        // create the 24 bit unsigned mantissa
        m = ieee->m | 0x800000;

        // make this a negative number
        m = -m;

        // normalise
        while ((m & 0x800000))
        {
            tms->e--; m <<= 1;
        }

        // assign mantissa
        tms->m = m;
    }

    // return output value
    return(output);
}

float CToken::CControllerCapabilities::TmsToFloat(unsigned long lValue) const
{
    // local data
    union
    {
        float   f;
        long    l;
    } output;
    struct tag_ieee
    {
        unsigned long m : 23;
        unsigned long e : 8;
        unsigned long s : 1;
    } *ieee;
    struct tag_tms
    {
        unsigned long m: 23;
        unsigned long s : 1;
        long e : 8;
    } *tms;
    long m;

    // create the ieee format
    ieee = (struct tag_ieee *)&output;
    tms = (struct tag_tms *)&lValue;

    // if tms is zero the set ieee zero
    if (-128 == tms->e)
    {
        ieee->e = 0; ieee->m = 0;
    }
    // if this is a tms positive then create an ieee positive
    else if (!tms->s)
    {
        ieee->e = tms->e + 127; ieee->s = tms->s; ieee->m = tms->m;
    }
    // this is a tms negative so create an ieee negative
    else
    {
        // create the negative number
        ieee->e = tms->e + 128; ieee->s = tms->s;

        // create the signed mantissa (bit 24 is implicit)
        m = tms->m | 0xff000000;

        // make this a positive number
        m = -m;

        // normalise
        if (m & 0x7fffff)
        {
            // remove leading zeros
            while (!(m & 0x800000))
            {
                ieee->e--; m <<= 1;
            }

            // remove implicit msb
            ieee->e--;
        }

        // store the mantissa
        ieee->m = m;
    }

    // return the ieee number
    return(output.f);
}

BOOL CToken::CControllerCapabilities::StoreNumber(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const
{
    CToken::token_t token;

    // store NUMBER token
    if (!p_tokenMap.Lookup(_T("NUMBER"), token))
        return FALSE;
    if (p_tokenLineLength >= p_tokenLineSize)
        return FALSE;
    p_tokenLine[p_tokenLineLength++] = token.Token;

    // store number format
    switch (m_floatingPointFormat)
    {
    case tms320FloatingPointFormat:
        if (!StoreTms320Number(p_ControllerClass, p_tokenMap, p_tokenLine, p_tokenLineSize, p_tokenLineLength, p_value))
            return FALSE;
        break;
    case ieee64FloatingPointFormat:
        if (!StoreIEEE64Number(p_ControllerClass, p_tokenMap, p_tokenLine, p_tokenLineSize, p_tokenLineLength, p_value))
            return FALSE;
        break;
    default:
        ASSERT(FALSE);
        return FALSE;
    }

    // store the end of expression token
    if (!p_tokenMap.Lookup(_T("EOX"), token))
        return FALSE;
    if (p_tokenLineLength >= p_tokenLineSize)
        return FALSE;
    p_tokenLine[p_tokenLineLength++] = token.Token;

    return TRUE;
}

BOOL CToken::CControllerCapabilities::StoreInteger(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT64 p_value) const
{
    CToken::token_t token;
    int bits;

    // if this token does not fit then we are done
    if (p_tokenLineLength >= p_tokenLineSize)
        return FALSE;

    // if this is in the range 0..255 then try to store an 8 bit integer
    bits = 0;
    if ((p_value < 0x100) && p_tokenMap.Lookup(_T("INTEGER8"), token))
    {
        p_tokenLine[p_tokenLineLength++] = token.Token;
        bits = 8;
    }
    // try to store as a 64 bit integer
    else if (p_tokenMap.Lookup(_T("INTEGER64"), token))
    {
        p_tokenLine[p_tokenLineLength++] = token.Token;
        bits = 64;
    }
    // can't store as an integer
    else
        return StoreNumber(p_ControllerClass, p_tokenMap, p_tokenLine, p_tokenLineSize, p_tokenLineLength, (double)p_value);

    // store number format
    switch (m_floatingPointFormat)
    {
    case ieee64FloatingPointFormat:
        switch (bits)
        {
        case 8:
            if (!StoreInteger8(p_ControllerClass, p_tokenMap, p_tokenLine, p_tokenLineSize, p_tokenLineLength, (UINT8)p_value))
                return FALSE;
            break;
        case 64:
            if (!StoreInteger64(p_ControllerClass, p_tokenMap, p_tokenLine, p_tokenLineSize, p_tokenLineLength, p_value))
                return FALSE;
            break;
        default:
            ASSERT(FALSE);
            return FALSE;
        }
        break;
    default:
        ASSERT(FALSE);
        return FALSE;
    }

    // store the end of expression token
    if (!p_tokenMap.Lookup(_T("EOX"), token))
        return FALSE;
    if (p_tokenLineLength >= p_tokenLineSize)
        return FALSE;
    p_tokenLine[p_tokenLineLength++] = token.Token;

    return TRUE;
}

BOOL CToken::CControllerCapabilities::StoreTms320Number(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const
{
    long tmsValue;
    int i;

    // store the number
    tmsValue = FloatToTms((float)p_value);

    // store the data
    for (i = 0;i< 4;i++)
    {
        if (p_tokenLineLength >= p_tokenLineSize)
            return FALSE;
        p_tokenLine[p_tokenLineLength++] = (char)((tmsValue >> (i * 8)) & 0xff);
    }

    return TRUE;
}

BOOL CToken::CControllerCapabilities::StoreIEEE64Number(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const
{
    union
    {
        double d;
        ULONGLONG l;
    } value;
    int i;

    // store the number
    value.d = p_value;

    // store the data
    for (i = 0;i< 8;i++)
    {
        if (p_tokenLineLength >= p_tokenLineSize)
            return FALSE;
        p_tokenLine[p_tokenLineLength++] = (char)((value.l >> (i * 8)) & 0xff);
    }

    return TRUE;
}

BOOL CToken::CControllerCapabilities::StoreInteger64(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT64 p_value) const
{
    // store the data
    for (int i = 0;i< 8;i++)
    {
        if (p_tokenLineLength >= p_tokenLineSize)
            return FALSE;
        p_tokenLine[p_tokenLineLength++] = (char)((p_value >> (i * 8)) & 0xff);
    }

    return TRUE;
}

BOOL CToken::CControllerCapabilities::StoreInteger8(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT8 p_value) const
{
    // store the data
    if (p_tokenLineLength >= p_tokenLineSize)
        return FALSE;
    p_tokenLine[p_tokenLineLength++] = (char)p_value;

    return TRUE;
}

BOOL CToken::CControllerCapabilities::ReadNumber(void *p_ControllerClass, BOOL p_timeout, double *pfValue) const
{
    // read number
    switch (m_floatingPointFormat)
    {
    case tms320FloatingPointFormat:
        return ReadTms320Number(p_ControllerClass, p_timeout, pfValue);
    case ieee64FloatingPointFormat:
        return ReadIEEE64Number(p_ControllerClass, p_timeout, pfValue);
    default:
        ASSERT(FALSE);
        return FALSE;
    }
}

BOOL CToken::CControllerCapabilities::ReadTms320Number(void *p_ControllerClass, BOOL p_timeout, double *p_value) const
{
    // local data
    unsigned char   chr[4];
    unsigned long   lValue;
    size_t   i, j, BytesRead;
    DWORD t0 = GetTickCount();

    // read this number
    lValue = 0; i = 0;
    do
    {
        // set the buffer size
        BytesRead = 4 - i;

        // read this character
        if (!ControllerRead(p_ControllerClass, CHANNEL_REMOTE, (char *)chr, &BytesRead))
            return FALSE;

        // if the character is not available then release CPU
        if (!BytesRead)
        {
            Sleep(TOKEN_RECEIVE_SLEEP_TIME);
        }
        else
        {
            // incorporate this character
            for (j = 0;j < BytesRead;j++)
            {
                lValue |= (chr[j] & 0xff) << (i++ * 8);
            }
        }
    } while ((i < 4) && ((GetTickCount() - t0) < (DWORD)((p_timeout ? TKN_NORMAL_COMMAND_TIMEOUT : TKN_LONG_COMMAND_TIMEOUT) * 1000)));

    // if timed out then fail
    if (i < 4)
        return FALSE;

    // store return value
    *p_value = TmsToFloat(lValue);

    return TRUE;
}

BOOL CToken::CControllerCapabilities::ReadIEEE64Number(void *p_ControllerClass, BOOL p_timeout, double *p_value) const
{
    // local data
    unsigned char   chr[8];
    union
    {
        double d;
        ULONGLONG l;
    } value;
    size_t   i, j, BytesRead;
    DWORD t0 = GetTickCount();

    // read this number
    value.l = 0; i = 0;
    do
    {
        // set the buffer size
        BytesRead = 8 - i;

        // read this character
        if (!ControllerRead(p_ControllerClass, CHANNEL_REMOTE, (char *)chr, &BytesRead))
            return FALSE;

        // if the character is not available then release CPU
        if (!BytesRead)
        {
            Sleep(TOKEN_RECEIVE_SLEEP_TIME);
        }
        else
        {
            // incorporate this character
            for (j = 0;j < BytesRead;j++)
            {
                value.l |= ((ULONGLONG)chr[j] & 0xff) << (i++ * 8);
            }
        }
    } while ((i < 8) && ((GetTickCount() - t0) < (DWORD)((p_timeout ? TKN_NORMAL_COMMAND_TIMEOUT : TKN_LONG_COMMAND_TIMEOUT) * 1000)));

    // if timed out then fail
	if (i < 8)
		return FALSE;

    // store return value
    *p_value = value.d;

    return TRUE;
}

BOOL CToken::ReadByte(char *chr, int timeout) const
{
    CTime timeStart = CTime::GetCurrentTime();
    size_t BytesRead;

    // read data
    do
    {
        // set the buffer size
        BytesRead = 1;

        // read this character
        if (!ControllerRead(m_ControllerClass, CHANNEL_REMOTE, chr, &BytesRead)) BytesRead = -1;
        else if (!BytesRead) Sleep(TOKEN_RECEIVE_SLEEP_TIME);
    } while (
            !BytesRead &&
            ((CTime::GetCurrentTime() - timeStart).GetTotalSeconds() < timeout)
            );

    return(1 == BytesRead);
}


BOOL CToken::StepRatio(INT Numerator, INT Denominator, long Axis) 
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // store the STEP_RATIO token
    if (!m_TokenMap.Lookup(_T("STEP_RATIO"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the numerator
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Numerator))
        return FALSE;

    // store the comma token
    if (!m_TokenMap.Lookup(_T("COMMA"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the denominator
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Denominator))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // if there was an axis specified then store it
    if (-1 != Axis)
    {
        // store the AXIS modifier token
        if (!m_TokenMap.Lookup(_T("MODIFIER"), token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
        if (!m_TokenMap.Lookup(_T("AXIS"), token) || ('M' != token.Type))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the OPENBRACKETS token
        if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Axis))
            return FALSE;

        // store the CLOSEBRACKETS token
        if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);
    }

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // success
    return TRUE;
}

#if !defined(__GNUC__)
BOOL CToken::PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, LONG Variable)
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (Append)
    {
        if (!PRMBLK[BlockNumber].InUse)
            return FALSE;
        if ((PRMBLK[BlockNumber].Values + 1) >= 32)
            return FALSE;
    }
    else
    {
        if (PRMBLK[BlockNumber].InUse)
            return FALSE;
    }
    if ((BlockType != PRMBLK_Vr) && (BlockType != PRMBLK_Table))
        return FALSE;

    // store the PRMBLK token
    if (!m_TokenMap.Lookup(_T("PRMBLK"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PRMBLK function
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Append ? 1 : 0))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockNumber))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block type
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockType))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the variable index
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Variable))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // store this parameter block
    if (Append)
        PRMBLK[BlockNumber].Values++;
    else
    {
        PRMBLK[BlockNumber].InUse = TRUE;
        PRMBLK[BlockNumber].BlockType = BlockType;
        PRMBLK[BlockNumber].Program = "";
        PRMBLK[BlockNumber].Process = -1;
        PRMBLK[BlockNumber].Values = 1;
    }

    // success
    return TRUE;
}

BOOL CToken::PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, const char *Variable)
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (Append)
    {
        if (!PRMBLK[BlockNumber].InUse)
            return FALSE;
        if ((PRMBLK[BlockNumber].Values + 1) >= 32)
            return FALSE;
    }
    else
    {
        if (PRMBLK[BlockNumber].InUse)
            return FALSE;
    }
    if ((BlockType != PRMBLK_Axis) && (BlockType != PRMBLK_System))
        return FALSE;

    // store the PRMBLK token
    if (!m_TokenMap.Lookup(_T("PRMBLK"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PRMBLK function
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Append ? 1 : 0))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockNumber))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block type
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockType))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the variable token
    if (!m_TokenMap.Lookup(CString(Variable), token) || (token.Type != 'V'))
        return FALSE;
    AppendToken_SystemVariable(tokenline, sizeof(tokenline), tp, token);

    // store the EOX token
    if (!m_TokenMap.Lookup(_T("EOX"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // store this parameter block
    if (Append)
        PRMBLK[BlockNumber].Values++;
    else
    {
        PRMBLK[BlockNumber].InUse = TRUE;
        PRMBLK[BlockNumber].BlockType = BlockType;
        PRMBLK[BlockNumber].Program = "";
        PRMBLK[BlockNumber].Process = -1;
        PRMBLK[BlockNumber].Values = 1;
    }

    // success
    return TRUE;
}

BOOL CToken::PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, const char *ProgramName, LONG ProcessNumber, const char *Variable)
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;
    int i;

    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (Append)
    {
        if (!PRMBLK[BlockNumber].InUse)
            return FALSE;
        if ((PRMBLK[BlockNumber].Values + 1) >= 32)
            return FALSE;
    }
    else
    {
        if (PRMBLK[BlockNumber].InUse)
            return FALSE;
    }
    if (BlockType != PRMBLK_Program)
        return FALSE;

    // store the PRMBLK token
    if (!m_TokenMap.Lookup(_T("PRMBLK"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PRMBLK function
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Append ? 1 : 0))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockNumber))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block type
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockType))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if (!m_TokenMap.Lookup(_T("QUOTES"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program name
    for (i = 0;ProgramName[i];i++)
        tokenline[tp++] = ProgramName[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the EOX token
    if(!m_TokenMap.Lookup(_T("EOX"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the process number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, ProcessNumber))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the QUOTES token
    if (!m_TokenMap.Lookup(_T("QUOTES"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the program name
    for (i = 0;Variable[i];i++)
        tokenline[tp++] = Variable[i];

    // store the QUOTES token
    if(!m_TokenMap.Lookup(_T("QUOTES"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the EOX token
    if(!m_TokenMap.Lookup(_T("EOX"),token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // store this parameter block
    if (Append)
        PRMBLK[BlockNumber].Values++;
    else
    {
        PRMBLK[BlockNumber].InUse = TRUE;
        PRMBLK[BlockNumber].BlockType = BlockType;
        PRMBLK[BlockNumber].Program = ProgramName;
        PRMBLK[BlockNumber].Process = ProcessNumber;
        PRMBLK[BlockNumber].Values = 1;
    }

    // success
    return TRUE;
}

BOOL CToken::PRMBLK_Append(LONG BlockNumber, VARIANT Variable)
{
    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;

    // process this block type
    switch (PRMBLK[BlockNumber].BlockType)
    {
    case PRMBLK_Axis:
    case PRMBLK_System:
        return PRMBLK_Define(BlockNumber, PRMBLK[BlockNumber].BlockType, TRUE, CStringA(COleVariant(Variable).bstrVal));
        break;
    case PRMBLK_Vr:
    case PRMBLK_Table:
        return PRMBLK_Define(BlockNumber, PRMBLK[BlockNumber].BlockType, TRUE, COleVariant(Variable).lVal);
        break;
    case PRMBLK_Program:
        return PRMBLK_Define(BlockNumber, PRMBLK[BlockNumber].BlockType, TRUE, PRMBLK[BlockNumber].Program, PRMBLK[BlockNumber].Process, CStringA(COleVariant(Variable).bstrVal));
        break;
    }

    return FALSE;
}

BOOL CToken::PRMBLK_Get(LONG BlockNumber, int Axis, BOOL All, CString &Values)
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (!PRMBLK[BlockNumber].InUse)
        return FALSE;
    if ((PRMBLK[BlockNumber].BlockType == PRMBLK_Axis) && (Axis == -1))
        return FALSE;

    // store the PRMBLK token
    if (!m_TokenMap.Lookup(_T("PRMBLK"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PRMBLK function
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, All ? 3 : 4))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockNumber))
        return FALSE;

    // store the axis number
    if (PRMBLK[BlockNumber].BlockType == PRMBLK_Axis)
    {
        // store the COMMA token
        if (!m_TokenMap.Lookup(_T("COMMA"), token))
            return FALSE;
        AppendToken(tokenline, sizeof(tokenline), tp, token);

        // store the block number
        if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, Axis))
            return FALSE;
    }

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the directory list
    BOOL bError = FALSE;
    BOOL bDone = FALSE;
    CTime timeStart = CTime::GetCurrentTime();
    do
    {
        // local data
        char    chr;

        // read data
        if (!ReadByte(&chr, TKN_NORMAL_COMMAND_TIMEOUT))
            bError = TRUE;

        // end of line?
        if (chr == '\n')
            bDone = 1;
        else if (chr != '\r')
            Values += chr;

        // timeout
        bError |= (CTime::GetCurrentTime() - timeStart).GetTotalSeconds() > TKN_NORMAL_COMMAND_TIMEOUT;
    } while (!bDone && !bError);

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // if values failed then error
    if (bError)
        return FALSE;

    return TRUE;
}

BOOL CToken::PRMBLK_Get(LONG BlockNumber, int Axis, BOOL All, VARIANT *Values)
{
    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (!PRMBLK[BlockNumber].InUse)
        return FALSE;

    // make sure that the return value is free
    VariantClear(Values);

    // get the values
    CString ControllerValues;
    if (!PRMBLK_Get(BlockNumber, Axis, All, ControllerValues))
        return FALSE;

    // parse the response
    COleSafeArray sa;
    sa.CreateOneDim(VT_VARIANT, PRMBLK[BlockNumber].Values);
    long index, pos;
    for (index = pos = 0; (index < PRMBLK[BlockNumber].Values) && (pos < ControllerValues.GetLength()); index++)
    {
        // get the length of this entry
        int length = ControllerValues.Find(',', pos) - pos;
        if (length < 0)
            length = ControllerValues.GetLength() - pos;

        // get this element
        CString Value = ControllerValues.Mid(pos, length);
        pos += length + 1;

        // if this is an incremental then get the index
        if (!All)
        {
            length = Value.Find(':');
            if (length < 0)
                continue;
            index = atoi(CStringA(Value.Left(length)));
            Value = Value.Mid(length + 1);
        }

        // store this element
        char *end;
        double double_val = strtod(CStringA(Value), &end);
        if (!*end)
            sa.PutElement(&index, (void *)&COleVariant(double_val).Detach());
        else
            sa.PutElement(&index, (void *)&COleVariant(Value).Detach());
    }
    *Values = sa.Detach();

    return TRUE;
}

BOOL CToken::PRMBLK_Delete(LONG BlockNumber)
{
    // local data
    token_t token;
    size_t tp = 0;
    char tokenline[100];
    double fError;

    // sanity check
    if ((BlockNumber < 0) || (BlockNumber >= (sizeof(PRMBLK) / sizeof(*PRMBLK))))
        return FALSE;
    if (!PRMBLK[BlockNumber].InUse)
        return FALSE;

    // store the PRMBLK token
    if (!m_TokenMap.Lookup(_T("PRMBLK"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the OPENBRACKETS token
    if (!m_TokenMap.Lookup(_T("OPENBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the PRMBLK function
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, 8))
        return FALSE;

    // store the COMMA token
    if (!m_TokenMap.Lookup(_T("COMMA"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // store the block number
    if (!m_ControllerCapabilities.StoreNumber(m_ControllerClass, m_TokenMap, tokenline, sizeof(tokenline), tp, BlockNumber))
        return FALSE;

    // store the CLOSEBRACKETS token
    if (!m_TokenMap.Lookup(_T("CLOSEBRACKETS"), token))
        return FALSE;
    AppendToken(tokenline, sizeof(tokenline), tp, token);

    // terminate the line
    AppendToken(tokenline, sizeof(tokenline), tp, ZERO);

    // send line
    if(!Write(tokenline,tp))
        return FALSE;

    // get the response
    if (!GetResponse(NULL,&fError))
        return FALSE;

    // if there was an error then fail
    if (fError != 0.0)
        return FALSE;

    // this parameter block is no longer in use
    PRMBLK[BlockNumber].InUse = FALSE;

    return TRUE;
}
#endif

