
#pragma once

#include <cstdint>

/*
 * A class that is responsible for checking simd supports, and provide wrappers around all the
 * SIMD intrinsics, as of right now, it ONLY supports x86-64 SIMD intrinsics
 * No plan for ARM support for now
 */



enum class SIMDLevel : uint8_t
{
    Scalar,
    SSE2, // 128-bit, process 4 floats at a time. This should be guaranteed on all x64 CPUs
    AVX2, // 256-bit, process 8 floats at a time
    AVX512, // 512-bit, process 16 floats at a time
    NEON // 128-bit, for ARM NEON
};

class SIMDManager
{
public:
    SIMDManager() = default;
    ~SIMDManager() = default;

    void CheckSIMDSupport();

    [[nodiscard]] uint32_t GetSIMDWidth() const;
    [[nodiscard]] SIMDLevel GetSIMDLevel() const
    {
        return m_SIMDLevel;
    }

private:
#if defined(__x86_64__) || defined(_M_X64)
    // x86-specific helper used by the CPUID-based detection path.
    void QueryCPUInfo(int LeafNumber, int SubleafNumber, uint32_t& Eax, uint32_t& Ebx, uint32_t& Ecx, uint32_t& Edx);
    SIMDLevel m_SIMDLevel = SIMDLevel::SSE2;
    // For NEON and other types of architectures, we don't need or define the above functions
    // Just set the width to the specific value
#elif defined(__aarch64__) || defined(_M_ARM64)
    SIMDLevel m_SIMDLevel = SIMDLevel::NEON;
#else
    SIMDLevel m_SIMDLevel = SIMDLevel::Scalar;
#endif
};

// Everything below this point is x86-64 only
#if defined(__x86_64__) || defined(_M_X64)
// For both SIMD intrinsics and XGETBV to check OS support for SIMD
#include <immintrin.h>
/*
 * Using explicit template specialization to provide SIMD traits for different levels
 * This saves us the trouble of writing multiple versions or ifdefs for different SIMD levels
 * I am not sure I am doing this correctly
 * But basically, it should allow us to define template functions/structs for explicitly set
 * template values
 * For example:
 *  template<int I>
 *  bool PrimaryTemplateFunc();
 *
 *  template<>
 *  bool SpecializeOneFunc<1>() { return false;}
 *  These lines would have created a template function which takes an integer template param
 *  And a specialization that returns false if the template param is 1
 */

// Declare an empty template struct for SIMD traits
// Note how we have no definitions, this is a bit similar to a forward declaration of class, struct, etc
// Without specializations we wrote below this would fail to compile if used
template<SIMDLevel>
struct SIMDTraits;


template<>
struct SIMDTraits<SIMDLevel::SSE2>
{
    static constexpr uint32_t SIMDWidth = 4;

    /*
     * Load 4 floats from memory into an SSE 128-bit wide register
     * the 'u' after load means it's unaligned, so our pointer's destination address
     * does not need to be aligned to 16-byte boundary. If we don't use the unaligned variant
     * and do not align our pointers input, things explode
     */
    static __m128 VectorizedLoad(const float* SrcPtr)
    {
        /*
         * Instrinsics have cursed syntaxes, leaving notes for my own sanity
         * _mm_ = SSE2 128 bit, _mm256_ = AVX, 256 bit, _mm512_ = AVX512, 512 bit
         * loadu = load unaligned, this loads things from memory into the register
         * set1 = set all of the register to a single value, similar to broadcast?
         * ps = packed, single-precision floats(4 floats)
         */
        return _mm_loadu_ps(SrcPtr);
    }
    // Set all of the wide register lanes to a single value(broadcast)
    static __m128 VectorizedBroadcast(const float& Val)
    {
        return _mm_set1_ps(Val);
    }
    // Zero out all lanes
    static __m128 VectorizedZero()
    {
        return _mm_setzero_ps();
    }
    // Store 4/8/16 floats, using the unaligned variant for the same reason above
    static void VectorizedStore(float* DstPtr, __m128 SrcVector)
    {
        _mm_storeu_ps(DstPtr, SrcVector);
    }
    // Add two register element by element, basically adding the contents of two 4-float wide register
    static __m128 VectorizedAdd(__m128 Lhs, __m128 Rhs)
    {
        return _mm_add_ps(Lhs, Rhs);
    }
    // Subtract two register element by element
    static __m128 VectorizedSub(__m128 Lhs, __m128 Rhs)
    {
        return _mm_sub_ps(Lhs, Rhs);
    }
    // Multiply two register element by element
    static __m128 VectorizedMul(__m128 Lhs, __m128 Rhs)
    {
        return _mm_mul_ps(Lhs, Rhs);
    }
    // Division
    static __m128 VectorizedDiv(__m128 Lhs, __m128 Rhs)
    {
        return _mm_div_ps(Lhs, Rhs);
    }
    /*
     * Reciprocal sqrt, 1/sqrt(input). For each lane
     * This is an approximation of the rsqrt
     * But for our purposes it's good enough
     */
    static __m128 VectorizedRSqrt(__m128 SrcVector)
    {
        return _mm_rsqrt_ps(SrcVector);
    }

