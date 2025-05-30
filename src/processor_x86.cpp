// This file is a part of Julia. License is MIT: https://julialang.org/license

// X86 specific processor detection and dispatch

// CPUID

#include "julia.h"
extern "C" JL_DLLEXPORT void jl_cpuid(int32_t CPUInfo[4], int32_t InfoType)
{
    asm volatile (
#if defined(__i386__) && defined(__PIC__)
        "xchg %%ebx, %%esi;"
        "cpuid;"
        "xchg %%esi, %%ebx;" :
        "=S" (CPUInfo[1]),
#else
        "cpuid" :
        "=b" (CPUInfo[1]),
#endif
        "=a" (CPUInfo[0]),
        "=c" (CPUInfo[2]),
        "=d" (CPUInfo[3]) :
        "a" (InfoType)
        );
}

extern "C" JL_DLLEXPORT void jl_cpuidex(int32_t CPUInfo[4], int32_t InfoType, int32_t subInfoType)
{
    asm volatile (
#if defined(__i386__) && defined(__PIC__)
        "xchg %%ebx, %%esi;"
        "cpuid;"
        "xchg %%esi, %%ebx;" :
        "=S" (CPUInfo[1]),
#else
        "cpuid" :
        "=b" (CPUInfo[1]),
#endif
        "=a" (CPUInfo[0]),
        "=c" (CPUInfo[2]),
        "=d" (CPUInfo[3]) :
        "a" (InfoType),
        "c" (subInfoType)
        );
}

namespace X86 {

enum class CPU : uint32_t {
    generic = 0,
    intel_nocona,
    intel_prescott,
    intel_atom_bonnell,
    intel_atom_silvermont,
    intel_atom_goldmont,
    intel_atom_goldmont_plus,
    intel_atom_tremont,
    intel_core2,
    intel_core2_penryn,
    intel_yonah,
    intel_corei7_nehalem,
    intel_corei7_westmere,
    intel_corei7_sandybridge,
    intel_corei7_ivybridge,
    intel_corei7_haswell,
    intel_corei7_broadwell,
    intel_corei7_skylake,
    intel_corei7_skylake_avx512,
    intel_corei7_cascadelake,
    intel_corei7_cooperlake,
    intel_corei7_cannonlake,
    intel_corei7_icelake_client,
    intel_corei7_icelake_server,
    intel_corei7_tigerlake,
    intel_corei7_alderlake,
    intel_corei7_sapphirerapids,
    intel_knights_landing,
    intel_knights_mill,

