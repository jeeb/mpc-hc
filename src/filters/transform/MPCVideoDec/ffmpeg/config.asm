%define ARCH_X86 1
%define CONFIG_GPL 1
%define HAVE_AMD3DNOW 1
%define HAVE_AMD3DNOWEXT 1
%define HAVE_MMX 1
%define HAVE_MMX2 1
%define HAVE_SSE 1
%define HAVE_SSSE3 1
%define HAVE_AVX 0

%ifdef ARCH_X86_64
    %define ARCH_X86_32 0
    %define ARCH_X86_64 1
    %define HAVE_FAST_64BIT 1
else
    %define ARCH_X86_32 1
    %define ARCH_X86_64 0
    %define HAVE_FAST_64BIT 0
%endif

%define HAVE_W32THREADS 1
%define HAVE_FAST_UNALIGNED 1
%define HAVE_ALIGNED_STACK 1
%define HAVE_ATTRIBUTE_MAY_ALIAS 1
%define HAVE_ATTRIBUTE_PACKED 1
%define HAVE_BSWAP 1
%define HAVE_CMOV 1
%define HAVE_DLFCN_H 1
%define HAVE_DLOPEN 1
%define HAVE_DOS_PATHS 1
%define HAVE_EBP_AVAILABLE 1
%define HAVE_EBX_AVAILABLE 1
%define HAVE_EXP2 1
%define HAVE_EXP2F 1
%define HAVE_FAST_CLZ 1
%define HAVE_FAST_CMOV 1
%define HAVE_GETPROCESSMEMORYINFO 1
%define HAVE_GETPROCESSTIMES 1
%define HAVE_ISATTY 1
%define HAVE_LLRINT 1
%define HAVE_LLRINTF 1
%define HAVE_LOCAL_ALIGNED_16 1
%define HAVE_LOCAL_ALIGNED_8 1
%define HAVE_LOG2 1
%define HAVE_LOG2F 1
%define HAVE_LRINT 1
%define HAVE_LRINTF 1
%define HAVE_MALLOC_H 1
%define HAVE_MAPVIEWOFFILE 1
%define HAVE_ROUND 1
%define HAVE_ROUNDF 1
%define HAVE_SETMODE 1
%define HAVE_SYMVER 1
%define HAVE_SYMVER_ASM_LABEL 1
%define HAVE_THREADS 1
%define HAVE_TRUNC 1
%define HAVE_TRUNCF 1
%define HAVE_VIRTUALALLOC 1
%define HAVE_XMM_CLOBBERS 1
%define HAVE_YASM 1
