#include "g7xx.h"
#include <assert.h>
#include <string.h>

uint8_t G711_ALawEncode(int16_t pcm16)
{
    int p = pcm16;
    unsigned a;
    if (p < 0)
    {
        p = ~p;
        a = 0x00;
    }
    else
    {
        a = 0x80;
    }

    p >>= 4;
    if (p >= 0x20)
    {
        if (p >= 0x100)
        {
            p >>= 4;
            a += 0x40;
        }
        if (p >= 0x40)
        {
            p >>= 2;
            a += 0x20;
        }
        if (p >= 0x20)
        {
            p >>= 1;
            a += 0x10;
        }
    }
    a += p;
    return a ^ 0x55;
}

int G711_ALawDecode(uint8_t alaw)
{
    alaw ^= 0x55;
    unsigned sign = alaw & 0x80;
    int linear = alaw & 0x1f;
    linear <<= 4;
    linear += 8;
    alaw &= 0x7f;
    if (alaw >= 0x20)
    {
        linear |= 0x100;
        unsigned shift = (alaw >> 4) - 1;
        linear <<= shift;
    }
    return sign ? linear : -linear;
}

uint8_t G711_ULawEncode(int16_t pcm16)
{
    int p = pcm16;
    unsigned u;
    if (p < 0)
    {
        p = ~p;
        u = 0x80 ^ 0x10 ^ 0xff;
    }
    else
    {
        u = 0x00 ^ 0x10 ^ 0xff;
    }
    p += 0x84;
    if (p > 0x7f00)
        p = 0x7f00;
    p >>= 3;
    if (p >= 0x100)
    {
        p >>= 4;
        u ^= 0x40;
    }
    if (p >= 0x40)
    {
        p >>= 2;
        u ^= 0x20;
    }
    if (p >= 0x20)
    {
        p >>= 1;
        u ^= 0x10;
    }
    u ^= p;
    return u;
}

int G711_ULawDecode(uint8_t ulaw)
{
    ulaw ^= 0xff;
    int linear = ulaw & 0x0f;
    linear <<= 3;
    linear |= 0x84;
    unsigned shift = ulaw >> 4;
    shift &= 7;
    linear <<= shift;
    linear -= 0x84;
    return (ulaw & 0x80) ? -linear : linear;
}

uint8_t G711_ALawToULaw(uint8_t alaw)
{
    uint8_t sign = alaw & 0x80;
    alaw ^= sign;
    alaw ^= 0x55;
    unsigned ulaw;
    if (alaw < 45)
    {
        if (alaw < 24)
            ulaw = (alaw < 8) ? (alaw << 1) + 1 : alaw + 8;
        else
            ulaw = (alaw < 32) ? (alaw >> 1) + 20 : alaw + 4;
    }
    else
    {
        if (alaw < 63)
            ulaw = (alaw < 47) ? alaw + 3 : alaw + 2;
        else
            ulaw = (alaw < 79) ? alaw + 1 : alaw;
    }
    ulaw ^= sign;
    return ulaw ^ 0x7f;
}

uint8_t G711_ULawToALaw(uint8_t ulaw)
{
    uint8_t sign = ulaw & 0x80;
    ulaw ^= sign;
    ulaw ^= 0x7f;
    unsigned alaw;
    if (ulaw < 48)
    {
        if (ulaw <= 32)
            alaw = (ulaw <= 15) ? ulaw >> 1 : ulaw - 8;
        else
            alaw = (ulaw <= 35) ? (ulaw << 1) - 40 : ulaw - 4;
    }
    else
    {
        if (ulaw <= 63)
            alaw = (ulaw == 48) ? ulaw - 3 : ulaw - 2;
        else
            alaw = (ulaw <= 79) ? ulaw - 1 : ulaw;
    }
    alaw ^= sign;
    return alaw ^ 0x55;
}

// 批量处理函数
unsigned G711_ALawEncodeBuf(uint8_t *dst, int16_t *src, size_t srcSize)
{
    size_t samples = srcSize >> 1;
    uint8_t *end = dst + samples;
    while (dst < end)
    {
        *dst++ = G711_ALawEncode(*src++);
    }
    return samples;
}

unsigned G711_ALawDecodeBuf(int16_t *dst, const uint8_t *src, size_t srcSize)
{
    int16_t *end = dst + srcSize;
    while (dst < end)
    {
        *dst++ = G711_ALawDecode(*src++);
    }
    return srcSize << 1;
}

unsigned G711_ULawEncodeBuf(uint8_t *dst, int16_t *src, size_t srcSize)
{
    size_t samples = srcSize >> 1;
    uint8_t *end = dst + samples;
    while (dst < end)
    {
        *dst++ = G711_ULawEncode(*src++);
    }
    return samples;
}

unsigned G711_ULawDecodeBuf(int16_t *dst, const uint8_t *src, size_t srcSize)
{
    int16_t *end = dst + srcSize;
    while (dst < end)
    {
        *dst++ = G711_ULawDecode(*src++);
    }
    return srcSize << 1;
}

unsigned G711_ALawToULawBuf(uint8_t *dst, const uint8_t *src, size_t srcSize)
{
    uint8_t *end = dst + srcSize;
    while (dst < end)
    {
        *dst++ = G711_ALawToULaw(*src++);
    }
    return srcSize;
}

