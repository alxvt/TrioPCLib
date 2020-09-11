/* ***********************************************************************
   * Project: coffsreg
   * File: sreg.h
   * Author: Simon Martin
   ***********************************************************************

    Modifications
    0.00 23/06/2005 created
*/

#if !defined(__COFF_H__) && !defined(_WIN32_WCE)
#define __COFF_H__
/*---------------------------------------------------------------------
  -- macros
  ---------------------------------------------------------------------*/
#define MAX_COFF_SECTIONS   50

/*---------------------------------------------------------------------
  -- standard includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- project includes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- data types
  ---------------------------------------------------------------------*/
class coff {
protected:
    typedef struct coff_file_header_t {
        unsigned short  nMagicNumber;               // magic number (0x93) indicates that this is a TMS320C30/40 COFF file
        unsigned short  nSectionHeaders;            // number of section headers
        unsigned long   lTimeDate;                  // time and date stamp; indicates when the file was created
        unsigned long   lSymbolTableFilePointer;    // file pointer; contains the symbol table's start adress
        unsigned long   lSymbolTableEntries;        // the number of entries in the symbol table
        unsigned short  nSizeOfOptionalHeader;      // size of the optional header. 0=> no header; 28 => header
        unsigned short  nFlags;                     // flags (see F_??? defines)
    } coff_file_header_t;

    typedef struct coff_optional_file_header_t {
        unsigned short  nMagicNumber;               // magic number (0x0108)
        unsigned short  nVersionStamp;              // version stamp - gives version of tools
        unsigned long   lSizeOf_text;               // size (in words) of the .text section
        unsigned long   lSizeOf_data;               // size (in words) of the .data section
        unsigned long   lSizeOf_bss;                // size (in words) of the .bss section
        unsigned long   l_textAddress;              // start address of the _text section
        unsigned long   lEntryPoint;                // entry point address
        unsigned long   l_dataAddress;              // start address of _data section
    } coff_optional_file_header_t;

    typedef struct coff_section_header_t {
        char            chSectionName[8];           // section name padded with nulls
        unsigned long   lPhysicalAddress;           // section's physical (run) address
        unsigned long   lVirtualAddress;            // section's virtual (load) address
        unsigned long   lSizeOfSection;             // section's size in words
        unsigned long   lRawDataFilePointer;        // file pointer to the raw data
        unsigned long   lRelocationDataFilePointer; // file pointer to the relocation data entries
        unsigned long   lLineNumberFilePointer;     // file pointer to the line number entries
        unsigned short  nRelocationEntries;         // number of relocation entries;
        unsigned short  nLineNumberEntries;         // number of line number entries
        unsigned short  nFlags;                     // flags (see STYP_??? defines)
        char            chReserved;                 // reserved
        char            chMemoryPage;               // memory page number
    } coff_section_header_t;

    enum coff_section_type_t {
        cst_header, cst_optional_header, cst_section_header, cst_section_data, cst_line_number, cst_symbol_table, cst_string_table
    };

    enum coff_file_type_t {
        cft_tms320, cft_mips
    };

    enum coff_file_type_t m_file_type;
    char *m_raw_data;
    unsigned int m_size_of_char;
    unsigned long m_raw_data_size, m_coff_data_start;
    coff_file_header_t *m_coff_file_header;
    coff_optional_file_header_t *m_coff_optional_file_header;
    coff_section_header_t *m_coff_section_header[MAX_COFF_SECTIONS];

protected:
    int extract_headers();

public:
    coff();
    virtual ~coff();
    int read(const char *file_name);
    int write(const char *file_name);
    int write_stripped(const char *file_name);
    int new_section(const char *section_name, unsigned long physical_address);
    int write_section(int section, long load_address, long size, const char *data);
    int get_sections() const { return m_coff_file_header->nSectionHeaders; }
    char *get_section_name(int section, char *section_name) const;
    int get_section_address(int section) const { return m_coff_section_header[section]->lPhysicalAddress; }
    int get_section_flags(int section) const { return m_coff_section_header[section]->nFlags; }
    int get_section_has_data(int section) const;
    unsigned long get_section_size(int section) const { return m_coff_section_header[section]->lSizeOfSection; }
    int get_size_of_char() const { return m_size_of_char; };
    char *get_section_data_pointer(int section) const { return &m_raw_data[m_coff_section_header[section]->lRawDataFilePointer]; }
};

/*---------------------------------------------------------------------
  -- function prototypes
  ---------------------------------------------------------------------*/

/*---------------------------------------------------------------------
  -- global variables
  ---------------------------------------------------------------------*/

#endif
