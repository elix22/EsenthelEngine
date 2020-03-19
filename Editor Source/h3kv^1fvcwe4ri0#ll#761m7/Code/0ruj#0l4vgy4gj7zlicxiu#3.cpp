/******************************************************************************/
// FILE
/******************************************************************************/
bool SafeDel(C Str &name, ReadWriteSync &rws)
{
   if(name.is()){WriteLock wl(rws); return FDelFile(name);} return true;
}
bool SafeOverwrite(File &src, C Str &dest, ReadWriteSync &rws)
{
   return EE.SafeOverwrite(src, dest, null, null, S+'@'+TextHex(Random()), &rws);
}
bool SafeOverwriteChunk(File &src, C Str &dest, ReadWriteSync &rws)
{
   return (src.size()>4) ? SafeOverwrite(src, dest, rws) : SafeDel(dest, rws);
}
bool SafeCopy(C Str &src, C Str &dest)
{
   File f; return f.readStdTry(src) && EE.SafeOverwrite(f, dest, &NoTemp(FileInfoSystem(src).modify_time_utc), null, S+"@new"+Random());
}
/******************************************************************************/
void RemoveChunk(C Str &file, C Str &chunk, ReadWriteSync &rws)
{
   ReadLock rl(rws);
   File src; if(src.readTry(file))
   {
      File temp; ChunkWriter cw(temp.writeMem());
      for(ChunkReader cr(src); File *s=cr(); )
         if(!EqualPath(cr.name(), chunk))
            if(File *d=cw.beginChunk(cr.name(), cr.ver()))
               s.copy(*d);
      src.del();
      cw.endChunk();
      temp.pos(0); SafeOverwriteChunk(temp, file, rws);
   }
}
/******************************************************************************/
cchar8 *SizeSuffix[]={"", " KB", " MB", " GB", " TB"};
Str FileSize(long size, char dot=',')
{
   const int f=10;
   size*=f;
   int i=0; for(; i<Elms(SizeSuffix)-1 && size>=1000*f; i++, size>>=10); // check for "1000*f" instead of "1024*f", because we want to avoid displaying things like "1 001 MB"
   Str s=TextInt(size/f, -1, 3); if(size<100*f && i){s+=dot; s+=size%10;} s+=SizeSuffix[i];
   return s;
}
Str FileSizeKB(long size)
{
   const int f=10;
   size*=f;
   int i=1; size>>=10;
   Str s=TextInt(size/f, -1, 3); if(size<100*f && i){s+=','; s+=size%10;} s+=SizeSuffix[i];
   return s;
}
/******************************************************************************/
void SavedImage        (C Str &name) {if(ImagePtr       e=ImagePtr     ().find(name))if(!IsServer)e->load(name);} // on server the file may be compressed
void SavedImageAtlas   (C Str &name) {if(ImageAtlasPtr  e=ImageAtlasPtr().find(name))if(!IsServer)e->load(name);}
void SavedEditSkel     (C Str &name) {}
void SavedSkel         (C Str &name) {if(Skeleton      *e=Skeletons .find(name))if(!IsServer)e->load(name);}
void SavedAnim         (C Str &name) {if(Animation     *e=Animations.find(name))if(!IsServer)e->load(name);}
void SavedMesh         (C Str &name) {if(MeshPtr        e=MeshPtr ().find(name))if(!IsServer){CacheLock cl(Meshes); e->load(name);}}
void SavedEditMtrl     (C Str &name) {}
void SavedEditWaterMtrl(C Str &name) {}
void SavedEditPhysMtrl (C Str &name) {}
void SavedMtrl         (C Str &name) {if(MaterialPtr    e= MaterialPtr().find(name))if(!IsServer)e->load(name);}
void SavedWaterMtrl    (C Str &name) {if(WaterMtrlPtr   e=WaterMtrlPtr().find(name))if(!IsServer)e->load(name);}
void SavedPhysMtrl     (C Str &name) {if(PhysMtrl      *e=PhysMtrls     .find(name))if(!IsServer)e->load(name);}
void SavedEditPhys     (C Str &name) {}
void SavedPhys         (C Str &name) {if(PhysBodyPtr    e=PhysBodyPtr().find(name))if(!IsServer){CacheLock cl(PhysBodies); e->load(name);}}
void SavedEnum         (C Str &name) {if(Enum          *e=Enums        .find(name))if(!IsServer)e->load(name);}
void SavedEditEnum     (C Str &name) {}
void SavedEditObjPar   (C Str &name) {if(EditObjectPtr  e=EditObjectPtr().find(name))if(!IsServer)e->load(name);}
void SavedGameObjPar   (C Str &name) {if(ObjectPtr      e=    ObjectPtr().find(name))if(!IsServer){CacheLock cl(Objects); e->load(name);}}
void SavedGameWayp     (C Str &name) {if(Game.Waypoint *e=Game.Waypoints. find(name))if(!IsServer)e->load(name);}
void SavedFont         (C Str &name) {if(FontPtr        e=      FontPtr().find(name))if(!IsServer)e->load(name);}
void SavedTextStyle    (C Str &name) {if(TextStylePtr   e= TextStylePtr().find(name))if(!IsServer)e->load(name);}
void SavedPanelImage   (C Str &name) {if(PanelImagePtr  e=PanelImagePtr().find(name))if(!IsServer)e->load(name);}
void SavedPanel        (C Str &name) {if(PanelPtr       e=     PanelPtr().find(name))if(!IsServer)e->load(name);}
void SavedGuiSkin      (C Str &name) {if(GuiSkinPtr     e=   GuiSkinPtr().find(name))if(!IsServer)e->load(name);}
void SavedGui          (C Str &name) {}
void SavedEnv          (C Str &name) {if(EnvironmentPtr e=EnvironmentPtr().find(name))if(!IsServer)e->load(name);}

void Saved(C Image           &img , C Str &name) {if(ImagePtr e=ImagePtr().find(name))img.copy(*e);}
void Saved(C ImageAtlas      &img , C Str &name) {SavedImageAtlas(name);}
void Saved(C IconSettings    &icon, C Str &name) {}
void Saved(C EditSkeleton    &skel, C Str &name) {}
void Saved(C     Skeleton    &skel, C Str &name) {if(Skeleton  *e=Skeletons .find(name))*e=skel;}
void Saved(C Animation       &anim, C Str &name) {if(Animation *e=Animations.find(name))*e=anim;}
void Saved(C Mesh            &mesh, C Str &name) {if(MeshPtr    e=MeshPtr ().find(name)){CacheLock cl(Meshes); e->create(mesh).setShader();}}
void Saved(C EditMaterial    &mtrl, C Str &name) {}
void Saved(C EditWaterMtrl   &mtrl, C Str &name) {}
void Saved(C EditPhysMtrl    &mtrl, C Str &name) {}
void Saved(C Material        &mtrl, C Str &name) {if( MaterialPtr e= MaterialPtr().find(name))*e=mtrl;}
void Saved(C WaterMtrl       &mtrl, C Str &name) {if(WaterMtrlPtr e=WaterMtrlPtr().find(name))*e=mtrl;}
void Saved(C PhysMtrl        &mtrl, C Str &name) {SavedPhysMtrl(name);}
void Saved(C PhysBody        &phys, C Str &name) {if(PhysBodyPtr  e=PhysBodyPtr().find(name)){CacheLock cl(PhysBodies); *e=phys;}}
void Saved(C Enum            &enm , C Str &name) {if(Enum        *e=Enums        .find(name))*e=enm;}
void Saved(C EditEnums       &enms, C Str &name) {}
void Saved(C EditObject      &obj , C Str &name) {if(EditObjectPtr e=EditObjectPtr().find(name))*e=obj;}
void Saved(C     Object      &obj , C Str &name) {if(    ObjectPtr e=    ObjectPtr().find(name)){CacheLock cl(Objects); *e=obj;}}
void Saved(C EditWaypoint    &wp  , C Str &name) {}
void Saved(C Game.Waypoint   &wp  , C Str &name) {if(Game.Waypoint *e=Game.Waypoints.find(name))*e=wp;}
void Saved(C EditFont        &font, C Str &name) {}
void Saved(C Font            &font, C Str &name) {SavedFont(name);}
void Saved(C EditTextStyle   &ts  , C Str &name) {}
void Saved(C EditPanelImage  &pi  , C Str &name) {}
void Saved(C PanelImage      &pi  , C Str &name) {SavedPanelImage(name);}
void Saved(C TextStyle       &ts  , C Str &name) {if(TextStylePtr e=TextStylePtr().find(name))*e=ts;}
void Saved(C EditPanel       &panl, C Str &name) {}
void Saved(C Panel           &panl, C Str &name) {if(PanelPtr     e=PanelPtr().find(name))*e=panl;}
void Saved(C EditGuiSkin     &skin, C Str &name) {}
void Saved(C     GuiSkin     &skin, C Str &name) {if(GuiSkinPtr   e=GuiSkinPtr().find(name))*e=skin;}
void Saved(C GuiObjs         &gui , C Str &name) {}
void Saved(C Lake            &lake, C Str &name) {}
void Saved(C River           &rivr, C Str &name) {}
void Saved(C EditEnv         &env , C Str &name) {}
void Saved(C Environment     &env , C Str &name) {if(EnvironmentPtr e=EnvironmentPtr().find(name))*e=env;}
void Saved(C Game.WorldSettings &s, C Str &name) {}

bool Save (C Image           &img , C Str &name                        ) {File f; img .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name)){Saved(img , name); return true;} return false;}
void Save (C ImageAtlas      &img , C Str &name                        ) {File f; img .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(img , name);}
void Save (C IconSettings    &icon, C Str &name                        ) {File f; icon.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(icon, name);}
void Save (C EditSkeleton    &skel, C Str &name                        ) {File f; skel.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(skel, name);}
void Save (C     Skeleton    &skel, C Str &name                        ) {File f; skel.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(skel, name);}
void Save (C Animation       &anim, C Str &name                        ) {File f; anim.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(anim, name);}
void Save (C Mesh            &mesh, C Str &name, C Str &resource_path=S) {File f; mesh.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); CacheLock cl(Meshes); if(SafeOverwrite(f, name))Saved(mesh, name);}
void Save (C EditMaterial    &mtrl, C Str &name                        ) {File f; mtrl.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C EditWaterMtrl   &mtrl, C Str &name                        ) {File f; mtrl.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C EditPhysMtrl    &mtrl, C Str &name                        ) {File f; mtrl.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C Material        &mtrl, C Str &name, C Str &resource_path=S) {File f; mtrl.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C WaterMtrl       &mtrl, C Str &name, C Str &resource_path=S) {File f; mtrl.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C PhysMtrl        &mtrl, C Str &name                        ) {File f; mtrl.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(mtrl, name);}
void Save (C PhysBody        &phys, C Str &name, C Str &resource_path=S) {File f; phys.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); CacheLock cl(PhysBodies); if(SafeOverwrite(f, name))Saved(phys, name);}
void Save (C Enum            &enm , C Str &name                        ) {File f; enm .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(enm , name);}
void Save (C EditEnums       &enms, C Str &name                        ) {File f; enms.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(enms, name);}
void Save (C EditObject      &obj , C Str &name, C Str &resource_path=S) {File f; obj .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(obj , name);}
void Save (C     Object      &obj , C Str &name, C Str &resource_path=S) {File f; obj .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); CacheLock cl(Objects); if(SafeOverwrite(f, name))Saved(obj, name);}
void Save (C EditWaypoint    &wp  , C Str &name                        ) {File f; wp  .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(wp  , name);}
void Save (C Game.Waypoint   &wp  , C Str &name                        ) {File f; wp  .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(wp  , name);}
void Save (C EditFont        &font, C Str &name                        ) {File f; font.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(font, name);}
void Save (C Font            &font, C Str &name                        ) {File f; font.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(font, name);}
void Save (C EditTextStyle   &ts  , C Str &name                        ) {File f; ts  .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(ts  , name);}
void Save (C TextStyle       &ts  , C Str &name, C Str &resource_path=S) {File f; ts  .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(ts  , name);}
void Save (C EditPanelImage  &pi  , C Str &name                        ) {File f; pi  .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(pi  , name);}
void Save (C PanelImage      &pi  , C Str &name                        ) {File f; pi  .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(pi  , name);}
void Save (C EditPanel       &panl, C Str &name                        ) {File f; panl.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(panl, name);}
void Save (C Panel           &panl, C Str &name, C Str &resource_path=S) {File f; panl.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(panl, name);}
void Save (C EditGuiSkin     &skin, C Str &name                        ) {File f; skin.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(skin, name);}
void Save (C     GuiSkin     &skin, C Str &name, C Str &resource_path=S) {File f; skin.save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(skin, name);}
void Save (C GuiObjs         &gui , C Str &name, C Str &resource_path=S) {File f; gui .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(gui , name);}
void Save (C Lake            &lake, C Str &name                        ) {File f; lake.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name, WorldAreaSync))Saved(lake, name);}
void Save (C River           &rivr, C Str &name                        ) {File f; rivr.save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name, WorldAreaSync))Saved(rivr, name);}
void Save (C EditEnv         &env , C Str &name                        ) {File f; env .save(f.writeMem()                                                    ); f.pos(0); if(SafeOverwrite(f, name))Saved(env , name);}
void Save (C Environment     &env , C Str &name, C Str &resource_path=S) {File f; env .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(env , name);}
void Save (C Game.WorldSettings &s, C Str &name, C Str &resource_path=S) {File f; s   .save(f.writeMem(), resource_path.is() ? resource_path : GetPath(name)); f.pos(0); if(SafeOverwrite(f, name))Saved(s   , name);}

bool Load (Mesh       &mesh, C Str &name, C Str &resource_path=S) {File f; if(f.readTry(name))return mesh.load(f, resource_path.is() ? resource_path : GetPath(name)); mesh.del  (); return false;}
bool Load (Material   &mtrl, C Str &name, C Str &resource_path=S) {File f; if(f.readTry(name))return mtrl.load(f, resource_path.is() ? resource_path : GetPath(name)); mtrl.reset(); return false;}
bool Load (WaterMtrl  &mtrl, C Str &name, C Str &resource_path=S) {File f; if(f.readTry(name))return mtrl.load(f, resource_path.is() ? resource_path : GetPath(name)); mtrl.reset(); return false;}
bool Load (EditObject &obj , C Str &name, C Str &resource_path=S) {File f; if(f.readTry(name))return obj .load(f, resource_path.is() ? resource_path : GetPath(name)); obj .del  (); return false;}
// other assets either don't use sub-assets, or are stored in game path and don't require "edit->game" path change

bool SaveCode(C Str &code, C Str &name)
{
 //FileText f; f.writeMem(HasUnicode(code) ? UTF_16 : ANSI); // avoid UTF_8 because it's slower to write/read, and as there can be lot of codes, we don't want to sacrifice performance when opening big projects
   FileText f; f.writeMem(HasUnicode(code) ? UTF_8 : ANSI); // FIXME restore above UTF_16 once GitHub supports it, because now it can corrupt files
   f.putText(code);
   return EE.SafeOverwrite(f, name);
}
Edit.ERROR_TYPE LoadCode(Str &code, C Str &name)
{
   FileText f; if(f.read(name)){f.getAll(code); return f.ok() ? Edit.EE_ERR_NONE : Edit.EE_ERR_FILE_READ_ERROR;}
   code.clear(); return Edit.EE_ERR_FILE_NOT_FOUND;
}

void SavedBase(ELM_TYPE type, C Str &path) // called when saved the base version
{
   if(ElmEdit(type))SavedEdit(type, path);
   else             SavedGame(type, path);
}
void SavedCode(C Str &path) // called when saved code
{
   
}
void SavedEdit(ELM_TYPE type, C Str &path) // called when saved the edit version
{
   switch(type)
   {
      case ELM_SKEL      : SavedEditSkel     (path); break;
      case ELM_PHYS      : SavedEditPhys     (path); break;
      case ELM_ENUM      : SavedEditEnum     (path); break;
      case ELM_OBJ_CLASS : SavedEditObjPar   (path); break;
      case ELM_OBJ       : SavedEditObjPar   (path); break;
      case ELM_MESH      : SavedMesh         (path); break;
      case ELM_MTRL      : SavedEditMtrl     (path); break;
      case ELM_WATER_MTRL: SavedEditWaterMtrl(path); break;
      case ELM_PHYS_MTRL : SavedEditPhysMtrl (path); break;
   }
}
void SavedGame(ELM_TYPE type, C Str &path) // called when saved the game version
{
   switch(type)
   {
      case ELM_ENUM       : SavedEnum      (path); break;
      case ELM_OBJ_CLASS  : SavedGameObjPar(path); break;
      case ELM_OBJ        : SavedGameObjPar(path); break;
      case ELM_MESH       : SavedMesh      (path); break;
      case ELM_MTRL       : SavedMtrl      (path); break;
      case ELM_WATER_MTRL : SavedWaterMtrl (path); break;
      case ELM_PHYS_MTRL  : SavedPhysMtrl  (path); break;
      case ELM_SKEL       : SavedSkel      (path); break;
      case ELM_PHYS       : SavedPhys      (path); break;
      case ELM_ANIM       : SavedAnim      (path); break;
      case ELM_GUI_SKIN   : SavedGuiSkin   (path); break;
      case ELM_GUI        : SavedGui       (path); break;
      case ELM_FONT       : SavedFont      (path); break;
      case ELM_TEXT_STYLE : SavedTextStyle (path); break;
      case ELM_PANEL_IMAGE: SavedPanelImage(path); break;
      case ELM_PANEL      : SavedPanel     (path); break;
      case ELM_ENV        : SavedEnv       (path); break;
      case ELM_IMAGE      : SavedImage     (path); break;
      case ELM_IMAGE_ATLAS: SavedImageAtlas(path); break;
      case ELM_ICON       : SavedImage     (path); break;
   }
}
/******************************************************************************/
class FileSizeGetter
{
   class Elm
   {
      UID  id;
      uint file_size;
   }
   Memc<Elm> elms;

