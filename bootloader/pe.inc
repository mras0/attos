struc IMAGE_DOS_HEADER
    .e_magic    resw 1
    .e_cblp     resw 1
    .e_cp       resw 1
    .e_crlc     resw 1
    .e_cparhdr  resw 1
    .e_minalloc resw 1
    .e_maxalloc resw 1
    .e_ss       resw 1
    .e_sp       resw 1
    .e_csum     resw 1
    .e_ip       resw 1
    .e_cs       resw 1
    .e_lfarlc   resw 1
    .e_ovno     resw 1
    .e_res      resw 4
    .e_oemid    resw 1
    .e_oeminfo  resw 1
    .e_res2     resw 10
    .e_lfanew   resd 1
endstruc
IMAGE_DOS_SIGNATURE equ 0x5A4D     ; MZ

struc IMAGE_FILE_HEADER
  .Machine              resw 1
  .NumberOfSections     resw 1
  .TimeDateStamp        resd 1
  .PointerToSymbolTable resd 1
  .NumberOfSymbols      resd 1
  .SizeOfOptionalHeader resw 1
  .Characteristics      resw 1
endstruc

IMAGE_FILE_MACHINE_AMD64 EQU 0x8664

struc IMAGE_OPTIONAL_HEADER32
    .Magic                       resw 1
    .MajorLinkerVersion          resb 1
    .MinorLinkerVersion          resb 1
    .SizeOfCode                  resd 1
    .SizeOfInitializedData       resd 1
    .SizeOfUninitializedData     resd 1
    .AddressOfEntryPoint         resd 1
    .BaseOfCode                  resd 1
    .BaseOfData                  resd 1
    .ImageBase                   resd 1
    .SectionAlignment            resd 1
    .FileAlignment               resd 1
    .MajorOperatingSystemVersion resw 1
    .MinorOperatingSystemVersion resw 1
    .MajorImageVersion           resw 1
    .MinorImageVersion           resw 1
    .MajorSubsystemVersion       resw 1
    .MinorSubsystemVersion       resw 1
    .Win32VersionValue           resd 1
    .SizeOfImage                 resd 1
    .SizeOfHeaders               resd 1
    .CheckSum                    resd 1
    .Subsystem                   resw 1
    .DllCharacteristics          resw 1
    .SizeOfStackReserve          resd 1
    .SizeOfStackCommit           resd 1
    .SizeOfHeapReserve           resd 1
    .SizeOfHeapCommit            resd 1
    .LoaderFlags                 resd 1
    .NumberOfRvaAndSizes         resd 1
    ;IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]
endstruc

struc IMAGE_OPTIONAL_HEADER64
    .Magic                       resw 1
    .MajorLinkerVersion          resb 1
    .MinorLinkerVersion          resb 1
    .SizeOfCode                  resd 1
    .SizeOfInitializedData       resd 1
    .SizeOfUninitializedData     resd 1
    .AddressOfEntryPoint         resd 1
    .BaseOfCode                  resd 1
    .ImageBase                   resq 1
    .SectionAlignment            resd 1
    .FileAlignment               resd 1
    .MajorOperatingSystemVersion resw 1
    .MinorOperatingSystemVersion resw 1
    .MajorImageVersion           resw 1
    .MinorImageVersion           resw 1
    .MajorSubsystemVersion       resw 1
    .MinorSubsystemVersion       resw 1
    .Win32VersionValue           resd 1
    .SizeOfImage                 resd 1
    .SizeOfHeaders               resd 1
    .CheckSum                    resd 1
    .Subsystem                   resw 1
    .DllCharacteristics          resw 1
    .SizeOfStackReserve          resq 1
    .SizeOfStackCommit           resq 1
    .SizeOfHeapReserve           resq 1
    .SizeOfHeapCommit            resq 1
    .LoaderFlags                 resd 1
    .NumberOfRvaAndSizes         resd 1
    ;IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES]
endstruc

struc IMAGE_NT_HEADERS
    .Signature      resd 1
    .FileHeader     resb IMAGE_FILE_HEADER_size
    .OptionalHeader resb IMAGE_OPTIONAL_HEADER64_size
endstruc

IMAGE_NT_SIGNATURE   equ 0x00004550 ; PE00
SIZE_OF_NT_SIGNATURE equ 4

IMAGE_SIZEOF_SHORT_NAME equ 8

struc IMAGE_SECTION_HEADER
  .Name                 resb IMAGE_SIZEOF_SHORT_NAME
  .PhysicalAddress:
  .VirtualSize:
  .Misc                 resd 1
  .VirtualAddress       resd 1
  .SizeOfRawData        resd 1
  .PointerToRawData     resd 1
  .PointerToRelocations resd 1
  .PointerToLinenumbers resd 1
  .NumberOfRelocations  resw 1
  .NumberOfLinenumbers  resw 1
  .Characteristics      resd 1
endstruc

IMAGE_SCN_CNT_CODE                  equ 0x00000020 ; The section contains executable code.
IMAGE_SCN_CNT_INITIALIZED_DATA      equ 0x00000040 ; The section contains initialized data.
IMAGE_SCN_CNT_UNINITIALIZED_DATA    equ 0x00000080 ; The section contains uninitialized data.
IMAGE_SCN_MEM_EXECUTE               equ 0x20000000 ; The section can be executed as code.
IMAGE_SCN_MEM_READ                  equ 0x40000000 ; The section can be read.
IMAGE_SCN_MEM_WRITE                 equ 0x80000000 ; The section can be written to.