unsigned G711_ULawToALawBuf(uint8_t *dst, const uint8_t *src, size_t srcSize)
{
    uint8_t *end = dst + srcSize;
    while (dst < end)
    {
        *dst++ = G711_ULawToALaw(*src++);
    }
    return srcSize;
}

// ===================== G726 内部函数 =====================

// 范围检查宏（在DEBUG模式下启用）
#ifdef DEBUG
#define CHECK_SM(x, bits) assert(((x) >> (bits)) == 0)
#define CHECK_UM(x, bits) assert(((x) >> (bits)) == 0)
#define CHECK_TC(x, bits) assert(((x) >> ((bits) - 1)) == ((x) < 0 ? -1 : 0))
#define CHECK_FL(x, bits) assert(((x) >> (bits)) == 0)
#define CHECK_UNSIGNED(x, bits) assert(((x) >> (bits)) == 0)
#else
#define CHECK_SM(x, bits) (void)(x)
#define CHECK_UM(x, bits) (void)(x)
#define CHECK_TC(x, bits) (void)(x)
#define CHECK_FL(x, bits) (void)(x)
#define CHECK_UNSIGNED(x, bits) (void)(x)
#endif

// G726 内部函数实现
static void EXPAND(unsigned S, unsigned LAW, int *SL)
{
    CHECK_UNSIGNED(S, 8);
    CHECK_UNSIGNED(LAW, 1);
    int linear;
    if (LAW)
    {
        linear = G711_ALawDecode(S);
    }
    else
    {
        linear = G711_ULawDecode(S);
    }
    *SL = linear >> 2;
    CHECK_TC(*SL, 14);
}

static void SUBTA(int SL, int SE, int *D)
{
    CHECK_TC(SL, 14);
    CHECK_TC(SE, 15);
    *D = SL - SE;
    CHECK_TC(*D, 16);
}

static void LOG(int D, unsigned *DL, int *DS)
{
    CHECK_TC(D, 16);
    *DS = D >> 15;
    unsigned DQM = (D < 0) ? -D : D;
    DQM &= 0x7fff;
    unsigned EXP = 0;
    unsigned x = DQM;
    if (x >= (1 << 8))
    {
        EXP |= 8;
        x >>= 8;
    }
    if (x >= (1 << 4))
    {
        EXP |= 4;
        x >>= 4;
    }
    if (x >= (1 << 2))
    {
        EXP |= 2;
        x >>= 2;
    }
    EXP |= x >> 1;
    unsigned MANT = ((DQM << 7) >> EXP) & 0x7f;
    *DL = (EXP << 7) + MANT;
    CHECK_UM(*DL, 11);
    CHECK_TC(*DS, 1);
}

static void QUAN(unsigned RATE, int DLN, int DS, unsigned *I)
{
    CHECK_TC(DLN, 12);
    CHECK_TC(DS, 1);
    int x;
    if (RATE == 2)
    {
        x = (DLN >= 261);
    }
    else
    {
        static const int16_t quan3[4] = {8, 218, 331, 0x7fff};
        static const int16_t quan4[8] = {3972 - 0x1000, 80, 178, 246, 300, 349, 400, 0x7fff};
        static const int16_t quan5[16] = {3974 - 0x1000, 4080 - 0x1000, 68, 139, 198, 250, 298, 339,
                                          378, 413, 445, 475, 502, 528, 553, 0x7fff};
        static const int16_t *const quan[3] = {quan3, quan4, quan5};
        const int16_t *levels = quan[RATE - 3];
        const int16_t *levels0 = levels;
        while (DLN >= *levels)
        {
            levels++;
        }
        x = (int)(levels - levels0 - 1);
        if (!x)
        {
            x = ~DS;
        }
    }
    int mask = (1 << RATE) - 1;
    *I = (x ^ DS) & mask;
    CHECK_UNSIGNED(*I, RATE);
}

static void SUBTB(unsigned DL, unsigned Y, int *DLN)
{
    CHECK_UM(DL, 11);
    CHECK_UM(Y, 13);
    *DLN = DL - (Y >> 2);
    CHECK_TC(*DLN, 12);
}

static void ADDA(int DQLN, unsigned Y, int *DQL)
{
    CHECK_TC(DQLN, 12);
    CHECK_UM(Y, 13);
    *DQL = DQLN + (Y >> 2);
    CHECK_TC(*DQL, 12);
}

static void ANTILOG(int DQL, int DQS, unsigned *DQ)
{
    CHECK_TC(DQL, 12);
    CHECK_TC(DQS, 1);
    unsigned DEX = (DQL >> 7) & 15;
    unsigned DMN = DQL & 127;
    unsigned DQT = (1 << 7) + DMN;
    unsigned DQMAG;
    if (DQL >= 0)
    {
        DQMAG = (DQT << 7) >> (14 - DEX);
    }
    else
    {
        DQMAG = 0;
    }
    *DQ = DQS ? DQMAG + (1 << 15) : DQMAG;
    CHECK_SM(*DQ, 16);
}

