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
#define new DEBUG_NEW
#endif

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include "TokenLib.h"
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

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

//-- token object manipulation
void * TokenClassNew(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_controllerClass)
{
    if (NULL == p_controllerClass) return NULL;
    else return new CToken(p_fireEventClass, p_fireEventFunction, p_controllerClass);
}

void TokenClassDelete(void *p_tokenClass)
{
    if (NULL == p_tokenClass) return;
    delete (CToken*)p_tokenClass;
}

BOOL TokenIsError(void *p_tokenClass)
{
	return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->IsError();
}

BOOL TokenSetControllerVersion(void *p_tokenClass, double p_controllerVersion)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetControllerVersion(p_controllerVersion);
}

BOOL TokenSetControllerType(void *p_tokenClass, int p_controllerType)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetControllerType(p_controllerType);
}

BOOL TokenGetTable(void *p_tokenClass, long StartPosition,long Elements,double *Values)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetTable(StartPosition, Elements, Values);
}

BOOL TokenGetSelected(void *p_tokenClass, CStringA &SelectedProgram)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetSelected(SelectedProgram);
}

BOOL TokenInsertLine(void *p_tokenClass, const char *Program, short LineNumber, const char *Line)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->InsertLine(Program, LineNumber, Line);
}

BOOL TokenSelect(void *p_tokenClass, const char *Program)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Select(Program);
}

BOOL TokenDir(void *p_tokenClass, CStringA &Dir, const char *Option)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Dir(Dir, Option);
}

BOOL TokenNew(void *p_tokenClass, const char *Program)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->New(Program);
}

BOOL TokenSetTable(void *p_tokenClass, long StartPosition,long Elements, const double Values[])
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetTable(StartPosition, Elements, Values);
}

BOOL TokenGetNamedVariable(void *p_tokenClass, int Variable,int Process, double *ReturnValue)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetNamedVariable(Variable, Process, ReturnValue);
}

BOOL TokenExecuteCommand(void *p_tokenClass, const char *Command)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->ExecuteCommand(Command);
}

BOOL TokenLinputCommand(void *p_tokenClass, short Channel,short StartVr)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->LinputCommand(Channel, StartVr);
}

BOOL TokenKeyCommand(void *p_tokenClass, short Channel,short *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->KeyCommand(Channel, Value);
}

BOOL TokenInputCommand(void *p_tokenClass, short Channel,double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->InputCommand(Channel, Value);
}

BOOL TokenGetCommand(void *p_tokenClass, short Channel,short *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetCommand(Channel, Value);
}

BOOL TokenProcessCommand(void *p_tokenClass, const char *Command, const char *Program, double Process, double *ReturnValue)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->ProcessCommand(Command, Program, Process, ReturnValue);
}

BOOL TokenCommand(void *p_tokenClass, const char *Command)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Command(Command);
}

BOOL TokenMotionCommand(void *p_tokenClass, const char *Command,int Elements,const double Array[],int Base, double *ReturnValue)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->MotionCommand(Command, Elements, Array, Base, ReturnValue);
}

BOOL TokenArrayCommand(void *p_tokenClass, const char *Command,int Elements,const double Array[],double *ReturnValue)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->ArrayCommand(Command, Elements, Array, ReturnValue);
}

BOOL TokenGetVariable(void *p_tokenClass, const char *Variable, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetVariable(Variable, Value);
}

BOOL TokenSetVariable(void *p_tokenClass, const char *Variable,double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetVariable(Variable, Value);
}

BOOL TokenGetVr(void *p_tokenClass, int Vr, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetVr(Vr, Value);
}

BOOL TokenSetVr(void *p_tokenClass, int Vr, double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetVr(Vr, Value);
}

BOOL TokenGetTokenTable(void *p_tokenClass)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetTokenTable();
}

BOOL TokenMarkCommand(void *p_tokenClass, short Axis, short *Value, short MarkNumber)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->MarkCommand(Axis, Value, MarkNumber);
}

BOOL TokenScope1(void *p_tokenClass, BOOL On)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Scope(On);
}

BOOL TokenScope5(void *p_tokenClass, BOOL On, long SamplePeriod, long TableStart, long TableEnd, const CStringList &ParamList)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Scope(On, SamplePeriod, TableStart, TableEnd, ParamList);
}

BOOL TokenTrigger(void *p_tokenClass)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->Trigger();
}

BOOL TokenMechatroLink(void *p_tokenClass, short Module, short Function, short NumberOfParameters, const double MLParameters[], double FAR* pdResult)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->MechatroLink(Module, Function, NumberOfParameters, MLParameters, pdResult);
}

BOOL TokenGetAxisVariable(void *p_tokenClass, const char *Variable, short Axis, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetAxisVariable(Variable, Axis, Value);
}

BOOL TokenSetAxisVariable(void *p_tokenClass, const char *Variable,short Axis, double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetAxisVariable(Variable, Axis, Value);
}

BOOL TokenGetProcessVariable(void *p_tokenClass, const char *Variable, short Process, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetProcessVariable(Variable, Process, Value);
}

BOOL TokenSetProcessVariable(void *p_tokenClass, const char *Variable,short Process, double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetProcessVariable(Variable, Process, Value);
}

BOOL TokenSetSlotVariable(void *p_tokenClass, const char *Variable,short Slot, double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetSlotVariable(Variable, Slot, Value);
}

BOOL TokenGetSlotVariable(void *p_tokenClass, const char *Variable, short Slot, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetSlotVariable(Variable, Slot, Value);
}

BOOL TokenSetPortVariable(void *p_tokenClass, const char *Variable,short Port, double Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->SetPortVariable(Variable, Port, Value);
}

BOOL TokenGetPortVariable(void *p_tokenClass, const char *Variable, short Port, double *Value)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->GetPortVariable(Variable, Port, Value);
}

BOOL TokenStepRatio(void *p_tokenClass, INT Numerator, INT Denominator, long Axis)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->StepRatio(Numerator, Denominator, Axis);
}

#if !defined(__GNUC__)
BOOL TokenPRMBLK_DefineVariable(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, const char *Variable)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Define(BlockNumber, BlockType, FALSE, Variable);
}

BOOL TokenPRMBLK_DefineIndex(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, LONG Variable)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Define(BlockNumber, BlockType, FALSE, Variable);
}

BOOL TokenPRMBLK_DefineProgram(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, const char *ProgramName, LONG ProcessNumber, const char *Variable)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Define(BlockNumber, BlockType, FALSE, ProgramName, ProcessNumber, Variable);
}

BOOL TokenPRMBLK_Append(void *p_tokenClass, LONG BlockNumber, VARIANT Variable)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Append(BlockNumber, Variable);
}

BOOL TokenPRMBLK_Get(void *p_tokenClass, LONG BlockNumber, int Axis, BOOL All, VARIANT *Values)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Get(BlockNumber, Axis, All, Values);
}

BOOL TokenPRMBLK_Delete(void *p_tokenClass, LONG BlockNumber)
{
    return (NULL == p_tokenClass) ? FALSE : ((CToken *)p_tokenClass)->PRMBLK_Delete(BlockNumber);
}
#endif

