#pragma once
#include <cstdint>
#include <type_traits>
#include <immintrin.h> // TODO: include only version of instructions that are needed

// Compile-time register width
#if defined(__AVX2__)
inline constexpr std::size_t kSimdBits = 256;
#define SIMD_AVX2
#else // SSE by default
inline constexpr std::size_t kSimdBits = 128;
#define SIMD_SSE
#endif

inline constexpr std::size_t kSimdBytes = kSimdBits / 8;

// Allowed element types
template<typename T>
concept SimdElement =
    std::is_same_v<T, int32_t> ||
    std::is_same_v<T, uint32_t> ||
    std::is_same_v<T, float>;

// Register type trait
template<typename T, std::size_t Bits> struct SimdReg;
template<> struct SimdReg<float, 128> { using type = __m128; };
template<> struct SimdReg<float, 256> { using type = __m256; };
template<> struct SimdReg<int32_t, 128> { using type = __m128i; };
template<> struct SimdReg<int32_t, 256> { using type = __m256i; };
template<> struct SimdReg<uint32_t, 128> { using type = __m128i; };
template<> struct SimdReg<uint32_t, 256> { using type = __m256i; };

// Simd<T>
template<SimdElement T>
struct Simd
{
    using value_type = T;
    using reg_type = typename SimdReg<T, kSimdBits>::type;

    static constexpr std::size_t bits = kSimdBits;
    static constexpr std::size_t bytes = kSimdBytes;
    static constexpr std::size_t lanes = bytes / sizeof(T);

    reg_type reg;

    // Construction