static void RECONST(unsigned RATE, unsigned I, int *DQLN, int *DQS)
{
    CHECK_UNSIGNED(I, RATE);
    static const int16_t reconst2[2] = {116, 365};
    static const int16_t reconst3[4] = {-2048, 135, 273, 373}; // Note: -2048 is 0xF800 in 16-bit two's complement
    static const int16_t reconst4[8] = {-2048, 4, 135, 213, 273, 323, 373, 425};
    static const int16_t reconst5[16] = {-2048, -66, 28, 104, 169, 224, 274, 318, 358, 395, 429, 459, 488, 514, 539, 566};
    static const int16_t *const reconst[4] = {reconst2, reconst3, reconst4, reconst5};
    int x = (int)I;
    int m = 1 << (RATE - 1);
    if (x & m)
    {
        *DQS = -1;
        x = ~x;
    }
    else
    {
        *DQS = 0;
    }
    *DQLN = reconst[RATE - 2][x & (m - 1)];
    CHECK_TC(*DQLN, 12);
    CHECK_TC(*DQS, 1);
}

static void FILTD(int WI, unsigned Y, unsigned *YUT)
{
    CHECK_TC(WI, 12);
    CHECK_UM(Y, 13);
    int DIF = (WI << 5) - (int)Y;
    int DIFSX = DIF >> 5;
    *YUT = (Y + DIFSX) & 0x1FFF; // Mask to 13 bits
    CHECK_UM(*YUT, 13);
}

static void FILTE(unsigned YUP, unsigned YL, unsigned *YLP)
{
    CHECK_UM(YUP, 13);
    CHECK_UM(YL, 19);
    int DIF = (YUP << 6) - (int)(YL >> 6); // Note: YL is 19 bits, so shift to align
    int DIFSX = DIF >> 6;
    *YLP = (YL + DIFSX) & 0x7FFFF; // Mask to 19 bits
    CHECK_UM(*YLP, 19);
}

static void FUNCTW(unsigned RATE, unsigned I, int *WI)
{
    CHECK_UNSIGNED(I, RATE);
    static const int16_t functw2[2] = {-22, 439}; // -22 is 4074-4096
    static const int16_t functw3[4] = {-4, 30, 137, 582};
    static const int16_t functw4[8] = {-12, 18, 41, 64, 112, 198, 355, 1122};
    static const int16_t functw5[16] = {14, 14, 24, 39, 40, 41, 58, 100, 141, 179, 219, 280, 358, 440, 529, 696};
    static const int16_t *const functw[4] = {functw2, functw3, functw4, functw5};
    unsigned signMask = 1 << (RATE - 1);
    unsigned n = (I & signMask) ? (2 * signMask - 1) - I : I;
    *WI = functw[RATE - 2][n & (signMask - 1)];
    CHECK_TC(*WI, 12);
}

static void LIMB(unsigned YUT, unsigned *YUP)
{
    CHECK_UM(YUT, 13);
    if ((YUT + 11264) & 0x2000)
    { // GEUL: if YUT+11264 has bit13 set? (overflow)
        *YUP = 544;
    }
    else if (!((YUT + 15840) & 0x2000))
    { // GELL: if YUT+15840 does not have bit13 set?
        *YUP = 5120;
    }
    else
    {
        *YUP = YUT;
    }
    CHECK_UM(*YUP, 13);
}

static void MIX(unsigned AL, unsigned YU, unsigned YL, unsigned *Y)
{
    CHECK_UM(AL, 7);
    CHECK_UM(YU, 13);
    CHECK_UM(YL, 19);
    int DIF = (int)YU - (int)(YL >> 6);
    int PROD = DIF * (int)AL;
    if (DIF < 0)
    {
        PROD += (1 << 6) - 1;
    }
    PROD >>= 6;
    *Y = ((YL >> 6) + PROD) & 0x1FFF; // Mask to 13 bits
    CHECK_UM(*Y, 13);
}

static void FILTA(unsigned FI, unsigned DMS, unsigned *DMSP)
{
    CHECK_UM(FI, 3);
    CHECK_UM(DMS, 12);
    int DIF = (FI << 9) - (int)DMS;
    int DIFSX = DIF >> 5;
    *DMSP = (DIFSX + DMS) & 0xFFF; // Mask to 12 bits
    CHECK_UM(*DMSP, 12);
}

static void FILTB(unsigned FI, unsigned DML, unsigned *DMLP)
{
    CHECK_UM(FI, 3);
    CHECK_UM(DML, 14);
    int DIF = (FI << 11) - (int)DML;
    int DIFSX = DIF >> 7;
    *DMLP = (DIFSX + DML) & 0x3FFF; // Mask to 14 bits
    CHECK_UM(*DMLP, 14);
}

static void FILTC(unsigned AX, unsigned AP, unsigned *APP)
{
    CHECK_UM(AX, 1);
    CHECK_UM(AP, 10);
    int DIF = (AX << 9) - (int)AP;
    int DIFSX = DIF >> 4;
    *APP = (DIFSX + AP) & 0x3FF; // Mask to 10 bits
    CHECK_UM(*APP, 10);
}

