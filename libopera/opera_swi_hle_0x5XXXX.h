#ifndef LIBOPERA_SWI_HLE_0X5XXXX_H_INCLUDED
#define LIBOPERA_SWI_HLE_0X5XXXX_H_INCLUDED

#include "inline.h"

#include "opera_fixedpoint_math.h"

#include <stdint.h>

/* void MulVec3Mat33_F16(vec3f16 dest, vec3f16 vec, mat33f16 mat); */
static
INLINE
void
opera_swi_hle_0x50000(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  vec3f16 *dest  = (vec3f16*)&ram_[r0_];
  vec3f16 *vec   = (vec3f16*)&ram_[r1_];
  mat33f16 *mat  = (mat33f16*)&ram_[r2_];

  MulVec3Mat33_F16(*dest,*vec,*mat);
}

/* void MulMat33Mat33_F16(mat33f16 dest, mat33f16 src1, mat33f16 src2); */
static
INLINE
void
opera_swi_hle_0x50001(uint8_t     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  mat33f16 *dest = (mat33f16*)&ram_[r0_];
  mat33f16 *src1 = (mat33f16*)&ram_[r1_];
  mat33f16 *src2 = (mat33f16*)&ram_[r2_];

  MulMat33Mat33_F16(*dest,*src1,*src2);
}

/* void MulManyVec3Mat33_F16(vec3f16 *dest, vec3f16 *src, mat33f16 mat, int32 count); */
static
INLINE
void
opera_swi_hle_0x50002(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  vec3f16 *dest = (vec3f16*)&ram_[r0_];
  vec3f16 *src  = (vec3f16*)&ram_[r1_];
  mat33f16 *mat = (mat33f16*)&ram_[r2_];
  int32_t count = (int32_t)r3_;

  MulManyVec3Mat33_F16(dest,src,*mat,count);
}

/* void MulObjectVec3Mat33_F16(void *objectlist[], ObjOffset1 *offsetstruct, int32 count); */
static
INLINE
void
opera_swi_hle_0x50003(char    *ram_,
                      uint8_t  r0_,
                      uint8_t  r1_,
                      uint8_t  r2_)
{

}

