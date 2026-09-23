#ifndef _G7XX_H
#define _G7XX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

// G711 函数接口
uint8_t G711_ALawEncode(int16_t pcm16);
int G711_ALawDecode(uint8_t alaw);
uint8_t G711_ULawEncode(int16_t pcm16);
int G711_ULawDecode(uint8_t ulaw);
uint8_t G711_ALawToULaw(uint8_t alaw);
uint8_t G711_ULawToALaw(uint8_t ulaw);
unsigned G711_ALawEncodeBuf(uint8_t *dst, int16_t *src, size_t srcSize);
unsigned G711_ALawDecodeBuf(int16_t *dst, const uint8_t *src, size_t srcSize);
unsigned G711_ULawEncodeBuf(uint8_t *dst, int16_t *src, size_t srcSize);
unsigned G711_ULawDecodeBuf(int16_t *dst, const uint8_t *src, size_t srcSize);
unsigned G711_ALawToULawBuf(uint8_t *dst, const uint8_t *src, size_t srcSize);
unsigned G711_ULawToALawBuf(uint8_t *dst, const uint8_t *src, size_t srcSize);

// G726 类型定义
typedef enum
{
    G726_uLaw = 0,
    G726_ALaw = 1,
    G726_PCM16 = 2
} G726_Law;

typedef enum
{
    G726_Rate16kBits = 2,
    G726_Rate24kBits = 3,
    G726_Rate32kBits = 4,
    G726_Rate40kBits = 5
} G726_Rate;

// G726 上下文结构
typedef struct
{
    G726_Law LAW;
    G726_Rate RATE;

    // 状态变量
    int A1;
    int A2;
    unsigned AP;
    int Bn[6];
    unsigned DML;
    unsigned DMS;
    unsigned DQn[6];
    int PK1;
    int PK2;
    unsigned SR1;
    unsigned SR2;
    unsigned TD;
    unsigned YL;
    unsigned YU;
} G726_Context;

// G726 函数接口
void G726_Reset(G726_Context *ctx);
void G726_SetLaw(G726_Context *ctx, G726_Law law);
void G726_SetRate(G726_Context *ctx, G726_Rate rate);
unsigned G726_Encode(G726_Context *ctx, unsigned pcm);
unsigned G726_Decode(G726_Context *ctx, unsigned adpcm);
unsigned G726_EncodeBuf(G726_Context *ctx, void *dst, int dstOffset, const void *src, size_t srcSize);
unsigned G726_DecodeBuf(G726_Context *ctx, void *dst, const void *src, int srcOffset, unsigned srcSize);

#ifdef __cplusplus
}
#endif

#endif // G711_G726_H