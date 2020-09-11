///////////////////////////////////////////////////////////////////////////////
// File:		CRC.H
// Description:	Contains the CCRC prototype. This class calculates the
//				CRC for a given character sequence using a fast lookup table
//				based algorithm
// Copyight Trio Motion Technology 1995

#ifndef __CRC_H__
#define __CRC_H__

#if _MSC_VER >= 1000
#pragma once
#endif // _MSC_VER >= 1000

// CRC library type
struct CRCLibraryType {
	char	szName[10];
	int		nWidth;
	long	lPolynomial, lInit;
	BOOL	bReflectInput, bReflectOutput;
};

class CCRC {
protected:
	CRCLibraryType *m_pCRCData;
	
	// lookup table
	long	m_lLookupTable[256];

	// crc calculation
	long	m_lCRC;
	
public:
			CCRC();
	BOOL	SelectCRCType(const char *szName);
	void	InitCRC();
	void	CRCString(const char *szString);
	void	CRCBuffer(const char *szBuffer, int nLength);
	long	GetCRC();
	
protected:
	long	Reflect(long lNumber, int nWidth);
	void	GenerateLookupTable();
};
#endif
