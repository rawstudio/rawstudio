#ifndef __X86__CPU_H__
#define __X86__CPU_H__

typedef enum {
	RS_CPU_FLAG_MMX   = 1<<0,
	RS_CPU_FLAG_SSE   = 1<<1,
	RS_CPU_FLAG_CMOV  = 1<<2,
	RS_CPU_FLAG_3DNOW = 1<<3,
	RS_CPU_FLAG_3DNOW_EXT = 1<<4,
	RS_CPU_FLAG_AMD_ISSE  = 1<<5,
	RS_CPU_FLAG_SSE2 =  1<<6,
	RS_CPU_FLAG_SSE3 =  1<<7,
	RS_CPU_FLAG_SSSE3 =  1<<8,
	RS_CPU_FLAG_SSE4_1 =  1<<9,
	RS_CPU_FLAG_SSE4_2 =  1<<10,
	RS_CPU_FLAG_AVX =  1<<11
} RSCpuFlags;

#if defined(__x86_64__)
#  define REG_a "rax"
#  define REG_b "rbx"
#  define REG_c "rcx"
#  define REG_d "rdx"
#  define REG_D "rdi"
#  define REG_S "rsi"
#  define PTR_SIZE "8"

#  define REG_SP "rsp"
#  define REG_BP "rbp"
#  define REGBP   rbp
#  define REGa    rax
#  define REGb    rbx
#  define REGc    rcx
#  define REGSP   rsp

#else

#  define REG_a "eax"
#  define REG_b "ebx"
#  define REG_c "ecx"
#  define REG_d "edx"
#  define REG_D "edi"
#  define REG_S "esi"
#  define PTR_SIZE "4"

#  define REG_SP "esp"
#  define REG_BP "ebp"
#  define REGBP   ebp
#  define REGa    eax
#  define REGb    ebx
#  define REGc    ecx
#  define REGSP   esp
#endif

#endif