    [[nodiscard]] static Simd fill_lanes_with_value(T val) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_set1_ps(val);
            else                       s.reg = _mm_set1_ps(val);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_set1_epi32(static_cast<int32_t>(val));
            else                       s.reg = _mm_set1_epi32(static_cast<int32_t>(val));
        }
        return s;
    }

    [[nodiscard]] static Simd fill_lanes_with_zero() noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_setzero_ps();
            else                       s.reg = _mm_setzero_ps();
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_setzero_si256();
            else                       s.reg = _mm_setzero_si128();
        }
        return s;
    }

    [[nodiscard]] static Simd fill_lanes_with_full_value() noexcept
    {
        if constexpr (std::is_same_v<T, float>)
            return Simd<int32_t>::fill_lanes_with_value(-1).as_float();
        else
            return fill_lanes_with_value(static_cast<T>(-1));
    }

    // Loads in reverse order
    template<typename... Args>
    [[nodiscard]] static Simd set(Args... vals) noexcept
        requires (std::is_same_v<T, float>)
    {
        static_assert(sizeof...(vals) == lanes);
        static_assert((std::is_convertible_v<Args, T> && ...));
        
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_set_ps(static_cast<T>(vals)...);
        else                       s.reg = _mm_set_ps(static_cast<T>(vals)...);
        return s;
    }

    // Load

    [[nodiscard]] static Simd load(const T* ptr) noexcept
    {
        // Requires ptr to be aligned to `bytes` (16 or 32 bytes).
        // UB and potential fault if not — use loadu() when unsure.
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_load_ps(ptr);
            else                       s.reg = _mm_load_ps(ptr);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_load_si256(reinterpret_cast<const __m256i*>(ptr));
            else                       s.reg = _mm_load_si128(reinterpret_cast<const __m128i*>(ptr));
        }
        return s;
    }

    [[nodiscard]] static Simd loadu(const T* ptr) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_loadu_ps(ptr);
            else                       s.reg = _mm_loadu_ps(ptr);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(ptr));
            else                       s.reg = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));
        }
        return s;
    }

    // Store

    void store(T* ptr) const noexcept
    {
        // ptr must be aligned to `bytes`. UB if not.
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) _mm256_store_ps(ptr, reg);
            else                       _mm_store_ps(ptr, reg);
        }
        else
        {
            if constexpr (bits == 256) _mm256_store_si256(reinterpret_cast<__m256i*>(ptr), reg);
            else                       _mm_store_si128(reinterpret_cast<__m128i*>(ptr), reg);
        }
    }

    void storeu(T* ptr) const noexcept
    {
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) _mm256_storeu_ps(ptr, reg);
            else                       _mm_storeu_ps(ptr, reg);
        }
        else
        {
            if constexpr (bits == 256) _mm256_storeu_si256(reinterpret_cast<__m256i*>(ptr), reg);
            else                       _mm_storeu_si128(reinterpret_cast<__m128i*>(ptr), reg);
        }
    }

    // Bitwise operations

    [[nodiscard]] static Simd bitwise_and(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_and_ps(a.reg, b.reg);
            else                       s.reg = _mm_and_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_and_si256(a.reg, b.reg);
            else                       s.reg = _mm_and_si128(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd bitwise_or(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_or_ps(a.reg, b.reg);
            else                       s.reg = _mm_or_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_or_si256(a.reg, b.reg);
            else                       s.reg = _mm_or_si128(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd bitwise_xor(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_xor_ps(a.reg, b.reg);
            else                       s.reg = _mm_xor_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_xor_si256(a.reg, b.reg);
            else                       s.reg = _mm_xor_si128(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd bitwise_not(const Simd& a) noexcept
    {
        return bitwise_xor(a, fill_lanes_with_full_value());
    }

    // andnot(a, b) = (~a) & b - directly supported in hardware
    [[nodiscard]] static Simd bitwise_andnot(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_andnot_ps(a.reg, b.reg);
            else                       s.reg = _mm_andnot_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_andnot_si256(a.reg, b.reg);
            else                       s.reg = _mm_andnot_si128(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd shift_left(const Simd& a, int32_t count) noexcept
        requires (!std::is_same_v<T, float>)
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_slli_epi32(a.reg, count);
        else                       s.reg = _mm_slli_epi32(a.reg, count);
        return s;
    }

    [[nodiscard]] static Simd shift_right(const Simd& a, int32_t count) noexcept
        requires (!std::is_same_v<T, float>)
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_srli_epi32(a.reg, count);
        else                       s.reg = _mm_srli_epi32(a.reg, count);
        return s;
    }

    [[nodiscard]] Simd operator&(const Simd& other) const noexcept { return bitwise_and(*this, other); }
    [[nodiscard]] Simd operator|(const Simd& other) const noexcept { return bitwise_or(*this, other); }
    [[nodiscard]] Simd operator^(const Simd& other) const noexcept { return bitwise_xor(*this, other); }
    [[nodiscard]] Simd operator~()                  const noexcept { return bitwise_not(*this); }
    [[nodiscard]] Simd operator<<(int32_t count)    const noexcept { return shift_left(*this, count); }
    [[nodiscard]] Simd operator>>(int32_t count)    const noexcept { return shift_right(*this, count); }

    Simd& operator&=(const Simd& other) noexcept { *this = bitwise_and(*this, other); return *this; }
    Simd& operator|=(const Simd& other) noexcept { *this = bitwise_or(*this, other);  return *this; }
    Simd& operator^=(const Simd& other) noexcept { *this = bitwise_xor(*this, other); return *this; }
    Simd& operator<<=(int32_t count)    noexcept { *this = shift_left(*this, count);  return *this; }
    Simd& operator>>=(int32_t count)    noexcept { *this = shift_right(*this, count); return *this; }

    // Arithmetic

    [[nodiscard]] static Simd add(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_add_ps(a.reg, b.reg);
            else                       s.reg = _mm_add_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_add_epi32(a.reg, b.reg);
            else                       s.reg = _mm_add_epi32(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd sub(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_sub_ps(a.reg, b.reg);
            else                       s.reg = _mm_sub_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_sub_epi32(a.reg, b.reg);
            else                       s.reg = _mm_sub_epi32(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd mul(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_mul_ps(a.reg, b.reg);
            else                       s.reg = _mm_mul_ps(a.reg, b.reg);
        }
        else
        {
            // mullo: low 32 bits of each 32x32->64 product (wrapping, same for signed/unsigned)
            if constexpr (bits == 256) s.reg = _mm256_mullo_epi32(a.reg, b.reg);
            else                       s.reg = _mm_mullo_epi32(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd div(const Simd& a, const Simd& b) noexcept
        requires (std::is_same_v<T, float>)
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_div_ps(a.reg, b.reg);
        else                       s.reg = _mm_div_ps(a.reg, b.reg);
        return s;
    }

    // a * b + c
    [[nodiscard]] static Simd mul_add(const Simd& a, const Simd& b, const Simd& c) noexcept
        requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_fmadd_ps(a.reg, b.reg, c.reg);
        else                       s.reg = _mm_fmadd_ps(a.reg, b.reg, c.reg);
        return s;
    }

    // a * b - c
    [[nodiscard]] static Simd mul_sub(const Simd& a, const Simd& b, const Simd& c) noexcept
        requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_fmsub_ps(a.reg, b.reg, c.reg);
        else                       s.reg = _mm_fmsub_ps(a.reg, b.reg, c.reg);
        return s;
    }

    // -(a * b) + c
    [[nodiscard]] static Simd neg_mul_add(const Simd& a, const Simd& b, const Simd& c) noexcept
        requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_fnmadd_ps(a.reg, b.reg, c.reg);
        else                       s.reg = _mm_fnmadd_ps(a.reg, b.reg, c.reg);
        return s;
    }

    // -(a * b) - c
    [[nodiscard]] static Simd neg_mul_sub(const Simd& a, const Simd& b, const Simd& c) noexcept
        requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_fnmsub_ps(a.reg, b.reg, c.reg);
        else                       s.reg = _mm_fnmsub_ps(a.reg, b.reg, c.reg);
        return s;
    }

    [[nodiscard]] Simd operator+(const Simd& other) const noexcept { return add(*this, other); }
    [[nodiscard]] Simd operator-(const Simd& other) const noexcept { return sub(*this, other); }
    [[nodiscard]] Simd operator*(const Simd& other) const noexcept { return mul(*this, other); }
    [[nodiscard]] Simd operator/(const Simd& other) const noexcept
        requires (std::is_same_v<T, float>)
        { return div(*this, other); }

    Simd& operator+=(const Simd& other) noexcept { *this = add(*this, other); return *this; }
    Simd& operator-=(const Simd& other) noexcept { *this = sub(*this, other); return *this; }
    Simd& operator*=(const Simd& other) noexcept { *this = mul(*this, other); return *this; }
    Simd& operator/=(const Simd& other) noexcept { *this = div(*this, other); return *this; }

    // Rounding - float only (integers are already exact)

    [[nodiscard]] static Simd round(const Simd& a) noexcept requires std::is_same_v<T, float>
    {
        Simd s;
        constexpr int kNearest = _MM_FROUND_TO_NEAREST_INT | _MM_FROUND_NO_EXC;
        if constexpr (bits == 256) s.reg = _mm256_round_ps(a.reg, kNearest);
        else                       s.reg = _mm_round_ps(a.reg, kNearest);
        return s;
    }

    [[nodiscard]] static Simd floor(const Simd& a) noexcept requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_floor_ps(a.reg);
        else                       s.reg = _mm_floor_ps(a.reg);
        return s;
    }

    [[nodiscard]] static Simd ceil(const Simd& a) noexcept requires std::is_same_v<T, float>
    {
        Simd s;
        if constexpr (bits == 256) s.reg = _mm256_ceil_ps(a.reg);
        else                       s.reg = _mm_ceil_ps(a.reg);
        return s;
    }

    // Comparisons - return a lane mask (0xFFFFFFFF or 0x0 per lane)

    [[nodiscard]] static Simd compare_equal(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_EQ_OQ);
            else                       s.reg = _mm_cmpeq_ps(a.reg, b.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_cmpeq_epi32(a.reg, b.reg);
            else                       s.reg = _mm_cmpeq_epi32(a.reg, b.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd compare_not_equal(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_NEQ_OQ);
            else                       s.reg = _mm_cmpneq_ps(a.reg, b.reg);
        }
        else
        {
            Simd eq = compare_equal(a, b);
            Simd ones = fill_lanes_with_full_value();
            if constexpr (bits == 256) s.reg = _mm256_xor_si256(eq.reg, ones.reg);
            else                       s.reg = _mm_xor_si128(eq.reg, ones.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd compare_less(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_LT_OQ);
            else                       s.reg = _mm_cmplt_ps(a.reg, b.reg);
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmpgt_epi32(b.reg, a.reg);
            else                       s.reg = _mm_cmpgt_epi32(b.reg, a.reg);
        }
        else // uint32_t — bias into signed range
        {
            const auto bias = fill_lanes_with_value(static_cast<T>(0x80000000u));
            return compare_less(sub(a, bias), sub(b, bias));
        }
        return s;
    }

    [[nodiscard]] static Simd compare_less_equal(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_LE_OQ);
            else                       s.reg = _mm_cmple_ps(a.reg, b.reg);
        }
        else
        {
            Simd gt = compare_greater(a, b);
            Simd ones = fill_lanes_with_full_value();
            if constexpr (bits == 256) s.reg = _mm256_xor_si256(gt.reg, ones.reg);
            else                       s.reg = _mm_xor_si128(gt.reg, ones.reg);
        }
        return s;
    }

    [[nodiscard]] static Simd compare_greater(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_GT_OQ);
            else                       s.reg = _mm_cmpgt_ps(a.reg, b.reg);
        }
        else if constexpr (std::is_same_v<T, int32_t>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmpgt_epi32(a.reg, b.reg);
            else                       s.reg = _mm_cmpgt_epi32(a.reg, b.reg);
        }
        else // uint32_t
        {
            const auto bias = fill_lanes_with_value(static_cast<T>(0x80000000u));
            return compare_greater(sub(a, bias), sub(b, bias));
        }
        return s;
    }

    [[nodiscard]] static Simd compare_greater_equal(const Simd& a, const Simd& b) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_cmp_ps(a.reg, b.reg, _CMP_GE_OQ);
            else                       s.reg = _mm_cmpge_ps(a.reg, b.reg);
        }
        else
        {
            Simd lt = compare_less(a, b);
            Simd ones = fill_lanes_with_full_value();
            if constexpr (bits == 256) s.reg = _mm256_xor_si256(lt.reg, ones.reg);
            else                       s.reg = _mm_xor_si128(lt.reg, ones.reg);
        }
        return s;
    }

    [[nodiscard]] Simd operator==(const Simd& other) const noexcept { return compare_equal(*this, other); }
    [[nodiscard]] Simd operator!=(const Simd& other) const noexcept { return compare_not_equal(*this, other); }
    [[nodiscard]] Simd operator< (const Simd& other) const noexcept { return compare_less(*this, other); }
    [[nodiscard]] Simd operator<=(const Simd& other) const noexcept { return compare_less_equal(*this, other); }
    [[nodiscard]] Simd operator> (const Simd& other) const noexcept { return compare_greater(*this, other); }
    [[nodiscard]] Simd operator>=(const Simd& other) const noexcept { return compare_greater_equal(*this, other); }

    // Blend

    // Copies byte from 'a' if mask bit is 0, from 'b' if 1.
    [[nodiscard]] static Simd blendv(const Simd& a, const Simd& b, const Simd& mask) noexcept
    {
        Simd s;
        if constexpr (std::is_same_v<T, float>)
        {
            if constexpr (bits == 256) s.reg = _mm256_blendv_ps(a.reg, b.reg, mask.reg);
            else                       s.reg = _mm_blendv_ps(a.reg, b.reg, mask.reg);
        }
        else
        {
            if constexpr (bits == 256) s.reg = _mm256_blendv_epi8(a.reg, b.reg, mask.reg);
            else                       s.reg = _mm_blendv_epi8(a.reg, b.reg, mask.reg);
        }
        return s;
    }

    // Type casts

    [[nodiscard]] Simd<int32_t> to_int32() const noexcept
        requires std::is_same_v<T, float>
    {
        Simd<int32_t> s;
        if constexpr (bits == 256) s.reg = _mm256_cvttps_epi32(reg);
        else                       s.reg = _mm_cvttps_epi32(reg);
        return s;
    }

    [[nodiscard]] Simd<float> to_float() const noexcept
        requires (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>)
    {
        Simd<float> s;
        if constexpr (bits == 256) s.reg = _mm256_cvtepi32_ps(reg);
        else                       s.reg = _mm_cvtepi32_ps(reg);
        return s;
    }

    // Reinterpret cast - zero cost, no instruction emitted

    [[nodiscard]] Simd<int32_t> as_int32() const noexcept
        requires std::is_same_v<T, float>
    {
        Simd<int32_t> s;
        if constexpr (bits == 256) s.reg = _mm256_castps_si256(reg);
        else                       s.reg = _mm_castps_si128(reg);
        return s;
    }

    [[nodiscard]] Simd<float> as_float() const noexcept
        requires (std::is_same_v<T, int32_t> || std::is_same_v<T, uint32_t>)
    {
        Simd<float> s;
        if constexpr (bits == 256) s.reg = _mm256_castsi256_ps(reg);
        else                       s.reg = _mm_castsi128_ps(reg);
        return s;
    }
};

using SimdF = Simd<float>;
using SimdI = Simd<int32_t>;