/* void MulObjectMat33_F16(void *objectlist[], ObjOffset2 *offsetstruct, mat33f16 mat, int32 count); */
static
INLINE
void
opera_swi_hle_0x50004(char     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{

}

/* void MulManyF16(frac16 *dest, frac16 *src1, frac16 *src2, int32 count); */
static
INLINE
void
opera_swi_hle_0x50005(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  frac16 *dest  = (frac16*)&ram_[r0_];
  frac16 *src1  = (frac16*)&ram_[r1_];
  frac16 *src2  = (frac16*)&ram_[r2_];
  int32_t count = (int32_t)r3_;

  MulManyF16(dest,src1,src2,count);
}

/* void MulScalerF16(frac16 *dest, frac16 *src, frac16 scaler, int32 count);  */
static
INLINE
void
opera_swi_hle_0x50006(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  frac16 *dest   = (frac16*)&ram_[r0_];
  frac16 *src    = (frac16*)&ram_[r1_];
  frac16 scaler  = r2_;
  int32_t count  = (int32_t)r3_;

  MulScalerF16(dest,src,scaler,count);
}

/* void MulVec4Mat44_F16(vec4f16 dest, vec4f16 vec, mat44f16 mat);  */
static
INLINE
void
opera_swi_hle_0x50007(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  vec4f16 *dest  = (vec4f16*)&ram_[r0_];
  vec4f16 *vec   = (vec4f16*)&ram_[r1_];
  mat44f16 *mat  = (mat44f16*)&ram_[r2_];

  MulVec4Mat44_F16(*dest,*vec,*mat);
}

/* void MulMat44Mat44_F16(mat44f16 dest, mat44f16 src1, mat44f16 src2);  */
static
INLINE
void
opera_swi_hle_0x50008(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  mat44f16 *dest = (mat44f16*)&ram_[r0_];
  mat44f16 *src1 = (mat44f16*)&ram_[r1_];
  mat44f16 *src2 = (mat44f16*)&ram_[r2_];

  MulMat44Mat44_F16(*dest,*src1,*src2);
}

/* void MulManyVec4Mat44_F16(vec4f16 *dest, vec4f16 *src, mat44f16 mat, int32 count);  */
static
INLINE
void
opera_swi_hle_0x50009(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  vec4f16 *dest   = (vec4f16*)&ram_[r0_];
  vec4f16 *src    = (vec4f16*)&ram_[r1_];
  mat44f16 *mat   = (mat44f16*)&ram_[r2_];
  int32_t count   = (int32_t)r3_;

  MulManyVec4Mat44_F16(dest,src,*mat,count);
}

/* void MulObjectVec4Mat44_F16(void *objectlist[], ObjeOffset1 *offsetstruct, int32 count);  */
static
INLINE
void
opera_swi_hle_0x5000A(char     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{

}

/* void MulObjectMat44_F16(void *objectlist[], ObjOffset2 *offsetstruct, mat44f16 mat, int32 count);  */
static
INLINE
void
opera_swi_hle_0x5000B(char     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{

}

/* frac16 Dot3_F16(vec3f16 v1, vec3f16 v2); */
static
INLINE
uint32_t
opera_swi_hle_0x5000C(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_)
{
  vec3f16 *v1  = (vec3f16*)&ram_[r0_];
  vec3f16 *v2  = (vec3f16*)&ram_[r1_];

  return (uint32_t)Dot3_F16(*v1,*v2);
}

/* frac16 Dot4_F16(vec4f16 v1, vec4f16 v2); */
static
INLINE
uint32_t
opera_swi_hle_0x5000D(char     *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_)
{
  vec4f16 *v1  = (vec4f16*)&ram_[r0_];
  vec4f16 *v2  = (vec4f16*)&ram_[r1_];

  return (uint32_t)Dot4_F16(*v1,*v2);
}

/* void Cross3_F16(vec3f16 dest, vec3f16 v1, vec3f16 v2); */
static
INLINE
void
opera_swi_hle_0x5000E(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_)
{
  vec3f16 *dest = (vec3f16*)&ram_[r0_];
  vec3f16 *v1   = (vec3f16*)&ram_[r1_];
  vec3f16 *v2   = (vec3f16*)&ram_[r2_];

  Cross3_F16(*dest,*v1,*v2);
}

/* frac16 AbsVec3_F16(vec3f16 vec); */
static
INLINE
uint32_t
opera_swi_hle_0x5000F(uint8_t *ram_,
                      uint32_t  r0_)
{
  vec3f16 *vec = (vec3f16*)&ram_[r0_];

  return (uint32_t)AbsVec3_F16(*vec);
}

/* frac16 AbsVec4_F16(vec3f16 vec); */
static
INLINE
uint32_t
opera_swi_hle_0x50010(uint8_t  *ram_,
                      uint32_t  r0_)
{
  vec4f16 *vec = (vec4f16*)&ram_[r0_];

  return (uint32_t)AbsVec4_F16(*vec);
}

/* void MulVec3Mat33DivZ_F16(vec3f16 dest, vec3f16 vec, mat33f16 mat, frac16 n); */
static
INLINE
void
opera_swi_hle_0x50011(uint8_t *ram_,
                      uint32_t  r0_,
                      uint32_t  r1_,
                      uint32_t  r2_,
                      uint32_t  r3_)
{
  vec3f16 *dest  = (vec3f16*)&ram_[r0_];
  vec3f16 *vec   = (vec3f16*)&ram_[r1_];
  mat33f16 *mat  = (mat33f16*)&ram_[r2_];
  frac16 n       = (frac16)r3_;

  MulVec3Mat33DivZ_F16(*dest,*vec,*mat,n);
}

/* void MulManyVec3Mat33DivZ_F16(mmv3m33d *s);  */
static
INLINE
void
opera_swi_hle_0x50012(uint8_t     *ram_,
                      uint32_t  r0_)
{
  vec3f16 *dest  = (vec3f16*)&ram_[*(uint32_t*)&ram_[r0_ + 0x00]];
  vec3f16 *src   = (vec3f16*)&ram_[*(uint32_t*)&ram_[r0_ + 0x04]];
  mat33f16 *mat  = (mat33f16*)&ram_[*(uint32_t*)&ram_[r0_ + 0x08]];
  frac16 n       = *(frac16*)&ram_[r0_ + 0x0C];
  uint32_t count = *(uint32_t*)&ram_[r0_ + 0x10];

  MulManyVec3Mat33DivZ_F16(dest,src,mat,n,count);
}

#endif