    amd_fam10h,
    amd_athlon_fx,
    amd_athlon_64,
    amd_athlon_64_sse3,
    amd_bdver1,
    amd_bdver2,
    amd_bdver3,
    amd_bdver4,
    amd_btver1,
    amd_btver2,
    amd_k8,
    amd_k8_sse3,
    amd_opteron,
    amd_opteron_sse3,
    amd_barcelona,
    amd_znver1,
    amd_znver2,
    amd_znver3,
    amd_znver4,
    amd_znver5,
};

static constexpr size_t feature_sz = 12;
static constexpr FeatureName feature_names[] = {
#define JL_FEATURE_DEF(name, bit, llvmver) {#name, bit, llvmver},
#define JL_FEATURE_DEF_NAME(name, bit, llvmver, str) {str, bit, llvmver},
#include "features_x86.h"
#undef JL_FEATURE_DEF
#undef JL_FEATURE_DEF_NAME
};
static constexpr uint32_t nfeature_names = sizeof(feature_names) / sizeof(FeatureName);

template<typename... Args>
static inline constexpr FeatureList<feature_sz> get_feature_masks(Args... args)
{
    return ::get_feature_masks<feature_sz>(args...);
}

#define JL_FEATURE_DEF_NAME(name, bit, llvmver, str) JL_FEATURE_DEF(name, bit, llvmver)
static constexpr auto feature_masks = get_feature_masks(
#define JL_FEATURE_DEF(name, bit, llvmver) bit,
#include "features_x86.h"
#undef JL_FEATURE_DEF
    -1);

namespace Feature {
enum : uint32_t {
#define JL_FEATURE_DEF(name, bit, llvmver) name = bit,
#include "features_x86.h"
#undef JL_FEATURE_DEF
};
#undef JL_FEATURE_DEF_NAME
static constexpr FeatureDep deps[] = {
    {ssse3, sse3},
    {fma, avx},
    {sse41, ssse3},
    {sse42, sse41},
    {avx, sse42},
    {f16c, avx},
    {avx2, avx},
    {vaes, avx},
    {vaes, aes},
    {vpclmulqdq, avx},
    {vpclmulqdq, pclmul},
    {avxvnni, avx2},
    {avxvnniint8, avx2},
    {avxvnniint16, avx2},
    {avxifma, avx2},
    {avxneconvert, avx2},
    {avx512f, avx2},
    {avx512dq, avx512f},
    {avx512ifma, avx512f},
    {avx512cd, avx512f},
    {avx512bw, avx512f},
    {avx512bf16, avx512bw},
    {avx512bitalg, avx512bw},
    {avx512vl, avx512f},
    {avx512vbmi, avx512bw},
    {avx512vbmi2, avx512bw},
    {avx512vnni, avx512f},
    {avx512vp2intersect, avx512f},
    {avx512vpopcntdq, avx512f},
    {avx512fp16, avx512bw},
    {avx512fp16, avx512dq},
    {avx512fp16, avx512vl},
    {amx_int8, amx_tile},
    {amx_bf16, amx_tile},
    {amx_fp16, amx_tile},
    {amx_complex, amx_tile},
    {sse4a, sse3},
    {xop, fma4},
    {fma4, avx},
    {fma4, sse4a},
    {xsaveopt, xsave},
    {xsavec, xsave},
    {xsaves, xsave},
    {sha512, avx2},
    {sm3, avx},
    {sm4, avx2},
};

// We require cx16 on 64bit by default. This can be overwritten with `-cx16`
// This isn't really compatible with 32bit but we mask it off there with required LLVM version
constexpr auto generic = get_feature_masks(cx16);
constexpr auto bonnell = get_feature_masks(sse3, ssse3, cx16, movbe, sahf);
constexpr auto silvermont = bonnell | get_feature_masks(sse41, sse42, popcnt,
                                                        pclmul, prfchw, rdrnd);
constexpr auto goldmont = silvermont | get_feature_masks(aes, sha, rdseed, xsave, xsaveopt,
                                                         xsavec, xsaves, clflushopt, fsgsbase);
constexpr auto goldmont_plus = goldmont | get_feature_masks(ptwrite, rdpid); // sgx
constexpr auto tremont = goldmont_plus | get_feature_masks(clwb, gfni);
constexpr auto knl = get_feature_masks(sse3, ssse3, sse41, sse42, cx16, sahf, popcnt,
                                       aes, pclmul, avx, xsave, xsaveopt, rdrnd, f16c, fsgsbase,
                                       avx2, bmi, bmi2, fma, lzcnt, movbe, adx, rdseed, prfchw,
                                       avx512f, avx512cd);
constexpr auto knm = knl | get_feature_masks(avx512vpopcntdq);
constexpr auto yonah = get_feature_masks(sse3);
constexpr auto prescott = yonah;
constexpr auto core2 = get_feature_masks(sse3, ssse3, cx16, sahf);
constexpr auto nocona = get_feature_masks(sse3, cx16);
constexpr auto penryn = nocona | get_feature_masks(ssse3, sse41, sahf);
constexpr auto nehalem = penryn | get_feature_masks(sse42, popcnt);
constexpr auto westmere = nehalem | get_feature_masks(pclmul);
constexpr auto sandybridge = westmere | get_feature_masks(avx, xsave, xsaveopt);
constexpr auto ivybridge = sandybridge | get_feature_masks(rdrnd, f16c, fsgsbase);
constexpr auto haswell = ivybridge | get_feature_masks(avx2, bmi, bmi2, fma, lzcnt, movbe);
constexpr auto broadwell = haswell | get_feature_masks(adx, rdseed, prfchw);
constexpr auto skylake = broadwell | get_feature_masks(aes, xsavec, xsaves, clflushopt); // sgx
constexpr auto skx = skylake | get_feature_masks(avx512f, avx512cd, avx512dq, avx512bw, avx512vl,
                                                 pku, clwb);
constexpr auto cascadelake = skx | get_feature_masks(avx512vnni);
constexpr auto cooperlake = cascadelake | get_feature_masks(avx512bf16);
constexpr auto cannonlake = skylake | get_feature_masks(avx512f, avx512cd, avx512dq, avx512bw,
                                                        avx512vl, pku, avx512vbmi, avx512ifma,
                                                        sha); // sgx
constexpr auto icelake = cannonlake | get_feature_masks(avx512bitalg, vaes, avx512vbmi2,
                                                        vpclmulqdq, avx512vpopcntdq,
                                                        gfni, clwb, rdpid);
constexpr auto icelake_server = icelake | get_feature_masks(pconfig, wbnoinvd);
constexpr auto tigerlake = icelake | get_feature_masks(avx512vp2intersect, movdiri,
                                                       movdir64b, shstk);
constexpr auto alderlake = skylake | get_feature_masks(clwb, sha, waitpkg, shstk, gfni, vaes, vpclmulqdq, pconfig,
                                                       rdpid, movdiri, pku, movdir64b, serialize, ptwrite, avxvnni);
constexpr auto sapphirerapids = icelake_server |
    get_feature_masks(amx_tile, amx_int8, amx_bf16, avx512bf16, avx512fp16, serialize, cldemote, waitpkg,
                      avxvnni, uintr, ptwrite, tsxldtrk, enqcmd, shstk, avx512vp2intersect, movdiri, movdir64b);

constexpr auto k8_sse3 = get_feature_masks(sse3, cx16);
constexpr auto amdfam10 = k8_sse3 | get_feature_masks(sse4a, lzcnt, popcnt, sahf);

constexpr auto btver1 = amdfam10 | get_feature_masks(ssse3, prfchw);
constexpr auto btver2 = btver1 | get_feature_masks(sse41, sse42, avx, aes, pclmul, bmi, f16c,
                                                   movbe, xsave, xsaveopt);

constexpr auto bdver1 = amdfam10 | get_feature_masks(xop, fma4, avx, ssse3, sse41, sse42, aes,
                                                     prfchw, pclmul, xsave);
constexpr auto bdver2 = bdver1 | get_feature_masks(f16c, bmi, tbm, fma);
constexpr auto bdver3 = bdver2 | get_feature_masks(xsaveopt, fsgsbase);
constexpr auto bdver4 = bdver3 | get_feature_masks(avx2, bmi2, mwaitx, movbe, rdrnd);

// technically xsaves is part of znver1, znver2, and znver3
// Disabled due to Erratum 1386
// See: https://github.com/JuliaLang/julia/issues/50102
constexpr auto znver1 = haswell | get_feature_masks(adx, aes, clflushopt, clzero, mwaitx, prfchw,
                                                    rdseed, sha, sse4a, xsavec);
constexpr auto znver2 = znver1 | get_feature_masks(clwb, rdpid, wbnoinvd);
constexpr auto znver3 = znver2 | get_feature_masks(shstk, pku, vaes, vpclmulqdq);
constexpr auto znver4 = znver3 | get_feature_masks(avx512f, avx512cd, avx512dq, avx512bw, avx512vl, avx512ifma, avx512vbmi,
                                                   avx512vbmi2, avx512vnni, avx512bitalg, avx512vpopcntdq, avx512bf16, gfni, shstk, xsaves);
constexpr auto znver5 = znver4 | get_feature_masks(avxvnni, movdiri, movdir64b, avx512vp2intersect, prefetchi, avxvnni);

}

static constexpr CPUSpec<CPU, feature_sz> cpus[] = {
    {"generic", CPU::generic, CPU::generic, 0, Feature::generic},
    {"bonnell", CPU::intel_atom_bonnell, CPU::generic, 0, Feature::bonnell},
    {"silvermont", CPU::intel_atom_silvermont, CPU::generic, 0, Feature::silvermont},
    {"goldmont", CPU::intel_atom_goldmont, CPU::generic, 0, Feature::goldmont},
    {"goldmont-plus", CPU::intel_atom_goldmont_plus, CPU::generic, 0, Feature::goldmont_plus},
    {"tremont", CPU::intel_atom_tremont, CPU::generic, 0, Feature::tremont},
    {"core2", CPU::intel_core2, CPU::generic, 0, Feature::core2},
    {"yonah", CPU::intel_yonah, CPU::generic, 0, Feature::yonah},
    {"prescott", CPU::intel_prescott, CPU::generic, 0, Feature::prescott},
    {"nocona", CPU::intel_nocona, CPU::generic, 0, Feature::nocona},
    {"penryn", CPU::intel_core2_penryn, CPU::generic, 0, Feature::penryn},
    {"nehalem", CPU::intel_corei7_nehalem, CPU::generic, 0, Feature::nehalem},
    {"westmere", CPU::intel_corei7_westmere, CPU::generic, 0, Feature::westmere},
    {"sandybridge", CPU::intel_corei7_sandybridge, CPU::generic, 0, Feature::sandybridge},
    {"ivybridge", CPU::intel_corei7_ivybridge, CPU::generic, 0, Feature::ivybridge},
    {"haswell", CPU::intel_corei7_haswell, CPU::generic, 0, Feature::haswell},
    {"broadwell", CPU::intel_corei7_broadwell, CPU::generic, 0, Feature::broadwell},
    {"skylake", CPU::intel_corei7_skylake, CPU::generic, 0, Feature::skylake},
    {"knl", CPU::intel_knights_landing, CPU::generic, 0, Feature::knl},
    {"knm", CPU::intel_knights_mill, CPU::generic, 0, Feature::knm},
    {"skylake-avx512", CPU::intel_corei7_skylake_avx512, CPU::generic, 0, Feature::skx},
    {"cascadelake", CPU::intel_corei7_cascadelake, CPU::generic, 0, Feature::cascadelake},
    {"cooperlake", CPU::intel_corei7_cooperlake, CPU::generic, 0, Feature::cooperlake},
    {"cannonlake", CPU::intel_corei7_cannonlake, CPU::generic, 0, Feature::cannonlake},
    {"icelake-client", CPU::intel_corei7_icelake_client, CPU::generic, 0, Feature::icelake},
    {"icelake-server", CPU::intel_corei7_icelake_server, CPU::generic, 0,
     Feature::icelake_server},
    {"tigerlake", CPU::intel_corei7_tigerlake, CPU::intel_corei7_icelake_client, 100000,
     Feature::tigerlake},
    {"alderlake", CPU::intel_corei7_alderlake, CPU::intel_corei7_skylake, 120000,
     Feature::alderlake},
    {"sapphirerapids", CPU::intel_corei7_sapphirerapids, CPU::intel_corei7_icelake_server, 120000,
     Feature::sapphirerapids},

    {"athlon64", CPU::amd_athlon_64, CPU::generic, 0, Feature::generic},
    {"athlon-fx", CPU::amd_athlon_fx, CPU::generic, 0, Feature::generic},
    {"k8", CPU::amd_k8, CPU::generic, 0, Feature::generic},
    {"opteron", CPU::amd_opteron, CPU::generic, 0, Feature::generic},

    {"athlon64-sse3", CPU::amd_athlon_64_sse3, CPU::generic, 0, Feature::k8_sse3},
    {"k8-sse3", CPU::amd_k8_sse3, CPU::generic, 0, Feature::k8_sse3},
    {"opteron-sse3", CPU::amd_opteron_sse3, CPU::generic, 0, Feature::k8_sse3},

    {"amdfam10", CPU::amd_fam10h, CPU::generic, 0, Feature::amdfam10},
    {"barcelona", CPU::amd_barcelona, CPU::generic, 0, Feature::amdfam10},

    {"btver1", CPU::amd_btver1, CPU::generic, 0, Feature::btver1},
    {"btver2", CPU::amd_btver2, CPU::generic, 0, Feature::btver2},

    {"bdver1", CPU::amd_bdver1, CPU::generic, 0, Feature::bdver1},
    {"bdver2", CPU::amd_bdver2, CPU::generic, 0, Feature::bdver2},
    {"bdver3", CPU::amd_bdver3, CPU::generic, 0, Feature::bdver3},
    {"bdver4", CPU::amd_bdver4, CPU::generic, 0, Feature::bdver4},

    {"znver1", CPU::amd_znver1, CPU::generic, 0, Feature::znver1},
    {"znver2", CPU::amd_znver2, CPU::generic, 0, Feature::znver2},
    {"znver3", CPU::amd_znver3, CPU::amd_znver2, 120000, Feature::znver3},
    {"znver4", CPU::amd_znver4, CPU::amd_znver3, 160000, Feature::znver4},
    {"znver5", CPU::amd_znver5, CPU::amd_znver4, 190000, Feature::znver5},
};
static constexpr size_t ncpu_names = sizeof(cpus) / sizeof(cpus[0]);

// For CPU model and feature detection on X86

const int SIG_INTEL = 0x756e6547; // Genu
const int SIG_AMD = 0x68747541; // Auth

static uint64_t get_xcr0(void)
{
    uint32_t eax, edx;
    asm volatile ("xgetbv" : "=a" (eax), "=d" (edx) : "c" (0));
    return (uint64_t(edx) << 32) | eax;
}

static CPU get_intel_processor_name(uint32_t family, uint32_t model, uint32_t brand_id,
                                    const uint32_t *features)
{
    if (brand_id != 0)
        return CPU::generic;
    switch (family) {
    case 3:
    case 4:
    case 5:
        return CPU::generic;
    case 6:
        switch (model) {
        case 0x01: // Pentium Pro processor
        case 0x03: // Intel Pentium II OverDrive processor, Pentium II processor, model 03
        case 0x05: // Pentium II processor, model 05, Pentium II Xeon processor,
            // model 05, and Intel Celeron processor, model 05
        case 0x06: // Celeron processor, model 06
        case 0x07: // Pentium III processor, model 07, and Pentium III Xeon processor, model 07
        case 0x08: // Pentium III processor, model 08, Pentium III Xeon processor,
            // model 08, and Celeron processor, model 08
        case 0x0a: // Pentium III Xeon processor, model 0Ah
        case 0x0b: // Pentium III processor, model 0Bh
        case 0x09: // Intel Pentium M processor, Intel Celeron M processor model 09.
        case 0x0d: // Intel Pentium M processor, Intel Celeron M processor, model
            // 0Dh. All processors are manufactured using the 90 nm process.
        case 0x15: // Intel EP80579 Integrated Processor and Intel EP80579
            // Integrated Processor with Intel QuickAssist Technology
            return CPU::generic;
        case 0x0e: // Intel Core Duo processor, Intel Core Solo processor, model
            // 0Eh. All processors are manufactured using the 65 nm process.
            return CPU::intel_yonah;
        case 0x0f: // Intel Core 2 Duo processor, Intel Core 2 Duo mobile
            // processor, Intel Core 2 Quad processor, Intel Core 2 Quad
            // mobile processor, Intel Core 2 Extreme processor, Intel
            // Pentium Dual-Core processor, Intel Xeon processor, model
            // 0Fh. All processors are manufactured using the 65 nm process.
        case 0x16: // Intel Celeron processor model 16h. All processors are
            // manufactured using the 65 nm process
            return CPU::intel_core2;
        case 0x17: // Intel Core 2 Extreme processor, Intel Xeon processor, model
            // 17h. All processors are manufactured using the 45 nm process.
            //
            // 45nm: Penryn , Wolfdale, Yorkfield (XE)
        case 0x1d: // Intel Xeon processor MP. All processors are manufactured using
            // the 45 nm process.
            return CPU::intel_core2_penryn;
        case 0x1a: // Intel Core i7 processor and Intel Xeon processor. All
            // processors are manufactured using the 45 nm process.
        case 0x1e: // Intel(R) Core(TM) i7 CPU         870  @ 2.93GHz.
            // As found in a Summer 2010 model iMac.
        case 0x1f:
        case 0x2e: // Nehalem EX
            return CPU::intel_corei7_nehalem;
        case 0x25: // Intel Core i7, laptop version.
        case 0x2c: // Intel Core i7 processor and Intel Xeon processor. All
            // processors are manufactured using the 32 nm process.
        case 0x2f: // Westmere EX
            return CPU::intel_corei7_westmere;
        case 0x2a: // Intel Core i7 processor. All processors are manufactured
            // using the 32 nm process.
        case 0x2d:
            return CPU::intel_corei7_sandybridge;
        case 0x3a:
        case 0x3e: // Ivy Bridge EP
            return CPU::intel_corei7_ivybridge;

            // Haswell:
        case 0x3c:
        case 0x3f:
        case 0x45:
        case 0x46:
            return CPU::intel_corei7_haswell;

            // Broadwell:
        case 0x3d:
        case 0x47:
        case 0x4f:
        case 0x56:
            return CPU::intel_corei7_broadwell;

            // Skylake:
        case 0x4e: // Skylake mobile
        case 0x5e: // Skylake desktop
        case 0x8e: // Kaby Lake mobile
        case 0x9e: // Kaby Lake desktop
        case 0xa5: // Comet Lake-H/S
        case 0xa6: // Comet Lake-U
            return CPU::intel_corei7_skylake;

            // Skylake Xeon:
        case 0x55:
            if (test_nbit(features, Feature::avx512bf16))
                return CPU::intel_corei7_cooperlake;
            if (test_nbit(features, Feature::avx512vnni))
                return CPU::intel_corei7_cascadelake;
            return CPU::intel_corei7_skylake_avx512;

            // Cannonlake:
        case 0x66:
            return CPU::intel_corei7_cannonlake;

            // Icelake:
        case 0x7d:
        case 0x7e:
        case 0x9d:
            return CPU::intel_corei7_icelake_client;

            // Icelake Xeon:
        case 0x6a:
        case 0x6c:
            return CPU::intel_corei7_icelake_server;

            // Tiger Lake
        case 0x8c:
        case 0x8d:
            return CPU::intel_corei7_tigerlake;
            //Alder Lake
        case 0x97:
        case 0x9a:
            return CPU::intel_corei7_alderlake;

            // Sapphire Rapids
        case 0x8f:
            return CPU::intel_corei7_sapphirerapids;

        case 0x1c: // Most 45 nm Intel Atom processors
        case 0x26: // 45 nm Atom Lincroft
        case 0x27: // 32 nm Atom Medfield
        case 0x35: // 32 nm Atom Midview
        case 0x36: // 32 nm Atom Midview
            return CPU::intel_atom_bonnell;

            // Atom Silvermont codes from the Intel software optimization guide.
        case 0x37:
        case 0x4a:
        case 0x4d:
        case 0x5d:
            // Airmont
        case 0x4c:
        case 0x5a:
        case 0x75:
            return CPU::intel_atom_silvermont;

            // Goldmont:
        case 0x5c:
        case 0x5f:
            return CPU::intel_atom_goldmont;
        case 0x7a:
            return CPU::intel_atom_goldmont_plus;
        case 0x86:
        case 0x96:
        case 0x9c:
            return CPU::intel_atom_tremont;

        case 0x57:
            return CPU::intel_knights_landing;

        case 0x85:
            return CPU::intel_knights_mill;

        default:
            return CPU::generic;
        }
        break;
    case 15: {
        switch (model) {
        case 0: // Pentium 4 processor, Intel Xeon processor. All processors are
            // model 00h and manufactured using the 0.18 micron process.
        case 1: // Pentium 4 processor, Intel Xeon processor, Intel Xeon
            // processor MP, and Intel Celeron processor. All processors are
            // model 01h and manufactured using the 0.18 micron process.
        case 2: // Pentium 4 processor, Mobile Intel Pentium 4 processor - M,
            // Intel Xeon processor, Intel Xeon processor MP, Intel Celeron
            // processor, and Mobile Intel Celeron processor. All processors
            // are model 02h and manufactured using the 0.13 micron process.
        default:
            return CPU::generic;

        case 3: // Pentium 4 processor, Intel Xeon processor, Intel Celeron D
            // processor. All processors are model 03h and manufactured using
            // the 90 nm process.
        case 4: // Pentium 4 processor, Pentium 4 processor Extreme Edition,
            // Pentium D processor, Intel Xeon processor, Intel Xeon
            // processor MP, Intel Celeron D processor. All processors are
            // model 04h and manufactured using the 90 nm process.
        case 6: // Pentium 4 processor, Pentium D processor, Pentium processor
            // Extreme Edition, Intel Xeon processor, Intel Xeon processor
            // MP, Intel Celeron D processor. All processors are model 06h
            // and manufactured using the 65 nm process.
#ifdef _CPU_X86_64_
            return CPU::intel_nocona;
#else
            return CPU::intel_prescott;
#endif
        }
    }
    default:
        break; /*"generic"*/
    }
    return CPU::generic;
}

static CPU get_amd_processor_name(uint32_t family, uint32_t model, const uint32_t *features)
{
    switch (family) {
    case 4:
    case 5:
    case 6:
    default:
        return CPU::generic;
    case 15:
        if (test_nbit(features, Feature::sse3))
            return CPU::amd_k8_sse3;
        switch (model) {
        case 1:
            return CPU::amd_opteron;
        case 5:
            return CPU::amd_athlon_fx;
        default:
            return CPU::amd_athlon_64;
        }
    case 16:
        switch (model) {
        case 2:
            return CPU::amd_barcelona;
        case 4:
        case 8:
        default:
            return CPU::amd_fam10h;
        }
    case 20:
        return CPU::amd_btver1;
    case 21:
        if (model >= 0x50 && model <= 0x6f)
            return CPU::amd_bdver4;
        if (model >= 0x30 && model <= 0x3f)
            return CPU::amd_bdver3;
        if (model >= 0x10 && model <= 0x1f)
            return CPU::amd_bdver2;
        if (model <= 0x0f)
            return CPU::amd_bdver1;
        return CPU::amd_btver1; // fallback
    case 22:
        return CPU::amd_btver2;
    case 23:
        // Known models:
        // Zen: 1, 17
        // Zen+: 8, 24
        // Zen2: 96, 113
        if (model >= 0x30)
            return CPU::amd_znver2;
        return CPU::amd_znver1;
    case 25:  // AMD Family 19h
        if (model <= 0x0f || (model >= 0x20 && model <= 0x5f))
            return CPU::amd_znver3;  // 00h-0Fh, 21h: Zen3
        if ((model >= 0x10 && model <= 0x1f) ||
            (model >= 0x60 && model <= 0x74) ||
            (model >= 0x78 && model <= 0x7b) ||
            (model >= 0xA0 && model <= 0xAf)) {
                return CPU::amd_znver4;
            }
        return CPU::amd_znver3; // fallback
    case 26:
        // if (model <= 0x77)
        return CPU::amd_znver5;
    }
}

template<typename T>
static inline void features_disable_avx512(T &features)
{
    using namespace Feature;
    unset_bits(features, avx512f, avx512dq, avx512ifma, avx512cd,
               avx512bw, avx512vl, avx512vbmi, avx512vpopcntdq, avx512vbmi2, avx512vnni,
               avx512bitalg, avx512vp2intersect, avx512bf16);
}

template<typename T>
static inline void features_disable_avx(T &features)
{
    using namespace Feature;
    unset_bits(features, avx, Feature::fma, f16c, xsave, avx2, xop, fma4,
               xsaveopt, xsavec, xsaves, vaes, vpclmulqdq);
}

template<typename T>
static inline void features_disable_amx(T &features)
{
    using namespace Feature;
    unset_bits(features, amx_bf16, amx_tile, amx_int8);
}

static NOINLINE std::pair<uint32_t,FeatureList<feature_sz>> _get_host_cpu(void)
{
    FeatureList<feature_sz> features = {};

    int32_t info0[4];
    jl_cpuid(info0, 0);
    uint32_t maxleaf = info0[0];
    if (maxleaf < 1)
        return std::make_pair(uint32_t(CPU::generic), features);
    int32_t info1[4];
    jl_cpuid(info1, 1);

    auto vendor = info0[1];
    auto brand_id = info1[1] & 0xff;

    auto family = (info1[0] >> 8) & 0xf; // Bits 8 - 11
    auto model = (info1[0] >> 4) & 0xf;  // Bits 4 - 7
    if (family == 6 || family == 0xf) {
        if (family == 0xf)
            // Examine extended family ID if family ID is F.
            family += (info1[0] >> 20) & 0xff; // Bits 20 - 27
        // Examine extended model ID if family ID is 6 or F.
        model += ((info1[0] >> 16) & 0xf) << 4; // Bits 16 - 19
    }

    // Fill in the features
    features[0] = info1[2];
    features[1] = info1[3];
    if (maxleaf >= 7) {
        int32_t info7[4];
        jl_cpuidex(info7, 7, 0);
        features[2] = info7[1];
        features[3] = info7[2];
        features[4] = info7[3];
    }
    int32_t infoex0[4];
    jl_cpuid(infoex0, 0x80000000);
    uint32_t maxexleaf = infoex0[0];
    if (maxexleaf >= 0x80000001) {
        int32_t infoex1[4];
        jl_cpuid(infoex1, 0x80000001);
        features[5] = infoex1[2];
        features[6] = infoex1[3];
    }
    if (maxleaf >= 0xd) {
        int32_t infod[4];
        jl_cpuidex(infod, 0xd, 0x1);
        features[7] = infod[0];
    }
    if (maxexleaf >= 0x80000008) {
        int32_t infoex8[4];
        jl_cpuidex(infoex8, 0x80000008, 0);
        features[8] = infoex8[1];
    }
    if (maxleaf >= 7) {
        int32_t info7[4];
        jl_cpuidex(info7, 7, 1);
        features[9] = info7[0];
        features[10] = info7[1];
    }
    if (maxleaf >= 0x14) {
        int32_t info14[4];
        jl_cpuidex(info14, 0x14, 0);
        features[11] = info14[1];
    }

    // Fix up AVX bits to account for OS support and match LLVM model
    uint64_t xcr0 = 0;
    bool hasxsave = test_all_bits(features[0], 1 << 27);
    if (hasxsave) {
        xcr0 = get_xcr0();
        hasxsave = test_all_bits(xcr0, 0x6);
    }
    bool hasavx = hasxsave && test_all_bits(features[0], 1 << 28);
    unset_bits(features, 32 + 27);
    if (!hasavx)
        features_disable_avx(features);
#ifdef _OS_DARWIN_
    // See https://github.com/llvm/llvm-project/commit/82921bf2baed96b700f90b090d5dc2530223d9c0
    // and https://github.com/apple/darwin-xnu/blob/a449c6a3b8014d9406c2ddbdc81795da24aa7443/osfmk/i386/fpu.c#L174
    // Darwin lazily saves the AVX512 context on first use
    bool hasavx512save = hasavx;
#else
    bool hasavx512save = hasavx && test_all_bits(xcr0, 0xe0);
#endif
    if (!hasavx512save)
        features_disable_avx512(features);
    // AMX requires additional context to be saved by the OS.
    bool hasamxsave = hasxsave && test_all_bits(xcr0, (1 << 17) | (1 << 18));
    if (!hasamxsave)
        features_disable_amx(features);
    // Ignore feature bits that we are not interested in.
    mask_features(feature_masks, &features[0]);

    uint32_t cpu;
    if (vendor == SIG_INTEL) {
        cpu = uint32_t(get_intel_processor_name(family, model, brand_id, &features[0]));
    }
    else if (vendor == SIG_AMD) {
        cpu = uint32_t(get_amd_processor_name(family, model, &features[0]));
    }
    else {
        cpu = uint32_t(CPU::generic);
    }
    /* Feature bits to register map
    feature[0] = ecx
    feature[1] = edx
    feature[2] = leaf 7 ebx
    feature[3] = leaf 7 ecx
    feature[4] = leaf 7 edx
    feature[5] = leaf 0x80000001 ecx
    feature[6] = leaf 0x80000001 edx
    feature[7] = leaf 0xd subleaf 1 eax
    feature[8] = leaf 0x80000008 ebx
    feature[9] = leaf 7 ebx subleaf 1 eax
    feature[10] = leaf 7 ebx subleaf 1 ebx
    feature[11] = leaf 0x14 ebx
    */
    return std::make_pair(cpu, features);
}

static inline const std::pair<uint32_t,FeatureList<feature_sz>> &get_host_cpu()
{
    static auto host_cpu = _get_host_cpu();
    return host_cpu;
}

static inline const CPUSpec<CPU,feature_sz> *find_cpu(uint32_t cpu)
{
    return ::find_cpu(cpu, cpus, ncpu_names);
}

static inline const CPUSpec<CPU,feature_sz> *find_cpu(llvm::StringRef name)
{
    return ::find_cpu(name, cpus, ncpu_names);
}

static inline const char *find_cpu_name(uint32_t cpu)
{
    return ::find_cpu_name(cpu, cpus, ncpu_names);
}

static inline const std::string &host_cpu_name()
{
    static std::string name =
        (CPU)get_host_cpu().first != CPU::generic ?
        std::string(find_cpu_name(get_host_cpu().first)) :
        jl_get_cpu_name_llvm();
    return name;
}

static inline const char *normalize_cpu_name(llvm::StringRef name)
{
    if (name == "atom")
        return "bonnell";
    if (name == "slm")
        return "silvermont";
    if (name == "glm")
        return "goldmont";
    if (name == "corei7")
        return "nehalem";
    if (name == "corei7-avx")
        return "sandybridge";
    if (name == "core-avx-i")
        return "ivybridge";
    if (name == "core-avx2")
        return "haswell";
    if (name == "skx")
        return "skylake-avx512";
#ifdef _CPU_X86_
    // i686 isn't a supported target but it's a common default one so just make it mean pentium4.
    if (name == "pentium4" || name == "i686")
        return "generic";
#else
    if (name == "x86-64" || name == "x86_64")
        return "generic";
#endif
    return nullptr;
}

template<size_t n>
static inline void enable_depends(FeatureList<n> &features)
{
    ::enable_depends(features, Feature::deps, sizeof(Feature::deps) / sizeof(FeatureDep));
}

template<size_t n>
static inline void disable_depends(FeatureList<n> &features)
{
    ::disable_depends(features, Feature::deps, sizeof(Feature::deps) / sizeof(FeatureDep));
}

static const llvm::SmallVector<TargetData<feature_sz>, 0> &get_cmdline_targets(const char *cpu_target)
{
    auto feature_cb = [] (const char *str, size_t len, FeatureList<feature_sz> &list) {
        auto fbit = find_feature_bit(feature_names, nfeature_names, str, len);
        if (fbit == UINT32_MAX)
            return false;
        set_bit(list, fbit, true);
        return true;
    };
    auto &targets = ::get_cmdline_targets<feature_sz>(cpu_target, feature_cb);
    for (auto &t: targets) {
        if (auto nname = normalize_cpu_name(t.name)) {
            t.name = nname;
        }
    }
    return targets;
}

static llvm::SmallVector<TargetData<feature_sz>, 0> jit_targets;

static TargetData<feature_sz> arg_target_data(const TargetData<feature_sz> &arg, bool require_host)
{
    TargetData<feature_sz> res = arg;
    const FeatureList<feature_sz> *cpu_features = nullptr;
    if (res.name == "native") {
        res.name = host_cpu_name();
        cpu_features = &get_host_cpu().second;
    }
    else if (auto spec = find_cpu(res.name)) {
        cpu_features = &spec->features;
    }
    else {
        res.en.flags |= JL_TARGET_UNKNOWN_NAME;
    }
    if (cpu_features) {
        for (size_t i = 0; i < feature_sz; i++) {
            res.en.features[i] |= (*cpu_features)[i];
        }
    }
    enable_depends(res.en.features);
    // Mask our rdrand/rdseed/rtm/xsaveopt features that LLVM doesn't use and rr disables
    unset_bits(res.en.features, Feature::rdrnd, Feature::rdseed, Feature::rtm, Feature::xsaveopt);
    for (size_t i = 0; i < feature_sz; i++)
        res.en.features[i] &= ~res.dis.features[i];
    if (require_host) {
        for (size_t i = 0; i < feature_sz; i++) {
            res.en.features[i] &= get_host_cpu().second[i];
        }
    }
    disable_depends(res.en.features);
    if (cpu_features) {
        // If the base feature if known, fill in the disable features
        for (size_t i = 0; i < feature_sz; i++) {
            res.dis.features[i] = feature_masks[i] & ~res.en.features[i];
        }
    }
    return res;
}

static int max_vector_size(const FeatureList<feature_sz> &features)
{
    if (test_nbit(features, Feature::avx512f))
        return 64;
    if (test_nbit(features, Feature::avx))
        return 32;
    // SSE is required
    return 16;
}

static uint32_t sysimg_init_cb(void *ctx, const void *id, jl_value_t** rejection_reason)
{
    // First see what target is requested for the JIT.
    const char *cpu_target = (const char *)ctx;
    auto &cmdline = get_cmdline_targets(cpu_target);
    TargetData<feature_sz> target = arg_target_data(cmdline[0], true);
    // Then find the best match in the sysimg
    auto sysimg = deserialize_target_data<feature_sz>((const uint8_t*)id);
    // We translate `generic` to `pentium4` or `x86-64` before sending it to LLVM
    // (see `get_llvm_target_noext`) which will be serialized into the sysimg target data.
    // Translate them back so we can actually match them.
    // We also track to see if the sysimg allows -cx16, however if the user does
    // something silly like add +cx16 on a 32bit target, we want to disable this
    // check, hence the pointer size check.
    bool sysimg_allows_no_cx16 = sizeof(void *) == 4;;
    for (auto &t: sysimg) {
        if (auto nname = normalize_cpu_name(t.name)) {
            t.name = nname;
        }

        // Take note to see if the sysimg explicitly allows an architecture without cx16
        sysimg_allows_no_cx16 |= !test_nbit(t.en.features, Feature::cx16);
    }
    if (!sysimg_allows_no_cx16 && !test_nbit(target.en.features, Feature::cx16)) {
        jl_error("Your CPU does not support the CX16 instruction, which is required "
                 "by this version of Julia!  This is often due to running inside of a "
                 "virtualized environment.  Please read "
                 "https://docs.julialang.org/en/v1/devdocs/sysimg/ for more.");
    }
    auto match = match_sysimg_targets(sysimg, target, max_vector_size, rejection_reason);
    if (match.best_idx == UINT32_MAX)
        return match.best_idx;
    // Now we've decided on which sysimg version to use.
    // Make sure the JIT target is compatible with it and save the JIT target.
    if (match.vreg_size != max_vector_size(target.en.features) &&
        (sysimg[match.best_idx].en.flags & JL_TARGET_VEC_CALL)) {
        if (match.vreg_size < 64) {
            features_disable_avx512(target.en.features);
        }
        if (match.vreg_size < 32) {
            features_disable_avx(target.en.features);
        }
    }
    jit_targets.push_back(std::move(target));
    return match.best_idx;
}

static uint32_t pkgimg_init_cb(void *ctx, const void *id, jl_value_t **rejection_reason)
{
    TargetData<feature_sz> target = jit_targets.front();
    auto pkgimg = deserialize_target_data<feature_sz>((const uint8_t*)id);
    for (auto &t: pkgimg) {
        if (auto nname = normalize_cpu_name(t.name)) {
            t.name = nname;
        }
    }
    auto match = match_sysimg_targets(pkgimg, target, max_vector_size, rejection_reason);
    return match.best_idx;
}

//This function serves as a fallback during bootstrapping, at that point we don't have a sysimage with native code
// so we won't call sysimg_init_cb, else this function shouldn't do anything.
static void ensure_jit_target(const char *cpu_target, bool imaging)
{
    auto &cmdline = get_cmdline_targets(cpu_target);
    check_cmdline(cmdline, imaging);
    if (!jit_targets.empty())
        return;
    for (auto &arg: cmdline) {
        auto data = arg_target_data(arg, jit_targets.empty());
        jit_targets.push_back(std::move(data));
    }
    auto ntargets = jit_targets.size();
    // Now decide the clone condition.
    for (size_t i = 1; i < ntargets; i++) {
        auto &t = jit_targets[i];
        if (t.en.flags & JL_TARGET_CLONE_ALL)
            continue;
        // Always clone when code checks CPU features
        t.en.flags |= JL_TARGET_CLONE_CPU;
        // The most useful one in general...
        t.en.flags |= JL_TARGET_CLONE_LOOP;
        auto &features0 = jit_targets[t.base].en.features;
        // Special case for KNL/KNM since they're so different
        if (!(t.dis.flags & JL_TARGET_CLONE_ALL)) {
            if ((t.name == "knl" || t.name == "knm") &&
                jit_targets[t.base].name != "knl" && jit_targets[t.base].name != "knm") {
                t.en.flags |= JL_TARGET_CLONE_ALL;
                break;
            }
        }
        static constexpr uint32_t clone_math[] = {Feature::fma, Feature::fma4};
        static constexpr uint32_t clone_simd[] = {Feature::sse3, Feature::ssse3,
                                                  Feature::sse41, Feature::sse42,
                                                  Feature::avx, Feature::avx2,
                                                  Feature::vaes, Feature::vpclmulqdq,
                                                  Feature::sse4a, Feature::avx512f,
                                                  Feature::avx512dq, Feature::avx512ifma,
                                                  Feature::avx512cd, Feature::avx512bw,
                                                  Feature::avx512vl, Feature::avx512vbmi,
                                                  Feature::avx512vpopcntdq, Feature::avxvnni,
                                                  Feature::avx512vbmi2, Feature::avx512vnni,
                                                  Feature::avx512bitalg, Feature::avx512bf16,
                                                  Feature::avx512vp2intersect, Feature::avx512fp16};
        for (auto fe: clone_math) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_MATH;
                break;
            }
        }
        for (auto fe: clone_simd) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_SIMD;
                break;
            }
        }
        static constexpr uint32_t clone_fp16[] = {Feature::avx512fp16};
        for (auto fe: clone_fp16) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_FLOAT16;
                break;
            }
        }
        static constexpr uint32_t clone_bf16[] = {Feature::avx512bf16};
        for (auto fe: clone_bf16) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_BFLOAT16;
                break;
            }
        }
    }
}

