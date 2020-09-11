/* ***********************************************************************
   * Project: coffsreg
   * File: coff.cpp
   * Author: Simon Martin
   ***********************************************************************

    Modifications
    0.00 23/06/2005 created
*/

#include "StdAfx.h"

#if !defined(_WIN32_WCE)

/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define COFF_FILE_HEADER_MAGIC_NUMBER_TMS           0x93
#define COFF_FILE_HEADER_MAGIC_NUMBER_MIPS          0x166
#define COFF_FILE_OPTIONAL_HEADER_MAGIC_NUMBER_TMS  0x108
#define COFF_FILE_OPTIONAL_HEADER_MAGIC_NUMBER_MIPS 0x10b

// FileHeader flags
#define F_RELFLG    0x0001 // relocation information stripped from the file
#define F_EXEC      0x0002 // the file is executable (has no unresolved external references)
#define F_LNNO      0x0004 // line numbers were stripped from the file
#define F_LSYMS     0x0008 // local symbols were stripped from the file
#define F_VERS      0x0010 // TMS320C40 object code
#define F_LITTLE    0x0100 // object data LSB first

// SectionHeader flags
#define STYP_REG    0x0000 // regular section (allocated, relocated, loaded)
#define STYP_DSECT  0x0001 // dummy section (relocated, not allocated, not loaded)
#define STYP_NOLOAD 0x0002 // noload section (allocated, relocated, not loaded)
#define STYP_GROUP  0x0004 // section is made of multiple input sections
#define STYP_COPY   0x0010 // copy section (not allocated, relocated, loaded; relocation and line number information processed normally
#define STYP_TEXT   0x0020 // section contains executable code
#define STYP_DATA   0x0040 // section contains initialized data
#define STYP_BSS    0x0080 // section contains uninitialized data
#define STYP_ALIGN  0x0f00 // align section by 2^n
#define STYP_BLOCK  0x1000 // use alignment as blocking factor

#ifndef max
#define max(x,y)    ((x) > (y) ? (x) : (y))
#endif

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/
#include "coff.h"

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

coff::coff()
    : m_raw_data(NULL)
    , m_raw_data_size(0)
    , m_coff_file_header(NULL)
    , m_coff_optional_file_header(NULL) {
}

coff::~coff() {
    if (m_raw_data) {
        free(m_raw_data);
        m_raw_data = NULL;
    }
}

int coff::extract_headers() {
    int retval = -1, section;
    long pos;

    // read the headers
    m_coff_file_header = (coff_file_header_t *)m_raw_data;
    pos = sizeof(coff_file_header_t);
    if (COFF_FILE_HEADER_MAGIC_NUMBER_TMS == m_coff_file_header->nMagicNumber) {
        m_file_type = cft_tms320;
        m_size_of_char = 4;
    }
    else if (COFF_FILE_HEADER_MAGIC_NUMBER_MIPS == m_coff_file_header->nMagicNumber) {
        m_file_type = cft_mips;
        m_size_of_char = 1;
    }
    else {
        TRACE("FILE HEADER magic number error [FILE %04x]\n", m_coff_file_header->nMagicNumber);
        goto fail;
    }
    if (m_coff_file_header->nSectionHeaders > MAX_COFF_SECTIONS) {
        TRACE("Too many sections [FILE %i, LIMIT %i]\n", m_coff_file_header->nSectionHeaders, MAX_COFF_SECTIONS);
        goto fail;
    }

    // if there is an optional header then process it
    if (m_coff_file_header->nSizeOfOptionalHeader) {
        m_coff_optional_file_header = (coff_optional_file_header_t *)&m_raw_data[pos];
        pos += m_coff_file_header->nSizeOfOptionalHeader;

        if (((cft_tms320 == m_file_type) && (COFF_FILE_OPTIONAL_HEADER_MAGIC_NUMBER_TMS  != m_coff_optional_file_header->nMagicNumber)) ||
            ((cft_mips   == m_file_type) && (COFF_FILE_OPTIONAL_HEADER_MAGIC_NUMBER_MIPS != m_coff_optional_file_header->nMagicNumber))) {
            TRACE("OPTIONAL FILE HEADER magic number error [FILE %04x]\n", m_coff_optional_file_header->nMagicNumber);
            goto fail;
        }
    }

    // get the section headers
    for (section = 0;section < m_coff_file_header->nSectionHeaders;section++) {
        m_coff_section_header[section] = (coff_section_header_t *)&m_raw_data[pos];
        pos += sizeof(coff_section_header_t);
    }

    // store the start of data index
    m_coff_data_start = pos;

    retval = 0;

fail:
    return retval;
}

