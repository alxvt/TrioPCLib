/* ***********************************************************************
   * Project: Controller DLL
   * Author: smartin
   ***********************************************************************
*/

#include "StdAfx.h"

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define MASK(Width) ((((1L << (Width - 1)) - 1L) <<1) | 1)

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "Crc.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
static CRCLibraryType CRCLibrary[] = {
    { "X25", 16, 0x1021, 0xffff, FALSE, FALSE },
    { "CRC-16", 16, 0x8005, 0, FALSE, FALSE },
    { "ARC", 16, 0x8005, 0, TRUE, TRUE },
    { "XMODEM", 16, 0x8408, 0, TRUE, TRUE },
    { "CRC-32", 32, 0x04c11db7,  (long int)0xffffffff, TRUE, TRUE },
    { "", 0, 0, 0, FALSE, FALSE }
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

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::CCRC
// Parameters:  N/A
// Return type: N/A
// Description: Class constructor
// Mods:
// 27/10/95 9:30    SPM     Initial version
CCRC::CCRC() {
    // begin
    // initialize member variables
    m_pCRCData = CRCLibrary;
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::SelectCRCType
// Parameters:  *szName pointer to the string that defines the CRC type
// Return type: BOOL    TRUE =>     CRC type recognized
//                      FALSE =>    not the above
// Description: selects the given CRC type and initializes the class
//              variables
// Mods:
// 27/10/95 9:30    SPM     Initial version
BOOL CCRC::SelectCRCType(const char *szName) {
    // local data
    int i;
    BOOL bOK = TRUE;

    // find this CRC definition
    for (i=0;
        CRCLibrary[i].szName[0] && strcmp(CRCLibrary[i].szName,szName) && bOK;
        i++) {
            // if no definition found then return
            if(!CRCLibrary[i].szName[0]) bOK = FALSE;
    }

    if (bOK) {
        // store formal parameters
        m_pCRCData = &CRCLibrary[i];

        // create the CRC lookup table
        GenerateLookupTable();
    }

    return(bOK);
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::Reflect
// Parameters:  lNumber number to be relfected
//              nWidth  the number of bits used in the number
// Return type: long    the reflection of the given number
// Description: inverts the bit order of the first (right) nWidth bits of the
//              given number
// Mods:
// 27/10/95 9:30    SPM     Initial version
long CCRC::Reflect(long lNumber,int nWidth) {
    // local data
    int     i;
    long    lReflection=0;

    // begin
    // reflect the bits
    for(i=nWidth-1;i>=0;i--) {
        // get the lowest bit in number
        // shift it to the next available bit in the reflection
        // OR it into the reflection
        lReflection|=(lNumber&1)<<i;

        // shift the number down to get the next bit
        lNumber>>=1;
    }

    // return the reflection
    return(lReflection);
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::GenerateLookupTable
// Parameters:  N/A
// Return type: void
// Description: generates the lookup table used by the CRC calculation. In
//              reality, it generates the XOR sequences that the bits in
//              a given byte would generate during the classic implementation
//              the CRC as a modulo 2 polynomial remainder. Taking into
//              account the fact that (a XOR b) XOR c = a XOR (b XOR c), then
//              it is possible to precalculate all the XORs involved and then
//              just apply them when required, avoiding the necessity for
//              succesive bit shifts during the CRC calculation which makes
//              it quicker. if the maths is done by hand, you will realize
//              that the resultant remainder is of nWidth + the width of the
//              data element being processed, but this algorithm only stores
//              nWidth bits. this is because the upper bits that correspond
//              to the input data bits are shifted out of the remainder leaving
//              only the lower nWidth bits. some CRC types invert the input bit
//              order, to take this into account the input byte and resultant
//              remainder are reflected if the reflected input flag is set
// Mods:
// 27/10/95 9:30    SPM     Initial version
void CCRC::GenerateLookupTable() {
    // local data
    long    lRemainder,lTopBit;
    int     i,j;

    // begin
    // create mask to check for the top bit in the register to be checked
    lTopBit = (long)1 << (m_pCRCData->nWidth - 1);

    // process all possible byte combinations
    for(i = 0; i < 256; i++) {
        // if this CRC model has inverted input then invert the bits in the word
        // shift the bits into the upper 8 bits of the remainder
        // store into the remainder
        lRemainder = (m_pCRCData->bReflectInput ? Reflect(i, 8) : (long)i) <<
            (m_pCRCData->nWidth - 8);

        // process all the bits in the input data
        for(j = 0; j < 8; j++)
            // if the msb is set then the next shift will create a number
            // which can be divided by the polynomial (the polynomial has
            // an implied leading 1)
            if(lRemainder & lTopBit)
                // shift the topmost bit out and perform the division
                lRemainder = (lRemainder << 1) ^ m_pCRCData->lPolynomial;
            // just shift the remainder as no division is possible
            else lRemainder <<= 1;

        // if the input is reflected then reflec tue resultant remainder
        // mask out the unused bits
        // store this element
        m_lLookupTable[i] =
            (m_pCRCData->bReflectInput ?
            Reflect(lRemainder, m_pCRCData->nWidth) : lRemainder) &
            MASK(m_pCRCData->nWidth);
    }
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::InitCRC
// Parameters:  N/A
// Return type: void
// Description: initializes the CRC
// Mods:
// 27/10/95 9:30    SPM     Initial version
void CCRC::InitCRC() {
    // begin
    // store the initialization number
    m_lCRC = m_pCRCData->lInit;
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::CRCBuffer
// Parameters:  szBuffer    string to be parsed into the CRC
//              nLength     length of buffer
// Return type: void
// Description: calculates the CRC according to the semi-fast lookup table
//              algorithm. there is a faster algorithm, but it was not
//              implemented as it was not fully understood at the moment that
//              this function was written, so it was not possible to debug it
//              correctly.
// Known problems:
// 1) a string of length 0 does not alter the CRC, whic means adding a blank
//    line to a program does not affect the CRC, even though the program
//    is different
// Mods:
// 27/10/95 9:30    SPM     Initial version
// 24/01/96 15:45   SPM     lines terminated with 0xaa to pick up insertion
//                          and/or removal of blank lines
void CCRC::CRCBuffer(const char *szBuffer, int nLength) {
    int     nChar = 0;
    const unsigned char *pChar = (const unsigned char *)szBuffer;

    // if the input is reflected then shift left
    if (m_pCRCData->bReflectInput)
    {
        while (nChar < nLength)
        {
            // calculate the CRC as the existing remainder shifted left to remove the
            // lower 8 bits OR the new character in the upper 8 bits XOR the remainder
            // that the lower 8 bits would have made if the division modulo 2 had been
            // performed
            // store as the new crc
            m_lCRC = ((m_lCRC >> 8) | (*pChar << (m_pCRCData->nWidth - 8))) ^
                m_lLookupTable[m_lCRC & 0xff];
            nChar++;
            pChar++;
        }
    }
    // the input is not reflected so shift right
    else
    {
        while (nChar < nLength)
        {
            // calculate the CRC as the existing remainder shifted right to remove the
            // upper 8 bits OR the new character in the lower 8 bits XOR the remainder
            // that the upper 8 bits would have made if the division modulo 2 had been
            // performed
            // store as the new crc
            m_lCRC = ((m_lCRC << 8) | *pChar) ^
                m_lLookupTable[(m_lCRC >> (m_pCRCData->nWidth - 8)) & 0xff];
            nChar++;
            pChar++;
        }
    }
}
/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::CRCString
// Parameters:  szString    string to be parsed into the CRC
// Return type: void
// Description: calculates the CRC according to the semi-fast lookup table
//              algorithm. there is a faster algorithm, but it was not
//              implemented as it was not fully understood at the moment that
//              this function was written, so it was not possible to debug it
//              correctly.
// Known problems:
// 1) a string of length 0 does not alter the CRC, whic means adding a blank
//    line to a program does not affect the CRC, even though the program
//    is different
// Mods:
// 27/10/95 9:30    SPM     Initial version
// 24/01/96 15:45   SPM     lines terminated with 0xaa to pick up insertion
//                          and/or removal of blank lines
void CCRC::CRCString(const char *szString) {
    // process this string
    CRCBuffer(szString, (int)strlen(szString));
}

/////////////////////////////////////////////////////////////////////////////
// Function:    CCRC::GetCRC
// Parameters:  N/A
// Return type: void
// Description: reads back the CRC. to generate the correct CRC the bits in
//              the bottom of the remainder that havent been processed must
//              be take into account, so the string is processed with some
//              fictitious null elements inserted
// Known problems:
// Mods:
// 27/10/95 9:30    SPM     Initial version
long CCRC::GetCRC() {
    // local data
    int i;

    // begin
    // repeat for the corresponding number of characters (2 for an int 4
    // for a long)
    for(i = 0; i < (m_pCRCData->nWidth / 8); i++)
        // if the input is reflected then shift right
        if (m_pCRCData->bReflectInput)
            // calculate the CRC as the existing remainder shifted left to remove the
            // lower 8 bits XOR the remainder that the lower 8 bits would have made if
            // the division modulo 2 had been performed. no further bits are ORed in as
            // they will never be propagated out to generate a remainder
            // store as the new crc
            m_lCRC = (m_lCRC >> 8) ^ m_lLookupTable[m_lCRC &  0xff];
        // input is not reflected so shift left
        else
            // calculate the CRC as the existing remainder shifted right to remove the
            // upper 8 bits XOR the remainder that the upper 8 bits would have made if
            // the division modulo 2 had been performed. no further bits are ORed in as
            // they will never be propagated out to generate a remainder
            // store as the new crc
            m_lCRC =
                (m_lCRC << 8) ^ m_lLookupTable[(m_lCRC >> (m_pCRCData->nWidth - 8)) & 0xff];

    // if the output is reflected then reflect it
    // return the left nWidth bits as the CRC
    return((m_pCRCData->bReflectOutput ? Reflect(m_lCRC, m_pCRCData->nWidth) : m_lCRC) &
        MASK(m_pCRCData->nWidth));
}