static std::pair<std::string,llvm::SmallVector<std::string, 0>>
get_llvm_target_noext(const TargetData<feature_sz> &data)
{
    std::string name = data.name;
    auto *spec = find_cpu(name);
    while (spec) {
        if (spec->llvmver <= JL_LLVM_VERSION)
            break;
        spec = find_cpu((uint32_t)spec->fallback);
        name = spec->name;
    }
    if (name == "generic") {
        // Use translate `generic` into what we actually require
#ifdef _CPU_X86_
        name = "pentium4";
#else
        name = "x86-64";
#endif
    }
    llvm::SmallVector<std::string, 0> features;
    for (auto &fename: feature_names) {
        if (fename.llvmver > JL_LLVM_VERSION)
            continue;
        if (test_nbit(data.en.features, fename.bit)) {
            features.insert(features.begin(), std::string("+") + fename.name);
        }
        else if (test_nbit(data.dis.features, fename.bit)) {
            features.push_back(std::string("-") + fename.name);
        }
    }
    features.push_back("+sse2");
    features.push_back("+mmx");
    features.push_back("+fxsr");
#ifdef _CPU_X86_64_
    // This is required to make LLVM happy if LLVM's feature based CPU arch guess
    // returns a value that may not have 64bit support.
    // This can happen with virtualization.
    features.push_back("+64bit");
#endif
    features.push_back("+cx8");
    return std::make_pair(std::move(name), std::move(features));
}