int coff::read(const char *filename) {
    int section, retval = -1;
    FILE *f = NULL;
    struct _stat stat_buffer;
    char errstring[256];

    // create a buffer big enough to take the whole file
    if(_stat(filename, &stat_buffer)) {
        strerror_s(errstring, sizeof(errstring), errno);
        TRACE("Error stating %s [%s]\n", filename, errstring);
        goto fail;
    }
    m_raw_data = (char *)malloc(m_raw_data_size = stat_buffer.st_size);
    if(!m_raw_data) {
        TRACE("Cannot create buffer to read file %s\n", filename);
        goto fail;
    }

    // read the file
#ifdef _WIN32
    if (fopen_s(&f, filename, "rb") != 0)
#else
    if ((f = fopen(filename, "rb")) == NULL)
#endif
    {
        strerror_s(errstring, sizeof(errstring), errno);
        TRACE("Error opening %s [%s]\n", filename, errstring);
        goto fail;
    }
    if (!fread(m_raw_data, m_raw_data_size, 1, f)) {
        strerror_s(errstring, sizeof(errstring), errno);
        TRACE("Error reading %s [%s]\n", filename, errstring);
        goto fail;
    }
    fclose(f);
    f = NULL;

    // build the header structure
    extract_headers();

    retval = 0;
    TRACE("--COFF file %s\n", filename);
    TRACE("  Sections=%i\n", m_coff_file_header->nSectionHeaders);
    for ( section = 0; section < m_coff_file_header->nSectionHeaders;section++) {
        char section_name[sizeof(m_coff_section_header[section]->chSectionName) + 1];

        memcpy(section_name, m_coff_section_header[section]->chSectionName, sizeof(m_coff_section_header[section]->chSectionName));
        section_name[sizeof(m_coff_section_header[section]->chSectionName)] = 0;
        TRACE("    %-8s => address = 0x%08lx, length = 0x%08lx, file = 0x%08lx\n", section_name, m_coff_section_header[section]->lPhysicalAddress, m_coff_section_header[section]->lSizeOfSection, m_coff_section_header[section]->lRawDataFilePointer);
    }

fail:
    if (f) {
        fclose(f);
    }

    return retval;
}

int coff::write(const char *filename) {
    int section, retval = -1;
    FILE *f;
    char errstring[256];

    TRACE("--COFF file %s\n", filename);
    TRACE("  Sections=%i\n", m_coff_file_header->nSectionHeaders);
    for ( section = 0; section < m_coff_file_header->nSectionHeaders;section++) {
        char section_name[sizeof(m_coff_section_header[section]->chSectionName) + 1];

        memcpy(section_name, m_coff_section_header[section]->chSectionName, sizeof(m_coff_section_header[section]->chSectionName));
        section_name[sizeof(m_coff_section_header[section]->chSectionName)] = 0;
        TRACE("    %-8s => address = 0x%08lx(0x%08lx), length = 0x%08lx, file = 0x%08lx\n", section_name, m_coff_section_header[section]->lPhysicalAddress, m_coff_section_header[section]->lVirtualAddress, m_coff_section_header[section]->lSizeOfSection, m_coff_section_header[section]->lRawDataFilePointer);
    }

    // write the file
#ifdef _WIN32
    if (fopen_s(&f, filename, "wb") != 0)
#else
    if ((f = fopen(filename, "wt")) == NULL)
#endif
    {
        strerror_s(errstring, sizeof(errstring), errno);
        TRACE("Error opening %s [%s]\n", filename, errstring);
        goto fail;
    }
    if (!fwrite(m_raw_data, m_raw_data_size, 1, f)) {
        strerror_s(errstring, sizeof(errstring), errno);
        TRACE("Error reading %s [%s]\n", filename, errstring);
        goto fail;
    }

    retval = 0;

fail:
    if (f) {
        fclose(f);
    }

    return retval;
}

