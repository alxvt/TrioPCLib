/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#ifndef __TOKENDLL_H__
#define __TOKENDLL_H__
/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "../Controller/ControllerTypes.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/
#ifdef __cplusplus
extern "C" {
#endif
extern long g_lFlushBeforeWrite;
extern long g_lLastError;

//-- token object manipulation
void * TokenClassNew(fireEventContext_t p_fireEventClass, fireEventFunction_t p_fireEventFunction, void *p_controller);
void TokenClassDelete(void *p_token);
BOOL TokenIsError(void *p_tokenClass);

//-- methods
BOOL TokenSetControllerVersion(void *p_tokenClass, double p_controllerVersion);
BOOL TokenSetControllerType(void *p_tokenClass, int p_controllerType);
BOOL TokenGetTable(void *p_tokenClass, long StartPosition, long Elements, double *Values);
BOOL TokenGetSelected(void *p_tokenClass, CString &SelectedProgram);
BOOL TokenInsertLine(void *p_tokenClass, const char *Program,short LineNumber, const char *Line);
BOOL TokenSelect(void *p_tokenClass, const char *Program);
BOOL TokenDir(void *p_tokenClass, CString &Dir, const char *csOption);
BOOL TokenNew(void *p_tokenClass, const char *Program);
BOOL TokenSetTable(void *p_tokenClass, long StartPosition, long Elements, const double Values[]);
BOOL TokenGetNamedVariable(void *p_tokenClass, int Variable,int Process, double *ReturnValue);
BOOL TokenExecuteCommand(void *p_tokenClass, const char *Command);
BOOL TokenLinputCommand(void *p_tokenClass, short Channel, short StartVr);
BOOL TokenKeyCommand(void *p_tokenClass, short Channel, short *Value);
BOOL TokenInputCommand(void *p_tokenClass, short Channel,double *Value);
BOOL TokenGetCommand(void *p_tokenClass, short Channel,short *Value);
BOOL TokenProcessCommand(void *p_tokenClass, const char *Command, const char *Program, double Process, double *ReturnValue); // run a process command
BOOL TokenCommand(void *p_tokenClass, const char *Command);
BOOL TokenMotionCommand(void *p_tokenClass, const char *Command,int Elements, const double Array[], int Base, double *ReturnValue); // run a motion command
BOOL TokenArrayCommand(void *p_tokenClass, const char *Command,int Elements, const double Array[], double *ReturnValue); // run a standard command
BOOL TokenGetVariable(void *p_tokenClass, const char *Variable, double *Value);    // get the variable value
BOOL TokenSetVariable(void *p_tokenClass, const char *Variable,double Value);                     // set the variable to value
BOOL TokenGetVr(void *p_tokenClass, int nVr, double *Value);    // get the variable value
BOOL TokenSetVr(void *p_tokenClass, int nVr, double Value);    // set the variable value
BOOL TokenGetTokenTable(void *p_tokenClass);                                           // get the token table
BOOL TokenMarkCommand(void *p_tokenClass, short Axis, short *Value, short MarkNumber);
BOOL TokenScope5(void *p_tokenClass, BOOL On, long SamplePeriod, long TableStart, long TableEnd, const CStringList &ParamList);
BOOL TokenScope1(void *p_tokenClass, BOOL On);
BOOL TokenTrigger(void *p_tokenClass);
BOOL TokenMechatroLink(void *p_tokenClass, short Module, short Function, short NumberOfParameters, const double MLParameters[], double *Result);
BOOL TokenGetAxisVariable(void *p_tokenClass, const char *Variable, short Axis, double *Value);    // get the variable value with the AXIS modifier
BOOL TokenSetAxisVariable(void *p_tokenClass, const char *Variable, short Axis, double Value);      // set the variable to value with the AXIS modifier
BOOL TokenGetProcessVariable(void *p_tokenClass, const char *Variable, short Process, double *Value);    // get the variable value with the AXIS modifier
BOOL TokenSetProcessVariable(void *p_tokenClass, const char *Variable, short Process, double Value);      // set the variable to value with the AXIS modifier
BOOL TokenGetSlotVariable(void *p_tokenClass, const char *Variable, short Slot, double *Value);    // get the variable value with the AXIS modifier
BOOL TokenSetSlotVariable(void *p_tokenClass, const char *Variable, short Slot, double Value);      // set the variable to value with the AXIS modifier
BOOL TokenGetPortVariable(void *p_tokenClass, const char *Variable, short Port, double *Value);    // get the variable value with the AXIS modifier
BOOL TokenSetPortVariable(void *p_tokenClass, const char *Variable, short Port, double Value);      // set the variable to value with the AXIS modifier
BOOL TokenStepRatio(void *p_tokenClass, INT Numerator, INT Denominator, long Axis);      // set the variable to value with the AXIS modifier
#if !defined(__GNUC__)
BOOL TokenPRMBLK_DefineVariable(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, const char *Variable); // define a parameter block
BOOL TokenPRMBLK_DefineIndex(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, long Index); // define a parameter block
BOOL TokenPRMBLK_DefineProgram(void *p_tokenClass, LONG BlockNumber, enum PRMBLK_Types BlockType, const char *ProgramName, LONG ProcessNumber, const char *Variable); // define a parameter block
BOOL TokenPRMBLK_Append(void *p_tokenClass, LONG BlockNumber, VARIANT Variable); // define a parameter block
BOOL TokenPRMBLK_Get(void *p_tokenClass, LONG BlockNumber, int Axis, BOOL All, VARIANT *Values); // define a parameter block
BOOL TokenPRMBLK_Delete(void *p_tokenClass, LONG BlockNumber);
#endif

#ifdef __cplusplus
}
#endif
/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- implementation
  ---------------------------------------------------------------------*/

#endif