static std::pair<std::string,llvm::SmallVector<std::string, 0>>
get_llvm_target_vec(const TargetData<feature_sz> &data)
{
    auto res0 = get_llvm_target_noext(data);
    append_ext_features(res0.second, data.ext_features);
    return res0;
}

static std::pair<std::string,std::string>
get_llvm_target_str(const TargetData<feature_sz> &data)
{
    auto res0 = get_llvm_target_noext(data);
    auto features = join_feature_strs(res0.second);
    append_ext_features(features, data.ext_features);
    return std::make_pair(std::move(res0.first), std::move(features));
}

}

using namespace X86;

JL_DLLEXPORT void jl_dump_host_cpu(void)
{
    dump_cpu_spec(get_host_cpu().first, get_host_cpu().second, feature_names, nfeature_names,
                  cpus, ncpu_names);
}

JL_DLLEXPORT jl_value_t* jl_check_pkgimage_clones(char *data)
{
    jl_value_t *rejection_reason = NULL;
    JL_GC_PUSH1(&rejection_reason);
    uint32_t match_idx = pkgimg_init_cb(NULL, data, &rejection_reason);
    JL_GC_POP();
    if (match_idx == UINT32_MAX)
        return rejection_reason;
    return jl_nothing;
}

JL_DLLEXPORT jl_value_t *jl_cpu_has_fma(int bits)
{
    TargetData<feature_sz> target = jit_targets.front();
    FeatureList<feature_sz> features = target.en.features;
    if ((bits == 32 || bits == 64) && (test_nbit(features, Feature::fma) || test_nbit(features, Feature::fma4)))
        return jl_true;
    else
        return jl_false;
}