static void FUNCTF(unsigned RATE, unsigned I, unsigned *FI)
{
    CHECK_UNSIGNED(I, RATE);
    static const int16_t functf2[2] = {0, 7};
    static const int16_t functf3[4] = {0, 1, 2, 7};
    static const int16_t functf4[8] = {0, 0, 0, 1, 1, 1, 3, 7};
    static const int16_t functf5[16] = {0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 3, 4, 5, 6, 6};
    static const int16_t *const functf[4] = {functf2, functf3, functf4, functf5};
    unsigned x = I;
    int mask = (1 << (RATE - 1)) - 1;
    if (x & (1 << (RATE - 1)))
    {
        x = x ^ mask; // Invert the lower bits
    }
    x &= mask;
    *FI = functf[RATE - 2][x];
    CHECK_UM(*FI, 3);
}

static void LIMA(unsigned AP, unsigned *AL)
{
    CHECK_UM(AP, 10);
    *AL = (AP > 256) ? 64 : (AP >> 2);
    CHECK_UM(*AL, 7);
}

static void SUBTC(unsigned DMSP, unsigned DMLP, unsigned TDP, unsigned Y, unsigned *AX)
{
    CHECK_UM(DMSP, 12);
    CHECK_UM(DMLP, 14);
    CHECK_UNSIGNED(TDP, 1);
    CHECK_UM(Y, 13);
    int DIF = (int)(DMSP << 2) - (int)DMLP;
    unsigned DIFM = (DIF < 0) ? -DIF : DIF;
    unsigned DTHR = DMLP >> 3;
    *AX = (Y >= 1536 && DIFM < DTHR) ? TDP : 1;
    CHECK_UM(*AX, 1);
}

static void TRIGA(unsigned TR, unsigned APP, unsigned *APR)
{
    CHECK_UNSIGNED(TR, 1);
    CHECK_UM(APP, 10);
    *APR = TR ? 256 : APP;
    CHECK_UM(*APR, 10);
}

static void ACCUM(int WAn[2], int WBn[6], int *SE, int *SEZ)
{
    CHECK_TC(WAn[0], 16);
    CHECK_TC(WAn[1], 16);
    CHECK_TC(WBn[0], 16);
    CHECK_TC(WBn[1], 16);
    CHECK_TC(WBn[2], 16);
    CHECK_TC(WBn[3], 16);
    CHECK_TC(WBn[4], 16);
    CHECK_TC(WBn[5], 16);
    int16_t SEZI = (int16_t)(WBn[0] + WBn[1] + WBn[2] + WBn[3] + WBn[4] + WBn[5]);
    int16_t SEI = (int16_t)(SEZI + WAn[0] + WAn[1]);
    *SEZ = SEZI >> 1;
    *SE = SEI >> 1;
    CHECK_TC(*SE, 15);
    CHECK_TC(*SEZ, 15);
}

static void ADDB(unsigned DQ, int SE, int *SR)
{
    CHECK_SM(DQ, 16);
    CHECK_TC(SE, 15);
    int DQI = (DQ & 0x8000) ? (0x8000 - DQ) : DQ; // If sign bit set, then DQ = negative magnitude
    *SR = (int16_t)(DQI + SE);
    CHECK_TC(*SR, 16);
}

static void ADDC(unsigned DQ, int SEZ, int *PK0, unsigned *SIGPK)
{
    CHECK_SM(DQ, 16);
    CHECK_TC(SEZ, 15);
    int DQI = (DQ & 0x8000) ? (0x8000 - DQ) : DQ;
    int DQSEZ = (int16_t)(DQI + SEZ);
    *PK0 = DQSEZ >> 15;
    *SIGPK = (DQSEZ == 0) ? 1 : 0;
    CHECK_TC(*PK0, 1);
    CHECK_UNSIGNED(*SIGPK, 1);
}

static void MagToFloat(unsigned mag, unsigned *exp, unsigned *mant)
{
    unsigned e = 0;
    unsigned m = mag << 1;
    if (m >= (1 << 8))
    {
        e |= 8;
        m >>= 8;
    }
    if (m >= (1 << 4))
    {
        e |= 4;
        m >>= 4;
    }
    if (m >= (1 << 2))
    {
        e |= 2;
        m >>= 2;
    }
    e |= m >> 1;
    *exp = e;
    *mant = mag ? (mag << 6) >> e : 1 << 5;
}

static void FLOATA(unsigned DQ, unsigned *DQ0)
{
    CHECK_SM(DQ, 16);
    unsigned DQS = (DQ >> 15);
    unsigned MAG = DQ & 0x7FFF;
    unsigned EXP, MANT;
    MagToFloat(MAG, &EXP, &MANT);
    *DQ0 = (DQS << 10) + (EXP << 6) + MANT;
    CHECK_FL(*DQ0, 11);
}

static void FLOATB(int SR, unsigned *SR0)
{
    CHECK_TC(SR, 16);
    unsigned SRS = (SR < 0) ? 1 : 0;
    unsigned MAG = (SRS) ? -SR : SR;
    MAG &= 0x7FFF;
    unsigned EXP, MANT;
    MagToFloat(MAG, &EXP, &MANT);
    *SR0 = (SRS << 10) + (EXP << 6) + MANT;
    CHECK_FL(*SR0, 11);
}