    // Bitwise AND across all lanes, not sure if it's gonna be useful
    // Maybe we can do some masking with this?
    static __m128 VectorizedAnd(__m128 Lhs, __m128 Rhs)
    {
        return _mm_and_ps(Lhs, Rhs);
    }

    // Per pair comparison, return the larger of the pairs
    static __m128 VectorizedMax(__m128 Lhs, __m128 Rhs)
    {
        return _mm_max_ps(Lhs, Rhs);
    }
    // Per pair comparison, return the smaller of the pairs
    static __m128 VectorizedMin(__m128 Lhs, __m128 Rhs)
    {
        return _mm_min_ps(Lhs, Rhs);
    }
};

// Same stuffs, just for AVX2
template<>
struct SIMDTraits<SIMDLevel::AVX2>
{
    static constexpr uint32_t SIMDWidth = 8;
    // Load
    static __m256 VectorizedLoad(const float* SrcPtr)
    {
        return _mm256_loadu_ps(SrcPtr);
    }
    // Store
    static void VectorizedStore(float* DstPtr, __m256 SrcVector)
    {
        _mm256_storeu_ps(DstPtr, SrcVector);
    }
    // Broadcast
    static __m256 VectorizedBroadcast(const float& Val)
    {
        return _mm256_set1_ps(Val);
    }
    // Zero
    static __m256 VectorizedZero()
    {
        return _mm256_setzero_ps();
    }
    // Add
    static __m256 VectorizedAdd(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_add_ps(Lhs, Rhs);
    }
    // Subtract
    static __m256 VectorizedSub(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_sub_ps(Lhs, Rhs);
    }
    // Multiply
    static __m256 VectorizedMul(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_mul_ps(Lhs, Rhs);
    }
    // Division
    static __m256 VectorizedDiv(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_div_ps(Lhs, Rhs);
    }
    // Rsqrt
    static __m256 VectorizedRSqrt(__m256 SrcVector)
    {
        return _mm256_rsqrt_ps(SrcVector);
    }
    // AND
    static __m256 VectorizedAnd(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_and_ps(Lhs, Rhs);
    }

    // Max
    static __m256 VectorizedMax(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_max_ps(Lhs, Rhs);
    }
    // Min
    static __m256 VectorizedMin(__m256 Lhs, __m256 Rhs)
    {
        return _mm256_min_ps(Lhs, Rhs);
    }
};

// AVX512 variants
template<>
struct SIMDTraits<SIMDLevel::AVX512>
{
    static constexpr uint32_t SIMDWidth = 16;

    // Load
    static __m512 VectorizedLoad(const float* SrcPtr)
    {
        return _mm512_loadu_ps(SrcPtr);
    }
    // Store
    static void VectorizedStore(float* DstPtr, __m512 SrcVector)
    {
        _mm512_storeu_ps(DstPtr, SrcVector);
    }
    // Broadcast
    static __m512 VectorizedBroadcast(const float& Val)
    {
        return _mm512_set1_ps(Val);
    }
    // Zero out
    static __m512 VectorizedZero()
    {
        return _mm512_setzero_ps();
    }
    // Add
    static __m512 VectorizedAdd(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_add_ps(Lhs, Rhs);
    }
    // Subtract
    static __m512 VectorizedSub(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_sub_ps(Lhs, Rhs);
    }
    // Multiply
    static __m512 VectorizedMul(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_mul_ps(Lhs, Rhs);
    }
    // Div
    static __m512 VectorizedDiv(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_div_ps(Lhs, Rhs);
    }
    // Rsqrt
    static __m512 VectorizedRSqrt(__m512 SrcVector)
    {
        // AVX512's rsqrt is a bit different
        // It got 14-bit precision as shown in the name, compared to AVX and SSE2's 12-bit
        return _mm512_rsqrt14_ps(SrcVector);
    }
    // AND
    static __m512 VectorizedAnd(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_and_ps(Lhs, Rhs);
    }