jl_image_t jl_init_processor_sysimg(jl_image_buf_t image, const char *cpu_target)
{
    if (!jit_targets.empty())
        jl_error("JIT targets already initialized");
    return parse_sysimg(image, sysimg_init_cb, (void *)cpu_target);
}

jl_image_t jl_init_processor_pkgimg(jl_image_buf_t image)
{
    if (jit_targets.empty())
        jl_error("JIT targets not initialized");
    if (jit_targets.size() > 1)
        jl_error("Expected only one JIT target");
    return parse_sysimg(image, pkgimg_init_cb, NULL);
}

std::pair<std::string,llvm::SmallVector<std::string, 0>> jl_get_llvm_target(const char *cpu_target, bool imaging, uint32_t &flags)
{
    ensure_jit_target(cpu_target, imaging);
    flags = jit_targets[0].en.flags;
    return get_llvm_target_vec(jit_targets[0]);
}

const std::pair<std::string,std::string> &jl_get_llvm_disasm_target(void)
{
    static const auto res = get_llvm_target_str(TargetData<feature_sz>{"generic", "",
            {feature_masks, 0}, {{}, 0}, 0});
    return res;
}
//This function parses the -C command line to figure out which targets to multiversion to.
#ifndef __clang_gcanalyzer__
llvm::SmallVector<jl_target_spec_t, 0> jl_get_llvm_clone_targets(const char *cpu_target)
{

    auto &cmdline = get_cmdline_targets(cpu_target);
    check_cmdline(cmdline, true);
    llvm::SmallVector<TargetData<feature_sz>, 0> image_targets;
    for (auto &arg: cmdline) {
        auto data = arg_target_data(arg, image_targets.empty());
        image_targets.push_back(std::move(data));
    }

    auto ntargets = image_targets.size();
    // Now decide the clone condition.
    for (size_t i = 1; i < ntargets; i++) {
        auto &t = image_targets[i];
        if (t.en.flags & JL_TARGET_CLONE_ALL)
            continue;
        // Always clone when code checks CPU features
        t.en.flags |= JL_TARGET_CLONE_CPU;
        // The most useful one in general...
        t.en.flags |= JL_TARGET_CLONE_LOOP;
        auto &features0 = image_targets[t.base].en.features;
        // Special case for KNL/KNM since they're so different
        if (!(t.dis.flags & JL_TARGET_CLONE_ALL)) {
            if ((t.name == "knl" || t.name == "knm") &&
                image_targets[t.base].name != "knl" && image_targets[t.base].name != "knm") {
                t.en.flags |= JL_TARGET_CLONE_ALL;
                break;
            }
        }
        static constexpr uint32_t clone_math[] = {Feature::fma, Feature::fma4};
        static constexpr uint32_t clone_simd[] = {Feature::sse3, Feature::ssse3,
                                                  Feature::sse41, Feature::sse42,
                                                  Feature::avx, Feature::avx2,
                                                  Feature::vaes, Feature::vpclmulqdq,
                                                  Feature::sse4a, Feature::avx512f,
                                                  Feature::avx512dq, Feature::avx512ifma,
                                                  Feature::avx512cd, Feature::avx512bw,
                                                  Feature::avx512vl, Feature::avx512vbmi,
                                                  Feature::avx512vpopcntdq, Feature::avxvnni,
                                                  Feature::avx512vbmi2, Feature::avx512vnni,
                                                  Feature::avx512bitalg, Feature::avx512bf16,
                                                  Feature::avx512vp2intersect, Feature::avx512fp16};
        for (auto fe: clone_math) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_MATH;
                break;
            }
        }
        for (auto fe: clone_simd) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_SIMD;
                break;
            }
        }
        static constexpr uint32_t clone_fp16[] = {Feature::avx512fp16};
        for (auto fe: clone_fp16) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_FLOAT16;
                break;
            }
        }
        static constexpr uint32_t clone_bf16[] = {Feature::avx512bf16};
        for (auto fe: clone_bf16) {
            if (!test_nbit(features0, fe) && test_nbit(t.en.features, fe)) {
                t.en.flags |= JL_TARGET_CLONE_BFLOAT16;
                break;
            }
        }
    }
    if (image_targets.empty())
        jl_error("No targets specified");
    llvm::SmallVector<jl_target_spec_t, 0> res;
    for (auto &target: image_targets) {
        auto features_en = target.en.features;
        auto features_dis = target.dis.features;
        for (auto &fename: feature_names) {
            if (fename.llvmver > JL_LLVM_VERSION) {
                unset_bits(features_en, fename.bit);
                unset_bits(features_dis, fename.bit);
            }
        }
        X86::disable_depends(features_en);
        jl_target_spec_t ele;
        std::tie(ele.cpu_name, ele.cpu_features) = get_llvm_target_str(target);
        ele.data = serialize_target_data(target.name, features_en, features_dis,
                                         target.ext_features);
        ele.flags = target.en.flags;
        ele.base = target.base;
        res.push_back(ele);
    }
    return res;
}
#endif