   // get
   bool created()C {           return path.is();}
   bool busy   ()  {cleanup(); return thread.active();}
   bool get    ()
   {
      cleanup();
      if(elms_thread.elms())
      {
         SyncLocker locker(lock);
         if(!elms.elms())Swap(elms_thread, elms);else
         {
            FREPA(elms_thread)elms.add(elms_thread[i]);
            elms_thread.clear();
         }
         return true;
      }
      return false;
   }

   // manage
   void clear() {elms.clear();}
   void stop () {thread.stop();}
   void del  ()
   {
      thread.del(); // del the thread first
      elms_thread.clear();
      elms       .clear();
      path       .clear();
   }
   void get(C Str &path)
   {
      del();
      if(path.is())
      {
         T.path=path;
         thread.create(Func, this);
      }
   }
  ~FileSizeGetter() {del();}

private:
   Str       path;
   Memc<Elm> elms_thread;
   SyncLock  lock;
   Thread    thread;

   void cleanup()
   {
      if(!thread.active())thread.del(); // delete to free resources
   }

   static bool Func(Thread &thread) {return ((FileSizeGetter*)thread.user).func();}
          bool func()
   {
      for(FileFind ff(path); !thread.wantStop() && ff(); )
      {
         if(ff.type==FSTD_FILE)
         {
            UID id; if(DecodeFileName(ff.name, id))
            {
               SyncLocker locker(lock);
               Elm &elm=elms_thread.New();
               elm.id=id;
               elm.file_size=ff.size;
            }
         }
      }
      return false;
   }
}
/******************************************************************************/
// SYNC / UNDO
/******************************************************************************/
void MAX1(TimeStamp &time, C TimeStamp &src_time) {if(src_time>time)time=src_time; time++;} // set as max from both and increase by one, "time=Max(time, src_time)+1"

bool Sync(TimeStamp &time, C TimeStamp &src_time) {if(src_time> time){time=src_time;        return true;} return false;}
bool Undo(TimeStamp &time, C TimeStamp &src_time) {if(src_time!=time){MAX1(time, src_time); return true;} return false;}

<TYPE> bool Sync(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(Sync(time, src_time)){data=src_data; return true;} return false;
}
<TYPE> bool UndoByTime(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(Undo(time, src_time)){data=src_data; return true;} return false;
}

<TYPE> bool SyncByValue(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(data!=src_data){data=src_data; time=src_time; return true;} return false;
}
<TYPE> bool SyncByValueEqual(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(!Equal(data, src_data)){data=src_data; time=src_time; return true;} return false;
}

<TYPE> bool UndoByValue(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(data!=src_data){data=src_data; MAX1(time, src_time); return true;} return false;
}
/*<TYPE> bool UndoByValueEqual(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data)
{
   if(!Equal(data, src_data)){data=src_data; MAX1(time, src_time); return true;} return false;
}*/

<TYPE> bool Undo(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data) // ByTimeAndValue, make this the default function because there can be a lot of changes in the same second on the local computer
{
   return UndoByTime (time, src_time, data, src_data) // first check by time because it's faster
       || UndoByValue(time, src_time, data, src_data);
}
/*<TYPE> bool UndoEqual(TimeStamp &time, C TimeStamp &src_time, TYPE &data, C TYPE &src_data) // ByTimeAndValue
{
   return UndoByTime      (time, src_time, data, src_data) // first check by time because it's faster
       || UndoByValueEqual(time, src_time, data, src_data);
}*/

void SetUndo(C Edit._Undo &undos, Button &undo, Button &redo)
{
   undo.enabled(undos.undosAvailable());
   redo.enabled(undos.redosAvailable());
}
/******************************************************************************/
// IMAGE
/******************************************************************************/
DIR_ENUM GetCubeDir(int face)
{
   switch(face)
   {
      default: return DIR_LEFT;
      case  1: return DIR_FORWARD;
      case  2: return DIR_RIGHT;
      case  3: return DIR_BACK;
      case  4: return DIR_DOWN;
      case  5: return DIR_UP;
   }
}
Str GetCubeFile(C Str &files, int face)
{
   Mems<FileParams> faces=FileParams.Decode(files);
   return (faces.elms()==1) ? files : InRange(face, faces) ? faces[face].encode() : S;
}
Str SetCubeFile(Str files, int face, C Str &file) // put 'file' into specified 'face' and return all files
{
   if(InRange(face, 6))
   {
      Mems<FileParams> faces=FileParams.Decode(files);
      if(faces.elms()==1){faces.setNum(6); REPAO(faces)=files;} // set all from original
      if(faces.elms()!=6)faces.clear(); // if invalid number then clear
      faces.setNum(6); // set 6 faces
      faces[face]=file; // set i-th face to target file
      files=FileParams.Encode(faces); // get all faces
   }
   return files;
}
/******************************************************************************/
bool HasAlpha(C Image &image) // if image has alpha channel
{
   if(!image.typeInfo().a)return false;
   Vec4 min, max; if(image.stats(&min, &max, null))return !(Equal(min.w, 1, 1.5/255) && Equal(max.w, 1, 1.5/255));
   return true;
}
bool HasColor(C Image &image) // if image is not monochromatic
{
   return !image.monochromatic();
}
bool NeedFullAlpha(Image &image, int dest_type)
{
   return image.typeInfo().a && (!InRange(dest_type, IMAGE_TYPES) || ImageTI[dest_type].a); // have to change only if source and dest have alpha channel
}
bool SetFullAlpha(Image &image, IMAGE_TYPE dest_type) // returns if any change was made
{
   if(NeedFullAlpha(image, dest_type))
   {
      if(image.compressed())return image.copyTry(image, -1, -1, -1, ImageTypeExcludeAlpha(ImageTypeUncompressed(image.type())), IMAGE_SOFT, 1);
      if(image.lock())
      {
         REPD(y, image.h())
         REPD(x, image.w()){Color c=image.color(x, y); c.a=255; image.color(x, y, c);}
         image.unlock();
         return true;
      }
   }
   return false;
}

enum
{
   IGNORE_ALPHA=1<<0,
   SRGB        =1<<1,
   MTRL_BASE_0 =1<<2,
   MTRL_BASE_1 =1<<3,
   MTRL_BASE_2 =1<<4,
   WATER_MTRL  =1<<5,
   MTRL_DETAIL =1<<6,
   MTRL_MACRO  =1<<7,
   MTRL_LIGHT  =1<<8,
}
class ImageHashHeader // !! try to don't make any changes to this class layout, because doing so will require a new hash for every texture !!
{
   VecI       size;
   IMAGE_TYPE type;
   byte       flags;