static void FMULT(int An, unsigned SRn, int *WAn)
{
    CHECK_TC(An, 16);
    CHECK_FL(SRn, 11);
    unsigned AnS = (An < 0) ? 1 : 0;
    unsigned AnMAG = AnS ? (-An) >> 2 : An >> 2;
    AnMAG &= 0x1FFF; // 13 bits
    unsigned AnEXP, AnMANT;
    MagToFloat(AnMAG, &AnEXP, &AnMANT);

    unsigned SRnS = SRn >> 10;
    unsigned SRnEXP = (SRn >> 6) & 15;
    unsigned SRnMANT = SRn & 63;

    unsigned WAnS = SRnS ^ AnS;
    unsigned WAnEXP = SRnEXP + AnEXP;
    unsigned WAnMANT = ((SRnMANT * AnMANT) + 48) >> 4;
    unsigned WAnMAG;
    if (WAnEXP <= 26)
    {
        WAnMAG = (WAnMANT << 7) >> (26 - WAnEXP);
    }
    else
    {
        WAnMAG = (WAnMANT << 7) << (WAnEXP - 26);
    }
    *WAn = WAnS ? -(int)WAnMAG : (int)WAnMAG;
    CHECK_TC(*WAn, 16);
}

static void LIMC(int A2T, int *A2P)
{
    CHECK_TC(A2T, 16);
    const int A2UL = 12288;
    const int A2LL = -12288; // Note: -12288 is 0xD000 in 16-bit two's complement
    if (A2T <= A2LL)
    {
        *A2P = A2LL;
    }
    else if (A2T >= A2UL)
    {
        *A2P = A2UL;
    }
    else
    {
        *A2P = A2T;
    }
    CHECK_TC(*A2P, 16);
}

static void LIMD(int A1T, int A2P, int *A1P)
{
    CHECK_TC(A1T, 16);
    CHECK_TC(A2P, 16);
    const int OME = 15360;
    int A1UL = OME - A2P;
    int A1LL = A2P - OME;
    if (A1T <= A1LL)
    {
        *A1P = A1LL;
    }
    else if (A1T >= A1UL)
    {
        *A1P = A1UL;
    }
    else
    {
        *A1P = A1T;
    }
    CHECK_TC(*A1P, 16);
}

static void TRIGB(unsigned TR, int AnP, int *AnR)
{
    CHECK_UNSIGNED(TR, 1);
    CHECK_TC(AnP, 16);
    *AnR = TR ? 0 : AnP;
    CHECK_TC(*AnR, 16);
}

static void UPA1(int PK0, int PK1, int A1, unsigned SIGPK, int *A1T)
{
    CHECK_TC(PK0, 1);
    CHECK_TC(PK1, 1);
    CHECK_TC(A1, 16);
    CHECK_UNSIGNED(SIGPK, 1);
    int UGA1;
    if (SIGPK == 0)
    {
        if (PK0 ^ PK1)
        {
            UGA1 = -192;
        }
        else
        {
            UGA1 = 192;
        }
    }
    else
    {
        UGA1 = 0;
    }
    *A1T = (int16_t)(A1 + UGA1 - (A1 >> 8));
    CHECK_TC(*A1T, 16);
}

static void UPA2(int PK0, int PK1, int PK2, int A1, int A2, unsigned SIGPK, int *A2T)
{
    CHECK_TC(PK0, 1);
    CHECK_TC(PK1, 1);
    CHECK_TC(PK2, 1);
    CHECK_TC(A1, 16);
    CHECK_TC(A2, 16);
    CHECK_UNSIGNED(SIGPK, 1);
    int UGA2;
    if (SIGPK == 0)
    {
        int UGA2A = (PK0 ^ PK2) ? -16384 : 16384;
        int FA1;
        if (A1 < -8191)
        {
            FA1 = -8191 << 2;
        }
        else if (A1 > 8191)
        {
            FA1 = 8191 << 2;
        }
        else
        {
            FA1 = A1 << 2;
        }
        int FA = (PK0 ^ PK1) ? FA1 : -FA1;
        UGA2 = (UGA2A + FA) >> 7;
    }
    else
    {
        UGA2 = 0;
    }
    *A2T = (int16_t)(A2 + UGA2 - (A2 >> 7));
    CHECK_TC(*A2T, 16);
}

static void UPB(unsigned RATE, int Un, int Bn, unsigned DQ, int *BnP)
{
    CHECK_TC(Un, 1);
    CHECK_TC(Bn, 16);
    CHECK_SM(DQ, 16);
    int UGBn;
    if (DQ & 0x7FFF)
    {
        UGBn = Un ? -128 : 128;
    }
    else
    {
        UGBn = 0;
    }
    int ULBn = (RATE == 5) ? Bn >> 9 : Bn >> 8;
    *BnP = (int16_t)(Bn + UGBn - ULBn);
    CHECK_TC(*BnP, 16);
}

static void XOR(unsigned DQn, unsigned DQ, int *Un)
{
    CHECK_FL(DQn, 11);
    CHECK_SM(DQ, 16);
    *Un = -((int)((DQn >> 10) ^ (DQ >> 15)));
    CHECK_TC(*Un, 1);
}

static void TONE(int A2P, unsigned *TDP)
{
    CHECK_TC(A2P, 16);
    *TDP = (A2P < (53760 - 65536)) ? 1 : 0; // 53760-65536 = -11776
    CHECK_UNSIGNED(*TDP, 1);
}

