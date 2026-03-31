//
// Created by YWvin on 2026/3/29.
//

#include "SIMD.hpp"
// For checking SIMD support on the CPU, MSVC uses intrin.h, while clang/gcc use cpuid.h
#ifdef _MSC_VER
#include <intrin.h>
#else
#include <cpuid.h>
#endif


void SIMDManager::CheckSIMDSupport()
{
    uint32_t Eax = 0;
    uint32_t Ebx = 0;
    uint32_t Ecx = 0;
    uint32_t Edx = 0;
    QueryCPUInfo(1, 0, Eax, Ebx, Ecx, Edx);

    // Bit wise stuffs to get the ECX bit 27 of leaf 1 to check for OSXSAVE
    // We need this thing to call xgetbv, otherwise we crash
    const bool HasOSXSAVE = (Ecx & (1 << 27)) != 0;
    // Bit 28 of leaf 1, AVX support
    const bool HasAVX = (Ecx & (1 << 28)) != 0;

    // If either of the above fails(is 0), we fall back to SSE2, which should widely supported right now?
    // I don't think anyone will run this on a 32-bit machine
    if (!HasOSXSAVE || !HasAVX)
    {
        m_SIMDLevel = SIMDLevel::Scalar;
        return;
    }

    /*
     * If we get here, can start checking for XGETBV
     * This basically means that the OS is able to save the contents in the wide registers
     * If it does not, then the register's contents get corrupted when we do things like context-switching
     * And the program explodes
     * We use xgetbv(0) to for all three compilers
     */
    const uint64_t Xcr0 = _xgetbv(0);
    const bool DoesOSSupportAVX = (Xcr0 & 0x06) == 0x06; // Bit 1 and Bit 2, for XMM and YMM (0-index, 0x06 = 00000110)
    // XMM - 128, YMM - 256, ZMM - 512
    if (!DoesOSSupportAVX)
    {
        m_SIMDLevel = SIMDLevel::SSE2;
        return;
    }
    // OS supports AVX, let's check CPUID for AVX2 and AVX512 support
    // Apparently these are called Extended Features
    QueryCPUInfo(7, 0, Eax, Ebx, Ecx, Edx);
    const bool HasAVX2 = (Edx & (1 << 5)) != 0;
    const bool HasAVX512F = (Edx & (1 << 16)) != 0;
    if (!HasAVX2)
    {
        m_SIMDLevel = SIMDLevel::SSE2;
        return;
    }
    // Check OS support for AVX512
    // Which are stored in bits 5, 6, 7 of Xcr0
    const bool DoesOSSupportAVX512 = (Xcr0 & 0xE0) == 0xE0;
    if (DoesOSSupportAVX512 && HasAVX512F)
    {
        m_SIMDLevel = SIMDLevel::AVX512;
    }
    else
    {
        m_SIMDLevel = SIMDLevel::AVX2;
    }
}

uint32_t SIMDManager::GetSIMDWidth() const {
    // Little helper to return how many floats we process given a SIMDLevel
    // Use this to help with loop iteration and increments, plus remainders
    // for example, using SSE2 on a 7-item array will leave out the last three
    switch (m_SIMDLevel)
    {
        case SIMDLevel::Scalar:
            return 1;
        case SIMDLevel::SSE2:
            return 4;
        case SIMDLevel::AVX2:
            return 8;
        case SIMDLevel::AVX512:
            return 16;
        default:
            return 1;
    }
}

void SIMDManager::QueryCPUInfo(int LeafNumber, int SubleafNumber, uint32_t &Eax, uint32_t &Ebx, uint32_t &Ecx,
    uint32_t &Edx)
{
    // x86 CPUs give an instruction called CPUID, it reports what features are supported by the CPU
    // We can invoke it and give it a leaf number(a bit similar to a query ID) to make it fill four registers
    // With bits indicating feature supports
    Eax = 0;
    Ebx = 0;
    Ecx = 0;
    Edx = 0;
#ifdef _MSC_VER
    //MSVC, use __cpuidex to write to an int[4]
    int CpuInfo[4] = {};
    __cpuidex(CpuInfo, LeafNumber, SubleafNumber);
    Eax = static_cast<uint32_t>(CpuInfo[0]);
    Ebx = static_cast<uint32_t>(CpuInfo[1]);
    Ecx = static_cast<uint32_t>(CpuInfo[2]);
    Edx = static_cast<uint32_t>(CpuInfo[3]);
#else
    // Clang/GCC, use __get_cpuid_count. This writes two individual uint
    // It returns 0 if a leaf is not supported
    __get_cpuid_count(LeafNumber, SubleafNumber, &Eax, &Ebx, &Ecx, &Edx);
    __get_cpuid_count(LeafNumber, SubleafNumber, &Eax, &Ebx, &Ecx, &Edx);
#endif
}