   ImageHashHeader(C Image &image, IMAGE_TYPE type)
   {
      ASSERT(SIZE(ImageHashHeader)==3*4+4); // make sure all compilers generate the same size
      Zero(T); // it's very important to zero entire data at the start, in case there's any extra padding, to make sure hash is always the same
      T.size=image.size3();
      T.type=type;
      if(image.cube())T.flags|=1;
   }
}
void ImageProps(C Image &image, UID *hash, IMAGE_TYPE *best_type=null, uint flags=SRGB, Edit.Material.TEX_QUALITY quality=Edit.Material.MEDIUM) // calculate image hash and best type for image compression
{
   if(hash || best_type)
   {
      if(image.is())
      {
         bool       sign=false;
         IMAGE_TYPE type=IMAGE_NONE;
         if(flags&WATER_MTRL)
         {
            if(flags&MTRL_BASE_0){MAX(quality, MinMtrlTexQualityBase0); flags|=SRGB;} // #WaterMaterialTextureLayout
            if(flags&MTRL_BASE_1){MAX(quality, MinMtrlTexQualityBase1); sign=true; type=((quality>=Edit.Material.FULL) ? IMAGE_R8G8_SIGN : IMAGE_BC5_SIGN);} // normal tex always uses BC5_SIGN (RG HQ) #WaterMaterialTextureLayout
            if(flags&MTRL_BASE_2){MAX(quality, MinMtrlTexQualityBase2); sign=true; type=((quality>=Edit.Material.FULL) ? IMAGE_R8_SIGN   : IMAGE_BC4_SIGN);} // bump   tex always uses BC4_SIGN (R  HQ) #WaterMaterialTextureLayout
         }else
         {
            if(flags&MTRL_BASE_0){MAX(quality, MinMtrlTexQualityBase0); flags|=SRGB;} // #MaterialTextureLayout
            if(flags&MTRL_BASE_1){MAX(quality, MinMtrlTexQualityBase1); sign=true; type=((quality>=Edit.Material.FULL) ? IMAGE_R8G8_SIGN : IMAGE_BC5_SIGN);} // normal tex always uses BC5_SIGN (RG HQ) #MaterialTextureLayout
            if(flags&MTRL_BASE_2){MAX(quality, MinMtrlTexQualityBase2);} // #MaterialTextureLayout
         }
         if(flags&MTRL_DETAIL){MAX(quality, MinMtrlTexQualityDetail);}
         if(flags&MTRL_MACRO ){MAX(quality, MinMtrlTexQualityMacro ); flags|=SRGB|IGNORE_ALPHA;}
         if(flags&MTRL_LIGHT ){MAX(quality, MinMtrlTexQualityLight ); flags|=SRGB|IGNORE_ALPHA;}

         MD5  h;
         bool bc1=true, // BC1 4-bit uses 1-bit alpha (0 or 255) (R,G,B,a?255:0)
              bc2=true, // BC2 8-bit uses 4-bit alpha
              bc4=true, // BC4 4-bit is (R,0,0,1)
              bc5=true, // BC5 8-bit is (R,G,0,1)
              srgb=FlagTest(flags, SRGB),
              calc_type=(type==IMAGE_NONE), // if have to calculate type, always calculate type if unknown (even if 'best_type' is null), because it affects hash
              force_alpha=((flags&IGNORE_ALPHA) && image.typeInfo().a), // if we want to ignore alpha, and source had alpha, then we need to adjust as if it has full alpha, this is done because: ignoring alpha may save the image in format that doesn't support the alpha channel, however if the same image is later used for something else, and now wants to use that alpha channel, then it needs to be created as a different texture (with different hash)
              extract=((hash && (sign ? image.hwType()!=IMAGE_R8G8B8A8_SIGN : (image.hwType()!=IMAGE_R8G8B8A8 && image.hwType()!=IMAGE_R8G8B8A8_SRGB))) // hash is based on RGBA format
                    || (calc_type && image.compressed()) // checking 'type' requires color reads so copy to RGBA soft to make them faster
                    || force_alpha); // forcing alpha requires modifying the alpha channel, so copy to 'temp' which we can modify
         Image temp; C Image *src=(extract ? &temp : &image);
         Color min(255, 255), max(0, 0);
         FREPD(face, image.faces())
         {
            int src_face=face;
            if(extract)
               if(image.extractMipMap(temp, sign ? IMAGE_R8G8B8A8_SIGN : image.sRGB() ? IMAGE_R8G8B8A8_SRGB : IMAGE_R8G8B8A8, 0, DIR_ENUM(face)))src_face=0; // !! RGBA is needed because below we use 'pixC' !!
               else goto error;
            if(src.lockRead(0, DIR_ENUM(src_face)))
            {
               if(force_alpha) // set before calculating hash
               {
                  byte alpha=(sign ? 127 : 255); // use 127 instead of 128, because this is for signed byte (which is in range -128..127)
                  REPD(z, temp.d())
                  REPD(y, temp.h())
                  REPD(x, temp.w())temp.pixC(x, y, z).a=alpha;
               }
               if(hash)
               {
                  int pitch =src.w()*src.bytePP(), // don't use 'src.pitch'                       to ignore any extra padding
                      pitch2=pitch  *src.h     (); // use "pitch*src.h()" instead of 'src.pitch2' to ignore any extra padding
                  FREPD(z, src.d()) // process in order
                  {
                     if(src.pitch()==pitch)h.update(src.data()                 + z*src.pitch2(), pitch2); // if don't have any extra padding then we can update hash for all Y's in one go
                     else FREPD(y, src.h())h.update(src.data() + y*src.pitch() + z*src.pitch2(), pitch ); // process in order
                  }
               }
               if(calc_type)REPD(z, src.d())
                            REPD(y, src.h())
                            REPD(x, src.w())
               {
                  Color c=src.color3D(x, y, z);
                  byte  bc2_a=((c.a*15+128)/255)*255/15;
                //if(c.a> 1 && c.a<254                    // BC1 supports only 0 and 255 alpha #BC1RGB
                //|| c.a<=1 && c.lum()>1      )bc1=false; // BC1 supports only black color at 0 alpha
                  if(Abs(c.a-bc2_a)>1         )bc2=false;
                //if(c.g>1 || c.b>1 || c.a<254)bc4=false;
                //if(         c.b>1 || c.a<254)bc5=false;
                  MIN(min.r, c.r); MIN(min.g, c.g); MIN(min.b, c.b); MIN(min.a, c.a);
                  MAX(max.r, c.r); MAX(max.g, c.g); MAX(max.b, c.b); MAX(max.a, c.a);
               }
               src.unlock();
            }else goto error;
         }

         if(calc_type)
         {
            if(                      min.a<254)bc1=false; // BC1 supports only           A=255 #BC1RGB
            if(max.g>1 || max.b>1 || min.a<254)bc4=false; // BC4 supports only G=0, B=0, A=255
            if(           max.b>1 || min.a<254)bc5=false; // BC5 supports only      B=0, A=255
            if((flags&MTRL_BASE_2) && !(flags&WATER_MTRL) && max.b-min.b>1)MAX(quality, Edit.Material.HIGH); // if this is Base2 Ext and have bump map #MaterialTextureLayout then disable MEDIUM quality BC1 and use HIGH quality BC7

            if(quality>=Edit.Material.FULL)
            {
               if(srgb || min.a<254 || max.b>1)type=IMAGE_R8G8B8A8_SRGB;else // sRGB or has Alpha or has Blue
               if(                     max.g>1)type=IMAGE_R8G8         ;else // has Green
                                               type=IMAGE_R8           ;
            }else
            {
               if(bc4 && !srgb                     )type=                         IMAGE_BC4 ;else // BC4 is 4-bit HQ so use it always if possible (doesn't support sRGB)
               if(bc1 && quality<Edit.Material.HIGH)type=(srgb ? IMAGE_BC1_SRGB : IMAGE_BC1);else // use BC1 only if we don't want HQ
               if(bc5 && !srgb                     )type=                         IMAGE_BC5 ;else // BC5 has better quality for RG than BC7 so check it first (doesn't support sRGB)
               if(SupportBC7                       )type=(srgb ? IMAGE_BC7_SRGB : IMAGE_BC7);else
               if(bc1                              )type=(srgb ? IMAGE_BC1_SRGB : IMAGE_BC1);else // check BC1 again, now without HQ
               if(bc2                              )type=(srgb ? IMAGE_BC2_SRGB : IMAGE_BC2);else
                                                    type=(srgb ? IMAGE_BC3_SRGB : IMAGE_BC3);
            }
         }
         if(best_type)*best_type=type;
         if(hash     )
         {
            h.update(&ImageHashHeader(image, type), SIZE(ImageHashHeader)); // need to append hash with a header, to make sure different sized/cube/srgb/sign/quality images will always have different hash
           *hash=h();
         }
      }else
      {
      error:
         if(hash     )hash.zero();
         if(best_type)*best_type=IMAGE_NONE;
      }
   }
}
/******************************************************************************/
void LoadTexture(C Project &proj, C UID &tex_id, Image &image)
{
   ImagePtr src=proj.texPath(tex_id);
   if(src)src->copyTry(image, -1, -1, -1, ImageTypeUncompressed(src->type()), IMAGE_SOFT, 1);else image.del(); // always copy, because: src texture will always be compressed, also soft doesn't require locking
}
void ExtractBaseTextures(C Project &proj, C UID &base_0, C UID &base_1, C UID &base_2, Image *color, Image *alpha, Image *bump, Image *normal, Image *smooth, Image *reflect, Image *glow)
{ // #MaterialTextureLayout
   uint tex=0;
   if(base_2.valid())
   {
      if(smooth || reflect || bump || alpha)
      {
         Image b2; LoadTexture(proj, base_2, b2);
         if(smooth )smooth .createSoftTry(b2.w(), b2.h(), 1, IMAGE_L8);
         if(reflect)reflect.createSoftTry(b2.w(), b2.h(), 1, IMAGE_L8);
         if(bump   )bump   .createSoftTry(b2.w(), b2.h(), 1, IMAGE_L8);
         if(alpha  )alpha  .createSoftTry(b2.w(), b2.h(), 1, IMAGE_L8);
         REPD(y, b2.h())
         REPD(x, b2.w())
         {
            Color c=b2.color(x, y);
            if(smooth ){smooth .pixel(x, y, c.r); if(c.r<254                           )tex|=BT_SMOOTH ;}
            if(reflect){reflect.pixel(x, y, c.g); if(c.g<254                           )tex|=BT_REFLECT;}
            if(bump   ){bump   .pixel(x, y, c.b); if(c.b>1 && Abs(c.b-128)>1 && c.b<254)tex|=BT_BUMP   ;} // BUMP_DEFAULT_TEX can be either 0, 128 or 255
            if(alpha  ){alpha  .pixel(x, y, c.a); if(c.a<254                           )tex|=BT_ALPHA  ;}
         }
      }
      if(base_0.valid() && (color || glow)) // base_0 && base_2
      {
         Image b0; LoadTexture(proj, base_0, b0);
         if(color)color.createSoftTry(b0.w(), b0.h(), 1, IMAGE_R8G8B8_SRGB);
         if(glow )glow .createSoftTry(b0.w(), b0.h(), 1, IMAGE_L8);
         REPD(y, b0.h())
         REPD(x, b0.w())
         {
            Color c=b0.color(x, y);
            if(color){color.color(x, y, c  ); if(c.r<254 || c.g<254 || c.b<254)tex|=BT_COLOR;}
            if(glow ){glow .pixel(x, y, c.a); if(c.a<254                      )tex|=BT_GLOW ;}
         }
      }
   }else
   if(base_0.valid() && (color || alpha)) // base_0 without base_2
   {
      Image b0; LoadTexture(proj, base_0, b0);
      if(color)color.createSoftTry(b0.w(), b0.h(), 1, IMAGE_R8G8B8_SRGB);
      if(alpha)alpha.createSoftTry(b0.w(), b0.h(), 1, IMAGE_L8);
      REPD(y, b0.h())
      REPD(x, b0.w())
      {
         Color c=b0.color(x, y);
         if(color){color.color(x, y, c  ); if(c.r<254 || c.g<254 || c.b<254)tex|=BT_COLOR;}
         if(alpha){alpha.pixel(x, y, c.a); if(c.a<254                      )tex|=BT_ALPHA;}
      }
   }
   if(base_1.valid() && normal)
   {
      Image b1; LoadTexture(proj, base_1, b1);
      normal.createSoftTry(b1.w(), b1.h(), 1, IMAGE_R8G8B8);
      REPD(y, b1.h())
      REPD(x, b1.w())
      {
         Vec4 n; n.xy=b1.colorF(x, y).xy;
         if(Abs(n.x)>1.5/127
         || Abs(n.y)>1.5/127)tex|=BT_NORMAL;
         n.z=CalcZ(n.xy);
         n.xyz=n.xyz*0.5+0.5;
         n.w=1;
         normal.colorF(x, y, n);
      }
   }
   if(color   && !(tex&BT_COLOR  ))color  .del();
   if(alpha   && !(tex&BT_ALPHA  ))alpha  .del();
   if(bump    && !(tex&BT_BUMP   ))bump   .del();
   if(normal  && !(tex&BT_NORMAL ))normal .del();
   if(smooth  && !(tex&BT_SMOOTH ))smooth .del();
   if(reflect && !(tex&BT_REFLECT))reflect.del();
   if(glow    && !(tex&BT_GLOW   ))glow   .del();
}
void ExtractWaterBaseTextures(C Project &proj, C UID &base_0, C UID &base_1, C UID &base_2, Image *color, Image *alpha, Image *bump, Image *normal, Image *smooth, Image *reflect, Image *glow)
{ // #WaterMaterialTextureLayout
   uint tex=0;
   if(base_0.valid() && color)
   {
      Image b0; LoadTexture(proj, base_0, b0);
      if(color)color.createSoftTry(b0.w(), b0.h(), 1, IMAGE_R8G8B8_SRGB);
      REPD(y, b0.h())
      REPD(x, b0.w())
      {
         Color c=b0.color(x, y);
         if(color){color.color(x, y, c); if(c.r<254 || c.g<254 || c.b<254)tex|=BT_COLOR;}
      }
   }
   if(base_1.valid() && normal)
   {
      Image b1; LoadTexture(proj, base_1, b1);
      normal.createSoftTry(b1.w(), b1.h(), 1, IMAGE_R8G8B8);
      REPD(y, b1.h())
      REPD(x, b1.w())
      {
         Vec4 n; n.xy=b1.colorF(x, y).xy;
         if(Abs(n.x)>1.5/127
         || Abs(n.y)>1.5/127)tex|=BT_NORMAL;
         n.z=CalcZ(n.xy);
         n.xyz=n.xyz*0.5+0.5;
         n.w=1;
         normal.colorF(x, y, n);
      }
   }
   if(base_2.valid() && bump)
   {
      Image b2; LoadTexture(proj, base_2, b2);
      if(bump)bump.createSoftTry(b2.w(), b2.h(), 1, IMAGE_L8);
      REPD(y, b2.h())
      REPD(x, b2.w())
      {
         flt c=b2.pixelF(x, y);
         if(bump){if(Abs(c)>1.5/127)tex|=BT_BUMP; bump.pixelF(x, y, c*0.5+0.5);}
      }
   }
   if(color   && !(tex&BT_COLOR  ))color  .del();
   if(alpha   && !(tex&BT_ALPHA  ))alpha  .del();
   if(bump    && !(tex&BT_BUMP   ))bump   .del();
   if(normal  && !(tex&BT_NORMAL ))normal .del();
   if(smooth  && !(tex&BT_SMOOTH ))smooth .del();
   if(reflect && !(tex&BT_REFLECT))reflect.del();
   if(glow    && !(tex&BT_GLOW   ))glow   .del();
}
void ExtractBaseTexturesOld(C Project &proj, C UID &base_0, C UID &base_1, Image *color, Image *alpha, Image *bump, Image *normal, Image *smooth, Image *reflect, Image *glow, MATERIAL_TECHNIQUE tech)
{
   uint tex=0;
   if(base_0.valid() && base_1.valid()) // both textures specified
   {
      if(color || bump)
      {
         Image b0; LoadTexture(proj, base_0, b0);
         if(color)color.createSoftTry(b0.w(), b0.h(), 1, IMAGE_R8G8B8_SRGB);
         if(bump )bump .createSoftTry(b0.w(), b0.h(), 1, IMAGE_L8);
         REPD(y, b0.h())
         REPD(x, b0.w())
         {
            Color c=b0.color(x, y);
            if(color){color.color(x, y, c  ); if(c.r<254 || c.g<254 || c.b<254)tex|=BT_COLOR;}
            if(bump ){bump .pixel(x, y, c.a); if(Abs(c.a-128)>1     && c.a<254)tex|=BT_BUMP ;} // old BUMP_DEFAULT_TEX was either 128 or 255
         }
      }
      if(alpha || normal || smooth || reflect || glow)
      {
         bool tex_alpha=(tech!=MTECH_DEFAULT); // old mtrl textures had Base1 W channel as either Alpha or Glow
         Image b1; LoadTexture(proj, base_1, b1);
                       if(normal )normal .createSoftTry(b1.w(), b1.h(), 1, IMAGE_R8G8B8);
                       if(smooth )smooth .createSoftTry(b1.w(), b1.h(), 1, IMAGE_L8);
                       if(reflect)reflect.createSoftTry(b1.w(), b1.h(), 1, IMAGE_L8);
         if(tex_alpha){if(alpha  )alpha  .createSoftTry(b1.w(), b1.h(), 1, IMAGE_L8);}
         else         {if(glow   )glow   .createSoftTry(b1.w(), b1.h(), 1, IMAGE_L8);}
         REPD(y, b1.h())
         REPD(x, b1.w())
         {
            Color c=b1.color(x, y);
            if(tex_alpha){if(alpha  ){alpha  .pixel(x, y, c.a); if(c.a<254)tex|=BT_ALPHA  ;}}
            else         {if(glow   ){glow   .pixel(x, y, c.a); if(c.a<254)tex|=BT_GLOW   ;}}
                          if(smooth ){smooth .pixel(x, y, c.b); if(c.b<254)tex|=BT_SMOOTH ;}
                          if(reflect){reflect.pixel(x, y, c.b); if(c.b<254)tex|=BT_REFLECT;}
                          if(normal )
            {
               Vec n; n.xy.set((c.r-128)/127.0, (c.g-128)/127.0); n.z=CalcZ(n.xy);
               normal.color(x, y, Color(c.r, c.g, Mid(Round(n.z*127+128), 0, 255))); if(Abs(c.r-128)>1 || Abs(c.g-128)>1)tex|=BT_NORMAL;
            }
         }
      }
   }else
   if(base_0.valid()) // only one texture specified
   {
      if(color || alpha)
      {
         Image b0; LoadTexture(proj, base_0, b0);
         if(color)color.createSoftTry(b0.w(), b0.h(), 1, IMAGE_R8G8B8_SRGB);
         if(alpha)alpha.createSoftTry(b0.w(), b0.h(), 1, IMAGE_L8);
         REPD(y, b0.h())
         REPD(x, b0.w())
         {
            Color c=b0.color(x, y);
            if(color){color.color(x, y, c  ); if(c.r<254 || c.g<254 || c.b<254)tex|=BT_COLOR;}
            if(alpha){alpha.pixel(x, y, c.a); if(c.a<254                      )tex|=BT_ALPHA;}
         }
      }
   }
   if(color   && !(tex&BT_COLOR  ))color  .del();
   if(alpha   && !(tex&BT_ALPHA  ))alpha  .del();
   if(bump    && !(tex&BT_BUMP   ))bump   .del();
   if(normal  && !(tex&BT_NORMAL ))normal .del();
   if(smooth  && !(tex&BT_SMOOTH ))smooth .del();
   if(reflect && !(tex&BT_REFLECT))reflect.del();
   if(glow    && !(tex&BT_GLOW   ))glow   .del();
}
void ExtractDetailTexture(C Project &proj, C UID &detail_tex, Image *color, Image *bump, Image *normal, Image *smooth)
{
   uint tex=0;
   if(detail_tex.valid())
      if(color || /*bump ||*/ normal || smooth)
   {
      Image det; LoadTexture(proj, detail_tex, det);
      if(color )color .createSoftTry(det.w(), det.h(), 1, IMAGE_L8);
    //if(bump  )bump  .createSoftTry(det.w(), det.h(), 1, IMAGE_L8);
      if(normal)normal.createSoftTry(det.w(), det.h(), 1, IMAGE_R8G8B8);
      if(smooth)smooth.createSoftTry(det.w(), det.h(), 1, IMAGE_L8);
      REPD(y, det.h())
      REPD(x, det.w())
      {
         Color c=det.color(x, y); // #MaterialTextureLayout
         if(color ){color .pixel(x, y, c.b); if(c.b<254)tex|=BT_COLOR ;}
         if(smooth){smooth.pixel(x, y, c.a); if(c.a<254)tex|=BT_SMOOTH;}
         if(normal)
         {
            Vec n; n.xy.set((c.r-128)/127.0, (c.g-128)/127.0); n.z=CalcZ(n.xy);
            normal.color(x, y, Color(c.r, c.g, Mid(Round(n.z*127+128), 0, 255))); if(Abs(c.r-128)>1 || Abs(c.g-128)>1)tex|=BT_NORMAL;
         }
      }
   }
   if(color  && !(tex&BT_COLOR ))color .del();
   if(bump   && !(tex&BT_BUMP  ))bump  .del();
   if(normal && !(tex&BT_NORMAL))normal.del();
   if(smooth && !(tex&BT_SMOOTH))smooth.del();
}
UID MergedBaseTexturesID(C UID &base_0, C UID &base_1, C UID &base_2) // this function generates ID of a merged texture created from two base textures, formula for this function can be freely modified as in worst case merged textures will just get regenerated
{
   MD5 id;
   id.update(&base_0, SIZE(base_0));
   id.update(&base_1, SIZE(base_1));
   id.update(&base_2, SIZE(base_2));
   return id();
}
/******************************************************************************/
VecI ImageSize(C VecI &src, C VecI2 &custom, bool pow2)
{
   VecI size=src;
   if( custom.x>0)size.x=custom.x;
   if( custom.y>0)size.y=custom.y;
   if(!custom.x && custom.y>0 && src.y)size.x=Max(1, (src.x*custom.y+src.y/2)/src.y); // keep aspect ratio
   if(!custom.y && custom.x>0 && src.x)size.y=Max(1, (src.y*custom.x+src.x/2)/src.x); // keep aspect ratio
   if(pow2)size.set(NearestPow2(size.x), NearestPow2(size.y), NearestPow2(size.z));
   return size;
}
VecI2 GetSize(C Str &name, C Str &value, C VecI &src)
{
   VecI2 size;
   if(value=="quarter")size.set(Max(1, src.x/4), Max(1, src.y/4));else
   if(value=="half"   )size.set(Max(1, src.x/2), Max(1, src.y/2));else
   if(value=="double" )size=src.xy*2;else
   {
      Vec2 sf; if(Contains(value, ','))sf=TextVec2(value);else sf=TextFlt(value);
      UNIT_TYPE unit=GetUnitType(value);
      size.x=Round(ConvertUnitType(sf.x, src.x, unit));
      size.y=Round(ConvertUnitType(sf.y, src.y, unit));
   }
   size=ImageSize(src, size, false).xy;
   if(Starts(name, "maxSize")){MIN(size.x, src.x); MIN(size.y, src.y);}
   return size;
}
int GetFilter(C Str &name)
{
   if(Contains(name, "nearest"  ) || Contains(name, "point") || Contains(name, "FilterNone"))return FILTER_NONE;
   if(Contains(name, "linear"   ))return FILTER_LINEAR;
   if(Contains(name, "cubic+"   ) || Contains(name, "cubicPlus"))return FILTER_CUBIC_PLUS; // !! check this before "cubic" !!
   if(Contains(name, "cubic"    ))return FILTER_CUBIC_FAST;
   if(Contains(name, "waifu"    ))return FILTER_WAIFU;
   if(Contains(name, "NoStretch"))return FILTER_NO_STRETCH;
                                  return -1;
}
bool GetClampWrap(C Str &name, bool default_clamp)
{
   if(Contains(name, "clamp"))return true ; // clamp=1
   if(Contains(name, "wrap" ))return false; // clamp=0
                              return default_clamp; // default
}
bool GetAlphaWeight(C Str &name) {return Contains(name, "alphaWeight");}
bool EditToGameImage(Image &edit, Image &game, bool pow2, bool srgb, bool alpha_lum, ElmImage.TYPE type, int mode, int mip_maps, bool has_color, bool has_alpha, bool ignore_alpha, bool env, C VecI2 &custom_size=0, C int *force_type=null)
{
   VecI size=edit.size3();
   if(!edit.cube() && IsCube((IMAGE_MODE)mode))switch(edit.cubeLayout())
   {
      case CUBE_LAYOUT_CROSS: size.x/=4; size.y/=3; break;
      case CUBE_LAYOUT_6x1  : size.x/=6;            break;
   }
   size=ImageSize(size, custom_size, pow2);

   Image temp, *src=&edit;

   if(force_type)
   {
      if(*force_type==IMAGE_NONE || *force_type<0)force_type=null;
   }
   if(alpha_lum)
   {
      if(&edit!=&game){src.copyTry(temp); src=&temp;}
      src.alphaFromBrightness().divRgbByAlpha();
   }
   if(ignore_alpha && src.typeInfo().a) // if want to ignore alpha then set it to full as some compressed texture formats will benefit from better quality (like PVRTC)
   {
      if(mip_maps<0)mip_maps=((src.mipMaps()==1) ? 1 : 0); // source will have now only one mip-map so we can't use "-1", auto-detect instead
      if(mode    <0)mode    =src.mode();                   // source will now be as IMAGE_SOFT      so we can't use "-1", auto-detect instead
      if(src.copyTry(temp, -1, -1, -1, IMAGE_R8G8B8_SRGB, IMAGE_SOFT, 1))src=&temp;
   }

   IMAGE_TYPE    dest_type;
   if(force_type)dest_type=IMAGE_TYPE(*force_type);else
   if(type==ElmImage.ALPHA)dest_type=IMAGE_A8;else
   if(type==ElmImage.FULL )dest_type=(has_color ? (srgb ? IMAGE_R8G8B8A8_SRGB : IMAGE_R8G8B8A8) : has_alpha ? (srgb ? IMAGE_L8A8_SRGB : IMAGE_L8A8) : (srgb ? IMAGE_L8_SRGB : IMAGE_L8));else
                           ImageProps(*src, null, &dest_type, (srgb ? SRGB : 0) | (ignore_alpha ? IGNORE_ALPHA : 0), (type==ElmImage.FULL) ? Edit.Material.FULL : (type==ElmImage.COMPRESSED2) ? Edit.Material.HIGH : Edit.Material.MEDIUM);

   if((src.type()==IMAGE_L8 || src.type()==IMAGE_L8_SRGB) &&  dest_type==IMAGE_A8
   ||  src.type()==IMAGE_A8                               && (dest_type==IMAGE_L8 || dest_type==IMAGE_L8_SRGB))
   {
      Image temp2; if(temp2.createSoftTry(src.w(), src.h(), src.d(), dest_type) && src.lockRead())
      {
         REPD(z, temp2.d())
         REPD(y, temp2.h())
         REPD(x, temp2.w())temp2.pixel3D(x, y, z, src.pixel3D(x, y, z));
         src.unlock();
         Swap(temp, temp2); src=&temp;
      }
   }
   return src.copyTry(game, size.x, size.y, size.z, dest_type, mode, mip_maps, FILTER_BEST, IC_CLAMP|IC_ALPHA_WEIGHT|(env ? IC_ENV_CUBE : 0));
}
bool EditToGameImage(Image &edit, Image &game, C ElmImage &data, C int *force_type=null)
{
   return EditToGameImage(edit, game, data.pow2(), data.sRGB(), data.alphaLum(), data.type, data.mode, data.mipMapsActual() ? 0 : 1, data.hasColor(), data.hasAlpha3(), data.ignoreAlpha(), data.envActual(), data.size, force_type);
}
/******************************************************************************/
void DrawPanelImage(C PanelImage &pi, C Rect &rect, bool draw_lines=false)
{
   Vec2 size=rect.size()/5;
   Rect r=rect; r.extend(-size/3);
   Rect_LU lu(r.lu(), size            ); Rect_RU ru(r.ru(), size.x*3, size.y  );
   Rect_LD ld(r.ld(), size.x, size.y*3); Rect_RD rd(r.rd(), size.x*3, size.y*3);
   pi.draw(lu); pi.draw(ru);
   pi.draw(ld); pi.draw(rd);
   if(draw_lines)
   {
      pi.drawScaledLines(RED, lu); pi.drawScaledLines(RED, ru);
      pi.drawScaledLines(RED, ld); pi.drawScaledLines(RED, rd);
      lu.draw(TURQ, false); ru.draw(TURQ, false);
      ld.draw(TURQ, false); rd.draw(TURQ, false);
   }
}
/******************************************************************************/
bool UpdateMtrlBase1Tex(C Image &src, Image &dest)
{
   Image temp; if(src.copyTry(temp, -1, -1, -1, IMAGE_R8G8B8A8, IMAGE_SOFT, 1))
   {
      // old: r=spec, g=NrmY, b=alpha, a=NrmX
      // new: r=NrmX, g=NrmY, b=spec , a=alpha
      REPD(y, temp.h())
      REPD(x, temp.w())
      {
         Color c=temp.color(x, y);
         c.set(c.a, c.g, c.r, c.b);
         temp.color(x, y, c);
      }
      return temp.copyTry(dest, -1, -1, -1, (src.type()==IMAGE_BC3 || src.type()==IMAGE_BC3_SRGB) ? IMAGE_BC7 : ImageTypeExcludeSRGB(src.type()), src.mode(), src.mipMaps(), FILTER_BEST, IC_WRAP);
   }
   return false;
}
/******************************************************************************/
void AdjustMaterialParams(EditMaterial &edit, Material &game, uint old_base_tex, uint new_base_tex, bool old_light_map)
{
   TimeStamp time; time.getUTC();
   game._adjustParams(old_base_tex, new_base_tex);
   SyncByValue     (edit.   tech_time, time, edit.tech     , game.technique);
   SyncByValueEqual(edit.  color_time, time, edit.color_s.w, game.color_l.w); // alpha
   SyncByValueEqual(edit.   bump_time, time, edit.bump     , game.bump     );
   SyncByValueEqual(edit. normal_time, time, edit.normal   , game.normal   );
   SyncByValueEqual(edit. smooth_time, time, edit.smooth   , game.smooth   );
   SyncByValueEqual(edit.reflect_time, time, edit.reflect  , game.reflect  );
   SyncByValueEqual(edit.   glow_time, time, edit.glow     , game.glow     );

   bool new_light_map=edit.hasLightMap(); if(old_light_map!=new_light_map)
   {
      if(!new_light_map              )game.ambient=0;else
      if(game.ambient.min()<=EPS_COL8)game.ambient=1;
      SyncByValueEqual(edit.ambient_time, time, edit.ambient, game.ambient);
   }
}
void AdjustMaterialParams(EditWaterMtrl &edit, WaterMtrl &game, uint old_base_tex, uint new_base_tex, bool old_light_map)
{
   TimeStamp time; time.getUTC();
   uint changed=(old_base_tex^new_base_tex);
   if(changed&BT_BUMP)
   {
      if(!(new_base_tex&BT_BUMP)           )game.wave_scale=0;else
      if(game.wave_scale<=EPS_MATERIAL_BUMP)game.wave_scale=0.1;
   }
   if(changed&(BT_BUMP|BT_NORMAL))
   {
      if(!(new_base_tex&BT_BUMP) && !(new_base_tex&BT_NORMAL))game.normal=0;else
      if(                               game.normal<=EPS_COL8)game.normal=1;
   }
   SyncByValueEqual(edit.wave_scale_time, time, edit.wave_scale, game.wave_scale);
   SyncByValueEqual(edit.    normal_time, time, edit.normal    , game.normal    );
}
/******************************************************************************/
bool ImportImage(Image &image, C Str &name, int type=-1, int mode=-1, int mip_maps=-1, bool decompress=false)
{
   if(image.ImportTry(name, type, mode, mip_maps))
   {
      if(image.compressed() && decompress && !image.copyTry(image, -1, -1, -1, ImageTypeUncompressed(image.type())))return false;
      return true;
   }
 /*if(name.is())
   {
      File f, dec; if(f.readTry(name+".cmpr"))if(Decompress(f, dec.writeMem()))
      {
         dec.pos(0); if(image.ImportTry(dec, type, mode, mip_maps))return true;
      }
   }*/
   return false;
}
/******************************************************************************/
int ChannelIndex(char c)
{
   switch(c)
   {
      case 'r': case 'R': case 'x': case 'X': return 0;
      case 'g': case 'G': case 'y': case 'Y': return 1;
      case 'b': case 'B': case 'z': case 'Z': return 2;
      case 'a': case 'A': case 'w': case 'W': return 3;
   }
   return -1;
}
bool ChannelMonoTransform(C Str &value)
{
   return value.length()<=1 // up to 1 channels is treated as mono
   || ChannelIndex(value[0])==ChannelIndex(value[1]) && ChannelIndex(value[0])==ChannelIndex(value[2]); // check that RGB channels are the same
}
bool PartialTransform   (C TextParam &p   ) {return Contains(p.value, '@');} // if transform is partial (affects only part of the image and not full), '@' means transform at position
bool  ResizeTransformAny(C Str       &name) {return Starts(name, "resize") || Starts(name, "maxSize");}
bool  ResizeTransform   (C Str       &name) {return ResizeTransformAny(name) && !Contains(name, "NoStretch");} // skip "NoStretch" because it's more like "crop"
bool    MonoTransform   (C TextParam &p   ) {return p.name=="grey" || p.name=="greyPhoto" || p.name=="bump" || (p.name=="channel" && ChannelMonoTransform(p.value));} // if result is always mono
bool NonMonoTransform   (C TextParam &p   ) // if can change a mono image to non-mono, this is NOT the same as "!MonoTransform"
{
   int values=Occurrences(p.value, ',');
   return p.name=="inverseR"
       || p.name=="inverseG"
       || p.name=="inverseRG"
       || p.name=="lerpRGB" && values>2
       || p.name=="iLerpRGB" && values>2
       || p.name=="mulRGB" && TextVecEx(p.value).anyDifferent()
       || p.name=="addRGB" && TextVecEx(p.value).anyDifferent()
       || p.name=="mulAddRGB" && values>2
       || p.name=="addMulRGB" && values>2
       || p.name=="mulRGBS" && TextVecEx(p.value).anyDifferent()
       || p.name=="mulRGBH" && values>1
       || p.name=="mulRGBHS" && values>1
       || p.name=="gamma" && TextVecEx(p.value).anyDifferent()
       || p.name=="brightness" && TextVecEx(p.value).anyDifferent()
       || p.name=="contrast" && TextVecEx(p.value).anyDifferent()
       || p.name=="contrastAlphaWeight" && TextVecEx(p.value).anyDifferent()
       || p.name=="addSat"
       || p.name=="addHueSat"
       || p.name=="setHueSat"
       || p.name=="setHueSatPhoto"
       || p.name=="lerpHueSat"
       || p.name=="rollHueSat"
       || p.name=="rollHueSatPhoto"
       || p.name=="min" && TextVecEx(p.value).anyDifferent()
       || p.name=="max" && TextVecEx(p.value).anyDifferent()
       || p.name=="channel" && !ChannelMonoTransform(p.value)
       || p.name=="scaleXY" && TextVec2Ex(p.value).anyDifferent()
       || p.name=="bumpToNormal";
}
bool HighPrecTransform(C Str &name)
{
   return ResizeTransform(name)
       || name=="mulRGB" || name=="addRGB" || name=="mulAddRGB" || name=="addMulRGB" || name=="mulA"
       || name=="mulRGBS" || name=="mulRGBH" || name=="mulRGBHS"
       || name=="normalize"
       || name=="scale" || name=="scaleXY"
       || name=="lerpRGB" || name=="iLerpRGB"
       || name=="blur" || name=="sharpen"
       || name=="bump"
       || name=="contrast" || name=="contrastLum" || name=="contrastAlphaWeight" || name=="contrastLumAlphaWeight"
       || name=="brightness" || name=="brightnessLum"
       || name=="gamma" || name=="gammaLum"
       || name=="SRGBToLinear" || name=="LinearToSRGB"
       || name=="greyPhoto"
       || name=="avgLum" || name=="medLum" || name=="avgContrastLum" || name=="medContrastLum"
       || name=="avgHue" || name=="medHue" || name=="addHue" || name=="setHue" || name=="contrastHue" || name=="medContrastHue" || name=="contrastHueAlphaWeight" || name=="contrastHuePow"
       || name=="lerpHue" || name=="lerpHueSat" || name=="rollHue" || name=="rollHueSat" || name=="lerpHuePhoto" || name=="lerpHueSatPhoto" || name=="rollHuePhoto" || name=="rollHueSatPhoto"
       || name=="addSat" || name=="mulSat" || name=="mulSatPhoto" || name=="avgSat" || name=="medSat" || name=="contrastSat" || name=="medContrastSat" || name=="contrastSatAlphaWeight"
       || name=="addHueSat" || name=="setHueSat" || name=="setHueSatPhoto"
       || name=="mulSatH" || name=="mulSatHS" || name=="mulSatHPhoto" || name=="mulSatHSPhoto"
       || name=="metalToReflect";
}
bool SizeDependentTransform(C TextParam &p)
{
   return p.name=="blur" // range depends on size
       || p.name=="sharpen" // range depends on size
       || p.name=="bump" // range depends on size
       || p.name=="crop" // coordinates/size depend on size
       || p.name=="resizeNoStretch"
       || p.name=="tile" // tile range depends on size
       || (p.name=="pos" || p.name=="position") && p.asVecI2().any() // coordinates depend on size
       || PartialTransform(p); // coordinates/size depend on size
}
bool ForcesMono(C Str &file)
{
   Mems<FileParams> files=FileParams.Decode(file);
   REPA(files) // go from end
   {
      FileParams &file=files[i];
      if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
      REPA(file.params) // go from end
      {
       C TextParam &param=file.params[i];
         if(   MonoTransform(param) && !PartialTransform(param))return true; // if current transform generates mono (fully) and no non-mono were encountered up to now, then result is mono
         if(NonMonoTransform(param))return false; // if there's at least one transform that can generate color then result is not mono
      }
   }
   return false;
}
Str BumpFromColTransform(C Str &color_map, int blur) // 'blur'<0 = empty (default)
{
   Mems<FileParams> files=FileParams.Decode(color_map);
   REPA(files) // go from end
   {
      FileParams &file=files[i];
      if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
      REPA(file.params) // go from end
      {
         TextParam &p=file.params[i]; if(p.name!="crop" && p.name!="resizeNoStretch" && p.name!="swapXY" && p.name!="mirrorX" && p.name!="mirrorY")file.params.remove(i, true); // allow only these transforms
      }
      if(!file.is())files.remove(i, true); // if nothing left then remove it
   }
   SetTransform(files, "bump", (blur<0) ? S : S+blur);
   return FileParams.Encode(files);
}
bool ExtractResize(MemPtr<FileParams> files, TextParam &resize)
{
   resize.del();
   REPA(files) // go from end
   {
      FileParams &file=files[i];
      if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
      REPAD(pi, file.params) // go from end
      {
       C TextParam &p=file.params[pi];
         if(ResizeTransform(p.name))
         {
            resize=p; // extract resize
                    file.params.remove(pi, true); // remove it
            if(!file.is())files.remove( i, true); // if nothing left then remove it
            return true; // extracted
         }else
         if(SizeDependentTransform(p))return false; // if encountered a size dependent transform, it means we can't keep looking
      }
   }
   return false;
}
/******************************************************************************/
void AdjustImage(Image &image, bool alpha, bool high_prec)
{
   IMAGE_TYPE   type=image.type();
   if(alpha    )type=ImageTypeIncludeAlpha(type);
   if(high_prec)type=ImageTypeHighPrec    (type);
   image.copyTry(image, -1, -1, -1, type);
}
void ContrastLum(Image &image, flt contrast, flt avg_lum, C BoxI &box)
{
   if(contrast!=1 && image.lock())
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         flt  c_lum=c.xyz.max(), want_lum=(c_lum-avg_lum)*contrast+avg_lum;
         if(c_lum>EPS)c.xyz*=want_lum/c_lum;else c.xyz=want_lum;
         image.color3DF(x, y, z, c);
      }
      image.unlock();
   }      
}
void AvgContrastLum(Image &image, flt contrast, dbl avg_lum, C BoxI &box)
{
   if(avg_lum && image.lock()) // lock for writing because we will use this lock for applying contrast too
   {
      dbl contrast_total=0, weight_total=0;
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z); dbl c_lum=c.xyz.max();
         if(dbl d=c_lum-avg_lum)
         {
            dbl contrast=Abs(d)/avg_lum, // div by 'avg_lum' so for bright values contrast will be proportionally the same
                weight=Sqr(d); // squared distance from avg_lum
            contrast_total+=weight*contrast;
              weight_total+=weight;
         }
      }
      if(weight_total)if(contrast_total/=weight_total)ContrastLum(image, contrast/contrast_total, avg_lum, box);
      image.unlock();
   }
}
void ContrastHue(Image &image, flt contrast, C Vec &avg_col, C BoxI &box)
{
   if(contrast!=1 && image.lock())
   {
      flt avg_hue=RgbToHsb(avg_col).x;
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=RgbToHsb(c.xyz);
         flt d_hue=HueDelta(avg_hue, c.x);
         d_hue*=contrast;
         Clamp(d_hue, -0.5, 0.5); // clamp so we don't go back
         c.x=d_hue+avg_hue;
         c.xyz=HsbToRgb(c.xyz);
         image.color3DF(x, y, z, c);
      }
      image.unlock();
   }
}
void AddHue(Image &image, flt hue, C BoxI &box)
{
   hue=Frac(hue);
   if(hue && image.lock())
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=RgbToHsb(c.xyz);
         c.x +=hue;
         c.xyz=HsbToRgb(c.xyz);
         image.color3DF(x, y, z, c);
      }
      image.unlock();
   }
}
void ContrastSat(Image &image, flt contrast, flt avg_sat, C BoxI &box)
{
   if(contrast!=1 && image.lock())
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=RgbToHsb(c.xyz);
         c.y=(c.y-avg_sat)*contrast+avg_sat;
         c.xyz=HsbToRgb(c.xyz);
         image.color3DF(x, y, z, c);
      }
      image.unlock();
   }
}
void MulRGBH(Image &image, flt red, flt yellow, flt green, flt cyan, flt blue, flt purple, C BoxI &box)
{
   flt mul[]={red, yellow, green, cyan, blue, purple, red, yellow}; // red and yellow are listed extra for wraparound
   REP(6)if(mul[i]!=1)goto mul; return; mul:
   for(int z=box.min.z; z<box.max.z; z++)
   for(int y=box.min.y; y<box.max.y; y++)
   for(int x=box.min.x; x<box.max.x; x++)
   {
      Vec4 c=image.color3DF(x, y, z);
      Vec  hsb=RgbToHsb(c.xyz);
      flt  hue=hsb.x*6; int hue_i=Trunc(hue); flt hue_frac=hue-hue_i;
      flt  hue_mul=Lerp(mul[hue_i], mul[hue_i+1], hue_frac);
      c.xyz*=hue_mul;
      image.color3DF(x, y, z, c);
   }
}
void MulRGBHS(Image &image, flt red, flt yellow, flt green, flt cyan, flt blue, flt purple, C BoxI &box)
{
   flt mul[]={red, yellow, green, cyan, blue, purple, red, yellow}; // red and yellow are listed extra for wraparound
   REP(6)if(mul[i]!=1)goto mul; return; mul:
   for(int z=box.min.z; z<box.max.z; z++)
   for(int y=box.min.y; y<box.max.y; y++)
   for(int x=box.min.x; x<box.max.x; x++)
   {
      Vec4 c=image.color3DF(x, y, z);
      Vec  hsb=RgbToHsb(c.xyz);
      flt  hue=hsb.x*6; int hue_i=Trunc(hue); flt hue_frac=hue-hue_i;
      flt  hue_mul=Lerp(mul[hue_i], mul[hue_i+1], hue_frac);
      c.xyz*=Lerp(1.0, hue_mul, hsb.y);
      image.color3DF(x, y, z, c);
   }
}
void MulSat(Image &image, flt mul, C BoxI &box)
{
   if(mul!=1 && image.lock())
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=RgbToHsb(c.xyz);
         c.y*=mul;
         c.xyz=HsbToRgb(c.xyz);
         image.color3DF(x, y, z, c);
      }
      image.unlock();
   }   
}
void MulSatH(Image &image, flt red, flt yellow, flt green, flt cyan, flt blue, flt purple, bool sat, bool photo, C BoxI &box)
{
   flt mul[]={red, yellow, green, cyan, blue, purple, red, yellow}; // red and yellow are listed extra for wraparound
   REP(6)if(mul[i]!=1)goto mul; return; mul:
   for(int z=box.min.z; z<box.max.z; z++)
   for(int y=box.min.y; y<box.max.y; y++)
   for(int x=box.min.x; x<box.max.x; x++)
   {
      Vec4 c=image.color3DF(x, y, z);
      flt  lin_lum; if(photo)lin_lum=LinearLumOfSRGBColor(c.xyz);
      Vec  hsb=RgbToHsb(c.xyz);
      flt  hue=hsb.x*6; int hue_i=Trunc(hue); flt hue_frac=hue-hue_i;
      flt  sat_mul=Lerp(mul[hue_i], mul[hue_i+1], hue_frac);
      if(sat)sat_mul=Lerp(1.0, sat_mul, hsb.y);
      hsb.y*=sat_mul;
      c.xyz=HsbToRgb(hsb);
      if(photo)
      {
         c.xyz=SRGBToLinear(c.xyz);
         if(flt cur_lin_lum=LinearLumOfLinearColor(c.xyz))c.xyz*=lin_lum/cur_lin_lum;
         c.xyz=LinearToSRGB(c.xyz);
      }
      image.color3DF(x, y, z, c);
   }
}
flt HueDelta(flt a, flt b) // returns -0.5 .. 0.5
{
   flt d=Frac(b-a); if(d>0.5)d-=1; return d;
}
Vec2  LerpToMad(flt from, flt to) {return Vec2(to-from, from);}
Vec2 ILerpToMad(flt from, flt to) {return Vec2(1/(to-from), from/(from-to));}
flt   FloatSelf(flt x) {return x;}
flt   PowMax   (flt x, flt y) {return (x<=0) ? 0 : Pow(x, y);}