static void TRANS(unsigned TD, unsigned YL, unsigned DQ, unsigned *TR)
{
    CHECK_UNSIGNED(TD, 1);
    CHECK_UM(YL, 19);
    CHECK_SM(DQ, 16);
    unsigned DQMAG = DQ & 0x7FFF;
    unsigned YLINT = YL >> 15;
    unsigned YLFRAC = (YL >> 10) & 31;
    unsigned THR1 = (32 + YLFRAC) << YLINT;
    unsigned THR2 = (YLINT > 9) ? (31 << 10) : THR1;
    unsigned DQTHR = (THR2 + (THR2 >> 1)) >> 1;
    *TR = (DQMAG > DQTHR) && TD;
    CHECK_UNSIGNED(*TR, 1);
}

static void COMPRESS(int SR, unsigned LAW, unsigned *SP)
{
    CHECK_TC(SR, 16);
    CHECK_UNSIGNED(LAW, 1);
    int x = SR;

// 如果需要重现G191的bug
#ifdef IMPLEMENT_G191_BUGS
    if (x == -0x8000)
    {
        x = -1;
    }
    else if (!LAW && x < 0)
    {
        x--;
    }
#endif

    // 钳位到14位
    if (x >= (1 << 13))
    {
        x = (1 << 13) - 1;
    }
    else if (x < -(1 << 13))
    {
        x = -(1 << 13);
    }

    if (LAW)
    {
        *SP = G711_ALawEncode(x << 2);
    }
    else
    {
        *SP = G711_ULawEncode(x << 2);
    }
    CHECK_UNSIGNED(*SP, 8);
}

static void SYNC(unsigned RATE, unsigned I, unsigned SP, int DLNX, int DSX, unsigned LAW, unsigned *SD)
{
    CHECK_UNSIGNED(SP, 8);
    CHECK_TC(DLNX, 12);
    CHECK_TC(DSX, 1);
    CHECK_UNSIGNED(LAW, 1);

    unsigned ID;
    unsigned IM;
    QUAN(RATE, DLNX, DSX, &ID);
    unsigned signMask = 1 << (RATE - 1);
    ID = ID ^ signMask;
    IM = I ^ signMask;

    unsigned s;
    if (LAW)
    {
        s = SP ^ 0x55;
        if (!(s & 0x80))
        {
            s = s ^ 0x7f;
        }
    }
    else
    {
        s = SP;
        if (s & 0x80)
        {
            s = s ^ 0x7f;
        }
    }

    if (ID < IM)
    {
        if (s < 0xff)
        {
            ++s;
        }
    }
    else if (ID > IM)
    {
        if (s > 0x00)
        {
            --s;
            if (s == 0x7f && !LAW)
            {
                --s;
            }
        }
    }

    if (LAW)
    {
        if (!(s & 0x80))
        {
            s = s ^ 0x7f;
        }
        s = s ^ 0x55;
    }
    else
    {
        if (s & 0x80)
        {
            s = s ^ 0x7f;
        }
    }
    *SD = s;
    CHECK_UNSIGNED(*SD, 8);
}

static void LIMO(int SR, int *SO)
{
    CHECK_TC(SR, 16);
    if (SR >= (1 << 13))
    {
        *SO = (1 << 13) - 1;
    }
    else if (SR < -(1 << 13))
    {
        *SO = -(1 << 13);
    }
    else
    {
        *SO = SR;
    }
    CHECK_TC(*SO, 14);
}

// ===================== G726 功能块 =====================

static void G726_InputPCMFormatConversionAndDifferenceSignalComputation(G726_Context *ctx, unsigned S, int SE, int *D)
{
    int SL;
    EXPAND(S, ctx->LAW, &SL);
    SUBTA(SL, SE, D);
}

static void G726_AdaptiveQuantizer(G726_Context *ctx, int D, unsigned Y, unsigned *I)
{
    unsigned DL;
    int DS;
    LOG(D, &DL, &DS);
    int DLN;
    SUBTB(DL, Y, &DLN);
    QUAN(ctx->RATE, DLN, DS, I);
}

static void G726_InverseAdaptiveQuantizer(G726_Context *ctx, unsigned I, unsigned Y, unsigned *DQ)
{
    int DQLN;
    int DQS;
    RECONST(ctx->RATE, I, &DQLN, &DQS);
    int DQL;
    ADDA(DQLN, Y, &DQL);
    ANTILOG(DQL, DQS, DQ);
}

static void G726_QuantizerScaleFactorAdaptation1(G726_Context *ctx, unsigned AL, unsigned *Y)
{
    MIX(AL, ctx->YU, ctx->YL, Y);
}

static void G726_QuantizerScaleFactorAdaptation2(G726_Context *ctx, unsigned I, unsigned Y)
{
    int WI;
    FUNCTW(ctx->RATE, I, &WI);
    unsigned YUT;
    FILTD(WI, Y, &YUT);
    unsigned YUP;
    LIMB(YUT, &YUP);
    unsigned YLP;
    FILTE(YUP, ctx->YL, &YLP);
    ctx->YU = YUP;
    ctx->YL = YLP;
}

static void G726_AdaptationSpeedControl1(G726_Context *ctx, unsigned *AL)
{
    LIMA(ctx->AP, AL);
}

