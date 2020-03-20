/******************************************************************************/
#include "stdafx.h"
/******************************************************************************/

/******************************************************************************/
   bool EditMaterial::hasBumpMap()C {return   bump_map.is() /*|| bump_from_color && color_map.is()*/;}
   bool EditMaterial::hasNormalMap()C {return normal_map.is() || hasBumpMap();}
   bool EditMaterial::hasDetailMap()C {return detail_color.is() || detail_bump.is() || detail_normal.is() || detail_smooth.is();}
   bool EditMaterial::hasLightMap()C {return light_map.is();}
   bool EditMaterial::hasBase1Tex()C {return hasNormalMap();}
   bool EditMaterial::hasBase2Tex()C {return smooth_map.is() || reflect_map.is() || hasBumpMap() || glow_map.is();}
   uint EditMaterial::baseTex()C {return (color_map.is() ? BT_COLOR : 0)|(alpha_map.is() ? BT_ALPHA : 0)|(hasBumpMap() ? BT_BUMP : 0)|(hasNormalMap() ? BT_NORMAL : 0)|(smooth_map.is() ? BT_SMOOTH : 0)|(reflect_map.is() ? BT_REFLECT : 0)|(glow_map.is() ? BT_GLOW : 0);}
   bool EditMaterial::usesTexAlpha()C {return tech!=MTECH_DEFAULT && (color_map.is() || alpha_map.is());}
   bool EditMaterial::usesTexBump()C {return (bump     >EPS_MATERIAL_BUMP || 1) && hasBumpMap  ();}
   bool EditMaterial::usesTexNormal()C {return  normal   >EPS_COL                 && hasNormalMap();}
   bool EditMaterial::usesTexGlow()C {return  glow     >EPS_COL                 && glow_map.is ();}
   bool EditMaterial::usesTexDetail()C {return  det_power>EPS_COL                 && hasDetailMap();}
   bool EditMaterial::needTanBin()C
   {
      return usesTexBump  ()
          || usesTexNormal()
          || usesTexDetail();
   }
   bool EditMaterial::equal(C EditMaterial &src)C
   {
      return flip_normal_y_time==src.flip_normal_y_time && tex_quality_time==src.tex_quality_time
      && color_map_time==src.color_map_time && alpha_map_time==src.alpha_map_time && bump_map_time==src.bump_map_time && normal_map_time==src.normal_map_time && smooth_map_time==src.smooth_map_time && reflect_map_time==src.reflect_map_time && glow_map_time==src.glow_map_time
      && detail_map_time==src.detail_map_time && macro_map_time==src.macro_map_time && light_map_time==src.light_map_time
      && cull_time==src.cull_time && tech_time==src.tech_time && downsize_tex_mobile_time==src.downsize_tex_mobile_time
      && color_time==src.color_time && ambient_time==src.ambient_time && smooth_time==src.smooth_time && reflect_time==src.reflect_time && normal_time==src.normal_time && bump_time==src.bump_time
      && glow_time==src.glow_time && tex_scale_time==src.tex_scale_time && detail_time==src.detail_time;
   }
   bool EditMaterial::newer(C EditMaterial &src)C
   {
      return flip_normal_y_time>src.flip_normal_y_time || tex_quality_time>src.tex_quality_time
      || color_map_time>src.color_map_time || alpha_map_time>src.alpha_map_time || bump_map_time>src.bump_map_time || normal_map_time>src.normal_map_time || smooth_map_time>src.smooth_map_time || reflect_map_time>src.reflect_map_time || glow_map_time>src.glow_map_time
      || detail_map_time>src.detail_map_time || macro_map_time>src.macro_map_time || light_map_time>src.light_map_time
      || cull_time>src.cull_time || tech_time>src.tech_time || downsize_tex_mobile_time>src.downsize_tex_mobile_time
      || color_time>src.color_time || ambient_time>src.ambient_time || smooth_time>src.smooth_time || reflect_time>src.reflect_time || normal_time>src.normal_time || bump_time>src.bump_time
      || glow_time>src.glow_time || tex_scale_time>src.tex_scale_time || detail_time>src.detail_time;
   }
   void EditMaterial::reset() {T=EditMaterial();}
   void EditMaterial::resetAlpha()
   {
      switch(tech)
      {
         case MTECH_ALPHA_TEST:
         case MTECH_GRASS     :
         case MTECH_GRASS_3D  :
         case MTECH_LEAF_2D   :
         case MTECH_LEAF      :
            color_s.w=0.5f; break;

         default: color_s.w=1; break;
      }
      color_time.getUTC();
   }
   void EditMaterial::separateNormalMap(C TimeStamp &time)
   {
      if(!normal_map.is() && hasNormalMap()) // if normal map is not specified, but is created from some other map
      {
                                               normal_map="<bump>"; // set normal map from bump map
         if(!ForcesMono(bump_map))SetTransform(normal_map, "grey"); // force grey scale, in case 'bump_map' may be RGB
                                               normal_map_time=time;
      }
   }
   void EditMaterial::separateAlphaMap(C Project &proj, C TimeStamp &time)
   {
      if(!alpha_map.is() && color_map.is() && hasBase2Tex()) // if alpha map not specified, but may come from color map, and will go to Base2 texture, #MaterialTextureLayout
      {
         Image color; if(proj.loadImages(color, null, color_map, true))if(HasAlpha(color)) // if color has alpha
         {
                         alpha_map="<color>"; // set alpha map from color map
            SetTransform(alpha_map, "channel", "a");
                         alpha_map_time=time;
         }
      }
   }
   void EditMaterial::cleanupMaps()
   { // no need to adjust time because this is called after maps have been changed
      if( alpha_map=="<color>") alpha_map.clear();
      if(normal_map=="<bump>" )normal_map.clear();
   }
   void EditMaterial::expandMap(Str &map, C MemPtr<FileParams> &color, C MemPtr<FileParams> &smooth, C MemPtr<FileParams> &bump)
   {
      bool normal=(&map==&normal_map);
      Mems<FileParams> files=FileParams::Decode(map);
      REPA(files)
      {
         FileParams &file=files[i];
       C MemPtr<FileParams> *src;
         if(file.name=="<color>" )src=&color ;else
         if(file.name=="<smooth>")src=&smooth;else
         if(file.name=="<bump>"  )src=&bump  ;else
            continue;
         if(src->elms()<=0)file.name.clear();else // if source is empty
         if(src->elms()==1) // if source has only one file
         {
          C FileParams &first=(*src)[0];
            file.name=first.name; // replace name with original
            FREPA(first.params)file.params.NewAt(i)=first.params[i]; // insert original parameters at the start
                    if(normal){file.params.NewAt(first.params.elms()).set("bumpToNormal"); flip_normal_y=false;} // need to force conversion to normal map
         }else
         if(i==0) // if source has multiple files, then we can add only if we're processing the first file (so all transforms from source will not affect anything already loaded)
         {
            file.name.clear(); // clear file name, but leave params/transforms to operate globally
            FREPA(*src)files.NewAt(i)=(*src)[i]; // add all files from source at the start
            if(normal){files.NewAt(src->elms()).params.New().set("bumpToNormal"); flip_normal_y=false;} // need to force conversion to normal map
            // !! here can't access 'file' anymore because its memory address could be invalid !!
         }
      }
      map=FileParams::Encode(files);
   }
   void EditMaterial::expandMaps()
   {
      Mems<FileParams> color =FileParams::Decode( color_map);
      Mems<FileParams> smooth=FileParams::Decode(smooth_map);
      Mems<FileParams> bump  =FileParams::Decode(  bump_map);
      expandMap(  color_map, color, smooth, bump);
      expandMap(  alpha_map, color, smooth, bump);
      expandMap(   bump_map, color, smooth, bump);
      expandMap( normal_map, color, smooth, bump);
      expandMap( smooth_map, color, smooth, bump);
      expandMap(reflect_map, color, smooth, bump);
      expandMap(   glow_map, color, smooth, bump);
   }
   void EditMaterial::newData()
   {
      flip_normal_y_time++; tex_quality_time++;
      color_map_time++; alpha_map_time++; bump_map_time++; normal_map_time++; smooth_map_time++; reflect_map_time++; glow_map_time++;
      detail_map_time++; macro_map_time++; light_map_time++;
      cull_time++; tech_time++; downsize_tex_mobile_time++;
      color_time++; ambient_time++; smooth_time++; reflect_time++; normal_time++; bump_time++; glow_time++; tex_scale_time++; detail_time++;
   }
   void EditMaterial::create(C Material &src, C TimeStamp &time)
   {
      cull=src.cull; cull_time=time;
      tech=src.technique; tech_time=time;
      color_s=src.colorS(); color_time=time;
      ambient=src.ambient; ambient_time=time;
      smooth=src.smooth; smooth_time=time;
      reflect=src.reflect; reflect_time=time;
      glow=src.glow; glow_time=time;
      normal=src.normal; normal_time=time;
      bump=src.bump; bump_time=time;
      tex_scale=src.tex_scale; tex_scale_time=time;
      det_scale=src.det_scale; detail_time=time;
      det_power=src.det_power; detail_time=time;
      base_0_tex=src.    base_0.id();
      base_1_tex=src.    base_1.id();
      base_2_tex=src.    base_2.id();
      detail_tex=src.detail_map.id();
       macro_tex=src. macro_map.id();
       light_tex=src. light_map.id();
   }
   void EditMaterial::copyTo(Material &dest, C Project &proj)C
   {
      dest.cull=cull;
      dest.technique=tech;
      dest.colorS(color_s);
      dest.ambient=ambient;
      dest.smooth=smooth;
      dest.reflect=reflect;
      dest.glow=glow;
      dest.normal=normal;
      dest.bump=bump;
      dest.tex_scale=tex_scale;
      dest.det_scale=det_scale;
      dest.det_power=det_power;
      dest.base_0    =proj.texPath(base_0_tex);
      dest.base_1    =proj.texPath(base_1_tex);
      dest.base_2    =proj.texPath(base_2_tex);
      dest.detail_map=proj.texPath(detail_tex);
      dest. macro_map=proj.texPath( macro_tex);
      dest. light_map=proj.texPath( light_tex);
      dest.validate();
   }
   void EditMaterial::copyTo(Edit::Material &dest)C
   {
      dest.technique=tech;
      dest.downsize_tex_mobile=downsize_tex_mobile;
      dest.cull=cull;
      dest.flip_normal_y=flip_normal_y;
      dest.tex_quality=tex_quality;
      dest.color_s=color_s;
      dest.ambient=ambient;
      dest.smooth=smooth;
      dest.reflect=reflect;
      dest.glow=glow;
      dest.normal=normal;
      dest.bump=bump;
      dest.    color_map=FileParams::Decode(color_map);
      dest.    alpha_map=FileParams::Decode(alpha_map);
      dest.     bump_map=FileParams::Decode(bump_map);
      dest.   normal_map=FileParams::Decode(normal_map);
      dest.   smooth_map=FileParams::Decode(smooth_map);
      dest.  reflect_map=FileParams::Decode(reflect_map);
      dest.     glow_map=FileParams::Decode(glow_map);
      dest.detail_color =FileParams::Decode(detail_color);
      dest.detail_bump  =FileParams::Decode(detail_bump);
      dest.detail_normal=FileParams::Decode(detail_normal);
      dest.detail_smooth=FileParams::Decode(detail_smooth);
      dest. macro_map   =FileParams::Decode(macro_map);
      dest. light_map   =FileParams::Decode(light_map);
   }
   uint EditMaterial::sync(C Edit::Material &src)
   {
      TimeStamp time; time.getUTC();
      uint changed=0;

      changed|=CHANGED_PARAM*SyncByValue(               tech_time, time, tech               , src.technique          );
      changed|=CHANGED_PARAM*SyncByValue(               cull_time, time, cull               , src.cull               );
      changed|=              SyncByValue(      flip_normal_y_time, time, flip_normal_y      , src.flip_normal_y      )*(CHANGED_PARAM|CHANGED_BASE|CHANGED_FNY); // set CHANGED_BASE too because this should trigger reloading base textures
      changed|=              SyncByValue(        tex_quality_time, time, tex_quality        , src.tex_quality        )*(CHANGED_PARAM|CHANGED_BASE            ); // set CHANGED_BASE too because this should trigger reloading base textures
      changed|=CHANGED_PARAM*SyncByValue(downsize_tex_mobile_time, time, downsize_tex_mobile, src.downsize_tex_mobile);

      changed|=CHANGED_PARAM*SyncByValueEqual(  color_time, time, color_s, src.color_s);
      changed|=CHANGED_PARAM*SyncByValueEqual(ambient_time, time, ambient, src.ambient);
      changed|=CHANGED_PARAM*SyncByValueEqual( smooth_time, time,  smooth, src. smooth);
      changed|=CHANGED_PARAM*SyncByValueEqual(reflect_time, time, reflect, src.reflect);
      changed|=CHANGED_PARAM*SyncByValueEqual(   glow_time, time,    glow, src.   glow);
      changed|=CHANGED_PARAM*SyncByValueEqual( normal_time, time,  normal, src. normal);
      changed|=CHANGED_PARAM*SyncByValueEqual(   bump_time, time,    bump, src.   bump);

      changed|=CHANGED_BASE *SyncByValue(  color_map_time, time,   color_map  , FileParams::Encode(ConstCast(src.  color_map  )));
      changed|=CHANGED_BASE *SyncByValue(  alpha_map_time, time,   alpha_map  , FileParams::Encode(ConstCast(src.  alpha_map  )));
      changed|=CHANGED_BASE *SyncByValue(   bump_map_time, time,    bump_map  , FileParams::Encode(ConstCast(src.   bump_map  )));
      changed|=CHANGED_BASE *SyncByValue( normal_map_time, time,  normal_map  , FileParams::Encode(ConstCast(src. normal_map  )));
      changed|=CHANGED_BASE *SyncByValue( smooth_map_time, time,  smooth_map  , FileParams::Encode(ConstCast(src. smooth_map  )));
      changed|=CHANGED_BASE *SyncByValue(reflect_map_time, time, reflect_map  , FileParams::Encode(ConstCast(src.reflect_map  )));
      changed|=CHANGED_BASE *SyncByValue(   glow_map_time, time,    glow_map  , FileParams::Encode(ConstCast(src.   glow_map  )));
      changed|=CHANGED_DET  *SyncByValue( detail_map_time, time, detail_color , FileParams::Encode(ConstCast(src.detail_color )));
      changed|=CHANGED_DET  *SyncByValue( detail_map_time, time, detail_bump  , FileParams::Encode(ConstCast(src.detail_bump  )));
      changed|=CHANGED_DET  *SyncByValue( detail_map_time, time, detail_normal, FileParams::Encode(ConstCast(src.detail_normal)));
      changed|=CHANGED_DET  *SyncByValue( detail_map_time, time, detail_smooth, FileParams::Encode(ConstCast(src.detail_smooth)));
      changed|=CHANGED_MACRO*SyncByValue(  macro_map_time, time,  macro_map   , FileParams::Encode(ConstCast(src.  macro_map  )));
      changed|=CHANGED_LIGHT*SyncByValue(  light_map_time, time,  light_map   , FileParams::Encode(ConstCast(src.  light_map  )));

      return changed;
   }
   uint EditMaterial::sync(C EditMaterial &src)
   {
      uint changed=0;

      changed|=Sync(flip_normal_y_time, src.flip_normal_y_time, flip_normal_y, src.flip_normal_y)*(CHANGED_PARAM|CHANGED_BASE|CHANGED_FNY); // set CHANGED_BASE too because this should trigger reloading base textures

      changed|=Sync(  color_map_time, src.  color_map_time,   color_map, src.  color_map)*CHANGED_BASE;
      changed|=Sync(  alpha_map_time, src.  alpha_map_time,   alpha_map, src.  alpha_map)*CHANGED_BASE;
      changed|=Sync(   bump_map_time, src.   bump_map_time,    bump_map, src.   bump_map)*CHANGED_BASE;
      changed|=Sync( normal_map_time, src. normal_map_time,  normal_map, src. normal_map)*CHANGED_BASE;
      changed|=Sync( smooth_map_time, src. smooth_map_time,  smooth_map, src. smooth_map)*CHANGED_BASE;
      changed|=Sync(reflect_map_time, src.reflect_map_time, reflect_map, src.reflect_map)*CHANGED_BASE;
      changed|=Sync(   glow_map_time, src.   glow_map_time,    glow_map, src.   glow_map)*CHANGED_BASE;

      if(changed&CHANGED_BASE)
      {
         base_0_tex=src.base_0_tex;
         base_1_tex=src.base_1_tex;
         base_2_tex=src.base_2_tex;
      }
      if(Sync(detail_map_time, src.detail_map_time))
      {
         changed|=CHANGED_DET;
         detail_color =src.detail_color;
         detail_bump  =src.detail_bump;
         detail_normal=src.detail_normal;
         detail_smooth=src.detail_smooth;
         detail_tex   =src.detail_tex;
      }
      if(Sync(macro_map_time, src.macro_map_time))
      {
         changed|=CHANGED_MACRO;
         macro_map=src.macro_map;
         macro_tex=src.macro_tex;
      }
      if(Sync(light_map_time, src.light_map_time))
      {
         changed|=CHANGED_LIGHT;
         light_map=src.light_map;
         light_tex=src.light_tex;
      }
      changed|=Sync(               cull_time, src.               cull_time,                cull, src.               cull)* CHANGED_PARAM;
      changed|=Sync(               tech_time, src.               tech_time,                tech, src.               tech)* CHANGED_PARAM;
      changed|=Sync(        tex_quality_time, src.        tex_quality_time,         tex_quality, src.        tex_quality)*(CHANGED_PARAM|CHANGED_BASE);
      changed|=Sync(downsize_tex_mobile_time, src.downsize_tex_mobile_time, downsize_tex_mobile, src.downsize_tex_mobile)* CHANGED_PARAM;

      changed|=Sync(    color_time, src.    color_time, color_s  , src.color_s  )*CHANGED_PARAM;
      changed|=Sync(  ambient_time, src.  ambient_time, ambient  , src.ambient  )*CHANGED_PARAM;
      changed|=Sync(   smooth_time, src.   smooth_time, smooth   , src.smooth   )*CHANGED_PARAM;
      changed|=Sync(  reflect_time, src.  reflect_time, reflect  , src.reflect  )*CHANGED_PARAM;
      changed|=Sync(     glow_time, src.     glow_time, glow     , src.glow     )*CHANGED_PARAM;
      changed|=Sync(tex_scale_time, src.tex_scale_time, tex_scale, src.tex_scale)*CHANGED_PARAM;
      changed|=Sync(   normal_time, src.   normal_time, normal   , src.normal   )*CHANGED_PARAM;
      changed|=Sync(     bump_time, src.     bump_time, bump     , src.bump     )*CHANGED_PARAM;
      if(Sync(detail_time, src.detail_time))
      {
         changed|=CHANGED_PARAM;
         det_scale=src.det_scale;
         det_power=src.det_power;
      }
      return changed;
   }
   uint EditMaterial::undo(C EditMaterial &src)
   {
      uint changed=0;

      changed|=Undo(flip_normal_y_time, src.flip_normal_y_time, flip_normal_y, src.flip_normal_y)*(CHANGED_PARAM|CHANGED_BASE|CHANGED_FNY); // set CHANGED_BASE too because this should trigger reloading base textures

      changed|=Undo(  color_map_time, src.  color_map_time,   color_map, src.  color_map)*CHANGED_BASE;
      changed|=Undo(  alpha_map_time, src.  alpha_map_time,   alpha_map, src.  alpha_map)*CHANGED_BASE;
      changed|=Undo(   bump_map_time, src.   bump_map_time,    bump_map, src.   bump_map)*CHANGED_BASE;
      changed|=Undo( normal_map_time, src. normal_map_time,  normal_map, src. normal_map)*CHANGED_BASE;
      changed|=Undo( smooth_map_time, src. smooth_map_time,  smooth_map, src. smooth_map)*CHANGED_BASE;
      changed|=Undo(reflect_map_time, src.reflect_map_time, reflect_map, src.reflect_map)*CHANGED_BASE;
      changed|=Undo(   glow_map_time, src.   glow_map_time,    glow_map, src.   glow_map)*CHANGED_BASE;

      if(changed&CHANGED_BASE)
      {
         base_0_tex=src.base_0_tex;
         base_1_tex=src.base_1_tex;
         base_2_tex=src.base_2_tex;
      }
      if(Undo(detail_map_time, src.detail_map_time))
      {
         changed|=CHANGED_DET;
         detail_color =src.detail_color;
         detail_bump  =src.detail_bump;
         detail_normal=src.detail_normal;
         detail_smooth=src.detail_smooth;
         detail_tex   =src.detail_tex;
      }
      if(Undo(macro_map_time, src.macro_map_time))
      {
         changed|=CHANGED_MACRO;
         macro_map=src.macro_map;
         macro_tex=src.macro_tex;
      }
      if(Undo(light_map_time, src.light_map_time))
      {
         changed|=CHANGED_LIGHT;
         light_map=src.light_map;
         light_tex=src.light_tex;
      }
      changed|=Undo(               cull_time, src.               cull_time,                cull, src.               cull)* CHANGED_PARAM;
      changed|=Undo(               tech_time, src.               tech_time,                tech, src.               tech)* CHANGED_PARAM;
      changed|=Undo(        tex_quality_time, src.        tex_quality_time,         tex_quality, src.        tex_quality)*(CHANGED_PARAM|CHANGED_BASE);
      changed|=Undo(downsize_tex_mobile_time, src.downsize_tex_mobile_time, downsize_tex_mobile, src.downsize_tex_mobile)* CHANGED_PARAM;

      changed|=Undo(    color_time, src.    color_time, color_s  , src.color_s  )*CHANGED_PARAM;
      changed|=Undo(  ambient_time, src.  ambient_time, ambient  , src.ambient  )*CHANGED_PARAM;
      changed|=Undo(   smooth_time, src.   smooth_time, smooth   , src.smooth   )*CHANGED_PARAM;
      changed|=Undo(  reflect_time, src.  reflect_time, reflect  , src.reflect  )*CHANGED_PARAM;
      changed|=Undo(     glow_time, src.     glow_time, glow     , src.glow     )*CHANGED_PARAM;
      changed|=Undo(tex_scale_time, src.tex_scale_time, tex_scale, src.tex_scale)*CHANGED_PARAM;
      changed|=Undo(   normal_time, src.   normal_time, normal   , src.normal   )*CHANGED_PARAM;
      changed|=Undo(     bump_time, src.     bump_time, bump     , src.bump     )*CHANGED_PARAM;
      if(Undo(detail_time, src.detail_time))
      {
         changed|=CHANGED_PARAM;
         det_scale=src.det_scale;
         det_power=src.det_power;
      }
      return changed;
   }
   bool EditMaterial::save(File &f)C
   {
      f.cmpUIntV(12);
      f<<flip_normal_y<<cull<<tex_quality<<tech<<downsize_tex_mobile;
      f<<color_s<<ambient<<smooth<<reflect<<glow<<normal<<bump<<tex_scale<<det_scale<<det_power;
      f<<base_0_tex<<base_1_tex<<base_2_tex<<detail_tex<<macro_tex<<light_tex;

      f<<color_map<<alpha_map<<bump_map<<normal_map<<smooth_map<<reflect_map<<glow_map
       <<detail_color<<detail_bump<<detail_normal<<detail_smooth
       <<macro_map
       <<light_map;

      f<<flip_normal_y_time<<tex_quality_time;
      f<<color_map_time<<alpha_map_time<<bump_map_time<<normal_map_time<<smooth_map_time<<reflect_map_time<<glow_map_time;
      f<<detail_map_time<<macro_map_time<<light_map_time;
      f<<cull_time<<tech_time<<downsize_tex_mobile_time;
      f<<color_time<<ambient_time<<smooth_time<<reflect_time<<normal_time<<bump_time<<glow_time<<tex_scale_time<<detail_time;
      return f.ok();
   }
   bool EditMaterial::load(File &f)
   {
      flt sss; bool bump_from_color=false; byte mip_map_blur; UID old_reflection_tex; Str old_reflection_map; TimeStamp sss_time, mip_map_blur_time, bump_from_color_time, old_reflection_map_time;
      reset(); switch(f.decUIntV())
      {
         case 12:
         {
            f>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile;
            f>>color_s>>ambient>>smooth>>reflect>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power;
            f>>base_0_tex>>base_1_tex>>base_2_tex>>detail_tex>>macro_tex>>light_tex;

            f>>color_map>>alpha_map>>bump_map>>normal_map>>smooth_map>>reflect_map>>glow_map
             >>detail_color>>detail_bump>>detail_normal>>detail_smooth
             >>macro_map
             >>light_map;

            f>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>reflect_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time;
            f>>color_time>>ambient_time>>smooth_time>>reflect_time>>normal_time>>bump_time>>glow_time>>tex_scale_time>>detail_time;
         }break;

         case 11:
         {
            f>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile;
            f>>color_s>>ambient>>smooth>>reflect>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power;
            f>>base_0_tex>>base_1_tex>>base_2_tex>>detail_tex>>macro_tex>>light_tex;

            f>>color_map>>alpha_map>>bump_map>>normal_map>>smooth_map>>reflect_map>>glow_map
             >>detail_color>>detail_bump>>detail_normal
             >>macro_map
             >>light_map;

            f>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>reflect_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time;
            f>>color_time>>ambient_time>>smooth_time>>reflect_time>>normal_time>>bump_time>>glow_time>>tex_scale_time>>detail_time;
         }break;

         case 10:
         {
            f>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            f>>color_map>>alpha_map>>bump_map>>normal_map>>smooth_map>>glow_map
             >>detail_color>>detail_bump>>detail_normal
             >>macro_map
             >>old_reflection_map
             >>light_map;

            f>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 9:
         {
            f>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            GetStr2(f, color_map); GetStr2(f, alpha_map); GetStr2(f, bump_map); GetStr2(f, normal_map); GetStr2(f, smooth_map); GetStr2(f, glow_map);
            GetStr2(f, detail_color); GetStr2(f, detail_bump); GetStr2(f, detail_normal);
            GetStr2(f, macro_map);
            GetStr2(f, old_reflection_map);
            GetStr2(f, light_map);

            f>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 8:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            GetStr2(f, color_map); GetStr2(f, alpha_map); GetStr2(f, bump_map); GetStr2(f, normal_map); GetStr2(f, smooth_map); GetStr2(f, glow_map);
            GetStr2(f, detail_color); GetStr2(f, detail_bump); GetStr2(f, detail_normal);
            GetStr2(f, macro_map);
            GetStr2(f, old_reflection_map);
            GetStr2(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 7:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tex_quality>>tech>>downsize_tex_mobile>>mip_map_blur;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            GetStr2(f, color_map); GetStr2(f, alpha_map); GetStr2(f, bump_map); GetStr2(f, normal_map); GetStr2(f, smooth_map); GetStr2(f, glow_map);
            GetStr2(f, detail_color); GetStr2(f, detail_bump); GetStr2(f, detail_normal);
            GetStr2(f, macro_map);
            GetStr2(f, old_reflection_map);
            GetStr2(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time>>tex_quality_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time>>mip_map_blur_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 6:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tech>>downsize_tex_mobile>>mip_map_blur;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            GetStr2(f, color_map); GetStr2(f, alpha_map); GetStr2(f, bump_map); GetStr2(f, normal_map); GetStr2(f, smooth_map); GetStr2(f, glow_map);
            GetStr2(f, detail_color); GetStr2(f, detail_bump); GetStr2(f, detail_normal);
            GetStr2(f, macro_map);
            GetStr2(f, old_reflection_map);
            GetStr2(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time>>mip_map_blur_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 5:
         {
            byte max_tex_size; f>>bump_from_color>>flip_normal_y>>cull>>tech>>max_tex_size>>mip_map_blur; downsize_tex_mobile=(max_tex_size>=1 && max_tex_size<=10);
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;

            GetStr2(f, color_map); GetStr2(f, alpha_map); GetStr2(f, bump_map); GetStr2(f, normal_map); GetStr2(f, smooth_map); GetStr2(f, glow_map);
            GetStr2(f, detail_color); GetStr2(f, detail_bump); GetStr2(f, detail_normal);
            GetStr2(f, macro_map);
            GetStr2(f, old_reflection_map);
            GetStr2(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time>>mip_map_blur_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 4:
         {
            byte max_tex_size; f>>bump_from_color>>flip_normal_y>>cull>>tech>>max_tex_size>>mip_map_blur; downsize_tex_mobile=(max_tex_size>=1 && max_tex_size<=10);
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;
            GetStr(f, color_map); GetStr(f, alpha_map); GetStr(f, bump_map); GetStr(f, normal_map); GetStr(f, smooth_map); GetStr(f, glow_map);
            GetStr(f, detail_color); GetStr(f, detail_bump); GetStr(f, detail_normal);
            GetStr(f, macro_map);
            GetStr(f, old_reflection_map);
            GetStr(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>downsize_tex_mobile_time>>mip_map_blur_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 3:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tech>>mip_map_blur;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;
            GetStr(f, color_map); GetStr(f, alpha_map); GetStr(f, bump_map); GetStr(f, normal_map); GetStr(f, smooth_map); GetStr(f, glow_map);
            GetStr(f, detail_color); GetStr(f, detail_bump); GetStr(f, detail_normal);
            GetStr(f, macro_map);
            GetStr(f, old_reflection_map);
            GetStr(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time>>mip_map_blur_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 2:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tech;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex>>light_tex;
            GetStr(f, color_map); GetStr(f, alpha_map); GetStr(f, bump_map); GetStr(f, normal_map); GetStr(f, smooth_map); GetStr(f, glow_map);
            GetStr(f, detail_color); GetStr(f, detail_bump); GetStr(f, detail_normal);
            GetStr(f, macro_map);
            GetStr(f, old_reflection_map);
            GetStr(f, light_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time>>light_map_time;
            f>>cull_time>>tech_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 1:
         {
            f>>bump_from_color>>flip_normal_y>>cull>>tech;
            f>>color_s>>ambient>>smooth>>sss>>glow>>normal>>bump>>tex_scale>>det_scale>>det_power>>reflect;
            f>>base_0_tex>>base_1_tex>>detail_tex>>macro_tex>>old_reflection_tex;
            GetStr(f, color_map); GetStr(f, alpha_map); GetStr(f, bump_map); GetStr(f, normal_map); GetStr(f, smooth_map); GetStr(f, glow_map);
            GetStr(f, detail_color); GetStr(f, detail_bump); GetStr(f, detail_normal);
            GetStr(f, macro_map);
            GetStr(f, old_reflection_map);

            f>>bump_from_color_time>>flip_normal_y_time;
            f>>color_map_time>>alpha_map_time>>bump_map_time>>normal_map_time>>smooth_map_time>>glow_map_time;
            f>>detail_map_time>>macro_map_time>>old_reflection_map_time;
            f>>cull_time>>tech_time;
            f>>color_time>>ambient_time>>smooth_time>>sss_time>>normal_time>>glow_time>>tex_scale_time>>detail_time>>reflect_time; bump_time=normal_time; if(!old_reflection_map.is())reflect=MATERIAL_REFLECT;else reflect_map=smooth_map;
         }break;

         case 0: break; // empty, this requires 'reset' to be called before

         default: goto error;
      }
      if(f.ok())
      {
         if(bump_from_color && !bump_map.is() && color_map.is()){bump_map=BumpFromColTransform(color_map, 3); if(!bump_map_time.is())bump_map_time++;}
         return true;
      }
   error:
      reset(); return false;
   }
   bool EditMaterial::load(C Str &name)
   {
      File f; if(f.readTry(name))return load(f);
      reset(); return false;
   }
EditMaterial::EditMaterial() : tech(MTECH_DEFAULT), tex_quality(Edit::Material::MEDIUM), flip_normal_y(false), cull(true), downsize_tex_mobile(0), color_s(1, 1, 1, 1), ambient(0, 0, 0), smooth(0), reflect(MATERIAL_REFLECT), glow(0), normal(0), bump(0), tex_scale(1), det_scale(4), det_power(0.3f), base_0_tex(UIDZero), base_1_tex(UIDZero), base_2_tex(UIDZero), detail_tex(UIDZero), macro_tex(UIDZero), light_tex(UIDZero) {}

/******************************************************************************/
