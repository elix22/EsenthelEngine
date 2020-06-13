/******************************************************************************/
#include "stdafx.h"
namespace EE{
/******************************************************************************/
SkyClass Sky;
/******************************************************************************/
SkyClass::SkyClass()
{
   frac(0.8f); // !! if changing default value, then also change in 'Environment.Sky' !!
  _dns_exp=1;
  _hor_exp=3.5f; // !! if changing default value, then also change in 'Environment.Sky' !!
  _hor_col_l=Vec4(SRGBToLinear(Vec(0.32f, 0.46f, 0.58f))*1.05f, 1.0f); // !! if changing default value, then also change in 'Environment.Sky' !!
  _sky_col_l=Vec4(SRGBToLinear(Vec(0.16f, 0.36f, 0.54f))*1.05f, 1.0f); // !! if changing default value, then also change in 'Environment.Sky' !!
  _stars_m.identity();
  _box_blend=0.5f;
   atmosphericPrecision(!MOBILE);
}
SkyClass& SkyClass::del()
{
  _mshr         .del  ();
   REPAO(_image).clear();
  _stars        .clear();
   return T;
}
SkyClass& SkyClass::create()
{
   Sh.SkyHorCol  ->set(LINEAR_GAMMA ? atmosphericHorizonColorL() : atmosphericHorizonColorS());
   Sh.SkySkyCol  ->set(LINEAR_GAMMA ? atmosphericSkyColorL    () : atmosphericSkyColorS    ());
   Sh.SkyBoxBlend->set(_box_blend);
   Sh.SkyStarOrn ->set(_stars_m  );
   Flt temp=_hor_exp; _hor_exp=-1; atmosphericHorizonExponent(temp); // set -1 to force reset
       temp=_dns_exp; _dns_exp=-1; atmosphericDensityExponent(temp); // set -1 to force reset

   MeshBase mshb; mshb.createIco(Ball(1), 0, 3); // res 3 gives 'dist'=0.982246876
  _mshr.create(mshb.reverse());
   #define SKY_MESH_MIN_DIST 0.98f // it's good to make it a bit smaller than 'dist' to have some epsilon for precision issues, this is the closest point on the mesh to the Vec(0,0,0), it's not equal to radius=1, because the mesh is composed out of triangles, and the triangle surfaces are closer
#if DEBUG && 0 // calculate actual distance
   Flt dist=1; C Vec *pos=mshb.vtx.pos();
   REPA(mshb.tri ){C VecI  &t=mshb.tri .ind(i); MIN(dist, Dist(VecZero, Tri (pos[t.x], pos[t.y], pos[t.z])));}
   REPA(mshb.quad){C VecI4 &q=mshb.quad.ind(i); MIN(dist, Dist(VecZero, Quad(pos[q.x], pos[q.y], pos[q.z], pos[q.w])));}
#endif
   return T;
}
/******************************************************************************/
Bool SkyClass::wantDepth()C {return frac()<1;}
/******************************************************************************/
SkyClass& SkyClass::clear()
{
  _is=false;
   return T;
}
SkyClass& SkyClass::atmospheric()
{
   T._is      =true;
   T._image[0]=null;
   T._image[1]=null;
   return T;
}
SkyClass& SkyClass::skybox(C ImagePtr &image)
{
   T._is      =(image!=null);
   T._image[0]=image;
   T._image[1]=null ;
   return T;
}
SkyClass& SkyClass::skybox(C ImagePtr &a, C ImagePtr &b)
{
   T._is=(a || b);
   if(a && b && a!=b){T._image[0]=a   ; T._image[1]=b   ;}else
   if(a             ){T._image[0]=a   ; T._image[1]=null;}else
   if(     b        ){T._image[0]=b   ; T._image[1]=null;}else
                     {T._image[0]=null; T._image[1]=null;}
   return T;
}
/******************************************************************************/
Vec4 SkyClass::atmosphericHorizonColorS()C {return LinearToSRGB(atmosphericHorizonColorL());}
Vec4 SkyClass::atmosphericSkyColorS    ()C {return LinearToSRGB(atmosphericSkyColorL    ());}

SkyClass& SkyClass::atmosphericHorizonColorS(C Vec4 &color_s) {return atmosphericHorizonColorL(SRGBToLinear(color_s));}
SkyClass& SkyClass::atmosphericSkyColorS    (C Vec4 &color_s) {return atmosphericSkyColorL    (SRGBToLinear(color_s));}

SkyClass& SkyClass::frac                       (  Flt       frac     ) {SAT(frac  );                                                                         T._frac         =frac               ; return T;}
SkyClass& SkyClass::atmosphericHorizonExponent (  Flt       exp      ) {MAX(exp, 0);              if(exp        !=T._hor_exp                               ){T._hor_exp      =exp                ; Sh.SkyHorExp  ->set(Max(T._hor_exp, EPS_GPU)                                              );} return T;} // avoid zero in case "Pow(1-Sat(inTex.y), SkyHorExp)" in shader would cause NaN or slow-downs
SkyClass& SkyClass::atmosphericHorizonColorL   (C Vec4     &color_l  ) {Flt alpha=Sat(color_l.w); if(color_l.xyz!=T._hor_col_l.xyz || alpha!=T._hor_col_l.w){T._hor_col_l.set(color_l.xyz, alpha); Sh.SkyHorCol  ->set(LINEAR_GAMMA ? atmosphericHorizonColorL() : atmosphericHorizonColorS());} return T;} // alpha must be saturated
SkyClass& SkyClass::atmosphericSkyColorL       (C Vec4     &color_l  ) {Flt alpha=Sat(color_l.w); if(color_l.xyz!=T._sky_col_l.xyz || alpha!=T._sky_col_l.w){T._sky_col_l.set(color_l.xyz, alpha); Sh.SkySkyCol  ->set(LINEAR_GAMMA ? atmosphericSkyColorL    () : atmosphericSkyColorS    ());} return T;} // alpha must be saturated
SkyClass& SkyClass::skyboxBlend                (  Flt       blend    ) {SAT(blend );              if(blend      !=T._box_blend                             ){T._box_blend    =blend              ; Sh.SkyBoxBlend->set(T._box_blend                                                          );} return T;}
SkyClass& SkyClass::atmosphericStars           (C ImagePtr &cube     ) {                                                                                     T._stars        =cube               ; return T;}
SkyClass& SkyClass::atmosphericStarsOrientation(C Matrix3  &orn      ) {                                                                                    {T._stars_m      =orn                ; Sh.SkyStarOrn ->set(T._stars_m                                                            );} return T;}
SkyClass& SkyClass::atmosphericPrecision       (  Bool      per_pixel) {                                                                                     T._precision    =per_pixel          ; return T;}
SkyClass& SkyClass::atmosphericDensityExponent (  Flt       exp      )
{
   SAT(exp); if(exp!=T._dns_exp)
   {
      T._dns_exp=exp;
      /* shader uses the formula based on "Flt AccumulatedDensity(Flt density, Flt range) {return 1-Pow(1-density, range);}"
         "1-Pow(SkyDnsExp, alpha)" but that gives the range 0..(1-SkyDnsExp), however we need it normalized, so:
         (1-Pow(SkyDnsExp, alpha)) / (1-SkyDnsExp) gives the range 0..1
           -Pow(SkyDnsExp, alpha) / (1-SkyDnsExp) + 1/(1-SkyDnsExp)
            Pow(SkyDnsExp, alpha) * -(1/(1-SkyDnsExp)) + 1/(1-SkyDnsExp)
            Pow(SkyDnsExp, alpha) *   mul              + add
      */
      Flt v=1-exp; if(v)v=1/v;
      Sh.SkyDnsExp   ->set(Max(T._dns_exp, EPS_GPU)); // avoid zero in case "Pow(0, alpha)" in shader would cause NaN or slow-downs
      Sh.SkyDnsMulAdd->set(Vec2(-v, v));
   }
   return T;
}
/******************************************************************************/
void SkyClass::setFracMulAdd()
{
   // !! in this method we use 'SkyFracMulAdd' as Object Opacity, and not Sky Opacity, so we use "1-sky_opacity" (reversed compared to drawing the sky) !!
   if(isActual())
   {
      Flt  from, to;
      Bool can_read_depth=Renderer.safeCanReadDepth(); // use 'safe' version because 'Renderer._ds' can be null here (for example when using RS_REFLECTION)
      from=(can_read_depth ? D.viewRange()*frac() : D.viewRange()); // we're using fraction only if we have depth access
      to  =D.viewRange();
      MIN(from, to-EPS_SKY_MIN_LERP_DIST); // make sure there's some distance between positions to avoid floating point issues, move 'from' instead of 'to' to make sure we always have zero opacity at the end

      //Flt obj_opacity=Length(O.pos)*SkyFracMulAdd.x+SkyFracMulAdd.y;
      //              1=       from  *SkyFracMulAdd.x+SkyFracMulAdd.y;
      //              0=       to    *SkyFracMulAdd.x+SkyFracMulAdd.y;
      Vec2 mul_add; mul_add.x=1/(from-to); mul_add.y=-to*mul_add.x;
      Sh.SkyFracMulAdd->set(mul_add);
   }else
   {
      Sh.SkyFracMulAdd->set(Vec2(0, 1));
   }
}
/******************************************************************************/
INLINE Shader* SkyTF(                  Int  textures  ,                           Bool dither, Bool cloud) {Shader* &s=Sh.SkyTF              [textures-1]                [dither][cloud]; if(SLOW_SHADER_LOAD && !s)s=Sh.getSkyTF(              textures  ,                 dither, cloud); return s;}
INLINE Shader* SkyT (Int multi_sample, Int  textures  ,                           Bool dither, Bool cloud) {Shader* &s=Sh.SkyT [multi_sample][textures-1]                [dither][cloud]; if(SLOW_SHADER_LOAD && !s)s=Sh.getSkyT (multi_sample, textures  ,                 dither, cloud); return s;}
INLINE Shader* SkyAF(                  Bool per_vertex,               Bool stars, Bool dither, Bool cloud) {Shader* &s=Sh.SkyAF              [per_vertex]         [stars][dither][cloud]; if(SLOW_SHADER_LOAD && !s)s=Sh.getSkyAF(              per_vertex,          stars, dither, cloud); return s;}
INLINE Shader* SkyA (Int multi_sample, Bool per_vertex, Bool density, Bool stars, Bool dither, Bool cloud) {Shader* &s=Sh.SkyA [multi_sample][per_vertex][density][stars][dither][cloud]; if(SLOW_SHADER_LOAD && !s)s=Sh.getSkyA (multi_sample, per_vertex, density, stars, dither, cloud); return s;}

void SkyClass::draw()
{
   if(isActual())
   {
      Shader *shader, *shader_multi=null;
      Bool    blend,
              density=(atmosphericDensityExponent()<1-EPS_GPU),
              dither =(D.dither() && !Renderer._col->highPrecision()),
              vertex = !_precision,
              stars  =((_stars   !=null) && (_hor_col_l.w<1-EPS_COL || _sky_col_l.w<1-EPS_COL)),
              cloud  =(Clouds.draw && Clouds.layered.merge_with_sky && Clouds.layered.layers() && Clouds.layered.layer[0].image && Clouds.layered.layer[0].color_l.w && (Clouds.layered.draw_in_mirror || !Renderer.mirror()));
      Int     tex    =((_image[0]!=null) + (_image[1]!=null)),
              multi  =(Renderer._col->multiSample() ? ((Renderer._cur_type==RT_DEFERRED) ? 1 : 2) : 0);
      Flt     from   =(Renderer.canReadDepth() ? D.viewRange()*frac() : D.viewRange()), // we're using fraction only if we have depth access
              to     =D.viewRange();
      blend=(from<to-EPS_SKY_MIN_LERP_DIST); // set blend mode if 'from' is far from 'to', and yes use < and not <= in case of precision issues for big values

      if(tex)
      {
         if(blend){shader=SkyT (0, tex, dither, cloud); if(multi)shader_multi=SkyT(multi, tex, dither, cloud);}
         else      shader=SkyTF(   tex, dither, cloud);
      }else
      {
         if(blend){shader=SkyA (0, vertex, density, stars, dither, cloud); if(multi)shader_multi=SkyA(multi, vertex, density, stars, dither, cloud);}
         else      shader=SkyAF(   vertex,          stars, dither, cloud);
      }

      // set shader parameters
      if(tex)
      {
         Sh.Cub[0]->set(_image[0]());
         Sh.Cub[1]->set(_image[1]());
      }else
      if(stars)
      {
         Sh.Cub[0]->set(_stars());
      }

      if(AstrosDraw && Sun.is())
      {
         Sh.SkySunHighlight->set(Vec2(Sun.highlight_front, Sun.highlight_back));
         Sh.SkySunPos      ->set(Sun.pos);
      }else
      {
         Sh.SkySunHighlight->set(Vec2Zero);
      }

      if(cloud)Clouds.layered.commit();

      Bool ds=true;
      Flt  sky_ball_mesh_size; if(blend)
      {
         //Flt sky_opacity=Length(O.pos)*SkyFracMulAdd.x+SkyFracMulAdd.y;
         //              0=       from  *SkyFracMulAdd.x+SkyFracMulAdd.y;
         //              1=       to    *SkyFracMulAdd.x+SkyFracMulAdd.y;
         Vec2 mul_add; mul_add.x=1/(to-from); mul_add.y=-from*mul_add.x;
         Sh.SkyFracMulAdd->set(mul_add);

         sky_ball_mesh_size=from;
       //sky_ball_mesh_size-=DepthError(D.viewFrom(), D.viewRange(), sky_ball_mesh_size, FovPerspective(D.viewFovMode()), Renderer._ds->hwTypeInfo().d); // draw smaller by DepthError to avoid depth precision issues
         if(sky_ball_mesh_size*SKY_MESH_MIN_DIST<=Frustum.view_quad_max_dist){sky_ball_mesh_size=to; ds=false;} // if the closest point on the mesh surface is in touch with the view quad, it means that the ball would not render fully, we have to render it with full size and with depth test disabled
      }else sky_ball_mesh_size=to;
   #if !REVERSE_DEPTH // for low precision depth we need to make sure that sky ball mesh is slightly smaller than view range, to avoid mesh clipping, this was observed on OpenGL with viewFrom=0.05, viewRange=1024, Cam.yaw=0, Cam.pitch=PI_2
      MIN(sky_ball_mesh_size, to*EPS_SKY_MIN_VIEW_RANGE); // alternatively we could try using D3DRS_CLIPPING, DepthClipEnable, GL_DEPTH_CLAMP
   #endif
      Renderer.set(Renderer._col, Renderer._ds, true, blend ? NEED_DEPTH_READ : NO_DEPTH_READ); // specify correct mode because without it the sky may cover everything completely
      D.alpha     (blend ? ALPHA_BLEND_DEC : ALPHA_NONE);
      D.depthWrite(false);
      if(FUNC_DEFAULT!=FUNC_LESS_EQUAL)D.depthFunc(FUNC_LESS_EQUAL); // to make sure we draw at the end of viewRange
    //D.cull      (true ); ignore changing culling, because we're inside the sky ball, so we will always see its faces, we could potentially set false (to ignore overhead on the GPU for cull testing if any) however we choose to just ignore it to reduce GPU state changes on the CPU which are probably more costly
      D.sampler3D (     ); // set in case of drawing clouds
      if(shader_multi)D.stencil(STENCIL_MSAA_TEST);
     _mshr.set();
      SetOneMatrix(MatrixM(sky_ball_mesh_size, CamMatrix.pos)); // normally we have to set matrixes after 'setEyeViewportCam', however since matrixes are always relative to the camera, and here we set exactly at the camera position, so the matrix will be the same for both eyes
      REPS(Renderer._eye, Renderer._eye_num)
      {
         Renderer.setEyeViewportCam();
         if(shader_multi){D.depth((multi==1) ? false : ds); D.stencilRef(STENCIL_REF_MSAA); shader_multi->begin(); _mshr.draw(); D.stencilRef(0);} // MS edges for deferred must not use depth testing
                          D.depth(                     ds);                                 shader      ->begin(); _mshr.draw();
      }
      D.sampler2D (    );
      D.depthWrite(true);
      if(FUNC_DEFAULT!=FUNC_LESS_EQUAL)D.depthFunc(FUNC_DEFAULT);
      D.stencil   (STENCIL_NONE);
   }
}
/******************************************************************************/
}
/******************************************************************************/