static void G726_AdaptationSpeedControl2(G726_Context *ctx, unsigned I, unsigned Y, unsigned TDP, unsigned TR)
{
    unsigned FI;
    FUNCTF(ctx->RATE, I, &FI);
    unsigned DMSP;
    FILTA(FI, ctx->DMS, &DMSP);
    ctx->DMS = DMSP;
    unsigned DMLP;
    FILTB(FI, ctx->DML, &DMLP);
    ctx->DML = DMLP;
    unsigned AX;
    SUBTC(ctx->DMS, ctx->DML, TDP, Y, &AX);
    unsigned APP;
    FILTC(AX, ctx->AP, &APP);
    unsigned APR;
    TRIGA(TR, APP, &APR);
    ctx->AP = APR;
}

static void G726_AdaptativePredictorAndReconstructedSignalCalculator1(G726_Context *ctx, int *SE, int *SEZ)
{
    int WBn[6];
    for (int i = 0; i < 6; i++)
    {
        FMULT(ctx->Bn[i], ctx->DQn[i], &WBn[i]);
    }
    int WAn[2];
    FMULT(ctx->A1, ctx->SR1, &WAn[0]);
    FMULT(ctx->A2, ctx->SR2, &WAn[1]);
    ACCUM(WAn, WBn, SE, SEZ);
}

static void G726_AdaptativePredictorAndReconstructedSignalCalculator2(G726_Context *ctx, unsigned DQ, unsigned TR, int SE, int SEZ, int *SR, int *A2P)
{
    int PK0;
    unsigned SIGPK;
    ADDC(DQ, SEZ, &PK0, &SIGPK);
    ADDB(DQ, SE, SR);
    ctx->SR2 = ctx->SR1;
    FLOATB(*SR, &ctx->SR1);
    unsigned DQ0;
    FLOATA(DQ, &DQ0);
    for (int i = 0; i < 6; i++)
    {
        int Un;
        XOR(ctx->DQn[i], DQ, &Un);
        int BnP;
        UPB(ctx->RATE, Un, ctx->Bn[i], DQ, &BnP);
        int BnR;
        TRIGB(TR, BnP, &BnR);
        ctx->Bn[i] = BnR;
    }
    int A2T;
    UPA2(PK0, ctx->PK1, ctx->PK2, ctx->A1, ctx->A2, SIGPK, &A2T);
    LIMC(A2T, A2P);
    int A2R;
    TRIGB(TR, *A2P, &A2R);
    ctx->A2 = A2R;
    int A1T;
    UPA1(PK0, ctx->PK1, ctx->A1, SIGPK, &A1T);
    int A1P;
    LIMD(A1T, *A2P, &A1P);
    int A1R;
    TRIGB(TR, A1P, &A1R);
    ctx->A1 = A1R;
    ctx->PK2 = ctx->PK1;
    ctx->PK1 = PK0;
    for (int i = 5; i > 0; i--)
    {
        ctx->DQn[i] = ctx->DQn[i - 1];
    }
    ctx->DQn[0] = DQ0;
}

static void G726_ToneAndTransitionDetector1(G726_Context *ctx, unsigned DQ, unsigned *TR)
{
    TRANS(ctx->TD, ctx->YL, DQ, TR);
}

static void G726_ToneAndTransitionDetector2(G726_Context *ctx, int A2P, unsigned TR, unsigned *TDP)
{
    TONE(A2P, TDP);
    int TDR;
    TRIGB(TR, (int)(*TDP), &TDR);
    ctx->TD = (unsigned)TDR;
}

static void G726_OutputPCMFormatConversionAndSynchronousCodingAdjustment(G726_Context *ctx, int SR, int SE, unsigned Y, unsigned I, unsigned *SD)
{
    unsigned SP;
    COMPRESS(SR, ctx->LAW, &SP);
    int SLX;
    EXPAND(SP, ctx->LAW, &SLX);
    int DX;
    SUBTA(SLX, SE, &DX);
    unsigned DLX;
    int DSX;
    LOG(DX, &DLX, &DSX);
    int DLNX;
    SUBTB(DLX, Y, &DLNX);
    SYNC(ctx->RATE, I, SP, DLNX, DSX, ctx->LAW, SD);
}

static void G726_DifferenceSignalComputation(G726_Context *ctx, int SL, int SE, int *D)
{
    SUBTA(SL, SE, D);
}

static void G726_OutputLimiting(G726_Context *ctx, int SR, int *SO)
{
    LIMO(SR, SO);
}

// ===================== G726 主接口 =====================

void G726_Reset(G726_Context *ctx)
{
    for (int i = 0; i < 6; i++)
    {
        ctx->Bn[i] = 0;
        ctx->DQn[i] = 32;
    }
    ctx->A1 = 0;
    ctx->A2 = 0;
    ctx->AP = 0;
    ctx->DML = 0;
    ctx->DMS = 0;
    ctx->PK1 = 0;
    ctx->PK2 = 0;
    ctx->SR1 = 32;
    ctx->SR2 = 32;
    ctx->TD = 0;
    ctx->YL = 34816;
    ctx->YU = 544;
}

void G726_SetLaw(G726_Context *ctx, G726_Law law)
{
    ctx->LAW = law;
}