    // Max
    static __m512 VectorizedMax(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_max_ps(Lhs, Rhs);
    }
    // Min
    static __m512 VectorizedMin(__m512 Lhs, __m512 Rhs)
    {
        return _mm512_min_ps(Lhs, Rhs);
    }
};
#endif // defined(__x86_64__) || defined(_M_X64)
#if defined(__aarch64__) || defined(_M_ARM64)
#include <arm_neon.h>
// ARM NEON intrinsics. NEON seems to only support up to 128-bit wide operations
// NEON intrinsics naturally tolerates unaligned operations
template<>
struct SIMDTraits<SIMDLevel::NEON>
{
    static constexpr uint32_t SIMDWidth = 4;
    // NOT The poly128_t type, that's for crypto!
    static float32x4_t VectorizedLoad(const float* SrcPtr)
    {
        return vld1q_f32(SrcPtr);
    }
    // Set all of the wide register lanes to a single value(broadcast)
    static float32x4_t VectorizedBroadcast(const float& Val)
    {
        return vdupq_n_f32(Val);
    }
    /*
     * NEON has no dedicated "set lanes to zero" intrinsic. Broadcasting 0 is the
     * conventional idiom — the compiler pattern-matches this to a single MOVI #0
     * instruction (no actual splat is emitted)
     */
    static float32x4_t VectorizedZero()
    {
        return vdupq_n_f32(0.f);
    }
    // Store 4/8/16 floats, using the unaligned variant for the same reason above
    static void VectorizedStore(float* DstPtr, float32x4_t SrcVector)
    {
        vst1q_f32(DstPtr, SrcVector);
    }
    // Add two register element by element, basically adding the contents of two 4-float wide register
    static float32x4_t VectorizedAdd(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vaddq_f32(Lhs, Rhs);
    }
    // Subtract two register element by element
    static float32x4_t VectorizedSub(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vsubq_f32(Lhs, Rhs);
    }
    // Multiply two register element by element
    static float32x4_t VectorizedMul(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vmulq_f32(Lhs, Rhs);
    }
    // Per-lane divide. AArch64-only intrinsic — ARMv7-A NEON has no vector divide
    static float32x4_t VectorizedDiv(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vdivq_f32(Lhs, Rhs);
    }
    /*
     * Reciprocal sqrt — NOT a 1:1 translation of _mm*_rsqrt_ps.
     *
     * NEON's vrsqrteq_f32 is only an ~8-bit estimate (the "e" in the name is for
     * "estimate"), versus ~12 bits on SSE/AVX and 14 bits on AVX512. Using the
     * raw estimate would let particle velocities drift noticeably more on ARM
     * than on x86.
     * To close that gap we apply one Newton-Raphson refinement step using NEON's
     * dedicated helper vrsqrtsq_f32, which internally computes (3 - a*b) / 2 —
     * exactly the term the NR iteration needs. The recurrence:
     *     x_{n+1} = x_n * vrsqrts(input, x_n * x_n)
     * After one step we land at ~16-bit precision (better than SSE/AVX rsqrt) at
     * the cost of one extra mul + one rsqrt-step op. If this ever shows up hot in
     * a profile, dropping the refinement is fine, but it would lead less accurate visuals
     */
    static float32x4_t VectorizedRSqrt(float32x4_t SrcVector)
    {
        float32x4_t Est = vrsqrteq_f32(SrcVector);
        Est = vmulq_f32(Est, vrsqrtsq_f32(SrcVector, vmulq_f32(Est, Est)));
        return Est;
    }

    /*
     * Bitwise AND. NEON has no vandq_f32, bitwise ops only exist on integer-typed
     * vector types. We reinterpret-cast through uint32x4_t to keep the float-in /
     * float-out interface symmetric with the x86 specializations. The
     * vreinterpretq_* intrinsics are zero-cost: they only re-tag the register for
     * the C compiler, no instruction is emitted
     */
    static float32x4_t VectorizedAnd(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(Lhs),
            vreinterpretq_u32_f32(Rhs)));
    }

    /*
     * Per-lane max. NEON follows IEEE-754 maxNum (NaN treated as missing, non-NaN
     * operand wins). SSE's _mm_max_ps returns the 2nd operand on any NaN. Behaves
     * identically across platforms whenever inputs are finite, which is the only
     * case our particle math produces
     */
    static float32x4_t VectorizedMax(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vmaxq_f32(Lhs, Rhs);
    }
    // Per-lane min. Same IEEE-754 minNum vs SSE NaN caveat as Max
    static float32x4_t VectorizedMin(float32x4_t Lhs, float32x4_t Rhs)
    {
        return vminq_f32(Lhs, Rhs);
    }
};
#endif

