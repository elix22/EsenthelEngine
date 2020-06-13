/******************************************************************************/
// defined play lists
Playlist Battle   , // this is battle playlist used for playing when battles
         Exploring, // exploring playlist
         Calm     ; // calm playlist
/******************************************************************************/
void InitPre()
{
   EE_INIT();
}
/******************************************************************************/
bool Init()
{
   if(!Battle.songs()) // create 'Battle' playlist if not yet created
   {
      Battle+=UID(179898080, 1327326228, 2705071249, 4171399536); // add "battle0" track to 'Battle' playlist
      Battle+=UID(746976398, 1110313615, 3079654847, 358289912);  // add "battle1" track to 'Battle' playlist
   }
   if(!Exploring.songs()) // create 'Exploring' playlist if not yet created
   {
      Exploring+=UID(2222101198, 1138675473, 1518921890, 1804050639); // add "explore" track to 'Exploring' playlist
   }
   if(!Calm.songs()) // create 'Calm' playlist if not yet created
   {
      Calm+=UID(4114948924, 1122167854, 3363163781, 3122373663); // add "calm" track to 'Calm' playlist
   }
   return true;
}
/******************************************************************************/
void Shut()
{
}
/******************************************************************************/
bool Update()
{
   if(Kb.bp(KB_ESC))return false;
   if(Kb.k('1'))Music.play(Battle   );
   if(Kb.k('2'))Music.play(Exploring);
   if(Kb.k('3'))Music.play(Calm     );
   if(Kb.k('4'))Music.play(null     );
   return true;
}
/******************************************************************************/
void Draw()
{
   D.clear(AZURE);

   if(Music.playlist()) // if any playlist playing
   {
      D.text(0, 0, S+"time " +Music.time()+" / "+Music.length()+" length");
   }else
   {
      D.text(0, 0, "No playlist playing");
   }
   D.text(0, -0.2, "Press 1-battle, 2-explore, 3-calm, 4-none");
}
/******************************************************************************/