extern "C" int jl_test_cpu_feature(jl_cpu_feature_t feature)
{
    if (feature >= 32 * feature_sz)
        return 0;
    return test_nbit(&get_host_cpu().second[0], feature);
}

// -- set/clear the FZ/DAZ flags on x86 & x86-64 --

// Cache of information recovered from `cpuid` since executing `cpuid` it at runtime is slow.
static uint32_t subnormal_flags = [] {
    int32_t info[4];
    jl_cpuid(info, 0);
    if (info[0] >= 1) {
        jl_cpuid(info, 1);
        if (info[3] & (1 << 26)) {
            // SSE2 supports both FZ and DAZ
            return 0x00008040;
        }
        else if (info[3] & (1 << 25)) {
            // SSE supports only the FZ flag
            return 0x00008000;
        }
    }
    return 0;
}();

// Returns non-zero if subnormals go to 0; zero otherwise.
extern "C" JL_DLLEXPORT int32_t jl_get_zero_subnormals(void)
{
    return _mm_getcsr() & subnormal_flags;
}

// Return zero on success, non-zero on failure.
extern "C" JL_DLLEXPORT int32_t jl_set_zero_subnormals(int8_t isZero)
{
    uint32_t flags = subnormal_flags;
    if (flags) {
        uint32_t state = _mm_getcsr();
        if (isZero)
            state |= flags;
        else
            state &= ~flags;
        _mm_setcsr(state);
        return 0;
    }
    else {
        // Report a failure only if user is trying to enable FTZ/DAZ.
        return isZero;
    }
}

// X86 does not support default NaNs
extern "C" JL_DLLEXPORT int32_t jl_get_default_nans(void)
{
    return 0;
}

extern "C" JL_DLLEXPORT int32_t jl_set_default_nans(int8_t isDefault)
{
    return isDefault;
}