void G726_SetRate(G726_Context *ctx, G726_Rate rate)
{
    ctx->RATE = rate;
}

static unsigned G726_EncodeDecode(G726_Context *ctx, unsigned input, bool encode)
{
    unsigned AL;
    G726_AdaptationSpeedControl1(ctx, &AL);
    unsigned Y;
    G726_QuantizerScaleFactorAdaptation1(ctx, AL, &Y);
    int SE, SEZ;
    G726_AdaptativePredictorAndReconstructedSignalCalculator1(ctx, &SE, &SEZ);
    unsigned I;
    if (encode)
    {
        if (ctx->LAW == G726_PCM16)
        {
            int SL = (int16_t)input >> 2;
            int D;
            G726_DifferenceSignalComputation(ctx, SL, SE, &D);
            G726_AdaptiveQuantizer(ctx, D, Y, &I);
        }
        else
        {
            int D;
            G726_InputPCMFormatConversionAndDifferenceSignalComputation(ctx, input, SE, &D);
            G726_AdaptiveQuantizer(ctx, D, Y, &I);
        }
    }
    else
    {
        I = input;
    }
    unsigned DQ;
    G726_InverseAdaptiveQuantizer(ctx, I, Y, &DQ);
    unsigned TR;
    G726_ToneAndTransitionDetector1(ctx, DQ, &TR);
    int SR;
    int A2P;
    G726_AdaptativePredictorAndReconstructedSignalCalculator2(ctx, DQ, TR, SE, SEZ, &SR, &A2P);
    unsigned TDP;
    G726_ToneAndTransitionDetector2(ctx, A2P, TR, &TDP);
    G726_AdaptationSpeedControl2(ctx, I, Y, TDP, TR);
    G726_QuantizerScaleFactorAdaptation2(ctx, I, Y);
    if (encode)
    {
        return I;
    }
    else
    {
        if (ctx->LAW == G726_PCM16)
        {
            int SO;
            G726_OutputLimiting(ctx, SR, &SO);
            return (unsigned)(SO << 2);
        }
        else
        {
            unsigned SD;
            G726_OutputPCMFormatConversionAndSynchronousCodingAdjustment(ctx, SR, SE, Y, I, &SD);
            return SD;
        }
    }
}

unsigned G726_Encode(G726_Context *ctx, unsigned pcm)
{
    return G726_EncodeDecode(ctx, pcm, true);
}

unsigned G726_Decode(G726_Context *ctx, unsigned adpcm)
{
    adpcm &= (1 << ctx->RATE) - 1;
    return G726_EncodeDecode(ctx, adpcm, false);
}

unsigned G726_EncodeBuf(G726_Context *ctx, void *dst, int dstOffset, const void *src, size_t srcSize)
{
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *in8 = (const uint8_t *)src;
    const uint16_t *in16 = (const uint16_t *)src;
    out += dstOffset >> 3;
    unsigned bitOffset = dstOffset & 7;
    unsigned bits = ctx->RATE;
    unsigned mask = (1 << bits) - 1;
    unsigned outBits;
    if (ctx->LAW != G726_PCM16)
    {
        outBits = bits * srcSize;
    }
    else
    {
        outBits = bits * (srcSize >> 1);
        srcSize &= ~1;
    }
    const uint8_t *end = in8 + srcSize;
    while (in8 < end)
    {
        unsigned pcm;
        if (ctx->LAW == G726_PCM16)
        {
            pcm = *in16++;
            in8 += 2;
        }
        else
        {
            pcm = *in8++;
        }
        unsigned adpcm = G726_Encode(ctx, pcm);
        adpcm <<= bitOffset;
        unsigned b = *out;
        b &= ~(mask << bitOffset);
        b |= adpcm;
        *out = (uint8_t)b;
        bitOffset += bits;
        if (bitOffset >= 8)
        {
            out++;
            bitOffset -= 8;
            if (bitOffset)
            {
                *out = (uint8_t)(adpcm >> (bits - bitOffset));
            }
        }
    }
    return outBits;
}

unsigned G726_DecodeBuf(G726_Context *ctx, void *dst, const void *src, int srcOffset, unsigned srcSize)
{
    uint8_t *out8 = (uint8_t *)dst;
    uint16_t *out16 = (uint16_t *)dst;
    const uint8_t *in = (const uint8_t *)src;
    in += srcOffset >> 3;
    unsigned bitOffset = srcOffset & 7;
    unsigned bits = ctx->RATE;
    unsigned totalBytes = 0;
    while (srcSize >= bits)
    {
        unsigned adpcm = *in;
        if (bitOffset + bits > 8)
        {
            adpcm |= in[1] << 8;
        }
        adpcm >>= bitOffset;
        adpcm &= (1 << bits) - 1;
        unsigned pcm = G726_Decode(ctx, adpcm);
        if (ctx->LAW == G726_PCM16)
        {
            *out16++ = (uint16_t)pcm;
            totalBytes += 2;
        }
        else
        {
            *out8++ = (uint8_t)pcm;
            totalBytes++;
        }
        bitOffset += bits;
        srcSize -= bits;
        if (bitOffset >= 8)
        {
            bitOffset -= 8;
            in++;
        }
    }
    return totalBytes;
}