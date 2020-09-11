/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __TOKEN_H__
#define __TOKEN_H__

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class CToken
{
protected:
    static const int m_knMaxNameLength;

    typedef struct command_t {
        int MinQuoteParameters,
            MaxQuoteParameters,
            MinNumericParameters,
            MaxNumericParameters,
            ModifierSet,
            Exception;
        union {
            struct {
                unsigned int IsCommand : 1;
                unsigned int IsOperator : 1;
                unsigned int IsFunction : 1;
                unsigned int UsesBrackets : 1;
                unsigned int IsRuntime : 1;
                unsigned int IsDisabled : 1;
                unsigned int IsPC : 1;
            } Flag;
            long Flags;
        };
    } command_t;

    typedef struct sysvar_t {
        int ModifierSet;
        union {
            struct {
                unsigned int IsWrite : 1;
                unsigned int IsDisabled : 1;
                unsigned int IsPC : 1;
            } Flag;
            long Flags;
        };
    } sysvar_t;

    typedef struct modifier_t {
        int MinNumericParameters,
            MaxNumericParameters,
            ModifierMask;
    } modifier_t;

    typedef struct constant_t {
        double Value;
    } constant_t;

    class token_t
    {
    public:
        char            Type;
        unsigned int    Token;
        char            Name[100];
        union
        {
            command_t      Command;
            sysvar_t       SysVar;
            modifier_t     Modifier;
            constant_t     Constant;
        };
    };

    class PRMBLK_t
    {
    public:
        BOOL InUse;
        PRMBLK_Types BlockType;
        CStringA Program;
        int Process;
        int Values;

        PRMBLK_t() : InUse(FALSE), Values(0) { }
    };

#ifdef _WIN32
    typedef CMap<CString, LPCTSTR, token_t, token_t&> tag_tokenMap;
#else
    friend class CMap;

    class CMap : public CObject
    {
    protected:
        struct map_less_than
        {
            bool operator()(const CString& s1, const CString& s2) const;
        };

        typedef std::map <CString, CToken::token_t, map_less_than> map_type;
        map_type _map;

    public:
        /* constructors */
        CMap() { _map.clear(); }

        /* methods */
        void SetAt(const CString &key, const CToken::token_t & newValue);
        BOOL Lookup(const CString &key, CToken::token_t& rValue) const;
    };

    typedef CMap tag_tokenMap;
#endif

    class CControllerCapabilities {
    public:
        enum floatingPointFormats { unknownFloatingPointFormat, tms320FloatingPointFormat, ieee64FloatingPointFormat };

    protected:
        enum processorTypes { unknownProcessor, tms320, mips, arm, simulator };
        enum floatingPointFormats m_floatingPointFormat;
        enum processorTypes m_processorType;

        CToken *m_token;
        BOOL m_escapeCommunications;
        double m_ControllerVersion;
        int m_ControllerType;

        BOOL SetControllerCapabilities();
        unsigned long FloatToTms(float fValue) const;
        float TmsToFloat(unsigned long lValue) const;

        BOOL StoreTms320Number(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const;
        BOOL StoreIEEE64Number(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const;
        BOOL StoreInteger64(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT64 p_value) const;
        BOOL StoreInteger8(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT8 p_value) const;

        BOOL ReadTms320Number(void *p_ControllerClass, BOOL p_timeout, double *p_value) const;
        BOOL ReadIEEE64Number(void *p_ControllerClass, BOOL p_timeout, double *p_value) const;

    public:
        CControllerCapabilities(CToken *token);

        BOOL SetControllerVersion(double p_controllerVersion);
        BOOL SetControllerType(int p_controllerType);

        enum floatingPointFormats GetFloatingPointFormat() const { return m_floatingPointFormat; }
        BOOL IsCommunicationsEscaped() const { return m_escapeCommunications; }

        BOOL StoreNumber(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const double p_value) const;
        BOOL StoreInteger(const void *p_ControllerClass, const tag_tokenMap &p_tokenMap, char *p_tokenLine, size_t p_tokenLineSize, size_t &p_tokenLineLength, const UINT64 p_value) const;
        BOOL ReadNumber(void *p_ControllerClass, BOOL p_timeout, double *p_value) const;

        double GetControllerVersion() const { return m_ControllerVersion; }
    } m_ControllerCapabilities;

    void *m_ControllerClass;
    tag_tokenMap m_TokenMap;                // token table
    token_t ZERO, UNKNOWN, EXTENDED_TOKEN;      // end of line token
    long m_lLastError;
	BOOL m_protocol_error;
	fireEventContext_t m_fireEventClass;
    fireEventFunction_t m_fireEventFunction;
    PRMBLK_t PRMBLK[64];

public:
	BOOL IsError() { return m_protocol_error; }
    BOOL SetControllerVersion(double p_controllerVersion) { return m_ControllerCapabilities.SetControllerVersion(p_controllerVersion); }
    BOOL SetControllerType(int p_controllerType) { return m_ControllerCapabilities.SetControllerType(p_controllerType); }
	BOOL GetTable(long StartPosition, long Elements, double *Values);
    BOOL GetSelected(CStringA &SelectedProgram);
    BOOL InsertLine(const char *Program, short LineNumber, const char *Line);
#if !defined(__GNUC__)
    BOOL InsertLine(const wchar_t *Program, short LineNumber, const wchar_t *Line);
#endif
    BOOL Select(const char *Program);
#if !defined(__GNUC__)
    BOOL Select(const wchar_t *Program);
#endif
    BOOL Dir(CStringA &csDir, const char *szOption);
#if !defined(__GNUC__)
    BOOL Dir(CStringW &csDir, const wchar_t *szOption);
#endif
    BOOL New(const char *Program);
#if !defined(__GNUC__)
    BOOL New(const wchar_t *Program);
#endif
    BOOL SetTable(long StartPosition, long Elements, const double Values[]);
    BOOL GetNamedVariable(int Variable, int Process, double *ReturnValue);
    BOOL ExecuteCommand(const char *Command);
#if !defined(__GNUC__)
    BOOL ExecuteCommand(const wchar_t *Command);
#endif
    BOOL LinputCommand(short Channel, short StartVr);
    BOOL KeyCommand(short Channel, short *Value);
    BOOL InputCommand(short Channel, double *Value);
    BOOL GetCommand(short Channel, short *Value);
    BOOL ProcessCommand(const char *Command, const char *Program, double Process, double *ReturnValue = NULL); // run a process command
#if !defined(__GNUC__)
    BOOL ProcessCommand(const wchar_t *Command, const wchar_t *Program, double Process, double *ReturnValue = NULL); // run a process command
#endif
    BOOL Command(const char *Command, double *ReturnValue = NULL);
#if !defined(__GNUC__)
    BOOL Command(const wchar_t *Command, double *ReturnValue = NULL);
#endif
    BOOL MotionCommand(const char *Command, int Elements, const double Array[], int Base, double *ReturnValue = NULL); // run a motion command
#if !defined(__GNUC__)
    BOOL MotionCommand(const wchar_t *Command, int Elements, const double Array[], int Base, double *ReturnValue = NULL); // run a motion command
#endif
    BOOL ArrayCommand(const char *Command, int Elements, const double Array[], double *ReturnValue = NULL); // run a standard command
#if !defined(__GNUC__)
    BOOL ArrayCommand(const wchar_t *Command, int Elements, const double Array[], double *ReturnValue = NULL); // run a standard command
#endif
    BOOL GetVariable(const char *Variable, double *Value);    // get the variable value
#if !defined(__GNUC__)
    BOOL GetVariable(const wchar_t *Variable, double *Value);    // get the variable value
#endif
    BOOL SetVariable(const char *Variable, double Value);                     // set the variable to value
#if !defined(__GNUC__)
    BOOL SetVariable(const wchar_t *Variable, double Value);                     // set the variable to value
#endif
    BOOL GetVr(int Vr, double *Value);    // get the variable value
    BOOL SetVr(int Vr, double Value);    // set the variable value
    BOOL GetTokenTable();                      // get the token table
    BOOL MarkCommand(short Axis, short *Value, short MarkNumber = 0);
    BOOL Scope(BOOL On, long SamplePeriod, long TableStart, long TableEnd, const CStringList &ParamList);
    BOOL Scope(BOOL On);
    BOOL Trigger();
    BOOL MechatroLink(short Module, short Function, short NumberOfParameters, const double MLParameters[], double *Result);
    BOOL GetAxisVariable(const char *Variable, short Axis, double *Value);
#if !defined(__GNUC__)
    BOOL GetAxisVariable(const wchar_t *Variable, short Axis, double *Value);
#endif
    BOOL SetAxisVariable(const char *Variable, short Axis, double Value);
#if !defined(__GNUC__)
    BOOL SetAxisVariable(const wchar_t *Variable, short Axis, double Value);
#endif
    BOOL GetProcessVariable(const char *Variable, short Process, double *Value);
#if !defined(__GNUC__)
    BOOL GetProcessVariable(const wchar_t *Variable, short Process, double *Value);
#endif
    BOOL SetProcessVariable(const char *Variable, short Process, double Value);
#if !defined(__GNUC__)
    BOOL SetProcessVariable(const wchar_t *Variable, short Process, double Value);
#endif
    BOOL GetSlotVariable(const char *Variable, short Slot, double *Value);
#if !defined(__GNUC__)
    BOOL GetSlotVariable(const wchar_t *Variable, short Slot, double *Value);
#endif
    BOOL SetSlotVariable(const char *Variable, short Slot, double Value);
#if !defined(__GNUC__)
    BOOL SetSlotVariable(const wchar_t *Variable, short Slot, double Value);
#endif
    BOOL GetPortVariable(const char *Variable, short Port, double *Value);
#if !defined(__GNUC__)
    BOOL GetPortVariable(const wchar_t *Variable, short Port, double *Value);
#endif
    BOOL SetPortVariable(const char *Variable, short Port, double Value);
#if !defined(__GNUC__)
    BOOL SetPortVariable(const wchar_t *Variable, short Port, double Value);
#endif
    BOOL StepRatio(INT Numerator, INT Denominator, long Axis);
#if !defined(__GNUC__)
    BOOL PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, const char *Variable);
    BOOL PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, LONG Variable);
    BOOL PRMBLK_Define(LONG BlockNumber, enum PRMBLK_Types BlockType, BOOL Append, const char *ProgramName, LONG ProcessNumber, const char *Variable);
    BOOL PRMBLK_Append(LONG BlockNumber, VARIANT Variable);
    BOOL PRMBLK_Get(LONG BlockNumber, int Axis, BOOL All, CString &Values);
    BOOL PRMBLK_Get(LONG BlockNumber, int Axis, BOOL All, VARIANT *Values);
    BOOL PRMBLK_Delete(LONG BlockNumber);
#endif

    CToken(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_ControllerClass);   // constructor
    virtual ~CToken();

protected:
    BOOL AppendToken(char *tokenline, size_t tokenline_size, size_t &tp, const token_t &token) const;
    BOOL AppendToken_SystemVariable(char *tokenline, size_t tokenline_size, size_t &tp, const token_t &token) const;

    void AtlTrace(const char *format, ...) const;
    BOOL IgnoreLine(int timeout) const;
    BOOL Write(char Buffer[], size_t Length, const BOOL p_enableEscape = TRUE) const;
    BOOL ReadByte(char *chr, int timeout) const;
	BOOL ReadByte(unsigned char *chr, int timeout) const { return ReadByte((char *)chr, timeout); }
    BOOL GetNamedVariable(int Variable,double *Value);
    BOOL GetResponse(double *Value, double *Error, BOOL Timeout = TRUE);
    BOOL TokenizeScopeParameter(char *tokenline, size_t tokenlineLength, size_t &tp, const char *Param);
    BOOL SetTableCommand(long StartPosition,long Elements, const double Values[]);
    BOOL SetTableValuesCommand(long StartPosition,long Elements, const double Values[]);
    BOOL GetTableCommand(long StartPosition, long Elements, double *Values);
    BOOL SetVrCommand(int Variable,double Value);
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

#endif