void TransformImage(Image &image, TextParam param, bool clamp)
{
   BoxI box(0, image.size3());
   int at_pos=TextPosI(param.value, '@'); if(at_pos>=0)
   {
      VecI4 v=TextVecI4(param.value()+at_pos+1); // X,Y,W,H
      RectI r(v.xy, v.xy+v.zw);
      box&=BoxI(VecI(r.min, 0), VecI(r.max, box.max.z));
      param.value.clip(at_pos);
   }

   if(HighPrecTransform(param.name))AdjustImage(image, false, true); // if transform might generate high precision values then make sure we can store them

   if(param.name=="crop")
   {
      VecI4 v=TextVecI4(param.value);
      image.crop(image, v.x, v.y, v.z, v.w);
   }else
   if(ResizeTransformAny(param.name))
   {
      VecI2 size        =GetSize       (param.name, param.value, image.size3());
      int   filter      =GetFilter     (param.name);
      bool  resize_clamp=GetClampWrap  (param.name, clamp);
      bool  alpha_weight=GetAlphaWeight(param.name);
      image.resize(size.x, size.y, InRange(filter, FILTER_NUM) ? FILTER_TYPE(filter) : FILTER_BEST, (resize_clamp?IC_CLAMP:IC_WRAP)|(alpha_weight?IC_ALPHA_WEIGHT:0));
   }else
   if(param.name=="tile")image.tile(param.asInt());else
   if(param.name=="inverseRGB")
   {
      if(image.highPrecision())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); c.xyz=1-c.xyz; image.color3DF(x, y, z, c);}
      }else
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Color c=image.color3D(x, y, z); c.r=255-c.r; c.g=255-c.g; c.b=255-c.b; image.color3D(x, y, z, c);}
      }
   }else
   if(param.name=="inverseR")
   {
      if(image.highPrecision())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); c.x=1-c.x; image.color3DF(x, y, z, c);}
      }else
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Color c=image.color3D(x, y, z); c.r=255-c.r; image.color3D(x, y, z, c);}
      }
   }else
   if(param.name=="inverseG")
   {
      if(image.highPrecision())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); c.y=1-c.y; image.color3DF(x, y, z, c);}
      }else
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Color c=image.color3D(x, y, z); c.g=255-c.g; image.color3D(x, y, z, c);}
      }
   }else
   if(param.name=="inverseRG")
   {
      if(image.highPrecision())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); c.x=1-c.x; c.y=1-c.y; image.color3DF(x, y, z, c);}
      }else
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Color c=image.color3D(x, y, z); c.r=255-c.r; c.g=255-c.g; image.color3D(x, y, z, c);}
      }
   }else
   if(param.name=="swapRG")
   {
      if(image.highPrecision())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); Swap(c.x, c.y); image.color3DF(x, y, z, c);}
      }else
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++){Color c=image.color3D(x, y, z); Swap(c.r, c.g); image.color3D(x, y, z, c);}
      }
   }else
   if(param.name=="swapXY")
   {
      Image temp; temp.createSoftTry(image.h(), image.w(), image.d(), image.type());
      if(temp.highPrecision())
      {
         REPD(y, image.h())
         REPD(x, image.w())temp.colorF(y, x, image.colorF(x, y));
      }else
      {
         REPD(y, image.h())
         REPD(x, image.w())temp.color(y, x, image.color(x, y));
      }
      Swap(temp, image);
   }else
   if(param.name=="mirrorX")image.mirrorX();else
   if(param.name=="mirrorY")image.mirrorY();else
   if(param.name=="normalize")image.normalize(true, true, true, true, &box);else
   if(param.name=="sat")
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.sat();
         image.color3DF(x, y, z, c);
      }
   }else
   if(param.name=="blur")
   {
      UNIT_TYPE unit=GetUnitType(param.value);
      Vec       r   =TextVecEx  (param.value);
      r.x=ConvertUnitType(r.x, image.w(), unit);
      r.y=ConvertUnitType(r.y, image.h(), unit);
      r.z=ConvertUnitType(r.z, image.d(), unit);
      if(box.min.allZero() && box.max==image.size3())image.blur(r, clamp);else
      {
         Image temp; image.blur(temp, r, clamp);
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)image.color3DF(x, y, z, temp.color3DF(x, y, z));
      }
   }else
   if(param.name=="sharpen")
   {
      Memc<Str> c; Split(c, param.value, ',');
      if(c.elms()>=1 && c.elms()<=2)
      {
         flt power=TextFlt(c[0]);
         flt range=((c.elms()>=2) ? TextFlt(c[1]) : 1);
         if(box.min.allZero() && box.max==image.size3())image.sharpen(power, range, clamp);else
         {
            Image temp; image.copyTry(temp); temp.sharpen(power, range, clamp);
            for(int z=box.min.z; z<box.max.z; z++)
            for(int y=box.min.y; y<box.max.y; y++)
            for(int x=box.min.x; x<box.max.x; x++)image.color3DF(x, y, z, temp.color3DF(x, y, z));
         }
      }
   }else
   if(param.name=="lerpRGB")
   {
      Memc<Str> c; Split(c, param.value, ',');
      switch(c.elms())
      {
         case 2: {Vec2 ma=LerpToMad(TextFlt(c[0]), TextFlt(c[1])); image.mulAdd(Vec4(Vec(ma.x), 1), Vec4(Vec(ma.y), 0), &box);} break;
         case 6: {Vec2 ma[3]={LerpToMad(TextFlt(c[0]), TextFlt(c[3])), LerpToMad(TextFlt(c[1]), TextFlt(c[4])), LerpToMad(TextFlt(c[2]), TextFlt(c[5]))}; image.mulAdd(Vec4(ma[0].x, ma[1].x, ma[2].x, 1), Vec4(ma[0].y, ma[1].y, ma[2].y, 0), &box);} break;
      }
   }else
   if(param.name=="iLerpRGB")
   {
      Memc<Str> c; Split(c, param.value, ',');
      switch(c.elms())
      {
         case 2: {Vec2 ma=ILerpToMad(TextFlt(c[0]), TextFlt(c[1])); image.mulAdd(Vec4(Vec(ma.x), 1), Vec4(Vec(ma.y), 0), &box);} break;
         case 6: {Vec2 ma[3]={ILerpToMad(TextFlt(c[0]), TextFlt(c[3])), ILerpToMad(TextFlt(c[1]), TextFlt(c[4])), ILerpToMad(TextFlt(c[2]), TextFlt(c[5]))}; image.mulAdd(Vec4(ma[0].x, ma[1].x, ma[2].x, 1), Vec4(ma[0].y, ma[1].y, ma[2].y, 0), &box);} break;
      }
   }else
   if(param.name=="mulA"  ){flt alpha=param.asFlt(); if(alpha!=1){AdjustImage(image, true, false); image.mulAdd(Vec4(1, 1, 1, alpha), 0, &box);}}else
   if(param.name=="mulRGB")image.mulAdd(Vec4(TextVecEx(param.value), 1), 0, &box);else
   if(param.name=="addRGB")image.mulAdd(1, Vec4(TextVecEx(param.value), 0), &box);else
   if(param.name=="mulAddRGB")
   {
      Memc<Str> c; Split(c, param.value, ',');
      switch(c.elms())
      {
         case 2: image.mulAdd(Vec4(Vec(TextFlt(c[0])), 1), Vec4(Vec(TextFlt(c[1])), 0), &box); break;
         case 6: image.mulAdd(Vec4(TextFlt(c[0]), TextFlt(c[1]), TextFlt(c[2]), 1), Vec4(TextFlt(c[3]), TextFlt(c[4]), TextFlt(c[5]), 0), &box); break;
      }
   }else
   if(param.name=="addMulRGB")
   {
      Memc<Str> c; Split(c, param.value, ',');
      switch(c.elms())
      {
         // x=x*m+a, x=(x+A)*M
         case 2: {flt add=TextFlt(c[0]), mul=TextFlt(c[1]);                                                               image.mulAdd(Vec4(Vec(mul), 1), Vec4(Vec(add*mul), 0), &box);} break;
         case 6: {Vec add(TextFlt(c[0]), TextFlt(c[1]), TextFlt(c[2])), mul(TextFlt(c[3]), TextFlt(c[4]), TextFlt(c[5])); image.mulAdd(Vec4(    mul , 1), Vec4(    add*mul , 0), &box);} break;
      }
   }else
   if(param.name=="mulRGBS")
   {
      Vec mul=TextVecEx(param.value);
      if(mul!=VecOne)
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         flt  sat=RgbToHsb(c.xyz).y;
         c.x=Lerp(c.x, c.x*mul.x, sat);  // red
         c.y=Lerp(c.y, c.y*mul.y, sat);  // green
         c.z=Lerp(c.z, c.z*mul.z, sat);  // blue
         image.color3DF(x, y, z, c);
      }
   }else
   if(param.name=="mulRGBH")
   {
      Mems<Str> vals; Split(vals, param.value, ',');
      switch(vals.elms())
      {
         case 1: {flt v=TextFlt(vals[0]); MulRGBH(image, v, v, v, v, v, v, box);} break;
         case 3: MulRGBH(image, TextFlt(vals[0]), 1, TextFlt(vals[1]), 1, TextFlt(vals[2]), 1, box); break;
         case 6: MulRGBH(image, TextFlt(vals[0]), TextFlt(vals[1]), TextFlt(vals[2]), TextFlt(vals[3]), TextFlt(vals[4]), TextFlt(vals[5]), box); break;
      }
   }else
   if(param.name=="mulRGBHS")
   {
      Mems<Str> vals; Split(vals, param.value, ',');
      switch(vals.elms())
      {
         case 1: {flt v=TextFlt(vals[0]); MulRGBHS(image, v, v, v, v, v, v, box);} break;
         case 3: MulRGBHS(image, TextFlt(vals[0]), 1, TextFlt(vals[1]), 1, TextFlt(vals[2]), 1, box); break;
         case 6: MulRGBHS(image, TextFlt(vals[0]), TextFlt(vals[1]), TextFlt(vals[2]), TextFlt(vals[3]), TextFlt(vals[4]), TextFlt(vals[5]), box); break;
      }
   }else
   if(param.name=="gamma")
   {
      Vec g=TextVecEx(param.value);
      if(g!=VecOne)
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); c.xyz.set(PowMax(c.x, g.x), PowMax(c.y, g.y), PowMax(c.z, g.z)); image.color3DF(x, y, z, c);}
   }else
   if(param.name=="gammaLum")
   {
      flt g=param.asFlt();
      if(g!=1)
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++){Vec4 c=image.color3DF(x, y, z); if(flt lum=c.xyz.max()){c.xyz*=PowMax(lum, g)/lum; image.color3DF(x, y, z, c);}}
   }else
   if(param.name=="SRGBToLinear")
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)image.color3DF(x, y, z, SRGBToLinear(image.color3DF(x, y, z)));
   }else
   if(param.name=="LinearToSRGB")
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)image.color3DF(x, y, z, LinearToSRGB(image.color3DF(x, y, z)));
   }else
   if(param.name=="brightness")
   {
      Vec b=TextVecEx(param.value), mul; if(b.any())
      {
         flt (*R)(flt);
         flt (*G)(flt);
         flt (*B)(flt);
         if(!b.x){b.x=1; mul.x=1; R=FloatSelf;}else if(b.x<0){b.x=SigmoidSqrt(b.x); mul.x=1/SigmoidSqrtInv(b.x); R=SigmoidSqrtInv;}else{mul.x=1/SigmoidSqrt(b.x); R=SigmoidSqrt;}
         if(!b.y){b.y=1; mul.y=1; G=FloatSelf;}else if(b.y<0){b.y=SigmoidSqrt(b.y); mul.y=1/SigmoidSqrtInv(b.y); G=SigmoidSqrtInv;}else{mul.y=1/SigmoidSqrt(b.y); G=SigmoidSqrt;}
         if(!b.z){b.z=1; mul.z=1; B=FloatSelf;}else if(b.z<0){b.z=SigmoidSqrt(b.z); mul.z=1/SigmoidSqrtInv(b.z); B=SigmoidSqrtInv;}else{mul.z=1/SigmoidSqrt(b.z); B=SigmoidSqrt;}
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=Sqr(c.xyz);
            c.x=R(c.x*b.x)*mul.x;
            c.y=G(c.y*b.y)*mul.y;
            c.z=B(c.z*b.z)*mul.z;
            c.xyz=Sqrt(c.xyz);
            image.color3DF(x, y, z, c);
         }
      }
   }else
   if(param.name=="brightnessLum")
   {
      flt b=param.asFlt(), mul; flt (*f)(flt);
      if(b)
      {
         if(b<0){b=SigmoidSqrt(b); mul=1/SigmoidSqrtInv(b); f=SigmoidSqrtInv;}else{mul=1/SigmoidSqrt(b); f=SigmoidSqrt;}
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            if(flt l=c.xyz.max())
            {
               flt new_lum=Sqr(l);
               new_lum=f(new_lum*b)*mul;
               new_lum=Sqrt(new_lum);
               c.xyz*=new_lum/l;
               image.color3DF(x, y, z, c);
            }
         }
      }
   }else
   if(param.name=="contrast")
   {
      Vec contrast=TextVecEx(param.value); if(contrast!=VecOne)
      {
         Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box))
         {
            // col=(col-avg)*contrast+avg
            // col=col*contrast+avg*(1-contrast)
            image.mulAdd(Vec4(contrast, 1), Vec4(avg.xyz*(Vec(1)-contrast), 0), &box);
         }
      }
   }else
   if(param.name=="contrastAlphaWeight")
   {
      Vec contrast=TextVecEx(param.value); if(contrast!=VecOne)
      {
         Vec avg; if(image.stats(null, null, null, null, null, &avg, &box))
         {
            image.mulAdd(Vec4(contrast, 1), Vec4(avg*(Vec(1)-contrast), 0), &box);
         }
      }
   }else
   if(param.name=="contrastLum")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box))ContrastLum(image, contrast, avg.xyz.max(), box);
      }
   }else
   if(param.name=="contrastLumAlphaWeight")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec avg; if(image.stats(null, null, null, null, null, &avg, &box))ContrastLum(image, contrast, avg.max(), box);
      }
   }else
   if(param.name=="contrastHue")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box))ContrastHue(image, contrast, avg.xyz, box);
      }
   }else
   if(param.name=="medContrastHue")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec4 med; if(image.stats(null, null, null, &med, null, null, &box))ContrastHue(image, contrast, med.xyz, box);
      }
   }else
   if(param.name=="contrastHueAlphaWeight")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec avg; if(image.stats(null, null, null, null, null, &avg, &box))ContrastHue(image, contrast, avg, box);
      }
   }else
   if(param.name=="contrastHuePow")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box) && image.lock())
         {
            flt avg_hue=RgbToHsb(avg.xyz).x;
            for(int z=box.min.z; z<box.max.z; z++)
            for(int y=box.min.y; y<box.max.y; y++)
            for(int x=box.min.x; x<box.max.x; x++)
            {
               Vec4 c=image.color3DF(x, y, z);
               c.xyz=RgbToHsb(c.xyz);
               flt d_hue=HueDelta(avg_hue, c.x);
               d_hue=Sign(d_hue)*Pow(Abs(d_hue)*2, contrast)/2; // *2 to get -1..1 range
               Clamp(d_hue, -0.5, 0.5); // clamp so we don't go back
               c.x=d_hue+avg_hue;
               c.xyz=HsbToRgb(c.xyz);
               image.color3DF(x, y, z, c);
            }
            image.unlock();
         }
      }
   }else
   if(param.name=="contrastSat")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         flt avg; if(image.statsSat(null, null, &avg, null, null, null, &box))ContrastSat(image, contrast, avg, box);
      }
   }else
   if(param.name=="medContrastSat")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         flt med; if(image.statsSat(null, null, null, &med, null, null, &box))ContrastSat(image, contrast, med, box);
      }
   }else
   if(param.name=="contrastSatAlphaWeight")
   {
      flt contrast=param.asFlt(); if(contrast!=1)
      {
         flt avg; if(image.statsSat(null, null, null, null, null, &avg, &box))ContrastSat(image, contrast, avg, box);
      }
   }else
   if(param.name=="avgLum")
   {
      Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box))if(flt avg_l=avg.xyz.max())image.mulAdd(Vec4(Vec(param.asFlt()/avg_l), 1), 0, &box);
   }else
   if(param.name=="medLum")
   {
      Vec4 med; if(image.stats(null, null, null, &med, null, null, &box))if(flt med_l=med.xyz.max())image.mulAdd(Vec4(Vec(param.asFlt()/med_l), 1), 0, &box);
   }else
   if(param.name=="avgContrastLum")
   {
      Vec4 avg; if(image.stats(null, null, &avg, null, null, null, &box))AvgContrastLum(image, param.asFlt(), avg.xyz.max(), box);
   }else
   if(param.name=="medContrastLum")
   {
      Vec4 med; if(image.stats(null, null, null, &med, null, null, &box))AvgContrastLum(image, param.asFlt(), med.xyz.max(), box);
   }else
   if(param.name=="addHue")AddHue(image, param.asFlt(), box);else
   if(param.name=="avgHue")
   {
      if(image.lock()) // lock for writing because we will use this lock for applying hue too
      {
         Vec4 col; if(image.stats(null, null, &col, null, null, null, &box))AddHue(image, HueDelta(RgbToHsb(col.xyz).x, param.asFlt()), box);
         image.unlock();
      }
   }else
   if(param.name=="medHue")
   {
      if(image.lock()) // lock for writing because we will use this lock for applying hue too
      {
         Vec4 col; if(image.stats(null, null, null, &col, null, null, &box))AddHue(image, HueDelta(RgbToHsb(col.xyz).x, param.asFlt()), box);
         image.unlock();
      }
   }else
   if(param.name=="addSat")
   {
      flt sat=param.asFlt(); if(sat && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=RgbToHsb(c.xyz);
            c.y+=sat;
            c.xyz=HsbToRgb(c.xyz);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="mulSat")
   {
      MulSat(image, param.asFlt(), box);
   }else
   if(param.name=="avgSat")
   {
      flt avg; if(image.statsSat(null, null, &avg, null, null, null, &box))if(avg)MulSat(image, param.asFlt()/avg, box);
   }else
   if(param.name=="medSat")
   {
      flt med; if(image.statsSat(null, null, null, &med, null, null, &box))if(med)MulSat(image, param.asFlt()/med, box);
   }else
   if(param.name=="mulSatPhoto")
   {
      flt sat=param.asFlt(); if(sat!=1 && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            flt  lin_lum=LinearLumOfSRGBColor(c.xyz);

            c.xyz=RgbToHsb(c.xyz);
            c.y*=sat;
            c.xyz=HsbToRgb(c.xyz);

            c.xyz=SRGBToLinear(c.xyz);
            if(flt cur_lin_lum=LinearLumOfLinearColor(c.xyz))c.xyz*=lin_lum/cur_lin_lum;
            c.xyz=LinearToSRGB(c.xyz);

            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="mulSatH"
   || param.name=="mulSatHS")
   {
      bool sat=(param.name=="mulSatHS");
      Mems<Str> vals; Split(vals, param.value, ',');
      switch(vals.elms())
      {
         case 1: {flt v=TextFlt(vals[0]); MulSatH(image, v, v, v, v, v, v, sat, false, box);} break;
         case 3: MulSatH(image, TextFlt(vals[0]), 1, TextFlt(vals[1]), 1, TextFlt(vals[2]), 1, sat, false, box); break;
         case 6: MulSatH(image, TextFlt(vals[0]), TextFlt(vals[1]), TextFlt(vals[2]), TextFlt(vals[3]), TextFlt(vals[4]), TextFlt(vals[5]), sat, false, box); break;
      }
   }else
   if(param.name=="mulSatHPhoto"
   || param.name=="mulSatHSPhoto")
   {
      bool sat=(param.name=="mulSatHSPhoto");
      Mems<Str> vals; Split(vals, param.value, ',');
      switch(vals.elms())
      {
         case 1: {flt v=TextFlt(vals[0]); MulSatH(image, v, v, v, v, v, v, sat, true, box);} break;
         case 3: MulSatH(image, TextFlt(vals[0]), 1, TextFlt(vals[1]), 1, TextFlt(vals[2]), 1, sat, true, box); break;
         case 6: MulSatH(image, TextFlt(vals[0]), TextFlt(vals[1]), TextFlt(vals[2]), TextFlt(vals[3]), TextFlt(vals[4]), TextFlt(vals[5]), sat, true, box); break;
      }
   }else
   if(param.name=="addHueSat")
   {
      Vec2 hue_sat=param.asVec2(); if(hue_sat.any() && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=RgbToHsb(c.xyz);
            c.xy+=hue_sat;
            c.xyz=HsbToRgb(c.xyz);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="setHue")
   {
      if(image.lock())
      {
         flt hue=param.asFlt();
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=RgbToHsb(c.xyz);
            c.x=hue;
            c.xyz=HsbToRgb(c.xyz);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="setHueSat")
   {
      if(image.lock())
      {
         Vec2 hue_sat=param.asVec2();
         Vec  rgb=HsbToRgb(Vec(hue_sat, 1));
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=rgb*c.xyz.max();
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="setHueSatPhoto") // photometric
   {
      if(image.lock())
      {
         Vec2 hue_sat=param.asVec2();
         Vec  rgb=HsbToRgb(Vec(hue_sat, 1));
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=rgb*SRGBLumOfSRGBColor(c.xyz);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="lerpHue")
   {
      Vec2 hue_alpha=param.asVec2(); if(hue_alpha.y && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            Vec hsb=RgbToHsb(c.xyz);
            hsb.x=hue_alpha.x;
            c.xyz=Lerp(c.xyz, HsbToRgb(hsb), hue_alpha.y);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="lerpHueSat")
   {
      Vec hue_sat_alpha=param.asVec(); if(hue_sat_alpha.z && image.lock())
      {
         Vec rgb=HsbToRgb(Vec(hue_sat_alpha.xy, 1));
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=Lerp(c.xyz, rgb*c.xyz.max(), hue_sat_alpha.z);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="rollHue")
   {
      Vec2 hue_alpha=param.asVec2(); if(hue_alpha.y && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            Vec hsb=RgbToHsb(c.xyz);
            hsb.x+=HueDelta(hsb.x, hue_alpha.x)*hue_alpha.y;
            c.xyz=HsbToRgb(hsb);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="rollHueSat")
   {
      Vec hue_sat_alpha=param.asVec(); if(hue_sat_alpha.z && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            Vec hsb=RgbToHsb(c.xyz);
            hsb.x+=HueDelta(hsb.x, hue_sat_alpha.x)*hue_sat_alpha.z;
            hsb.y =Lerp    (hsb.y, hue_sat_alpha.y, hue_sat_alpha.z);
            c.xyz=HsbToRgb(hsb);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="rollHuePhoto")
   {
      Vec2 hue_alpha=param.asVec2(); if(hue_alpha.y && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            if(flt l=SRGBLumOfSRGBColor(c.xyz))
            {
               Vec hsb=RgbToHsb(c.xyz);
               hsb.x+=HueDelta(hsb.x, hue_alpha.x)*hue_alpha.y;
               c.xyz=HsbToRgb(hsb);
               c.xyz*=l/SRGBLumOfSRGBColor(c.xyz);
               image.color3DF(x, y, z, c);
            }
         }
         image.unlock();
      }
   }else
   if(param.name=="rollHueSatPhoto")
   {
      Vec hue_sat_alpha=param.asVec(); if(hue_sat_alpha.z && image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            if(flt l=SRGBLumOfSRGBColor(c.xyz))
            {
               Vec hsb=RgbToHsb(c.xyz);
               hsb.x+=HueDelta(hsb.x, hue_sat_alpha.x)*hue_sat_alpha.z;
               hsb.y =Lerp    (hsb.y, hue_sat_alpha.y, hue_sat_alpha.z);
               c.xyz=HsbToRgb(hsb);
               c.xyz*=l/SRGBLumOfSRGBColor(c.xyz);
               image.color3DF(x, y, z, c);
            }
         }
         image.unlock();
      }
   }else
   if(param.name=="grey")
   {
      if(image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=c.xyz.max();
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="greyPhoto")
   {
      if(image.lock())
      {
         for(int z=box.min.z; z<box.max.z; z++)
         for(int y=box.min.y; y<box.max.y; y++)
         for(int x=box.min.x; x<box.max.x; x++)
         {
            Vec4 c=image.color3DF(x, y, z);
            c.xyz=SRGBLumOfSRGBColor(c.xyz);
            image.color3DF(x, y, z, c);
         }
         image.unlock();
      }
   }else
   if(param.name=="min")
   {
      Vec min=TextVecEx(param.value);
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=Min(c.xyz, min);
         image.color3DF(x, y, z, c);
      }
   }else
   if(param.name=="max")
   {
      Vec max=TextVecEx(param.value);
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.xyz=Max(c.xyz, max);
         image.color3DF(x, y, z, c);
      }
   }else
   if(param.name=="metalToReflect")
   {
      for(int z=box.min.z; z<box.max.z; z++)
      for(int y=box.min.y; y<box.max.y; y++)
      for(int x=box.min.x; x<box.max.x; x++)
      {
         Vec4 c=image.color3DF(x, y, z);
         c.x=Lerp(0.04, 1, c.x);
         c.y=Lerp(0.04, 1, c.y);
         c.z=Lerp(0.04, 1, c.z);
         image.color3DF(x, y, z, c);
      }
   }else
   if(param.name=="channel") // Warning: this loses sRGB for 1..2 channels, because there are no IMAGE_R8_SRGB, IMAGE_R8G8_SRGB, IMAGE_F32_SRGB, IMAGE_F32_2_SRGB
   {
      int channels=param.value.length();
      if( channels>=1 && channels<=4)
      {
         int   chn[4]; REPAO(chn)=ChannelIndex(param.value[i]);
         Image temp;
         bool  srgb=image.sRGB();
         if(image.highPrecision())
         {
            temp.createSoftTry(image.w(), image.h(), image.d(), channels==1 ? IMAGE_F32 : channels==2 ? IMAGE_F32_2 : channels==3 ? (srgb ? IMAGE_F32_3_SRGB : IMAGE_F32_3) : (srgb ? IMAGE_F32_4_SRGB : IMAGE_F32_4));
            Vec4 d(0, 0, 0, 1);
            REPD(z, image.d())
            REPD(y, image.h())
            REPD(x, image.w())
            {
               Vec4 c=image.color3DF(x, y, z);
               REPA(d.c){int ch=chn[i]; if(InRange(ch, c.c))d.c[i]=c.c[ch];}
               temp.color3DF(x, y, z, d);
            }
         }else
         {
            temp.createSoftTry(image.w(), image.h(), image.d(), channels==1 ? IMAGE_R8 : channels==2 ? IMAGE_R8G8 : channels==3 ? (srgb ? IMAGE_R8G8B8_SRGB : IMAGE_R8G8B8) : (srgb ? IMAGE_R8G8B8A8_SRGB : IMAGE_R8G8B8A8));
            Color d(0, 0, 0, 255);
            REPD(z, image.d())
            REPD(y, image.h())
            REPD(x, image.w())
            {
               Color c=image.color3D(x, y, z);
               REPA(d.c){int ch=chn[i]; if(InRange(ch, c.c))d.c[i]=c.c[ch];}
               temp.color3D(x, y, z, d);
            }
         }
         Swap(temp, image);
      }
   }else
   if(param.name=="alphaFromBrightness" || param.name=="alphaFromLum" || param.name=="alphaFromLuminance")
   {
      image.alphaFromBrightness();
   }else
   if(param.name=="bump")
   {
      Vec2 blur=-1; // x=min, y=max, -1=auto
      if(param.value.is())
      {
         UNIT_TYPE unit=GetUnitType(param.value);
         flt       full=image.size().avgF();
         if(Contains(param.value, ','))
         {
            blur=param.asVec2(); // use 2 values if specified
            blur.x=ConvertUnitType(blur.x, full, unit);
            blur.y=ConvertUnitType(blur.y, full, unit);
         }else
         {
            blur.y=param.asFlt(); // if 1 value specified then use as max
            blur.y=ConvertUnitType(blur.y, full, unit);
         }
      }
      CreateBumpFromColor(image, image, blur.x, blur.y);
   }else
   if(param.name=="bumpToNormal")
   {
      image.bumpToNormal(image, param.value.is() ? param.asFlt() : image.size().avgF()*BUMP_TO_NORMAL_SCALE);
   }else
   if(param.name=="scale") // the formula is ok (for normal too), it works as if the bump was scaled vertically by 'scale' factor
   {
      flt scale=param.asFlt(); if(scale!=1)
      {
         if(image.typeChannels()<=1 || image.monochromatic())image.mulAdd(Vec4(Vec(scale), 1), Vec4(Vec(-0.5*scale+0.5), 0), &box);else // if image is 1-channel or monochromatic then we need to transform all RGB together
         if(!scale                                          )image.mulAdd(Vec4(Vec(    0), 1), Vec4(        0.5, 0.5, 1, 0), &box);else // if zero scale then set Vec(0.5, 0.5, 1)
         if(image.lock())
         {
            scale=1/scale;
            for(int z=box.min.z; z<box.max.z; z++)
            for(int y=box.min.y; y<box.max.y; y++)
            for(int x=box.min.x; x<box.max.x; x++)
            {
               Vec4 c=image.color3DF(x, y, z); Vec &n=c.xyz;
               n=n*2-1;
               n.normalize();
               n.z*=scale;
               n.normalize();
               n=n*0.5+0.5;
               image.color3DF(x, y, z, c);
            }
            image.unlock();
         }
      }
   }else
   if(param.name=="scaleXY")
   {
      Vec2 r=TextVec2Ex(param.value);
      // v2=(v2-0.5)*r+0.5
      // v2=v2*r-0.5*r+0.5
      if(image.typeChannels()<=1 || image.monochromatic()){flt a=r.avg(); image.mulAdd(Vec4(Vec(a), 1), Vec4(Vec(-0.5*a+0.5)  , 0), &box);} // if image is 1-channel or monochromatic then we need to transform all RGB together
      else                                                                image.mulAdd(Vec4(  r, 1, 1), Vec4(    -0.5*r+0.5, 0, 0), &box);
   }else
   if(param.name=="fixTransparent")
   {
      image.transparentToNeighbor(true, param.value.is() ? param.asFlt() : 1);
   }
}
void TransformImage(Image &image, C MemPtr<TextParam> &params, bool clamp)
{
   FREPA(params)TransformImage(image, params[i], clamp); // process in order
}
/******************************************************************************/
// TEXT
/******************************************************************************/
bool ValidChar(char c) {return c>=32 && c<128;}
bool ValidText(C Str &text, int min=1, int max=-1)
{
   if(text.length()>=min && (text.length()<=max || max<0))
   {
      REPA(text)if(!ValidChar(text[i]))return false;
      return true;
   }
   return false;
}
bool ValidFileName(C Str &name) {return name.length()>=1 && CleanFileName(name)==name;}
bool ValidFileNameForUpload(C Str &name, int max=128)
{
   if(ValidFileName(name) && ValidText(name, 1, max))
   {
      REPA(name)if(name[i]=='@' || name[i]=='~')return false; // because of "@new" and "~" is special char on unix ?
      return true;
   }
   return false;
}
bool ValidPass(C Str &pass) {return ValidText(pass, 4, 16) && !HasUnicode(pass);}
bool ValidEnum(C Str &name)
{
   if(!name.is())return false;
   FREPA(name)
   {
      uint flag=CharFlag(name[i]);
      bool ok=((flag&(CHARF_ALPHA|CHARF_UNDER)) || (i && (flag&CHARF_DIG))); // digit cannot be used as first character
      if(! ok)return false;
   }
   return true;
}
bool ValidSupport(C Str &support)
{
   return !support.is() || ValidEmail(support) || ValidURL(support);
}
bool ValidVideo(C Str &video)
{
   return !video.is() || (ValidURL(video) && (StartsPath(video, "https://www.youtube.com/embed")));
}
Str YouTubeEmbedToFull(C Str &video) {return Replace(video, "/embed/", "/watch?v=");}
Str YouTubeFullToEmbed(C Str &video) {return Replace(video, "/watch?v=", "/embed/");}
UID PassToMD5(C Str &pass) {return MD5Mem((Str8)CaseDown(pass), pass.length());}
Str NameToEnum(C Str &name)
{
   Str e; FREPA(name)
   {
      char c=name[i];
      if(c==' ')c='_';
      if(c>='0' && c<='9'
      || c>='a' && c<='z'
      || c>='A' && c<='Z'
      || c=='_')e+=c;
   }
   return e;
}
Str TimeAgo(C DateTime &date) {DateTime now; now.getUTC(); return TimeText(now-date, TIME_NAME_MED)+" ago";}
char CountS(int n) {return (n==1) ? '\0' : 's';}
Str  Plural(Str name) // convert to plural name
{
   bool case_up=Equal(CaseUp(name), name, true);
   char last   =CaseDown(name.last());
   if(name=="child"                      )name+="ren"               ;else // child  -> children
   if(name=="potato"                     )name+="es"                ;else // potato -> potatoes
   if(name=="hero"                       )name+="es"                ;else // hero   -> heroes
   if(name=="mouse"                      )name.remove(1, 4)+="ice"  ;else // mouse  -> mice
   if(name=="man"                        )name.remove(1, 2)+="en"   ;else // man    -> men
   if(name=="woman"                      )name.remove(3, 2)+="en"   ;else // woman  -> women
   if(name=="goose"                      )name.remove(1, 4)+="eese" ;else // goose  -> geese
   if(name=="person"                     )name.remove(1, 5)+="eople";else // person -> people
   if(last=='y'                          )name.removeLast()+="ies"  ;else // body   -> bodies
   if(last=='x' || last=='h' || last=='s')name+="es"                ;else // box    -> boxes, mesh -> meshes, bus -> buses
   if(last=='f')
   {
      if(name!="dwarf" && name!="roof")name.removeLast()+="ves"; // leaf -> leaves, elf -> elves (dwarf -> dwarfs, roof -> roofs)
   }else
   if(Ends(name, "fe"))name.removeLast().removeLast()+="ves";else // life -> lives, knife -> knives
      name+='s';
   return case_up ? CaseUp(name) : name;
}
/******************************************************************************/
int Occurrences(C Str &s, char c)
{
   int o=0; REPA(s)if(s[i]==c)o++; return o;
}
/******************************************************************************/
Str VecI2AsText(C VecI2 &v) // try to keep as one value if XY are the same
{
   Str s; s=v.x; if(v.y!=v.x)s+=S+","+v.y; 
   return s;
}
VecI2 TextVecI2Ex(cchar *t)
{
   return Contains(t, ',') ? TextVecI2(t) : VecI2(TextInt(t));
}
Vec2 TextVec2Ex(cchar *t)
{
   return Contains(t, ',') ? TextVec2(t) : Vec2(TextFlt(t));
}
Vec TextVecEx(cchar *t)
{
   return Contains(t, ',') ? TextVec(t) : Vec(TextFlt(t));
}
Str TextVecEx(C Vec &v, int precision=-3)
{
   return (Equal(v.x, v.y) && Equal(v.x, v.z)) ? TextReal(v.x, precision) : v.asText(precision);
}
/******************************************************************************/
Str RelativePath  (C Str &path) {return SkipStartPath(path, GetPath(App.exe()));}
Str EditToGamePath(  Str  path)
{
   Str out;
   for(path.tailSlash(false); ; )
   {
      Str base=GetBase(path); path=GetPath(path);
      if(!base.is()   )return path.tailSlash(true)+out; // needed for "/xxx" unix style paths
      if( base=="Edit")return path.tailSlash(true)+"Game\\"+out; // replace "edit" with "game"
      out=(out.is() ? base.tailSlash(true)+out : base);
   }
}
/******************************************************************************/
cchar8 *FormatSuffixes[]=
{
   "_BC1",
   "_BC3", // for Web
   "_ETC2_R",
   "_ETC2_RG",
   "_ETC2_RGB",
   "_ETC2_RGBA",
   "_PVRTC1_2",
   "_PVRTC1_4",
   "_SIMPLE", // used for simplified Materials
};  int FormatSuffixElms=Elms(FormatSuffixes);
cchar8* FormatSuffixSimple() {return "_SIMPLE";}
cchar8* FormatSuffix(IMAGE_TYPE type)
{
   switch(type)
   {
      default: return null;

      // we can return the same suffix for non-sRGB and sRGB, unsigned and signed, because depending on sRGB/signed they will already have different hash
      case IMAGE_BC1: case IMAGE_BC1_SRGB: return "_BC1";
      case IMAGE_BC3: case IMAGE_BC3_SRGB: return "_BC3";

      case IMAGE_ETC2_R   : case IMAGE_ETC2_R_SIGN   : return "_ETC2_R";
      case IMAGE_ETC2_RG  : case IMAGE_ETC2_RG_SIGN  : return "_ETC2_RG";
      case IMAGE_ETC2_RGB : case IMAGE_ETC2_RGB_SRGB : return "_ETC2_RGB";
      case IMAGE_ETC2_RGBA: case IMAGE_ETC2_RGBA_SRGB: return "_ETC2_RGBA";

      case IMAGE_PVRTC1_2: case IMAGE_PVRTC1_2_SRGB: return "_PVRTC1_2";
      case IMAGE_PVRTC1_4: case IMAGE_PVRTC1_4_SRGB: return "_PVRTC1_4";
   }
}
Str8 ImageDownSizeSuffix(int size)
{
   if(size>0 && size<INT_MAX)return S+"_"+size;
   return S;
}
/******************************************************************************/
TextParam* FindTransform(MemPtr<FileParams> files, C Str &name) // this ignores partial(non full size) transforms
{
   REPA(files) // go from end
   {
      FileParams &file=files[i];
      if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
      REPA(file.params) // go from end
      {
         TextParam &p=file.params[i]; if(p.name==name && !PartialTransform(p))return &p;
      }
   }
   return null;
}
void DelTransform(MemPtr<FileParams> files, C Str &name) // this ignores partial(non full size) transforms 
{
   REPA(files) // go from end
   {
      FileParams &file=files[i];
      if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
      REPAD(pi, file.params) // go from end
      {
         TextParam &p=file.params[pi]; if(p.name==name && !PartialTransform(p))
         {
                    file.params.remove(pi, true);
            if(!file.is())files.remove( i, true); // if nothing left then remove it
            return;
         }
      }
   }
}
void SetTransform(MemPtr<FileParams> files, C Str &name, C Str &value=S) // this ignores partial(non full size) transforms 
{
   if(files.elms()) // set only if we have something (to ignore setting for completely empty)
   {
      if(files.elms()>1 && files.last().name.is())files.New(); // if we have more than one image, then we need to make sure that we add the parameter not to one of the images, but as last file that has no name specified to set transform for everything
      TextParam *p;
      REPA(files) // go from end
      {
         FileParams &file=files[i];
         if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
         REPA(file.params) // go from end
         {
            p=&file.params[i]; if(p.name==name && !PartialTransform(*p))goto found;
         }
      }
      p=&files.last().params.New().setName(name); // if didn't found then create a new one
   found:
      p.setValue(value);
   }
}
void SetResizeTransform(MemPtr<FileParams> files, C Str &name, C Str &value=S) // this ignores partial(non full size) transforms 
{
   if(files.elms()) // set only if we have something (to ignore setting for completely empty)
   {
      if(files.elms()>1 && files.last().name.is())files.New(); // if we have more than one image, then we need to make sure that we add the parameter not to one of the images, but as last file that has no name specified to set transform for everything
      TextParam *p;
      REPA(files) // go from end
      {
         FileParams &file=files[i];
         if(i && file.name.is())break; // stop on first file that has name (but allow the first which means there's only one file) so we don't process transforms for only 1 of multiple images
         REPA(file.params) // go from end
         {
            p=&file.params[i];
            if(p.name==name && !PartialTransform(*p))goto found;
            if(SizeDependentTransform(*p))goto need_new; // if encountered a size-dependent transform then it means we can't change any resize transforms before that, but need to create a new one
         }
      }
   need_new:
      p=&files.last().params.New().setName(name); // if didn't found then create a new one
   found:
      p.setValue(value);
   }
}
void SetTransform(Str &file, C Str &name, C Str &value=S)
{
   Mems<FileParams> files=FileParams.Decode(file);
   SetTransform(files, name, value);
   file=FileParams.Encode(files);
}
/******************************************************************************/
SOUND_CODEC TextSoundCodec(C Str &t)
{
   if(t=="raw"
   || t=="wav"
   || t=="uncompressed")return SOUND_WAV;
   if(t=="vorbis"      )return SOUND_SND_VORBIS;
   if(t=="opus"        )return SOUND_SND_OPUS;
                        return SOUND_NONE;
}
/******************************************************************************/
// MESH
/******************************************************************************/
int VisibleVtxs      (C MeshLod &mesh) {int num =0; REPA(mesh)if(!(mesh.parts[i].part_flag&MSHP_HIDDEN))num +=mesh.parts[i].vtxs           (); return num ;}
int VisibleTris      (C MeshLod &mesh) {int num =0; REPA(mesh)if(!(mesh.parts[i].part_flag&MSHP_HIDDEN))num +=mesh.parts[i].tris           (); return num ;}
int VisibleTrisTotal (C MeshLod &mesh) {int num =0; REPA(mesh)if(!(mesh.parts[i].part_flag&MSHP_HIDDEN))num +=mesh.parts[i].trisTotal      (); return num ;}
int VisibleQuads     (C MeshLod &mesh) {int num =0; REPA(mesh)if(!(mesh.parts[i].part_flag&MSHP_HIDDEN))num +=mesh.parts[i].quads          (); return num ;}
int VisibleSize      (C MeshLod &mesh) {int size=0; REPA(mesh)if(!(mesh.parts[i].part_flag&MSHP_HIDDEN))size+=mesh.parts[i].render.memUsage(); return size;}
flt VisibleLodQuality(C Mesh    &mesh, int lod_index)
{
   Clamp(lod_index, 0, mesh.lods());
 C MeshLod &base=mesh,
           &lod =mesh.lod(lod_index);
   Int      v   =VisibleVtxs     (base),
            f   =VisibleTrisTotal(base);
   return Avg(v ? flt(VisibleVtxs     (lod))/v : 1,
              f ? flt(VisibleTrisTotal(lod))/f : 1);
}
/******************************************************************************/
void KeepParams(C Mesh &src, Mesh &dest)
{
   REPAD(d, dest)
   {
      MeshPart &dest_part=dest.parts[d];
      int        src_part_i=-1;
      REPAD(s, src)
      {
       C MeshPart &src_part=src.parts[s];
         if(Equal(src_part.name, dest_part.name)) // if same name
            if(src_part_i<0 || Abs(s-d)<Abs(src_part_i-d))src_part_i=s; // if new index is closer to original index
      }
      if(InRange(src_part_i, src))
      {
       C MeshPart &src_part=src.parts[src_part_i];
         dest_part.part_flag=src_part.part_flag;
         dest_part.drawGroup(src_part.drawGroup(), src.drawGroupEnum());
         dest_part.variations(src_part.variations());
         REP(dest_part.variations())if(i)dest_part.variation(i, src_part.variation(i));
      }
   }
   dest.drawGroupEnum(src.drawGroupEnum()); // keep the same draw group enum
}
void EditToGameMesh(C Mesh &edit, Mesh &game, Skeleton *skel, Enum *draw_group, C Matrix *matrix)
{
   game.create(edit, GameMeshFlagAnd);
   // cleanup mesh
   REPD(l, game.lods()) // have to go from end because we're removing LOD's
   {
      MeshLod &lod=game.lod(l);
      // remove LOD's
      if(NegativeSB(lod.dist2) // negative distance (marked as disabled)
      || InRange(l+1, game.lods()) && (l ? lod.dist2 : 0)>=game.lod(l+1).dist2) // distance is higher than the next one (have to check next one and not previous one, because we need to delete those with negative distance first. Force 0 dist for #0 LOD because currently it's uneditable and assumed to be 0 however it may not be)
      {
      remove_lod:
         game.removeLod(l);
      }else
      {
         // remove hidden mesh parts
         REPA(lod)if(lod.parts[i].part_flag&MSHP_HIDDEN)lod.parts.remove(i);
         if(!lod.parts.elms())goto remove_lod;
      }
   }
   game.joinAll(true, true, false, MeshJoinAllTestVtxFlag, -1); // disable vtx weld, because: 1) mesh isn't scaled/transformed yet 2) it would be performed only if some parts were merged 3) there are no tangents yet. Instead let's always do it manually
   if(matrix)game.transform(*matrix); // transform before welding
   game.setAutoTanBin() // calculate tangents before welding
       .weldVtx(VTX_ALL, EPS, EPS_COL8_COS, -1).skeleton(skel).drawGroupEnum(draw_group).setBox(); // use 8-bit for vtx normals because they can't handle more anyway, always recalculate box because of transform and welding
   game.setRender().delBase();
}
/******************************************************************************/
bool HasMaterial(C MeshPart &part, C MaterialPtr &material)
{
   REP(part.variations())if(part.variation(i)==material)return true;
   return false;
}
/******************************************************************************/
bool FixVtxNrm(MeshBase &base)
{
   bool ok=false; if(base.vtx.nrm())
   {
      MeshBase temp(base, VTX_POS|FACE_IND); temp.setNormals(); // copy to a temp mesh and set its vtx normals
      if(base.vtxs()==temp.vtxs() && temp.vtx.nrm()) // safety checks
      {
         ok=true;
         REPA(base.vtx)
         {
            Vec &nrm=base.vtx.nrm(i); if(!nrm.any()) // if any 'base' vtx normal is zero
            {
               nrm=temp.vtx.nrm(i); if(!nrm.any())ok=false; // copy from 'temp'
            }
         }
      }
   }
   return ok;
}
void FixMesh(Mesh &mesh)
{
   mesh.setBase(true).delRender() // create base if empty
       .material(null).variations(0) // clear any existing materials, they will be set later according to 'mtrls'
       .skeleton(null).drawGroupEnum(null) // clear 'skeleton', clear 'drawGroupEnum'
       .removeUnusedVtxs(); // remove unused vertexes

   // check if some vtx normals are wrong
   REP(mesh.lods())
   {
      MeshLod &lod=mesh.lod(i); REPA(lod)
      {
         MeshPart &part=lod.parts[i];
         MeshBase &base=part.base;
         if(base.vtx.nrm())REPA(base.vtx)if(!base.vtx.nrm(i).any()) // all zero
         {
            if(!FixVtxNrm(base)) // if didn't fix yet, then it's possible that vtx shares only co-planar faces
            {
               base.explodeVtxs();
               FixVtxNrm(base);
               if(!base.vtx.tan() || !base.vtx.bin())base.setTanBin(); //if(!base.vtx.tan())base.setTangents(); if(!base.vtx.bin())base.setBinormals(); // set in case mesh doesn't have them yet, need to call before 'weldVtx'
               base.weldVtx(VTX_ALL, EPSD, EPS_COL_COS, -1); // use small epsilon in case mesh is scaled down, do not remove degenerate faces because they're not needed because we're doing this only because of 'explodeVtxs'
            }
            break;
         }
      }
   }
}
bool SamePartInAllLods(C Mesh &mesh, int part)
{
#if 0 // checks only 1 part
   if(InRange(part, mesh.parts))
   {
      cchar8 *name=mesh.parts[part].name;
      for(Int i=mesh.lods(); --i>=1; )
      {
       C MeshLod &lod=mesh.lod(i);
         if(!InRange(part, lod) || !Equal(name, lod.parts[part].name))return false;
      }
      return true;
   }
   return false;
#else // checks all parts
   for(Int i=mesh.lods(); --i>=1; )
   {
    C MeshLod &lod=mesh.lod(i); if(lod.parts.elms()!=mesh.parts.elms())return false;
      REPA(lod.parts)if(!Equal(lod.parts[i].name, mesh.parts[i].name))return false;
   }
   return true;
#endif
}
void SetDrawGroup(Mesh &mesh, MeshLod &lod, int part, int group, Enum *draw_group_enum)
{
   if(SamePartInAllLods(mesh, part))
   {
      REP(mesh.lods())
      {
         MeshLod &lod=mesh.lod(i);
         if(InRange(part, lod))lod.parts[part].drawGroup(group, draw_group_enum);
      }
   }else
   {
      if(InRange(part, lod))lod.parts[part].drawGroup(group, draw_group_enum);
   }
}
/******************************************************************************/
// SKELETON
/******************************************************************************/
Str BoneNeutralName(C Str &name)
{
   Str n=Replace(name, "right", CharAlpha);
       n=Replace(n   , "left" , CharAlpha);
       n.replace('r', CharBeta).replace('l', CharBeta).replace('R', CharBeta).replace('L', CharBeta);
   return n;
}
/******************************************************************************/
// OBJECT
/******************************************************************************/
// following functions are used to determine whether object should override mesh/phys
bool OverrideMeshSkel(C Mesh *mesh, C Skeleton *skel) {return (mesh && mesh->is()) || (skel && skel->is());}
bool OverridePhys    (C PhysBody *body              ) {return (body && body->is());}

int CompareObj(C Game.Area.Data.AreaObj &a, C Game.Area.Data.AreaObj &b) // this function is used for sorting object before they're saved into game area
{
   if(int c=Compare(a.mesh().id()      , b.mesh().id()      ))return c; // first compare by mesh
   if(int c=Compare(a.meshVariationID(), b.meshVariationID()))return c; // then  compare by mesh variation
   if(int c=Compare(a.matrix.pos       , b.matrix.pos       ))return c; // last  compare by position
   return 0;
}
/******************************************************************************/
// ANIMATION
/******************************************************************************/
void SetRootMoveRot(Animation &anim, C Vec *root_move, C Vec *root_rot)
{
   if(root_rot)
   {
      const int precision=4; // number of keyframes per 90 deg, can be modified, this is needed because rotation interpolation is done by interpolating axis vectors, and few samples are needed to get smooth results
      int num=(Equal(*root_rot, VecZero) ? Equal(anim.rootStart().angle(), 0) ? 0 : 1 : 1+Max(1, Round(root_rot.length()*(precision/PI_2))));
      anim.keys.orns.setNum(num);
      if(num)
      {
         OrientD orn=anim.rootStart();
         anim.keys.orns[0].time=0;
         anim.keys.orns[0].orn =orn;
         if(num>=2)
         {
            num--;
            VecD axis=*root_rot; dbl angle=axis.normalize(); MatrixD3 rot; rot.setRotate(axis, angle/num);
            for(int i=1; i<=num; i++)
            {
               orn.mul(rot, true);
               anim.keys.orns[i].time=flt(i)/num*anim.length();
               anim.keys.orns[i].orn =orn;
            }
         }
      }
   }
   if(root_move)
   {
      const int precision=10; // number of keyframes per meter, can be modified
      bool no_rot=(!root_rot || anim.keys.orns.elms()<=1);
      int  num=(Equal(*root_move, VecZero) ? Equal(anim.rootStart().pos, VecZero) ? 0 : 1 : no_rot ? 2 : 1+Max(1, Round(root_move.length()*precision)));
      anim.keys.poss.setNum(num);
      if(num)
      {
         anim.keys.poss[0].time=0;
         anim.keys.poss[0].pos =anim.rootStart().pos;
         if(num>=2)
         {
            if(no_rot)
            {
               anim.keys.poss[1].time=anim.length();
               anim.keys.poss[1].pos =anim.rootStart().pos+*root_move;
            }else
            {
               num--;
               VecD pos=anim.rootStart().pos, dir=*root_move/num, axis=*root_rot; dbl angle=axis.normalize(); MatrixD3 rot; rot.setRotate(axis, angle/num);
               for(int i=1; i<=num; i++)
               {
                  dir*=rot; pos+=dir;
                  anim.keys.poss[i].time=flt(i)/num*anim.length();
                  anim.keys.poss[i].pos =pos;
               }
            }
         }
      }
   }
   if(root_move || root_rot)
   {
      anim.keys.setTangents(anim.loop(), anim.length());
      anim.setRootMatrix();
   }
}
/******************************************************************************/
// MATH
/******************************************************************************/
inline bool NegativeSB(flt  x) {return FlagTest  ((uint&)x, SIGN_BIT);} // have to work with SIGN_BIT for special case of -0
inline void      CHSSB(flt &x) {       FlagToggle((uint&)x, SIGN_BIT);} // have to work with SIGN_BIT for special case of -0
/******************************************************************************/
int UniquePairs(int elms) {return elms*(elms-1)/2;}
/******************************************************************************/
bool Distance2D(C Vec2 &point, C Edge &edge, flt &dist, flt min_length=0.025) // calculate 2D distance between 'point' and 'edge' projected to screen, true is returned if 'edge' is at least partially visible (not behind camera), 'dist' will contain distance between point and edge
{
   Edge e=edge;
   if(Clip(e, Plane(ActiveCam.matrix.pos + D.viewFrom()*ActiveCam.matrix.z, -ActiveCam.matrix.z)))
   {
      Edge2 e2(PosToScreen(e.p[0]), PosToScreen(e.p[1]));
      if(e2.length()<min_length)return false; // if edge is too short then skip it
      dist=Dist(point, e2);
      return true;
   }
   return false;
}
int MatrixAxis(C Vec2 &screen_pos, C Matrix &matrix)
{
   int axis=-1;
   flt d, dist=0;
   if(Distance2D(screen_pos, Edge(matrix.pos, matrix.pos+matrix.x), d))if(axis<0 || d<dist){dist=d; axis=0;}
   if(Distance2D(screen_pos, Edge(matrix.pos, matrix.pos+matrix.y), d))if(axis<0 || d<dist){dist=d; axis=1;}
   if(Distance2D(screen_pos, Edge(matrix.pos, matrix.pos+matrix.z), d))if(axis<0 || d<dist){dist=d; axis=2;}
   if(dist>0.05)axis=-1;
   return axis;
}
void MatrixAxis(Edit.Viewport4 &v4, C Matrix &matrix, int &axis, Vec *axis_vec=null)
{
   int editing=-1; REPA(MT)if(MT.b(i, MT.touch(i) ? 0 : 1) && v4.getView(MT.guiObj(i))){editing=i; break;}
   if( editing<0)
   {
      axis=-1;
      if(editing< 0)REPA(MT)if(!MT.touch(i) && v4.getView(MT.guiObj(i))){editing=i; break;} // get mouse
      if(editing>=0)if(Edit.Viewport4.View *view=v4.getView(MT.guiObj(editing)))
      {
         view.setViewportCamera();
         axis=MatrixAxis(MT.pos(editing), matrix);
      }
      if(axis_vec)switch(axis)
      {
         case  0: *axis_vec=!matrix.x; break;
         case  1: *axis_vec=!matrix.y; break;
         case  2: *axis_vec=!matrix.z; break;
         default:  axis_vec.zero()   ; break;
      }
   }
}
int GetNearestAxis(C Matrix &matrix, C Vec &dir)
{
   flt dx=Abs(Dot(!matrix.x, dir)),
       dy=Abs(Dot(!matrix.y, dir)),
       dz=Abs(Dot(!matrix.z, dir));
   return MaxI(dx, dy, dz);
}
bool UniformScale(C Matrix3 &m) {return UniformScale(m.scale());}
bool UniformScale(C Vec     &s)
{
   return Equal(s.x/s.y, 1)
       && Equal(s.x/s.z, 1);
}
/******************************************************************************/
flt CamMoveScale(bool perspective=true    ) {return perspective ? ActiveCam.dist*Tan(D.viewFov()/2) : D.viewFov();}
Vec2   MoveScale(Edit.Viewport4.View &view) {return Vec2(D.w()*2, D.h()*2)/view.viewport.size();}

flt AlignDirToCamEx(C Vec &dir, C Vec2 &delta, C Vec &cam_x=ActiveCam.matrix.x, C Vec &cam_y=ActiveCam.matrix.y) {return (!Vec2(Dot(cam_x, dir), Dot(cam_y, dir)) * delta).sum();}
Vec AlignDirToCam  (C Vec &dir, C Vec2 &delta, C Vec &cam_x=ActiveCam.matrix.x, C Vec &cam_y=ActiveCam.matrix.y) {return !dir * AlignDirToCamEx(dir, delta, cam_x, cam_y);}
/******************************************************************************/
flt MatrixLength(C Vec &x, C Vec &y, C Vec &z, C Vec &dir) // matrix length along direction
{
   return Abs(Dot(x, dir))
         +Abs(Dot(y, dir))
         +Abs(Dot(z, dir));
}
/******************************************************************************/
void Include(RectI &rect,           C VecI2 &x) {if(rect.valid())rect|=x;else          rect=x; }
void Include(RectI &rect,           C RectI &x) {if(rect.valid())rect|=x;else          rect=x; }
void Include(Rect  &rect,           C Rect  &x) {if(rect.valid())rect|=x;else          rect=x; }
void Include(Rect  &rect, bool &is, C Vec2  &x) {if(is          )rect|=x;else{is=true; rect=x;}}
void Include(Rect  &rect, bool &is, C Rect  &x) {if(is          )rect|=x;else{is=true; rect=x;}}
void Include(Box   &box , bool &is, C Vec   &v) {if(is          )box |=v;else{is=true; box =v;}}
void Include(Box   &box , bool &is, C Box   &b) {if(is          )box |=b;else{is=true; box =b;}}
/******************************************************************************/
void DrawMatrix(C Matrix &matrix, int bold_axis)
{
   matrix.draw(); switch(bold_axis)
   {
      case 0: DrawArrow2(RED  , matrix.pos, matrix.pos+matrix.x, 0.005); break;
      case 1: DrawArrow2(GREEN, matrix.pos, matrix.pos+matrix.y, 0.005); break;
      case 2: DrawArrow2(BLUE , matrix.pos, matrix.pos+matrix.z, 0.005); break;
   }
}
/******************************************************************************/
// MISC
/******************************************************************************/
void Hide(GuiObj &go) {go.hide();}
/******************************************************************************/
Rect GetRect(C Rect &rect, Memt<Rect> &rects) // !! this will modify 'rects' !!
{
   for(flt x=D.w(); ; ) // start with right side
   {
      for(flt y=D.h(); ; ) // start from top
      {
         Rect temp=rect; temp+=Vec2(x, y)-temp.ru(); // move to test position

         if(!rects.elms())return temp;

         if(temp.min.y<-D.h())break; // we're outside the screen

         bool cuts=false;
         REPA(rects)if(Cuts(temp, rects[i]))
         {
            MIN(y, rects[i].min.y-EPS);
            cuts=true;
         }
         if(!cuts)return temp;
      }

      // find the maximum out of rectangles min.x and remove those rectangles
      int found=-1; REPA(rects)if(found==-1 || rects[i].min.x>x){found=i; x=rects[i].min.x;}
                    REPA(rects)if(rects[i].min.x>=x-EPS)rects.remove(i);
   }
}
/******************************************************************************/
void Include(MemPtr<UID> ids, C UID &id)
{
   if(id.valid())ids.binaryInclude(id);
}
/******************************************************************************/
bool Same(C Memc<UID> &a, C Memc<UID> &b)
{
   if(a.elms()!=b.elms())return false;
   REPA(a)if(a[i]!=b[i])return false;
   return true;
}
bool Same(C Memc<ObjData> &a, C Memc<ObjData> &b)
{
   if(a.elms()!=b.elms())return false;
   REPA(a)
   {
    C ObjData &oa=a[i];
      REPA(b)
      {
       C ObjData &ob=b[i];
         if(oa.id==ob.id)if(oa.equal(ob))goto oa_equal;else break;
      }
      return false; // not found or not equal
      oa_equal:;
   }
   return true;
}
void GetNewer(C Memc<ObjData> &a, C Memc<ObjData> &b, Memc<UID> &newer) // get id's of 'a' objects that are newer than 'b'
{
   REPA(a)
   {
    C ObjData &oa=a[i], *ob=null;
      REPA(b)if(b[i].id==oa.id){ob=&b[i]; break;}
      if(!ob || oa.newer(*ob))newer.add(oa.id);
   }
}
/******************************************************************************/
bool EmbedObject(C Box &obj_box, C VecI2 &area_xy, flt area_size)
{
   return obj_box.min.x/area_size<area_xy.x-0.5 || obj_box.max.x/area_size>area_xy.x+1.5
       || obj_box.min.z/area_size<area_xy.y-0.5 || obj_box.max.z/area_size>area_xy.y+1.5;
}
/******************************************************************************/
bool SameOS(OS_VER a, OS_VER b)
{
   return OSWindows(a) && OSWindows(b)
       || OSMac    (a) && OSMac    (b)
       || OSLinux  (a) && OSLinux  (b)
       || OSAndroid(a) && OSAndroid(b)
       || OSiOS    (a) && OSiOS    (b);
}
/******************************************************************************/
UID GetFileNameID(Str name)
{
   for(; name.is(); name=GetPath(name)){UID id=FileNameID(name); if(id.valid())return id;}
   return UIDZero;
}
UID AsID(C Elm *elm) {return elm ? elm.id : UIDZero;}
/******************************************************************************/
void SetPath(WindowIO &win_io, C Str &path, bool clear=false)
{
   Mems<FileParams> fps=FileParams.Decode(path); if(fps.elms()==1)
   {
      Str first=FFirstUp(fps[0].name);
      if(FileInfoSystem(first).type==FSTD_FILE)first=GetPath(first);
      fps[0].name=SkipStartPath(fps[0].name, first);
      win_io.path(S, first).textline.set(fps[0].encode()); return;
   }else
   if(fps.elms()>1)
   {
      win_io.path(S).textline.set(path); return;
   }
   if(clear)win_io.path(S).textline.clear();
}
/******************************************************************************/
bool ParamTypeID        (PARAM_TYPE type           ) {return type==PARAM_ID || type==PARAM_ID_ARRAY;}
bool ParamTypeCompatible(PARAM_TYPE a, PARAM_TYPE b) {return a==b || (ParamTypeID(a) && ParamTypeID(b));}
bool ParamCompatible    (C Param   &a, C Param   &b) {return a.name==b.name && ParamTypeCompatible(a.type, b.type);}
/******************************************************************************/
class Rename
{
   Str src, dest;

   Rename& set(C Str &src, C Str &dest) {T.src=src; T.dest=dest; return T;}
   
   bool operator==(C Rename &rename)C {return Equal(src, rename.src, true) && Equal(dest, rename.dest, true);}
   bool operator!=(C Rename &rename)C {return !(T==rename);}
   
   /*enum CHECK
   {
      SAME,
      REVERSE,
      REQUIRED,
      UNKNOWN,
   }
   CHECK check(C Rename &rename)C
   {
      bool src_equal=Equal( src, rename. src, true),
          dest_equal=Equal(dest, rename.dest, true);
      if(src_equal && dest_equal)return SAME; // this is the same change

      bool src_equal_dest=Equal( src, rename.dest, true),
          dest_equal_src =Equal(dest, rename.src , true);
      if(src_equal_dest && dest_equal_src)return REVERSE; // this is a reverse change

      return (src_equal || dest_equal || src_equal_dest || dest_equal_src) ? REQUIRED : UNKNOWN; // if any of the names is used, then it is required
   }*/
}
/*void Add(MemPtr<Rename> diff, C Rename &rename, bool optimize=true)
{
   if(optimize)REPA(diff)switch(diff[i].check(rename)) // need to go from the end
   {
      case Rename.REQUIRED: goto add; // we already know that this is required, so skip checking
      case Rename.SAME    :                       return; // no need to apply the same change twice
      case Rename.REVERSE : diff.remove(i, true); return;
    //case Rename.UNKNOWN : break; // keep on checking
   }
add:
   diff.add(rename);
}
void AddReverse(MemPtr<Rename> diff, C Rename &rename, bool optimize=true)
{
   if(optimize)REPA(diff)switch(diff[i].check(rename)) // need to go from the end
   {
      case Rename.REQUIRED: goto add; // we already know that this is required, so skip checking
      case Rename.SAME    : diff.remove(i, true); return; //                                        !! HERE SAME AND REVERSE ARE SWITCHED !!
      case Rename.REVERSE :                       return; // no need to apply the same change twice !! HERE SAME AND REVERSE ARE SWITCHED !!
    //case Rename.UNKNOWN : break; // keep on checking
   }
add:
   diff.New().set(rename.dest, rename.src); // we're reversing so we need to replace dest with src
}
void Diff(MemPtr<Rename> diff, C MemPtr<Rename> &current, C MemPtr<Rename> &desired, bool optimize=true)
{
   diff.clear();
   int min=Min(current.elms(), desired.elms()), equal=0; for(; equal<min; equal++)if(current[equal]!=desired[equal])break; // calculate amount of equal changes

   // reverse 'current'
   for(int i=current.elms(); --i>=equal; )AddReverse(diff, current[i], optimize); // need to go from back

   // apply 'desired'
   for(int i=equal; i<desired.elms(); i++)Add(diff, desired[i], optimize); // need to go from start
}
/******************************************************************************/
enum UNIT_TYPE
{
   UNIT_DEFAULT,
   UNIT_PIXEL  ,
   UNIT_REAL   ,
   UNIT_PERCENT, // 1/100
   UNIT_PERMIL , // 1/1000
}
UNIT_TYPE UnitType(C Str &s)
{
   if(s=="px"                 )return UNIT_PIXEL;
   if(s=="x"                  )return UNIT_REAL;
   if(s=="pc" || s=='%'       )return UNIT_PERCENT;
   if(s=="pm" || s==CharPermil)return UNIT_PERMIL;
   return UNIT_DEFAULT;
}
UNIT_TYPE GetUnitType(C Str &s)
{
   if(s.is())
   {
      const uint flag_and=CHARF_DIG10|CHARF_SIGN|CHARF_ALPHA|CHARF_UNDER|CHARF_SPACE;
      uint flag=CharFlag(s.last())&flag_and;
      int  pos =s.length()-1; for(; pos>0 && (CharFlag(s[pos-1])&flag_and)==flag; pos--);
      return UnitType(s()+pos);
   }
   return UNIT_DEFAULT;
}
flt ConvertUnitType(flt value, flt full, UNIT_TYPE unit)
{
   switch(unit)
   {
      default          : return value;
      case UNIT_REAL   : return value     *full;
      case UNIT_PERCENT: return value/ 100*full;
      case UNIT_PERMIL : return value/1000*full;
   }
}
/******************************************************************************/
// GUI
/******************************************************************************/
Color BackgroundColor()
{
   return Gui.backgroundColor();
}
Color BackgroundColorLight()
{
   Color col=BackgroundColor();
   byte  lum=col.lum(), add=44; Color col_add(add); if(lum)col_add=ColorMul(col, flt(add)/lum); // set normalized color (col/col.lum)*add
   return ColorAdd(col, col_add);
}
Color GuiListTextColor()
{
   if(Gui.skin && Gui.skin->list.text_style)return Gui.skin->list.text_style->color;
   return BLACK;
}
const Color LitSelColor=RED, SelColor=YELLOW, LitColor=CYAN, DefColor=WHITE, InvalidColor=PURPLE;
Color GetLitSelCol(bool lit, bool sel, C Color &none=DefColor)
{
   if(lit && sel)return LitSelColor;
   if(       sel)return SelColor;
   if(lit       )return LitColor;
                 return none;
}
bool ErrorCopy(C Str &src, C Str &dest)
{
   Gui.msgBox(S, S+"Error copying\n\""+src+"\"\nto\n\""+dest+'"');
   return false;
}
bool ErrorRecycle(C Str &name)
{
   Gui.msgBox(S, S+"Error recycling\n\""+name+"\"");
   return false;
}
bool ErrorCreateDir(C Str &name)
{
   Gui.msgBox(S, S+"Error creating folder\n\""+name+"\"");
   return false;
}
bool RecycleLoud  (C Str &name            ) {return FRecycle   (name     ) ? true : ErrorRecycle  (name);}
bool CreateDirLoud(C Str &name            ) {return FCreateDirs(name     ) ? true : ErrorCreateDir(name);}
bool SafeCopyLoud (C Str &src, C Str &dest) {return SafeCopy   (src, dest) ? true : ErrorCopy     (src, dest);}
/******************************************************************************/
// SOUND
/******************************************************************************/
class BitRateQuality
{
   flt quality;
   int bit_rate;
}
const BitRateQuality BitRateQualities[]=
{
   {-0.2,  32*1000}, // aoTuV only
   {-0.1,  45*1000},
   { 0.0,  64*1000},
   { 0.1,  80*1000},
   { 0.2,  96*1000},
   { 0.3, 112*1000},
   { 0.4, 128*1000},
   { 0.5, 160*1000},
   { 0.6, 192*1000},
   { 0.7, 224*1000},
   { 0.8, 256*1000},
   { 0.9, 320*1000},
   { 1.0, 500*1000},
};
flt VorbisBitRateToQuality(int rel_bit_rate) // relative bit rate in bits per second (bit rate for 44.1kHz stereo)
{
   for(int i=1; i<Elms(BitRateQualities); i++)if(rel_bit_rate<=BitRateQualities[i].bit_rate)
   {
    C BitRateQuality &p=BitRateQualities[i-1],
                     &n=BitRateQualities[i  ];
      flt step=LerpR(p.bit_rate, n.bit_rate, rel_bit_rate);
      return   Lerp (p.quality , n.quality , step);
   }
   return 1;
}
/******************************************************************************/
// DEPRECATED
/******************************************************************************/
int DecIntV(File &f)
{
   Byte v; f>>v;
   Bool positive=((v>>6)&1);
   UInt u=(v&63);
   if(v&128)
   {
      f>>v; u|=((v&127)<<6);
      if(v&128)
      {
         f>>v; u|=((v&127)<<(6+7));
         if(v&128)
         {
            f>>v; u|=((v&127)<<(6+7+7));
            if(v&128)
            {
               f>>v; u|=(v<<(6+7+7+7));
            }
         }
      }
   }
   return positive ? u+1 : -Int(u);
}

void GetStr2(File &f, Str &s) {s=GetStr2(f);}
Str  GetStr2(File &f)
{
   Int length=DecIntV(f);
   if( length<0) // unicode
   {
      CHS(length); MIN(length, f.left()/2);
      Str s; s.reserve(length); REP(length){char c; f>>c; s+=c;} return s;
   }else
   if(length)
   {
      MIN(length, f.left());
      Str8 s; s.reserve(length); REP(length){char8 c; f>>c; s+=c;} return s;
   }
   return S;
}

void PutStr(File &f, C Str &s)
{
   uint length =s.length();
   bool unicode=HasUnicode(s);

   f.putUInt(unicode ? length^SIGN_BIT : length);
   if(length)
   {
      if(unicode){          f.putN(s(), length);}
      else       {Str8 t=s; f.putN(t(), length);}
   }
}
Str GetStr(File &f)
{
   uint length=f.getUInt();
   if(  length&SIGN_BIT) // unicode
   {
         length^=SIGN_BIT; MIN(length, f.left()/2);
      if(length){Str s; s.reserve(length); REP(length){char c; f>>c; s+=c;} return s;}
   }else
   {
      MIN(length, f.left());
      if (length){Str8 s; s.reserve(length); REP(length){char8 c; f>>c; s+=c;} return s;}
   }
   return S;
}
void GetStr(File &f, Str &s) {s=GetStr(f);}

<TYPE      > bool Save(File &f, C Memc<TYPE> &m              ) {  f.putInt(m.elms()); FREPA(m)if(!m[i].save(f      ))return false; return f.ok();}
<TYPE, USER> bool Save(File &f, C Memc<TYPE> &m, C USER &user) {  f.putInt(m.elms()); FREPA(m)if(!m[i].save(f, user))return false; return f.ok();}
<TYPE      > bool Save(File &f, C Memx<TYPE> &m              ) {  f.putInt(m.elms()); FREPA(m)if(!m[i].save(f      ))return false; return f.ok();}
<TYPE      > bool Load(File &f,   Memc<TYPE> &m              ) {m.setNum(f.getInt()); FREPA(m)if(!m[i].load(f      ))goto   error; if(f.ok())return true; error: m.clear(); return false;}
<TYPE, USER> bool Load(File &f,   Memc<TYPE> &m, C USER &user) {m.setNum(f.getInt()); FREPA(m)if(!m[i].load(f, user))goto   error; if(f.ok())return true; error: m.clear(); return false;}
<TYPE      > bool Load(File &f,   Memx<TYPE> &m              ) {m.setNum(f.getInt()); FREPA(m)if(!m[i].load(f      ))goto   error; if(f.ok())return true; error: m.clear(); return false;}

Mems<FileParams> _DecodeFileParams(C Str &str)
{
   Mems<FileParams> files; if(str.is())
   {
      Memc<Str> strs=Split(str, '|'); // get list of all files
      files.setNum(strs.elms()); FREPA(files)
      {
         FileParams &file=files[i];
         Memc<Str> fp=Split(strs[i], '?'); // file_name?params
         file.name=(fp.elms() ? fp[0] : S);
         if(fp.elms()>=2)
         {
            Memc<Str> name_vals=Split(fp[1], '&'); FREPA(name_vals)
            {
               Memc<Str> name_val=Split(name_vals[i], '=');
               if(name_val.elms()==2)file.params.New().set(name_val[0], name_val[1]);
            }
         }
      }
   }
   return files;
}
/******************************************************************************/