int coff::new_section(const char *section_name, unsigned long physical_address) {
    int section = -1;
    char *aux;
    unsigned long raw_data_pointer = 0;

    // sanity check
    if (m_coff_file_header->nSectionHeaders >= MAX_COFF_SECTIONS) {
        TRACE("Too many sections adding %s\n", section_name);
        goto fail;
    }

    // assign memory
    aux = (char *)realloc(m_raw_data, m_raw_data_size + sizeof(coff_section_header_t));
    if (!aux) {
        TRACE("Cannot create buffer for section %s\n", section_name);
        goto fail;
    }
    m_raw_data = aux;

    // rebuild headers
    extract_headers();

    // create space
    memmove(
        &m_raw_data[m_coff_data_start + sizeof(coff_section_header_t)],
        &m_raw_data[m_coff_data_start],
        m_raw_data_size - m_coff_data_start
    );
    m_raw_data_size += sizeof(coff_section_header_t);

    // adjust existing headers
    if(m_coff_file_header->lSymbolTableFilePointer) m_coff_file_header->lSymbolTableFilePointer += sizeof(coff_section_header_t);
    for (section = 0;section < m_coff_file_header->nSectionHeaders;section++) {
        if(m_coff_section_header[section]->lRawDataFilePointer) m_coff_section_header[section]->lRawDataFilePointer += sizeof(coff_section_header_t);
        if(m_coff_section_header[section]->lRelocationDataFilePointer) m_coff_section_header[section]->lRelocationDataFilePointer += sizeof(coff_section_header_t);
        if(m_coff_section_header[section]->lLineNumberFilePointer) m_coff_section_header[section]->lLineNumberFilePointer += sizeof(coff_section_header_t);
        if(get_section_has_data(section)) raw_data_pointer = max(raw_data_pointer, m_coff_section_header[section]->lRawDataFilePointer + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char));
    }

    // create new header
    m_coff_section_header[m_coff_file_header->nSectionHeaders] = m_coff_section_header[m_coff_file_header->nSectionHeaders - 1] + 1;
    memset(m_coff_section_header[m_coff_file_header->nSectionHeaders]->chSectionName, 0, sizeof(m_coff_section_header[m_coff_file_header->nSectionHeaders]->chSectionName));
    memcpy(m_coff_section_header[m_coff_file_header->nSectionHeaders]->chSectionName, section_name, strlen(section_name));
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lPhysicalAddress = physical_address;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lVirtualAddress = physical_address;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lSizeOfSection = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lRawDataFilePointer = raw_data_pointer;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lRelocationDataFilePointer = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->lLineNumberFilePointer = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->nRelocationEntries = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->nLineNumberEntries = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->nFlags = STYP_DATA | STYP_GROUP;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->chReserved = 0;
    m_coff_section_header[m_coff_file_header->nSectionHeaders]->chMemoryPage = 0;

    // create return value
    section = m_coff_file_header->nSectionHeaders++;

fail:
    return section;
}

int coff::write_section(int section, long load_address, long size, const char *data) {
    char *aux;
    int i, retval = -1;

    // round to a word boundary
    if (size & 3) size += 4 - (size & 3);

    // assign memory
    size += 8;
    aux = (char *)realloc(m_raw_data, m_raw_data_size + size);
    if (!aux) {
        TRACE("Cannot create buffer for section %i\n", section);
        goto fail;
    }
    m_raw_data = aux;

    // rebuild headers
    extract_headers();

    // create space
    memmove(
        &m_raw_data[m_coff_section_header[section]->lRawDataFilePointer] + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char) + size,
        &m_raw_data[m_coff_section_header[section]->lRawDataFilePointer] + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char),
        m_raw_data_size - (m_coff_section_header[section]->lRawDataFilePointer + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char))
    );
    m_raw_data_size += size;

    // adjust headers
    if(m_coff_file_header->lSymbolTableFilePointer) m_coff_file_header->lSymbolTableFilePointer += size;
    for (i = 0;i < m_coff_file_header->nSectionHeaders;i++) {
        if (i > section) {
            if(m_coff_section_header[i]->lRawDataFilePointer) m_coff_section_header[i]->lRawDataFilePointer += size;
        }
        if(m_coff_section_header[i]->lRelocationDataFilePointer) m_coff_section_header[i]->lRelocationDataFilePointer += size;
        if(m_coff_section_header[i]->lLineNumberFilePointer) m_coff_section_header[i]->lLineNumberFilePointer += size;
    }

    // store data
    for (i = 0; i < 4;i++) {
        m_raw_data[m_coff_section_header[section]->lRawDataFilePointer + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char) + i] = (char)((size - 8) >> (i * 8)) & 0xff;
    }
    for (i = 4; i < 8;i++) {
        m_raw_data[m_coff_section_header[section]->lRawDataFilePointer + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char) + i] = (char)(load_address >> (i * 8)) & 0xff;
    }
    memcpy(&m_raw_data[m_coff_section_header[section]->lRawDataFilePointer + (m_coff_section_header[section]->lSizeOfSection * m_size_of_char) + i], data, size - i);
    m_coff_section_header[section]->lSizeOfSection += size / m_size_of_char;

    retval = 0;

fail:
    return retval;
}

int coff::get_section_has_data(int section) const {
    return (m_coff_section_header[section]->nFlags & (STYP_TEXT | STYP_DATA)) ? 1 : 0;
}

char *coff::get_section_name(int section, char *section_name) const {
    memcpy(section_name, m_coff_section_header[section]->chSectionName, sizeof(m_coff_section_header[section]->chSectionName));
    section_name[sizeof(m_coff_section_header[section]->chSectionName)] = 0;

    return section_name;
}

#endif
