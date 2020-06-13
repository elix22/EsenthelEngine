/******************************************************************************/
#include "!Header.h"
#include "Sky.h"
/******************************************************************************/
#define ANIM_NONE   0
#define ANIM_YES    1
#define ANIM_SMOOTH 2

#include "!Set Prec Struct.h"
BUFFER(Particle)
   Vec2 ParticleFrames=Vec2(1, 1);
BUFFER_END
#include "!Set Prec Default.h"
/******************************************************************************/
void Particle_VS(VtxInput vtx,
             out Vec4  outVtx :POSITION ,
             out VecH4 outCol :COLOR    ,
             out Vec2  outTex :TEXCOORD0
          #if SOFT
           , out Vec2  outZS  :TEXCOORD1
          #endif
          #if ANIM==ANIM_SMOOTH
           , out Vec   outAnim:TEXCOORD2
           #endif
                )
{
   outTex=vtx.tex();
   outCol=(PALETTE ? vtx.colorF() : vtx.colorFast()); // use linear color for palette

   Half  size  =vtx.size(),
         angle =vtx._tan.w;
   Vec   pos   =TransformPos(vtx.pos());
   VecH2 offset=VecH2(outTex)*VecH2(2, -2)+VecH2(-1, 1);
   VecH2 cos_sin; CosSin(cos_sin.x, cos_sin.y, angle); offset=Rotate(offset, cos_sin);

   if(MOTION_STRETCH)
      if(pos.z>0)
   {
      #define PARTICLE_PROJECT 100
      VecH  vel =TransformDir(vtx.tan()); if(vel.z<0)vel=-vel; // view space velocity, always make it along the camera direction, so we won't have a situation where the 'pos1' is behind the camera
      Vec   pos1=pos+vel/PARTICLE_PROJECT;
      VecH2 vel2=(pos1.xy/pos1.z - pos.xy/pos.z)*PARTICLE_PROJECT; // 2D velocity
      Half  len =Length2(vel2); if(len>0)
      {
         len=Sqrt(len);
       //Flt max_stretch=5; if(len>max_stretch){vel2*=max_stretch/len; len=max_stretch;} // NaN
         VecH2 x=vel2*(vel2.x/len),
               y=vel2*(vel2.y/len);
         offset=VecH2(offset.x*(x.x+1) + offset.y*y.x, offset.x*x.y + offset.y*(y.y+1));
         if(MOTION_AFFECTS_ALPHA)
         {
            if(PALETTE)outCol  /=1+len; // in RM_PALETTE each component
            else       outCol.a/=1+len; // in RM_BLEND   only alpha
         }
      }
   }
   pos.xy+=offset*size;

   // sky
   Flt d=Length(pos); Half opacity=Sat(d*SkyFracMulAdd.x + SkyFracMulAdd.y);
   if(PALETTE)outCol  *=opacity; // in RM_PALETTE each component
   else       outCol.a*=opacity; // in RM_BLEND   only alpha

   #if SOFT
   {
      outZS.x=pos.z;
      outZS.y=size;

      if(pos.z > -size)
      {
         Flt wanted_z=Max(Viewport.from+EPS, pos.z-size);
         if(pos.z>=wanted_z)pos*=wanted_z/pos.z;else
         {
            pos.xy*=(wanted_z+size)/(pos.z+size); // this is optional (scales UV tex when behind camera)
            pos.z  = wanted_z;
         }
      }
   }
   #endif
   if(ANIM!=ANIM_NONE)
   {
      Flt frame=vtx.tex1().x;
   #if 0 // integer version
      UInt frames=UInt(Round(ParticleFrames.x*ParticleFrames.y));
      UInt f     =UInt(Trunc(frame))%frames;
      #if ANIM==ANIM_SMOOTH // frame blending
      {
         UInt f1=(f+1)%frames;
         outAnim.xy =outTex;
         outAnim.z  =Frac(frame);
         outAnim.x +=f1%UInt(Round(ParticleFrames.x));
         outAnim.y +=f1/UInt(Round(ParticleFrames.x));
         outAnim.xy/=              ParticleFrames    ;
      }
      #endif
      outTex.x+=f%UInt(Round(ParticleFrames.x));
      outTex.y+=f/UInt(Round(ParticleFrames.x));
      outTex  /=             ParticleFrames    ;
   #else // float version
      Flt frames=ParticleFrames.x*ParticleFrames.y; frame=Frac(frame/frames)*frames; // frame=[0..frames)
      Flt f; frame=modf(frame, f);
      #if ANIM==ANIM_SMOOTH // frame blending
      {
         Flt f1=f+1; if(f1+0.5>=frames)f1=0; // f1=(f+1)%frames;
                outAnim.xy =outTex;
                outAnim.z  =frame ; // frame step [0..1)
         Flt y; outAnim.x +=ParticleFrames.x*modf(f1/ParticleFrames.x, y); // outAnim.x+=f1%Round(ParticleFrames.x);
                outAnim.y +=y                                            ; // outAnim.y+=f1/Round(ParticleFrames.x);
                outAnim.xy/=ParticleFrames                               ;
      }
      #endif
      Flt y; outTex.x+=ParticleFrames.x*modf(f/ParticleFrames.x, y); // outTex.x+=f%Round(ParticleFrames.x);
             outTex.y+=y                                           ; // outTex.y+=f/Round(ParticleFrames.x);
             outTex  /=ParticleFrames                              ;
   #endif
   }
#if GL // needed for iOS PVRTC Pow2 #ParticleImgPart
   outTex.xy*=ImgSize.xy;
#endif
   outVtx=Project(pos);
}
/******************************************************************************/
VecH4 Particle_PS(PIXEL,
                  VecH4 inCol :COLOR    ,
                  Vec2  inTex :TEXCOORD0
               #if SOFT
                , Vec2  inZS  :TEXCOORD1
               #endif
               #if ANIM==ANIM_SMOOTH
                , Vec   inAnim:TEXCOORD2
               #endif
               #if !PALETTE
                , out Half outAlpha:TARGET2 // #RTOutput.Blend
               #endif
                 ):TARGET
{
   VecH4 tex=Tex(Img, inTex);
   #if ANIM==ANIM_SMOOTH
      tex=Lerp(tex, Tex(Img, inAnim.xy), inAnim.z);
   #endif
   #if SOFT
   {
      Flt z0    =inZS.x-tex.a*inZS.y,
          z1    =inZS.x+tex.a*inZS.y;
          tex.a*=Sat((TexDepthPoint(PixelToUV(pixel))-z0)/inZS.y); // fade out at occluder
          tex.a*=Sat(z1/(z1-z0+HALF_MIN));                         // smooth visibility fraction when particle near (or behind) camera, NaN
   }
   #endif
   if(PALETTE)inCol*=tex.a;
   else       inCol*=tex  ;
#if !PALETTE
   outAlpha=inCol.a;
#endif
   return inCol;
}
/******************************************************************************/
