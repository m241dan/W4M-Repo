/****************************************************************************
 * [S]imulated [M]edieval [A]dventure multi[U]ser [G]ame      |   \\._.//   *
 * -----------------------------------------------------------|   (0...0)   *
 * SMAUG 1.4 (C) 1994, 1995, 1996, 1998  by Derek Snider      |    ).:.(    *
 * -----------------------------------------------------------|    {o o}    *
 * SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,      |   / ' ' \   *
 * Scryn, Rennard, Swordbearer, Gorog, Grishnakh, Nivek,      |~'~.VxvxV.~'~*
 * Tricops and Fireblade                                      |             *
 * ------------------------------------------------------------------------ *
 * Merc 2.1 Diku Mud improvments copyright (C) 1992, 1993 by Michael        *
 * Chastain, Michael Quan, and Mitchell Tse.                                *
 * Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,          *
 * Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
 * ------------------------------------------------------------------------ *
 *                            Battle & death module                         *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include "mud.h"

extern char lastplayercmd[MAX_INPUT_LENGTH];

OBJ_DATA *used_weapon;  /* Used to figure out which weapon later */

/*
 * Local functions.
 */
void new_dam_message( CHAR_DATA * ch, CHAR_DATA * victim, int dam, unsigned int dt, int hit_wear, bool crit, EXT_BV damtype );
void group_gain( CHAR_DATA * ch, CHAR_DATA * victim );
int xp_compute( CHAR_DATA * gch, CHAR_DATA * victim );
int align_compute( CHAR_DATA * gch, CHAR_DATA * victim );
ch_ret one_hit( CHAR_DATA * ch, CHAR_DATA * victim, int dt );
void show_condition( CHAR_DATA * ch, CHAR_DATA * victim );

bool loot_coins_from_corpse( CHAR_DATA * ch, OBJ_DATA * corpse )
{
   OBJ_DATA *content, *content_next;
   int oldgold = ch->gold;

   for( content = corpse->first_content; content; content = content_next )
   {
      content_next = content->next_content;

      if( content->item_type != ITEM_MONEY )
         continue;
      if( !can_see_obj( ch, content ) )
         continue;
      if( !CAN_WEAR( content, ITEM_TAKE ) && ch->level < sysdata.level_getobjnotake )
         continue;
      if( IS_OBJ_STAT( content, ITEM_PROTOTYPE ) && !can_take_proto( ch ) )
         continue;

      act( AT_ACTION, "You get $p from $P", ch, content, corpse, TO_CHAR );
      act( AT_ACTION, "$n gets $p from $P", ch, content, corpse, TO_ROOM );
      obj_from_obj( content );
      check_for_trap( ch, content, TRAP_GET );
      if( char_died( ch ) )
         return FALSE;

      oprog_get_trigger( ch, content );
      if( char_died( ch ) )
         return FALSE;

      ch->gold += content->value[0] * content->count;
      extract_obj( content );
   }

   if( ch->gold - oldgold > 1 && ch->position > POS_SLEEPING )
   {
      char buf[MAX_INPUT_LENGTH];

      snprintf( buf, MAX_INPUT_LENGTH, "%d", ch->gold - oldgold );
      do_split( ch, buf );
   }
   return TRUE;
}

/*
 * Check to see if player's attacks are (still?) suppressed
 * #ifdef TRI
 */
bool is_attack_supressed( CHAR_DATA * ch )
{
   TIMER *timer;

   if( IS_NPC( ch ) )
      return FALSE;

   timer = get_timerptr( ch, TIMER_ASUPRESSED );

   if( !timer )
      return FALSE;

   /*
    * perma-supression -- bard? (can be reset at end of fight, or spell, etc) 
    */
   if( timer->value == -1 )
      return TRUE;

   /*
    * this is for timed supressions 
    */
   if( timer->count >= 1 )
      return TRUE;

   return FALSE;
}

/*
 * Check to see if weapon is poisoned.
 */
bool is_wielding_poisoned( CHAR_DATA * ch )
{
   OBJ_DATA *obj;

   if( !used_weapon )
      return FALSE;

   if( ( obj = get_eq_char( ch, WEAR_WIELD ) ) != NULL && used_weapon == obj && IS_OBJ_STAT( obj, ITEM_POISONED ) )
      return TRUE;
   if( ( obj = get_eq_char( ch, WEAR_DUAL_WIELD ) ) != NULL && used_weapon == obj && IS_OBJ_STAT( obj, ITEM_POISONED ) )
      return TRUE;

   return FALSE;
}

/*
 * hunting, hating and fearing code				-Thoric
 */
bool is_hunting( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( !ch->hunting || ch->hunting->who != victim )
      return FALSE;

   return TRUE;
}

bool is_hating( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( !ch->hating || ch->hating->who != victim )
      return FALSE;

   return TRUE;
}

bool is_fearing( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( !ch->fearing || ch->fearing->who != victim )
      return FALSE;

   return TRUE;
}

void stop_hunting( CHAR_DATA * ch )
{
   if( ch->hunting )
   {
      STRFREE( ch->hunting->name );
      DISPOSE( ch->hunting );
      ch->hunting = NULL;
   }
   return;
}

void stop_hating( CHAR_DATA * ch )
{
   if( ch->hating )
   {
      STRFREE( ch->hating->name );
      DISPOSE( ch->hating );
      ch->hating = NULL;
   }
   return;
}

void stop_fearing( CHAR_DATA * ch )
{
   if( ch->fearing )
   {
      STRFREE( ch->fearing->name );
      DISPOSE( ch->fearing );
      ch->fearing = NULL;
   }
   return;
}

void start_hunting( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( ch->hunting )
      stop_hunting( ch );

   CREATE( ch->hunting, HHF_DATA, 1 );
   ch->hunting->name = QUICKLINK( victim->name );
   ch->hunting->who = victim;
   return;
}

void start_hating( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( ch->hating )
      stop_hating( ch );

   CREATE( ch->hating, HHF_DATA, 1 );
   ch->hating->name = QUICKLINK( victim->name );
   ch->hating->who = victim;
   return;
}

void start_fearing( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( ch->fearing )
      stop_fearing( ch );

   CREATE( ch->fearing, HHF_DATA, 1 );
   ch->fearing->name = QUICKLINK( victim->name );
   ch->fearing->who = victim;
   return;
}

/*
 * Get the current armor class for a vampire based on time of day
 */
short VAMP_AC( CHAR_DATA * ch )
{
   return 0;
}

/*
 * Control the fights going on.
 * Called periodically by update_handler.
 * Many hours spent fixing bugs in here by Thoric, as noted by residual
 * debugging checks.  If you never get any of these error messages again
 * in your logs... then you can comment out some of the checks without
 * worry.
 *
 * Note:  This function also handles some non-violence updates.
 */
void violence_update( void )
{
   CHAR_DATA *ch;
   CHAR_DATA *lst_ch;
   CHAR_DATA *victim;
   CHAR_DATA *rch;
   TRV_WORLD *lcw;
   TRV_DATA *lcr;
   TIMER *timer, *timer_next;
   ch_ret retcode;
   int attacktype, cnt;
   static int pulse = 0;

   lst_ch = NULL;
   pulse = ( pulse + 1 ) % 100;

   lcw = trworld_create( TR_CHAR_WORLD_BACK );
   for( ch = last_char; ch; lst_ch = ch, ch = trvch_wnext( lcw ) )
   {
      set_cur_char( ch );

      /*
       * See if we got a pointer to someone who recently died...
       * if so, either the pointer is bad... or it's a player who
       * "died", and is back at the healer...
       * Since he/she's in the char_list, it's likely to be the later...
       * and should not already be in another fight already
       */
      if( char_died( ch ) )
         continue;

      /*
       * Experience gained during battle deceases as battle drags on
       */
      if( ch->fighting && ( ++ch->fighting->duration % 24 ) == 0 )
         ch->fighting->xp = ( ( ch->fighting->xp * 9 ) / 10 );

      for( timer = ch->first_timer; timer; timer = timer_next )
      {
         timer_next = timer->next;
         if( --timer->count <= 0 )
         {
            if( timer->type == TIMER_ASUPRESSED )
            {
               if( timer->value == -1 )
               {
                  timer->count = 1000;
                  continue;
               }
            }

            if( timer->type == TIMER_NUISANCE )
               DISPOSE( ch->pcdata->nuisance );

            if( timer->type == TIMER_DO_FUN )
            {
               int tempsub;

               tempsub = ch->substate;
               ch->substate = timer->value;
               ( timer->do_fun ) ( ch, "" );
               if( char_died( ch ) )
                  break;
               ch->substate = tempsub;
            }
            extract_timer( ch, timer );
         }
      }

      if( char_died( ch ) )
         continue;

      if( ch->stopkill )
      {
         ch->stopkill = FALSE;
         send_to_char( "You may now use the kill command again.\r\n", ch );
         continue;
      }

      if( char_died( ch ) )
         continue;

      /*
       * check for exits moving players around 
       */
      if( ( retcode = pullcheck( ch, pulse ) ) == rCHAR_DIED || char_died( ch ) )
         continue;

      /*
       * Let the battle begin! 
       */
      if( !ch->target || IS_AFFECTED( ch, AFF_PARALYSIS ) ||  ch->position != POS_FIGHTING )
         continue;

      victim = ch->target->victim;
      retcode = rNONE;

      if( xIS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
      {
         log_printf( "violence_update: %s fighting %s in a SAFE room.", ch->name, victim->name );
         stop_fighting( ch, TRUE );
      }
      else if( IS_AWAKE( ch ) )
         retcode = multi_hit( ch, ch->target, TYPE_UNDEFINED );
      else
         stop_fighting( ch, FALSE );

      if( char_died( ch ) )
         continue;

      if( retcode == rVICT_OOR )
      {
         if( IS_NPC( ch ) ) // Need to do something better here. Can't stop fighting and lose fight data just because out of range
         {
            stop_fighting( ch, FALSE );
            start_hunting( ch, ch->target->victim );
         }
         else
            ch_printf( ch, "%s is too far away to auto-attack them.\r\n", victim->name );
         continue;
      }

      if( retcode == rVICT_LOS )
      {
         if( IS_NPC( ch ) )
         {
            stop_fighting( ch, FALSE );
            start_hunting( ch, ch->target->victim );
         }
         else
            ch_printf( ch, "%s is out of your line of sight.\r\n", victim->name );
         continue;
      }

      if( retcode == rCHAR_DIED || ( victim = who_fighting( ch ) ) == NULL )
         continue;

      /*
       *  Mob triggers
       *  -- Added some victim death checks, because it IS possible.. -- Alty
       */
      rprog_rfight_trigger( ch );
      if( char_died( ch ) || char_died( victim ) )
         continue;
      mprog_hitprcnt_trigger( ch, victim );
      if( char_died( ch ) || char_died( victim ) )
         continue;
      mprog_fight_trigger( ch, victim );
      if( char_died( ch ) || char_died( victim ) )
         continue;

      /*
       * NPC special attack flags            -Thoric
       */
      if( IS_NPC( ch ) )
      {
         if( !xIS_EMPTY( ch->attacks ) )
         {
            attacktype = -1;
            if( 30 + ( ch->level / 4 ) >= number_percent(  ) )
            {
               cnt = 0;
               for( ;; )
               {
                  if( cnt++ > 10 )
                  {
                     attacktype = -1;
                     break;
                  }
                  attacktype = number_range( 7, MAX_ATTACK_TYPE - 1 );
                  if( xIS_SET( ch->attacks, attacktype ) )
                     break;
               }
               switch ( attacktype )
               {
                  case ATCK_BASH:
                     do_bash( ch, "" );
                     retcode = global_retcode;
                     break;
                  case ATCK_STUN:
                     do_stun( ch, "" );
                     retcode = global_retcode;
                     break;
                  case ATCK_GOUGE:
                     do_gouge( ch, "" );
                     retcode = global_retcode;
                     break;
                  case ATCK_FEED:
                     do_feed( ch, "" );
                     retcode = global_retcode;
                     break;
                  case ATCK_DRAIN:
                     retcode = spell_energy_drain( skill_lookup( "energy drain" ), ch->level, ch, victim );
                     break;
                  case ATCK_FIREBREATH:
                     retcode = spell_fire_breath( skill_lookup( "fire breath" ), ch->level, ch, victim );
                     break;
                  case ATCK_FROSTBREATH:
                     retcode = spell_frost_breath( skill_lookup( "frost breath" ), ch->level, ch, victim );
                     break;
                  case ATCK_ACIDBREATH:
                     retcode = spell_acid_breath( skill_lookup( "acid breath" ), ch->level, ch, victim );
                     break;
                  case ATCK_LIGHTNBREATH:
                     retcode = spell_lightning_breath( skill_lookup( "lightning breath" ), ch->level, ch, victim );
                     break;
                  case ATCK_GASBREATH:
                     retcode = spell_gas_breath( skill_lookup( "gas breath" ), ch->level, ch, victim );
                     break;
                  case ATCK_SPIRALBLAST:
                     retcode = spell_spiral_blast( skill_lookup( "spiral blast" ), ch->level, ch, victim );
                     break;
                  case ATCK_POISON:
                     retcode = spell_poison( gsn_poison, ch->level, ch, victim );
                     break;
                  case ATCK_NASTYPOISON:
                     /*
                      * retcode = spell_nasty_poison( skill_lookup( "nasty poison" ), ch->level, ch, victim );
                      */
                     break;
                  case ATCK_GAZE:
                     /*
                      * retcode = spell_gaze( skill_lookup( "gaze" ), ch->level, ch, victim );
                      */
                     break;
                  case ATCK_BLINDNESS:
                     retcode = spell_blindness( gsn_blindness, ch->level, ch, victim );
                     break;
                  case ATCK_CAUSESERIOUS:
                     retcode = spell_cause_serious( skill_lookup( "cause serious" ), ch->level, ch, victim );
                     break;
                  case ATCK_EARTHQUAKE:
                     retcode = spell_earthquake( skill_lookup( "earthquake" ), ch->level, ch, victim );
                     break;
                  case ATCK_CAUSECRITICAL:
                     retcode = spell_cause_critical( skill_lookup( "cause critical" ), ch->level, ch, victim );
                     break;
                  case ATCK_CURSE:
                     retcode = spell_curse( skill_lookup( "curse" ), ch->level, ch, victim );
                     break;
                  case ATCK_FLAMESTRIKE:
                     retcode = spell_flamestrike( skill_lookup( "flamestrike" ), ch->level, ch, victim );
                     break;
                  case ATCK_HARM:
                     retcode = spell_harm( skill_lookup( "harm" ), ch->level, ch, victim );
                     break;
                  case ATCK_FIREBALL:
                     retcode = spell_fireball( skill_lookup( "fireball" ), ch->level, ch, victim );
                     break;
                  case ATCK_COLORSPRAY:
                     retcode = spell_colour_spray( skill_lookup( "colour spray" ), ch->level, ch, victim );
                     break;
                  case ATCK_WEAKEN:
                     retcode = spell_weaken( skill_lookup( "weaken" ), ch->level, ch, victim );
                     break;
               }
               if( attacktype != -1 && ( retcode == rCHAR_DIED || char_died( ch ) ) )
                  continue;
            }
         }

         /*
          * NPC special defense flags          -Thoric
          */
         if( !xIS_EMPTY( ch->defenses ) )
         {
            attacktype = -1;
            if( 50 + ( ch->level / 4 ) > number_percent(  ) )
            {
               cnt = 0;
               for( ;; )
               {
                  if( cnt++ > 10 )
                  {
                     attacktype = -1;
                     break;
                  }
                  attacktype = number_range( 2, MAX_DEFENSE_TYPE - 1 );
                  if( xIS_SET( ch->defenses, attacktype ) )
                     break;
               }

               switch ( attacktype )
               {
                  case DFND_CURELIGHT:
                     /*
                      * A few quick checks in the cure ones so that a) less spam and
                      * b) we don't have mobs looking stupider than normal by healing
                      * themselves when they aren't even being hit (although that
                      * doesn't happen TOO often 
                      */
                     if( ch->hit < ch->max_hit )
                     {
                        act( AT_MAGIC, "$n mutters a few incantations...and looks a little better.", ch, NULL, NULL,
                             TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "cure light" ), ch->level, ch, ch );
                     }
                     break;
                  case DFND_CURESERIOUS:
                     if( ch->hit < ch->max_hit )
                     {
                        act( AT_MAGIC, "$n mutters a few incantations...and looks a bit better.", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "cure serious" ), ch->level, ch, ch );
                     }
                     break;
                  case DFND_CURECRITICAL:
                     if( ch->hit < ch->max_hit )
                     {
                        act( AT_MAGIC, "$n mutters a few incantations...and looks healthier.", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "cure critical" ), ch->level, ch, ch );
                     }
                     break;
                  case DFND_HEAL:
                     if( ch->hit < ch->max_hit )
                     {
                        act( AT_MAGIC, "$n mutters a few incantations...and looks much healthier.", ch, NULL, NULL,
                             TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "heal" ), ch->level, ch, ch );
                     }
                     break;
                  case DFND_DISPELMAGIC:
                     if( victim->first_affect )
                     {
                        act( AT_MAGIC, "$n utters an incantation...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_dispel_magic( skill_lookup( "dispel magic" ), ch->level, ch, victim );
                     }
                     break;
                  case DFND_DISPELEVIL:
                     act( AT_MAGIC, "$n utters an incantation...", ch, NULL, NULL, TO_ROOM );
                     retcode = spell_dispel_evil( skill_lookup( "dispel evil" ), ch->level, ch, victim );
                     break;
                  case DFND_TELEPORT:
                     retcode = spell_teleport( skill_lookup( "teleport" ), ch->level, ch, ch );
                     break;
                  case DFND_SHOCKSHIELD:
                     if( !IS_AFFECTED( ch, AFF_SHOCKSHIELD ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "shockshield" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
                  case DFND_VENOMSHIELD:
                     if( !IS_AFFECTED( ch, AFF_VENOMSHIELD ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations ...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "venomshield" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
                  case DFND_ACIDMIST:
                     if( !IS_AFFECTED( ch, AFF_ACIDMIST ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations ...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "acidmist" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
                  case DFND_FIRESHIELD:
                     if( !IS_AFFECTED( ch, AFF_FIRESHIELD ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "fireshield" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
                  case DFND_ICESHIELD:
                     if( !IS_AFFECTED( ch, AFF_ICESHIELD ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "iceshield" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
                  case DFND_TRUESIGHT:
                     if( !IS_AFFECTED( ch, AFF_TRUESIGHT ) )
                        retcode = spell_smaug( skill_lookup( "true" ), ch->level, ch, ch );
                     else
                        retcode = rNONE;
                     break;
                  case DFND_SANCTUARY:
                     if( !IS_AFFECTED( ch, AFF_SANCTUARY ) )
                     {
                        act( AT_MAGIC, "$n utters a few incantations...", ch, NULL, NULL, TO_ROOM );
                        retcode = spell_smaug( skill_lookup( "sanctuary" ), ch->level, ch, ch );
                     }
                     else
                        retcode = rNONE;
                     break;
               }
               if( attacktype != -1 && ( retcode == rCHAR_DIED || char_died( ch ) ) )
                  continue;
            }
         }
      }

      /*
       * Fun for the whole family!
       */
      lcr = trvch_create( ch, TR_CHAR_ROOM_FORW );
      for( rch = ch->in_room->first_person; rch; rch = trvch_next( lcr ) )
      {
         if( IS_AWAKE( rch ) && !rch->fighting )
         {
            /*
             * PC's auto-assist others in their group.
             */
            if( !IS_NPC( ch ) || IS_AFFECTED( ch, AFF_CHARM ) )
            {
               if( ( ( !IS_NPC( rch ) && rch->desc )
                     || IS_AFFECTED( rch, AFF_CHARM ) ) && is_same_group( ch, rch ) && !is_safe( rch, victim, TRUE ) )
                     multi_hit( rch, victim, TYPE_UNDEFINED );

               continue;
            }

            /*
             * NPC's assist NPC's of same type or 12.5% chance regardless.
             *
             * Will revisit this later with more PvE style friendly mechanics 
             * -Davenge
            if( IS_NPC( rch ) && !IS_AFFECTED( rch, AFF_CHARM ) && !xIS_SET( rch->act, ACT_NOASSIST )
                && !xIS_SET( rch->act, ACT_PET ) )
            {
               if( char_died( ch ) )
                  break;
               if( rch->pIndexData == ch->pIndexData || number_bits( 3 ) == 0 )
               {
                  CHAR_DATA *vch;
                  CHAR_DATA *target;
                  int number;

                  target = NULL;
                  number = 0;
                  for( vch = ch->in_room->first_person; vch; vch = vch->next_in_room )
                  {
                     if( can_see( rch, vch ) && is_same_group( vch, victim ) && number_range( 0, number ) == 0 )
                     {
                        if( vch->mount && vch->mount == rch )
                           target = NULL;
                        else
                        {
                           target = vch;
                           number++;
                        }
                     }
                  }

                  if( target )
                     multi_hit( rch, target, TYPE_UNDEFINED );
               }
            } */
         }
      }
      trv_dispose( &lcr );
   }
   trworld_dispose( &lcw );
   return;
}

ch_ret multi_hit( CHAR_DATA *ch, CHAR_DATA *victim, int dt )
{
   TARGET_DATA *temp_target;
   ch_ret retcode;
   if( ( temp_target = get_target_2( ch, victim, -1 ) ) != NULL )
   {
      retcode = multi_hit( ch, temp_target, dt );
      return retcode;
   }
   else
      return rERROR;
}

/*
 * Do one group of attacks.
 */
ch_ret multi_hit( CHAR_DATA * ch, TARGET_DATA *target, int dt )
{
   OBJ_DATA *offhand;
   int schance;
   ch_ret retcode;

   /*
    * add timer to pkillers 
    */
   if( !IS_NPC( ch ) && !IS_NPC( target->victim ) )
   {
      if( xIS_SET( ch->act, PLR_NICE ) )
         return rNONE;
      add_timer( ch, TIMER_RECENTFIGHT, 11, NULL, 0 );
      add_timer( target->victim, TIMER_RECENTFIGHT, 11, NULL, 0 );
   }

   if( is_attack_supressed( ch ) )
      return rNONE;

   if( IS_NPC( ch ) && xIS_SET( ch->act, ACT_NOATTACK ) )
      return rNONE;

   /* Make sure  target was passed! -Davenge */

   if( !target )
   {
      bug( "CH: %s called multi_hit called and passed a NULL target! uh oh...", ch->name );
      return rNONE;
   }

   /*
    * The temp_targets I setup don't link, saving lines of code by linking themhere 
    * -Davenge
    */
   if( dt == TYPE_UNDEFINED && !ch->target )
      set_new_target( ch, target );

    /* Range Checks -Davenge */

   if( !range_check( ch, target, dt, FALSE ) )
      return rVICT_OOR;

   /* Check for Line of Sight -Davenge */

   if( !check_los( ch, target->victim ) )
      return rVICT_LOS;

   if( ( retcode = one_hit( ch, target->victim, dt ) ) != rNONE )
      return retcode;

   if( who_fighting( ch ) != target->victim || is_skill( dt ) )
      return rNONE;

   offhand = get_eq_char( ch, WEAR_DUAL_WIELD );
   if( offhand && offhand->item_type == ITEM_WEAPON  )
   {
      retcode = one_hit( ch, target->victim, dt );
      if( retcode != rNONE || who_fighting( ch ) != target->victim )
         return retcode;
   }


   /*
    * NPC predetermined number of attacks         -Thoric
    */
   if( IS_NPC( ch ) && ch->numattacks > 0 )
   {
      for( schance = 0; schance < ch->numattacks; schance++ )
      {
         retcode = one_hit( ch, target->victim, dt );
         if( retcode != rNONE || who_fighting( ch ) != target->victim )
            return retcode;
      }
      return retcode;
   }

   retcode = rNONE;

   return retcode;
}


/*
 * Weapon types, haus
 */
int weapon_prof_bonus_check( CHAR_DATA * ch, OBJ_DATA * wield, int *gsn_ptr )
{
   return 0;
}

/*
 * Offensive shield level modifier
 */
short off_shld_lvl( CHAR_DATA * ch, CHAR_DATA * victim )
{
   short lvl;

   if( !IS_NPC( ch ) )  /* players get much less effect */
   {
      lvl = UMAX( 1, ( ch->level - 10 ) / 2 );
      if( number_percent(  ) + ( victim->level - lvl ) < 40 )
      {
         if( CAN_PKILL( ch ) && CAN_PKILL( victim ) )
            return ch->level;
         else
            return lvl;
      }
      else
         return 0;
   }
   else
   {
      lvl = ch->level / 2;
      if( number_percent(  ) + ( victim->level - lvl ) < 70 )
         return lvl;
      else
         return 0;
   }
}

/*
 * Hit one guy once.
 */
ch_ret one_hit( CHAR_DATA * ch, CHAR_DATA * victim, int dt )
{
   OBJ_DATA *wield, *vic_eq;
   HIT_DATA *hit_data;
   EXT_BV damtype;

   int dam, prof_bonus, prof_gsn = -1;
   int max_range, hit_wear, subcount, strvcon, wpnroll, speroll, intvwis;
   int subtable[21];
   ch_ret retcode = rNONE;
   bool crit, physical;
   static bool dual_flip = FALSE;

   /*
    * Can't beat a dead char!
    * Guard against weird room-leavings.
    */
   if( victim->position == POS_DEAD )
      return rVICT_DIED;

   add_queue( ch, COMBAT_LAG_TIMER );

   used_weapon = NULL;
   /*
    * Figure out the weapon doing the damage         -Thoric
    * Dual wield support -- switch weapons each attack
    */
   if( ( wield = get_eq_char( ch, WEAR_DUAL_WIELD ) ) != NULL && dt == TYPE_UNDEFINED )
   {
      if( dual_flip == FALSE )
      {
         dual_flip = TRUE;
         wield = get_eq_char( ch, WEAR_WIELD );
      }
      else
         dual_flip = FALSE;
   }
   else
      wield = get_eq_char( ch, WEAR_WIELD );

   used_weapon = wield;

   if( wield )
   {
      prof_bonus = weapon_prof_bonus_check( ch, wield, &prof_gsn );
      max_range = ch->range + wield->range;
   }
   else
   {
      prof_bonus = 0;
      max_range = ch->range;
   }

   if( dt == TYPE_UNDEFINED && ch->target->range > max_range ) //Handle range for wield being used -Davenge
      return rNONE;

   if( dt == TYPE_UNDEFINED )
   {
      dt = TYPE_HIT;
      if( wield && wield->item_type == ITEM_WEAPON )
      {
         dt += wield->value[3];
         damtype = wield->damtype;
      }
      else
         damtype = ch->damtype;
   }
   else
   {
      damtype = skill_table[dt]->damtype;
      if( xIS_SET( damtype, DAM_INHERITED ) && used_weapon )
         xSET_BITS( damtype, used_weapon->damtype );
      else if( xIS_SET( damtype, DAM_INHERITED ) && !used_weapon )
         xSET_BITS( damtype, ch->damtype );
   }

   if( dt >= TYPE_HIT || skill_table[dt]->type == SKILL_SKILL )
      physical = TRUE;

   if( physical )
   {
      hit_data = generate_hit_data( victim );

      hit_wear = hit_data->locations[number_range( 0, ( hit_data->max_locations - 1 ) )];

      vic_eq = get_eq_char( victim, abs(hit_wear) );

      if( hit_wear == MISS_GENERAL )
      {
         //Miss -Davenge
         damage( ch, victim, 0, dt, hit_wear, FALSE, damtype );
         tail_chain( );
         DISPOSE( hit_data );
         return rNONE;
      }
      if( hit_wear < 0 && wield->weight < 0 )
      {
         int counter, did_it_hit;

         subcount = weight_ratio_dex( get_curr_dex( ch ), wield->weight );

         for( counter = 0; counter < subcount; counter++ )
            subtable[counter] = 1;

         subcount += weight_ratio_dex( get_curr_dex( victim ), vic_eq->weight );

         for( ; counter < subcount; counter++ )
            subtable[counter] = -1;

         did_it_hit = number_range( 0, ( subcount -1 ) );
         if( did_it_hit == -1 )
         {
            //Miss -Davenge
            damage( ch, victim, 0, dt, hit_wear, FALSE, damtype );
            tail_chain( );
            DISPOSE( hit_data );
            return rNONE;
         }
      }
   }
   /*
    * Hit.
    * Calc damage.
    */
   if( physical )
   {
      if( !wield )   /* bare hand dice formula fixed by Thoric */
         /*
          * Fixed again by korbillian@mud.tka.com 4000 (Cold Fusion Mud)
          */
         wpnroll = number_range( ch->barenumdie, ch->baresizedie * ch->barenumdie ) + ch->damplus;
      else
         wpnroll = number_range( wield->value[1], wield->value[2] );
      /*
       * STR v. CON calculation, flat addition/subtraction to weapon roll -Davenge
       */
      strvcon = get_curr_str( ch ) - get_curr_con( victim );
      dam = wpnroll + strvcon;
      if( IS_BETA( ) )
         ch_printf( ch, "Weapon Roll: %d Stat Roll(pSTR - vCON): %d Init Dam Total: %d\r\n", wpnroll, strvcon, dam );
   }
   else
   {
      /*
       * Need to come up with base damage for spells -Davenge
       */
      speroll = 0;
      /*
       * Roll int vs. wis for more base damage stuff -Davenge
       */
      intvwis = get_curr_int( ch ) - get_curr_wis( victim );
      dam = speroll + intvwis;
      if( IS_BETA( ) )
         ch_printf( ch, "Spell Roll: %d Stat Roll(pINT - vWIS): %d Init Dam Total: %d\r\n", speroll, intvwis, dam );
   }

   if( dam <= 0 )
      dam = 1;

   /*
    * Did we crit? -Davenge
    */

    crit = get_crit( ch, dt );
    if( IS_BETA() && crit )
       send_to_char( "Attack is a critical strike.\r\n", ch );
   /*
    * Wear_loc native bonuses -Davenge
    */
   if( physical )
   {
      if( hit_wear == HIT_HEAD )
         dam = (int)( dam * 1.2 );
      if( hit_wear == HIT_ARMS || hit_wear == HIT_HANDS || hit_wear == HIT_LEGS || hit_wear == HIT_FEET )
         dam = (int)( dam * .9 );
      if( IS_BETA( ) )
         ch_printf( ch, "Damage after hit_location native bonuses: %d\r\n", dam );
   }

   /*
    * Calculate Damage after attack vs. ac -Davenge
    */
   if( physical )
   {
      dam = attack_ac_mod( ch, victim, dam );
      if( IS_BETA( ) )
         ch_printf( ch, "Damage after attack vs. defense: %d\r\n", dam );
   }
   else
   {
      dam = mattack_mdefense_mod( ch, victim, dam );
      if( IS_BETA( ) )
         ch_printf( ch, "Damage after magic attack vs. magic defense: %d\r\n", dam );
   }
   /*
    * Handle Res Pen -Davenge
    */
   dam = res_pen( ch, victim, dam, damtype );
   if( IS_BETA( ) )
      ch_printf( ch, "Damage after Resistances and Penetrations calculated: %d\r\n", dam );
   /*
    * Handle Weight of Weapon Vs. Armor Weight -Davenge
    */
   if( physical )
   {
      dam = calc_weight_mod( ch, victim, hit_wear, dam, crit );
      if( IS_BETA( ) )
         ch_printf( ch, "Damage after Weight difference calculations(And Crit if you crit): %d\r\n", dam );
   }
   else if( !physical && crit )
   {
      dam = (int)( dam * 1.5 ); //Magical Crit
      if( IS_BETA() )
         ch_printf( ch, "Damage after magical crit factored in: %d\r\n", dam );
   }
   /*
    * Calculate Proficiencies -Davenge
    * -Need to figure out where to put this-
    */


   if( !IS_AWAKE( victim ) )
      dam *= 2;

   if( dam <= 0 )
      dam = 1;

   DISPOSE( hit_data );
   log_string( victim->name );
   if( ( retcode = damage( ch, victim, dam, dt, hit_wear, crit, damtype ) ) != rNONE )
      return retcode;
   if( char_died( ch ) )
      return rCHAR_DIED;
   else if( !ch->fighting )
      set_fighting( ch, victim );
   if( char_died( victim ) )
      return rVICT_DIED;
   else if( !victim->fighting )
      set_fighting( victim, ch );

   retcode = rNONE;
   if( dam == 0 )
      return retcode;

   /*
    * Weapon spell support            -Thoric
    * Each successful hit casts a spell
    */
   if( wield && !IS_SET( victim->immune, RIS_MAGIC ) && !xIS_SET( victim->in_room->room_flags, ROOM_NO_MAGIC ) )
   {
      AFFECT_DATA *aff;

      for( aff = wield->pIndexData->first_affect; aff; aff = aff->next )
         if( aff->location == APPLY_WEAPONSPELL && IS_VALID_SN( aff->modifier ) && skill_table[aff->modifier]->spell_fun )
            retcode = ( *skill_table[aff->modifier]->spell_fun ) ( aff->modifier, ( wield->level + 3 ) / 3, ch, victim );

      if( retcode == rSPELL_FAILED )
         retcode = rNONE;  // Luc, 6/11/2007

      if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
         return retcode;
      for( aff = wield->first_affect; aff; aff = aff->next )
         if( aff->location == APPLY_WEAPONSPELL && IS_VALID_SN( aff->modifier ) && skill_table[aff->modifier]->spell_fun )
            retcode = ( *skill_table[aff->modifier]->spell_fun ) ( aff->modifier, ( wield->level + 3 ) / 3, ch, victim );

      if( retcode == rSPELL_FAILED )
         retcode = rNONE;  // Luc, 6/11/2007

      if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
         return retcode;
   }

   /*
    * magic shields that retaliate          -Thoric
    */
   if( IS_AFFECTED( victim, AFF_FIRESHIELD ) && !IS_AFFECTED( ch, AFF_FIRESHIELD ) )
      retcode = spell_smaug( skill_lookup( "flare" ), off_shld_lvl( victim, ch ), victim, ch );
   if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      return retcode;

   if( IS_AFFECTED( victim, AFF_ICESHIELD ) && !IS_AFFECTED( ch, AFF_ICESHIELD ) )
      retcode = spell_smaug( skill_lookup( "iceshard" ), off_shld_lvl( victim, ch ), victim, ch );
   if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      return retcode;

   if( IS_AFFECTED( victim, AFF_SHOCKSHIELD ) && !IS_AFFECTED( ch, AFF_SHOCKSHIELD ) )
      retcode = spell_smaug( skill_lookup( "torrent" ), off_shld_lvl( victim, ch ), victim, ch );
   if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      return retcode;

   if( IS_AFFECTED( victim, AFF_ACIDMIST ) && !IS_AFFECTED( ch, AFF_ACIDMIST ) )
      retcode = spell_smaug( skill_lookup( "acidshot" ), off_shld_lvl( victim, ch ), victim, ch );
   if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      return retcode;

   if( IS_AFFECTED( victim, AFF_VENOMSHIELD ) && !IS_AFFECTED( ch, AFF_VENOMSHIELD ) )
      retcode = spell_smaug( skill_lookup( "venomshot" ), off_shld_lvl( victim, ch ), victim, ch );
   if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      return retcode;

   tail_chain(  );
   return retcode;
}

/*
 * Hit one guy with a projectile.
 * Handles use of missile weapons (wield = missile weapon)
 * or thrown items/weapons
 */
ch_ret projectile_hit( CHAR_DATA * ch, CHAR_DATA * victim, OBJ_DATA * wield, OBJ_DATA * projectile, short dist )
{
   int victim_ac;
   int thac0_00;
   int thac0_32;
   int plusris;
   int dam;
   int diceroll;
   int prof_bonus;
   int prof_gsn = -1;
   int proj_bonus;
   int dt;
   ch_ret retcode;

   if( !projectile )
      return rNONE;

   if( projectile->item_type == ITEM_PROJECTILE || projectile->item_type == ITEM_WEAPON )
   {
      dt = TYPE_HIT + projectile->value[3];
      proj_bonus = number_range( projectile->value[1], projectile->value[2] );
   }
   else
   {
      dt = TYPE_UNDEFINED;
      proj_bonus = number_range( 1, URANGE( 2, get_obj_weight( projectile ), 100 ) );
   }

   /*
    * Can't beat a dead char!
    */
   if( victim->position == POS_DEAD || char_died( victim ) )
   {
      extract_obj( projectile );
      return rVICT_DIED;
   }

   if( wield )
      prof_bonus = weapon_prof_bonus_check( ch, wield, &prof_gsn );
   else
      prof_bonus = 0;

   if( dt == TYPE_UNDEFINED )
   {
      dt = TYPE_HIT;
      if( wield && wield->item_type == ITEM_MISSILE_WEAPON )
         dt += wield->value[3];
   }

   /*
    * Calculate to-hit-armor-class-0 versus armor.
    */
   if( IS_NPC( ch ) )
   {
      thac0_00 = ch->mobthac0;
      thac0_32 = 0;
   }
   else
   {
      thac0_00 = class_table[ch->Class]->thac0_00;
      thac0_32 = class_table[ch->Class]->thac0_32;
   }
   victim_ac = UMAX( -19, ( int )( GET_AC( victim ) / 10 ) );

   /*
    * if you can't see what's coming... 
    */
   if( !can_see_obj( victim, projectile ) )
      victim_ac += 1;
   if( !can_see( ch, victim ) )
      victim_ac -= 4;

   /*
    * Weapon proficiency bonus 
    */
   victim_ac += prof_bonus;

   /*
    * The moment of excitement!
    */
   while( ( diceroll = number_bits( 5 ) ) >= 20 )
      ;

   if( diceroll == 0 || ( diceroll != 19 && diceroll < victim_ac ) )
   {
      /*
       * Miss. 
       */
      if( prof_gsn != -1 )
         learn_from_failure( ch, prof_gsn );

      /*
       * Do something with the projectile 
       */
      if( number_percent(  ) < 50 )
         extract_obj( projectile );
      else
      {
         if( projectile->in_obj )
            obj_from_obj( projectile );
         if( projectile->carried_by )
            obj_from_char( projectile );
         obj_to_room( projectile, victim->in_room );
      }
//      damage( ch, victim, 0, dt );
      tail_chain(  );
      return rNONE;
   }

   /*
    * Hit.
    * Calc damage.
    */

   if( !wield )   /* dice formula fixed by Thoric */
      dam = proj_bonus;
   else
      dam = number_range( wield->value[1], wield->value[2] ) + ( proj_bonus / 10 );

   /*
    * Bonuses.
    */
   dam += GET_ATTACK( ch );

   if( prof_bonus )
      dam += prof_bonus / 4;


   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_enhanced_damage] > 0 )
   {
      dam += ( int )( dam * LEARNED( ch, gsn_enhanced_damage ) / 120 );
      learn_from_success( ch, gsn_enhanced_damage );
   }

   if( !IS_AWAKE( victim ) )
      dam *= 2;

   if( dam <= 0 )
      dam = 1;

   plusris = 0;

   if( IS_OBJ_STAT( projectile, ITEM_MAGIC ) )
      dam = ris_damage( victim, dam, RIS_MAGIC );
   else
      dam = ris_damage( victim, dam, RIS_NONMAGIC );

   /*
    * check for RIS_PLUSx                -Thoric 
    */
   if( dam )
   {
      int x, res, imm, sus, mod;

      if( plusris )
         plusris = RIS_PLUS1 << UMIN( plusris, 7 );

      /*
       * initialize values to handle a zero plusris 
       */
      imm = res = -1;
      sus = 1;

      /*
       * find high ris 
       */
      for( x = RIS_PLUS1; x <= RIS_PLUS6; x <<= 1 )
      {
         if( IS_SET( victim->immune, x ) )
            imm = x;
         if( IS_SET( victim->resistant, x ) )
            res = x;
         if( IS_SET( victim->susceptible, x ) )
            sus = x;
      }
      mod = 10;
      if( imm >= plusris )
         mod -= 10;
      if( res >= plusris )
         mod -= 2;
      if( sus <= plusris )
         mod += 2;

      /*
       * check if immune 
       */
      if( mod <= 0 )
         dam = -1;
      if( mod != 10 )
         dam = ( dam * mod ) / 10;
   }

   if( prof_gsn != -1 )
   {
      if( dam > 0 )
         learn_from_success( ch, prof_gsn );
      else
         learn_from_failure( ch, prof_gsn );
   }

   /*
    * immune to damage 
    */
   if( dam == -1 )
   {
      if( dt >= 0 && dt < num_skills )
      {
         SKILLTYPE *skill = skill_table[dt];
         bool found = FALSE;

         if( skill->imm_char && skill->imm_char[0] != '\0' )
         {
            act( AT_HIT, skill->imm_char, ch, NULL, victim, TO_CHAR );
            found = TRUE;
         }
         if( skill->imm_vict && skill->imm_vict[0] != '\0' )
         {
            act( AT_HITME, skill->imm_vict, ch, NULL, victim, TO_VICT );
            found = TRUE;
         }
         if( skill->imm_room && skill->imm_room[0] != '\0' )
         {
            act( AT_ACTION, skill->imm_room, ch, NULL, victim, TO_NOTVICT );
            found = TRUE;
         }
         if( found )
         {
            if( number_percent(  ) < 50 )
               extract_obj( projectile );
            else
            {
               if( projectile->in_obj )
                  obj_from_obj( projectile );
               if( projectile->carried_by )
                  obj_from_char( projectile );
               obj_to_room( projectile, victim->in_room );
            }
            return rNONE;
         }
      }
      dam = 0;
   }
//   if( ( retcode = damage( ch, victim, dam, dt ) ) != rNONE )
   {
      extract_obj( projectile );
      return rNONE;
   }
   if( char_died( ch ) )
   {
      extract_obj( projectile );
      return rCHAR_DIED;
   }
   if( char_died( victim ) )
   {
      extract_obj( projectile );
      return rVICT_DIED;
   }

   retcode = rNONE;
   if( dam == 0 )
   {
      if( number_percent(  ) < 50 )
         extract_obj( projectile );
      else
      {
         if( projectile->in_obj )
            obj_from_obj( projectile );
         if( projectile->carried_by )
            obj_from_char( projectile );
         obj_to_room( projectile, victim->in_room );
      }
      return retcode;
   }

   /*
    * weapon spells  -Thoric 
    */
   if( wield && !IS_SET( victim->immune, RIS_MAGIC ) && !xIS_SET( victim->in_room->room_flags, ROOM_NO_MAGIC ) )
   {
      AFFECT_DATA *aff;

      for( aff = wield->pIndexData->first_affect; aff; aff = aff->next )
         if( aff->location == APPLY_WEAPONSPELL && IS_VALID_SN( aff->modifier ) && skill_table[aff->modifier]->spell_fun )
            retcode = ( *skill_table[aff->modifier]->spell_fun ) ( aff->modifier, ( wield->level + 3 ) / 3, ch, victim );
      if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      {
         extract_obj( projectile );
         return retcode;
      }
      for( aff = wield->first_affect; aff; aff = aff->next )
         if( aff->location == APPLY_WEAPONSPELL && IS_VALID_SN( aff->modifier ) && skill_table[aff->modifier]->spell_fun )
            retcode = ( *skill_table[aff->modifier]->spell_fun ) ( aff->modifier, ( wield->level + 3 ) / 3, ch, victim );
      if( retcode != rNONE || char_died( ch ) || char_died( victim ) )
      {
         extract_obj( projectile );
         return retcode;
      }
   }

   extract_obj( projectile );

   tail_chain(  );
   return retcode;
}

/*
 * Calculate damage based on resistances, immunities and suceptibilities
 *					-Thoric
 */
short ris_damage( CHAR_DATA * ch, short dam, int ris )
{
   short modifier;

   modifier = 10;
   if( IS_SET( ch->immune, ris ) && !IS_SET( ch->no_immune, ris ) )
      modifier -= 10;
   if( IS_SET( ch->resistant, ris ) && !IS_SET( ch->no_resistant, ris ) )
      modifier -= 2;
   if( IS_SET( ch->susceptible, ris ) && !IS_SET( ch->no_susceptible, ris ) )
   {
      if( IS_NPC( ch ) && IS_SET( ch->immune, ris ) )
         modifier += 0;
      else
         modifier += 2;
   }
   if( modifier <= 0 )
      return -1;
   if( modifier == 10 )
      return dam;
   return ( dam * modifier ) / 10;
}

ch_ret damage( CHAR_DATA * ch, CHAR_DATA * victim, int dam, int dt )
{
   EXT_BV damtype;
   xSET_BITS( damtype, ch->damtype );

   return damage( ch, victim, dam, dt, HIT_BODY, FALSE, damtype );
}

/*
 * Inflict damage from a hit.   This is one damn big function.
 */
ch_ret damage( CHAR_DATA * ch, CHAR_DATA * victim, int dam, int dt, int hit_wear, bool crit, EXT_BV damtype )
{
   char log_buf[MAX_STRING_LENGTH];
   char filename[256];
   bool npcvict;
   bool loot;
   ch_ret retcode;
   short dampmod;
   CHAR_DATA *gch /*, *lch */ ;
   short anopc = 0;  /* # of (non-pkill) pc in a (ch) */
   short bnopc = 0;  /* # of (non-pkill) pc in b (victim) */


   retcode = rNONE;

   if( !ch )
   {
      bug( "%s", "Damage: null ch!" );
      return rERROR;
   }
   if( !victim )
   {
      bug( "%s", "Damage: null victim!" );
      return rVICT_DIED;
   }

   if( victim->position == POS_DEAD )
      return rVICT_DIED;

   npcvict = IS_NPC( victim );

   /*
    * Need to rewrite damage resistances here           -Thoric
    */
   {
      if( dam == -1 )
      {
         if( dt >= 0 && dt < num_skills )
         {
            bool found = FALSE;
            SKILLTYPE *skill = skill_table[dt];

            if( skill->imm_char && skill->imm_char[0] != '\0' )
            {
               act( AT_HIT, skill->imm_char, ch, NULL, victim, TO_CHAR );
               found = TRUE;
            }
            if( skill->imm_vict && skill->imm_vict[0] != '\0' )
            {
               act( AT_HITME, skill->imm_vict, ch, NULL, victim, TO_VICT );
               found = TRUE;
            }
            if( skill->imm_room && skill->imm_room[0] != '\0' )
            {
               act( AT_ACTION, skill->imm_room, ch, NULL, victim, TO_NOTVICT );
               found = TRUE;
            }
            if( found )
               return rNONE;
         }
         dam = 0;
      }
   }

   /*
    * Precautionary step mainly to prevent people in Hell from finding
    * a way out. --Shaddai
    */
   if( xIS_SET( victim->in_room->room_flags, ROOM_SAFE ) )
      dam = 0;

   log_string( victim->name );

   if( dam && npcvict && ch != victim )
   {
      if( !xIS_SET( victim->act, ACT_SENTINEL ) )
      {
         if( victim->hunting )
         {
            if( victim->hunting->who != ch )
            {
               STRFREE( victim->hunting->name );
               victim->hunting->name = QUICKLINK( ch->name );
               victim->hunting->who = ch;
            }
         }
         else if( !xIS_SET( victim->act, ACT_PACIFIST ) )   /* Gorog */
            start_hunting( victim, ch );
      }

      if( victim->hating )
      {
         if( victim->hating->who != ch )
         {
            STRFREE( victim->hating->name );
            victim->hating->name = QUICKLINK( ch->name );
            victim->hating->who = ch;
         }
      }
      else if( !xIS_SET( victim->act, ACT_PACIFIST ) )   /* Gorog */
         start_hating( victim, ch );
   }

   if( victim != ch )
   {
      /*
       * Certain attacks are forbidden.
       * Most other attacks are returned.
       */
      if( is_safe( ch, victim, TRUE ) )
         return rNONE;
      check_attacker( ch, victim );

      if( victim->position > POS_STUNNED )
      {
         if( !victim->target )
            set_new_target( victim, get_target_2( victim, ch, -1 ) );

         /*
          * vwas: victim->position = POS_FIGHTING;
          * modified for NPC only - Davenge
          */
         if( IS_NPC( victim ) && victim->fighting )
            victim->position = POS_FIGHTING;

         /*
          * If victim is charmed, ch might attack victim's master.
          */
         if( IS_NPC( ch )
             && npcvict
             && IS_AFFECTED( victim, AFF_CHARM )
             && victim->master && victim->master->in_room == ch->in_room && number_bits( 3 ) == 0 )
         {
            stop_fighting( ch, FALSE );
            retcode = multi_hit( ch, victim->master, TYPE_UNDEFINED );
            return retcode;
         }
      }


      /*
       * More charm stuff.
       */
      if( victim->master == ch )
         stop_follower( victim );

      /*
       * Pkill stuff.  If a deadly attacks another deadly or is attacked by
       * one, then ungroup any nondealies.  Disabled untill I can figure out
       * the right way to do it.
       */

      /*
       * count the # of non-pkill pc in a ( not including == ch ) 
       */
      for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
         if( is_same_group( ch, gch ) && !IS_NPC( gch ) && !IS_PKILL( gch ) && ( ch != gch ) )
            anopc++;

      /*
       * count the # of non-pkill pc in b ( not including == victim ) 
       */
      for( gch = victim->in_room->first_person; gch; gch = gch->next_in_room )
         if( is_same_group( victim, gch ) && !IS_NPC( gch ) && !IS_PKILL( gch ) && ( victim != gch ) )
            bnopc++;

      /*
       * only consider disbanding if both groups have 1(+) non-pk pc,
       * or when one participant is pc, and the other group has 1(+)
       * pk pc's (in the case that participant is only pk pc in group) 
       */
      if( ( bnopc > 0 && anopc > 0 ) || ( bnopc > 0 && !IS_NPC( ch ) ) || ( anopc > 0 && !IS_NPC( victim ) ) )
      {
         /*
          * Disband from same group first 
          */
         if( is_same_group( ch, victim ) )
         {
            /*
             * Messages to char and master handled in stop_follower 
             */
            act( AT_ACTION, "$n disbands from $N's group.",
                 ( ch->leader == victim ) ? victim : ch, NULL,
                 ( ch->leader == victim ) ? victim->master : ch->master, TO_NOTVICT );
            if( ch->leader == victim )
               stop_follower( victim );
            else
               stop_follower( ch );
         }
         /*
          * if leader isnt pkill, leave the group and disband ch 
          */
         if( ch->leader != NULL && !IS_NPC( ch->leader ) && !IS_PKILL( ch->leader ) )
         {
            act( AT_ACTION, "$n disbands from $N's group.", ch, NULL, ch->master, TO_NOTVICT );
            stop_follower( ch );
         }
         else
         {
            for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
               if( is_same_group( gch, ch ) && !IS_NPC( gch ) && !IS_PKILL( gch ) && gch != ch )
               {
                  act( AT_ACTION, "$n disbands from $N's group.", ch, NULL, gch->master, TO_NOTVICT );
                  stop_follower( gch );
               }
         }
         /*
          * if leader isnt pkill, leave the group and disband victim 
          */
         if( victim->leader != NULL && !IS_NPC( victim->leader ) && !IS_PKILL( victim->leader ) )
         {
            act( AT_ACTION, "$n disbands from $N's group.", victim, NULL, victim->master, TO_NOTVICT );
            stop_follower( victim );
         }
         else
         {
            for( gch = victim->in_room->first_person; gch; gch = gch->next_in_room )
               if( is_same_group( gch, victim ) && !IS_NPC( gch ) && !IS_PKILL( gch ) && gch != victim )
               {
                  act( AT_ACTION, "$n disbands from $N's group.", gch, NULL, gch->master, TO_NOTVICT );
                  stop_follower( gch );
               }
         }
      }

      /*
       * Inviso attacks ... not.
       */
      if( IS_AFFECTED( ch, AFF_INVISIBLE ) )
      {
         affect_strip( ch, gsn_invis );
         affect_strip( ch, gsn_mass_invis );
         xREMOVE_BIT( ch->affected_by, AFF_INVISIBLE );
         act( AT_MAGIC, "$n fades into existence.", ch, NULL, NULL, TO_ROOM );
      }

      /*
       * Take away Hide 
       */
      if( IS_AFFECTED( ch, AFF_HIDE ) )
         xREMOVE_BIT( ch->affected_by, AFF_HIDE );
      if( dt >= TYPE_HIT || is_physical( &skill_table[dt]->damtype ) )
      {

         if( check_parry( ch, victim ) )
            return rNONE;
         if( check_dodge( ch, victim ) )
            return rNONE;
//         if( check_counter( ch, victim ) )
  //          return rNONE;

      }
    //  if( check_phase( ch, victim ) )
      //   return rNONE;

      /*
       * Check control panel settings and modify damage
       */
      if( IS_NPC( ch ) )
      {
         if( npcvict )
            dampmod = sysdata.dam_mob_vs_mob;
         else
            dampmod = sysdata.dam_mob_vs_plr;
      }
      else
      {
         if( npcvict )
            dampmod = sysdata.dam_plr_vs_mob;
         else
            dampmod = sysdata.dam_plr_vs_plr;
      }
      if( dampmod > 0 )
         dam = ( dam * dampmod ) / 100;
   }

   if( ch != victim )
      new_dam_message( ch, victim, dam, dt, hit_wear, crit, damtype );

   /*
    * Hurt the victim.
    * Inform the victim of his new state.
    */
   victim->hit -= dam;

   if( !IS_NPC( victim ) && victim->level >= LEVEL_IMMORTAL && victim->hit < 1 )
      victim->hit = 1;

   /*
    * Make sure newbies dont die 
    */
   if( !IS_NPC( victim ) && NOT_AUTHED( victim ) && victim->hit < 1 )
      victim->hit = 1;

   if( dam > 0 && dt > TYPE_HIT
       && !IS_AFFECTED( victim, AFF_POISON )
       && is_wielding_poisoned( ch ) && !IS_SET( victim->immune, RIS_POISON ) && !saves_poison_death( ch->level, victim ) )
   {
      AFFECT_DATA af;

      af.type = gsn_poison;
      af.duration = 20;
      af.location = APPLY_STR;
      af.modifier = -2;
      af.bitvector = meb( AFF_POISON );
      affect_join( victim, &af );
      victim->mental_state = URANGE( 20, victim->mental_state + ( IS_PKILL( victim ) ? 1 : 2 ), 100 );
   }

   if( !npcvict && get_trust( victim ) >= LEVEL_IMMORTAL && get_trust( ch ) >= LEVEL_IMMORTAL && victim->hit < 1 )
      victim->hit = 1;
   update_pos( victim );

   switch ( victim->position )
   {
      case POS_MORTAL:
         act( AT_DYING, "$n is mortally wounded, and will die soon, if not aided.", victim, NULL, NULL, TO_ROOM );
         act( AT_DANGER, "You are mortally wounded, and will die soon, if not aided.", victim, NULL, NULL, TO_CHAR );
         break;

      case POS_INCAP:
         act( AT_DYING, "$n is incapacitated and will slowly die, if not aided.", victim, NULL, NULL, TO_ROOM );
         act( AT_DANGER, "You are incapacitated and will slowly die, if not aided.", victim, NULL, NULL, TO_CHAR );
         break;

      case POS_STUNNED:
         if( !IS_AFFECTED( victim, AFF_PARALYSIS ) )
         {
            act( AT_ACTION, "$n is stunned, but will probably recover.", victim, NULL, NULL, TO_ROOM );
            act( AT_HURT, "You are stunned, but will probably recover.", victim, NULL, NULL, TO_CHAR );
         }
         break;

      case POS_DEAD:
         if( dt >= 0 && dt < num_skills )
         {
            SKILLTYPE *skill = skill_table[dt];

            if( skill->die_char && skill->die_char[0] != '\0' )
               act( AT_DEAD, skill->die_char, ch, NULL, victim, TO_CHAR );
            if( skill->die_vict && skill->die_vict[0] != '\0' )
               act( AT_DEAD, skill->die_vict, ch, NULL, victim, TO_VICT );
            if( skill->die_room && skill->die_room[0] != '\0' )
               act( AT_DEAD, skill->die_room, ch, NULL, victim, TO_NOTVICT );
         }
         act( AT_DEAD, "$n is DEAD!!", victim, 0, 0, TO_ROOM );
         act( AT_DEAD, "You have been KILLED!!\r\n", victim, 0, 0, TO_CHAR );
         break;
   }

   /*
    * Sleep spells and extremely wounded folks.
    */
   if( !IS_AWAKE( victim ) /* lets make NPC's not slaughter PC's */
       && !IS_AFFECTED( victim, AFF_PARALYSIS ) )
   {
      if( victim->fighting && victim->fighting->who->hunting && victim->fighting->who->hunting->who == victim )
         stop_hunting( victim->fighting->who );

      if( victim->fighting && victim->fighting->who->hating && victim->fighting->who->hating->who == victim )
         stop_hating( victim->fighting->who );

      if( !npcvict && IS_NPC( ch ) )
         stop_fighting( victim, TRUE );
      else
         stop_fighting( victim, FALSE );
   }

   /*
    * Payoff for killing things.
    */
   if( victim->position == POS_DEAD )
   {
      OBJ_DATA *new_corpse;

      clear_target( ch );
      clear_target( victim );

      stop_fighting( ch, TRUE );

      group_gain( ch, victim );

      if( !npcvict )
      {
         snprintf( log_buf, MAX_STRING_LENGTH, "%s (%d) killed by %s at %d",
                   victim->name, victim->level, ( IS_NPC( ch ) ? ch->short_descr : ch->name ), victim->in_room->vnum );
         log_string( log_buf );
         to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );

         if( !IS_NPC( ch ) && !IS_IMMORTAL( ch ) && ch->pcdata->clan
             && ch->pcdata->clan->clan_type != CLAN_ORDER && ch->pcdata->clan->clan_type != CLAN_GUILD && victim != ch )
         {
            snprintf( filename, 256, "%s%s.record", CLAN_DIR, ch->pcdata->clan->name );
            snprintf( log_buf, MAX_STRING_LENGTH, "&P(%2d) %-12s &wvs &P(%2d) %s &P%s ... &w%s",
                      ch->level,
                      ch->name,
                      victim->level,
                      !CAN_PKILL( victim ) ? "&W<Peaceful>" :
                      victim->pcdata->clan ? victim->pcdata->clan->badge :
                      "&P(&WUnclanned&P)", victim->name, ch->in_room->area->name );
            if( victim->pcdata->clan && victim->pcdata->clan->name == ch->pcdata->clan->name )
               ;
            else
               append_to_file( filename, log_buf );
         }

         /*
          * Dying penalty:
          * 1/2 way back to previous level.
          */
         if( victim->experience[victim->Class] > exp_level( victim, victim->level ) )
            gain_exp( victim, ( exp_level( victim, victim->level ) - victim->experience[victim->Class] ) / 2 );

         /*
          * New penalty... go back to the beginning of current level.
          victim->experience[victim->Class] = exp_level( victim, victim->level );
          */
      }
      else if( !IS_NPC( ch ) && IS_NPC( victim ) ) /* keep track of mob vnum killed */
      {
         add_kill( ch, victim );

         /*
          * Add to kill tracker for grouped chars, as well. -Halcyon
          */
         for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
            if( is_same_group( gch, ch ) && !IS_NPC( gch ) && gch != ch )
               add_kill( gch, victim );
      }

      check_killer( ch, victim );

      if( ch->in_room == victim->in_room )
         loot = legal_loot( ch, victim );
      else
         loot = FALSE;

      set_cur_char( victim );
      new_corpse = raw_kill( ch, victim );
      victim = NULL;

      if( !IS_NPC( ch ) && loot && new_corpse && new_corpse->item_type == ITEM_CORPSE_NPC
          && new_corpse->in_room == ch->in_room && can_see_obj( ch, new_corpse ) && ch->position > POS_SLEEPING )
      {
         /*
          * Autogold by Scryn 8/12 
          */
         if( xIS_SET( ch->act, PLR_AUTOGOLD ) && !loot_coins_from_corpse( ch, new_corpse ) )
            return rBOTH_DIED;

         if( new_corpse && !obj_extracted( new_corpse ) && new_corpse->in_room == ch->in_room
             && ch->position > POS_SLEEPING && can_see_obj( ch, new_corpse ) )
         {
            if( xIS_SET( ch->act, PLR_AUTOLOOT ) )
               do_get( ch, "all corpse" );
            else
               do_look( ch, "in corpse" );
            if( !char_died( ch ) && xIS_SET( ch->act, PLR_AUTOSAC ) && !obj_extracted( new_corpse )
                && new_corpse->in_room == ch->in_room && ch->position > POS_SLEEPING && can_see_obj( ch, new_corpse ) )
               do_sacrifice( ch, "corpse" );
         }
      }

      if( IS_SET( sysdata.save_flags, SV_KILL ) )
         save_char_obj( ch );
      return rVICT_DIED;
   }

   if( victim == ch )
      return rNONE;

   /*
    * Take care of link dead people.
    */
   if( !npcvict && !victim->desc && !IS_SET( victim->pcdata->flags, PCFLAG_NORECALL ) )
   {
      if( number_range( 0, victim->wait ) == 0 )
      {
         do_recall( victim, "" );
         return rNONE;
      }
   }

   /*
    * Wimp out?
    */
   if( npcvict && dam > 0 )
   {
      if( ( xIS_SET( victim->act, ACT_WIMPY ) && number_bits( 1 ) == 0
            && victim->hit < victim->max_hit / 2 )
          || ( IS_AFFECTED( victim, AFF_CHARM ) && victim->master && victim->master->in_room != victim->in_room ) )
      {
         start_fearing( victim, ch );
         stop_hunting( victim );
         do_flee( victim, "" );
      }
   }

   if( !npcvict && victim->hit > 0 && victim->hit <= victim->wimpy && victim->wait == 0 )
      do_flee( victim, "" );
   else if( !npcvict && xIS_SET( victim->act, PLR_FLEE ) )
      do_flee( victim, "" );

   tail_chain(  );
   return rNONE;
}



/*
 * Changed is_safe to have the show_messg boolian.  This is so if you don't
 * want to show why you can't kill someone you can't turn it off.  This is
 * useful for things like area attacks.  --Shaddai
 */
bool is_safe( CHAR_DATA * ch, CHAR_DATA * victim, bool show_messg )
{
   if( char_died( victim ) || char_died( ch ) )
      return TRUE;

   /*
    * Thx Josh! 
    */
   if( who_fighting( ch ) == ch )
      return FALSE;

   if( !victim )  /*Gonna find this is_safe crash bug -Blod */
   {
      bug( "Is_safe: %s opponent does not exist!", ch->name );
      return TRUE;
   }
   if( !victim->in_room )
   {
      bug( "Is_safe: %s has no physical location!", victim->name );
      return TRUE;
   }

   if( xIS_SET( victim->in_room->room_flags, ROOM_SAFE ) )
   {
      if( show_messg )
      {
         set_char_color( AT_MAGIC, ch );
         send_to_char( "A magical force prevents you from attacking.\r\n", ch );
      }
      return TRUE;
   }

   if( IS_PACIFIST( ch ) ) /* Fireblade */
   {
      if( show_messg )
      {
         set_char_color( AT_MAGIC, ch );
         ch_printf( ch, "You are a pacifist and will not fight.\r\n" );
      }
      return TRUE;
   }

   if( IS_PACIFIST( victim ) )   /* Gorog */
   {
      char buf[MAX_STRING_LENGTH];
      if( show_messg )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s is a pacifist and will not fight.\r\n", capitalize( victim->short_descr ) );
         set_char_color( AT_MAGIC, ch );
         send_to_char( buf, ch );
      }
      return TRUE;
   }

   if( !IS_NPC( ch ) && ch->level >= LEVEL_IMMORTAL )
      return FALSE;

   if( !IS_NPC( ch ) && !IS_NPC( victim ) && ch != victim && IS_SET( victim->in_room->area->flags, AFLAG_NOPKILL ) )
   {
      if( show_messg )
      {
         set_char_color( AT_IMMORT, ch );
         send_to_char( "The gods have forbidden player killing in this area.\r\n", ch );
      }
      return TRUE;
   }

   if( IS_NPC( ch ) || IS_NPC( victim ) )
      return FALSE;

   if( calculate_age( ch ) < 18 || ch->level < 5 )
   {
      if( show_messg )
      {
         set_char_color( AT_WHITE, ch );
         send_to_char( "You are not yet ready, needing age or experience, if not both. \r\n", ch );
      }
      return TRUE;
   }

   if( calculate_age( victim ) < 18 || victim->level < 5 )
   {
      if( show_messg )
      {
         set_char_color( AT_WHITE, ch );
         send_to_char( "They are yet too young to die.\r\n", ch );
      }
      return TRUE;
   }

   if( ch->level - victim->level > 5 || victim->level - ch->level > 5 )
   {
      if( show_messg )
      {
         set_char_color( AT_IMMORT, ch );
         send_to_char( "The gods do not allow murder when there is such a difference in level.\r\n", ch );
      }
      return TRUE;
   }

   if( get_timer( victim, TIMER_PKILLED ) > 0 )
   {
      if( show_messg )
      {
         set_char_color( AT_GREEN, ch );
         send_to_char( "That character has died within the last 5 minutes.\r\n", ch );
      }
      return TRUE;
   }

   if( get_timer( ch, TIMER_PKILLED ) > 0 )
   {
      if( show_messg )
      {
         set_char_color( AT_GREEN, ch );
         send_to_char( "You have been killed within the last 5 minutes.\r\n", ch );
      }
      return TRUE;
   }

   return FALSE;
}

/*
 * just verify that a corpse looting is legal
 */
bool legal_loot( CHAR_DATA * ch, CHAR_DATA * victim )
{
   /*
    * anyone can loot mobs 
    */
   if( IS_NPC( victim ) )
      return TRUE;
   /*
    * non-charmed mobs can loot anything 
    */
   if( IS_NPC( ch ) && !ch->master )
      return TRUE;
   /*
    * members of different clans can loot too! -Thoric 
    */
   if( !IS_NPC( ch ) && !IS_NPC( victim )
       && IS_SET( ch->pcdata->flags, PCFLAG_DEADLY ) && IS_SET( victim->pcdata->flags, PCFLAG_DEADLY ) )
      return TRUE;
   return FALSE;
}

/*
 * See if an attack justifies a KILLER flag.
 */
void check_killer( CHAR_DATA * ch, CHAR_DATA * victim )
{
   /*
    * NPC's are fair game.
    */
   if( IS_NPC( victim ) )
   {
      if( !IS_NPC( ch ) )
      {
         int level_ratio;
         /*
          * Fix for crashes when killing mobs of level 0
          * * by Joe Fabiano -rinthos@yahoo.com
          * * on 03-16-03.
          */
         if( victim->level < 1 )
            level_ratio = URANGE( 1, ch->level, MAX_LEVEL );
         else
            level_ratio = URANGE( 1, ch->level / victim->level, MAX_LEVEL );
         if( ch->pcdata->clan )
            ch->pcdata->clan->mkills++;
         ch->pcdata->mkills++;
         ch->in_room->area->mkills++;
         if( ch->pcdata->deity )
         {
            if( victim->race == ch->pcdata->deity->npcrace )
               adjust_favor( ch, 3, level_ratio );
            else if( victim->race == ch->pcdata->deity->npcfoe )
               adjust_favor( ch, 17, level_ratio );
            else
               adjust_favor( ch, 2, level_ratio );
         }
      }
      return;
   }



   /*
    * If you kill yourself nothing happens.
    */

   if( ch == victim || ch->level >= LEVEL_IMMORTAL )
      return;

   /*
    * Any character in the arena is ok to kill.
    * Added pdeath and pkills here
    */
   if( in_arena( ch ) )
   {
      if( !IS_NPC( ch ) && !IS_NPC( victim ) )
      {
         ch->pcdata->pkills++;
         victim->pcdata->pdeaths++;
      }
      return;
   }

   /*
    * So are killers and thieves.
    */
   if( xIS_SET( victim->act, PLR_KILLER ) || xIS_SET( victim->act, PLR_THIEF ) )
   {
      if( !IS_NPC( ch ) )
      {
         if( ch->pcdata->clan )
         {
            if( victim->level < 10 )
               ch->pcdata->clan->pkills[0]++;
            else if( victim->level < 15 )
               ch->pcdata->clan->pkills[1]++;
            else if( victim->level < 20 )
               ch->pcdata->clan->pkills[2]++;
            else if( victim->level < 30 )
               ch->pcdata->clan->pkills[3]++;
            else if( victim->level < 40 )
               ch->pcdata->clan->pkills[4]++;
            else if( victim->level < 50 )
               ch->pcdata->clan->pkills[5]++;
            else
               ch->pcdata->clan->pkills[6]++;
         }
         ch->pcdata->pkills++;
         ch->in_room->area->pkills++;
      }
      return;
   }

   /*
    * clan checks               -Thoric 
    */
   if( !IS_NPC( ch ) && !IS_NPC( victim )
       && IS_SET( ch->pcdata->flags, PCFLAG_DEADLY ) && IS_SET( victim->pcdata->flags, PCFLAG_DEADLY ) )
   {
      /*
       * not of same clan? Go ahead and kill!!! 
       */
      if( !ch->pcdata->clan
          || !victim->pcdata->clan
          || ( ch->pcdata->clan->clan_type != CLAN_NOKILL
               && victim->pcdata->clan->clan_type != CLAN_NOKILL && ch->pcdata->clan != victim->pcdata->clan ) )
      {
         if( ch->pcdata->clan )
         {
            if( victim->level < 10 )
               ch->pcdata->clan->pkills[0]++;
            else if( victim->level < 15 )
               ch->pcdata->clan->pkills[1]++;
            else if( victim->level < 20 )
               ch->pcdata->clan->pkills[2]++;
            else if( victim->level < 30 )
               ch->pcdata->clan->pkills[3]++;
            else if( victim->level < 40 )
               ch->pcdata->clan->pkills[4]++;
            else if( victim->level < 50 )
               ch->pcdata->clan->pkills[5]++;
            else
               ch->pcdata->clan->pkills[6]++;
         }
         ch->pcdata->pkills++;
         ch->hit = ch->max_hit;
         ch->mana = ch->max_mana;
         ch->move = ch->max_move;
         if( ch->pcdata )
            ch->pcdata->condition[COND_BLOODTHIRST] = ( 10 + ch->level );
         update_pos( victim );
         if( victim != ch )
         {
            act( AT_MAGIC, "Bolts of blue energy rise from the corpse, seeping into $n.", ch, victim->name, NULL, TO_ROOM );
            act( AT_MAGIC, "Bolts of blue energy rise from the corpse, seeping into you.", ch, victim->name, NULL, TO_CHAR );
         }
         if( victim->pcdata->clan )
         {
            if( ch->level < 10 )
               victim->pcdata->clan->pdeaths[0]++;
            else if( ch->level < 15 )
               victim->pcdata->clan->pdeaths[1]++;
            else if( ch->level < 20 )
               victim->pcdata->clan->pdeaths[2]++;
            else if( ch->level < 30 )
               victim->pcdata->clan->pdeaths[3]++;
            else if( ch->level < 40 )
               victim->pcdata->clan->pdeaths[4]++;
            else if( ch->level < 50 )
               victim->pcdata->clan->pdeaths[5]++;
            else
               victim->pcdata->clan->pdeaths[6]++;
         }
         victim->pcdata->pdeaths++;
         adjust_favor( victim, 11, 1 );
         adjust_favor( ch, 2, 1 );
         add_timer( victim, TIMER_PKILLED, 115, NULL, 0 );
         WAIT_STATE( victim, 3 * PULSE_VIOLENCE );
         /*
          * xSET_BIT(victim->act, PLR_PK); 
          */
         return;
      }
   }

   /*
    * Charm-o-rama.
    */
   if( IS_AFFECTED( ch, AFF_CHARM ) )
   {
      if( !ch->master )
      {
         bug( "Check_killer: %s bad AFF_CHARM", IS_NPC( ch ) ? ch->short_descr : ch->name );
         affect_strip( ch, gsn_charm_person );
         xREMOVE_BIT( ch->affected_by, AFF_CHARM );
         return;
      }

      /*
       * stop_follower( ch ); 
       */
      if( ch->master )
         check_killer( ch->master, victim );
      return;
   }

   /*
    * NPC's are cool of course (as long as not charmed).
    * Hitting yourself is cool too (bleeding).
    * So is being immortal (Alander's idea).
    * And current killers stay as they are.
    */
   if( IS_NPC( ch ) )
   {
      if( !IS_NPC( victim ) )
      {
         int level_ratio;
         if( victim->pcdata->clan )
            victim->pcdata->clan->mdeaths++;
         victim->pcdata->mdeaths++;
         victim->in_room->area->mdeaths++;
         level_ratio = URANGE( 1, ch->level / victim->level, LEVEL_AVATAR );
         if( victim->pcdata->deity )
         {
            if( ch->race == victim->pcdata->deity->npcrace )
               adjust_favor( victim, 12, level_ratio );
            else if( ch->race == victim->pcdata->deity->npcfoe )
               adjust_favor( victim, 15, level_ratio );
            else
               adjust_favor( victim, 11, level_ratio );
         }
      }
      return;
   }


   if( !IS_NPC( ch ) )
   {
      if( ch->pcdata->clan )
         ch->pcdata->clan->illegal_pk++;
      ch->pcdata->illegal_pk++;
      ch->in_room->area->illegal_pk++;
   }
   if( !IS_NPC( victim ) )
   {
      if( victim->pcdata->clan )
      {
         if( ch->level < 10 )
            victim->pcdata->clan->pdeaths[0]++;
         else if( ch->level < 15 )
            victim->pcdata->clan->pdeaths[1]++;
         else if( ch->level < 20 )
            victim->pcdata->clan->pdeaths[2]++;
         else if( ch->level < 30 )
            victim->pcdata->clan->pdeaths[3]++;
         else if( ch->level < 40 )
            victim->pcdata->clan->pdeaths[4]++;
         else if( ch->level < 50 )
            victim->pcdata->clan->pdeaths[5]++;
         else
            victim->pcdata->clan->pdeaths[6]++;
      }
      victim->pcdata->pdeaths++;
      victim->in_room->area->pdeaths++;
   }

   if( xIS_SET( ch->act, PLR_KILLER ) )
      return;

   set_char_color( AT_WHITE, ch );
   send_to_char( "A strange feeling grows deep inside you, and a tingle goes up your spine...\r\n", ch );
   set_char_color( AT_IMMORT, ch );
   send_to_char( "A deep voice booms inside your head, 'Thou shall now be known as a deadly murderer!!!'\r\n", ch );
   set_char_color( AT_WHITE, ch );
   send_to_char( "You feel as if your soul has been revealed for all to see.\r\n", ch );
   xSET_BIT( ch->act, PLR_KILLER );
   if( xIS_SET( ch->act, PLR_ATTACKER ) )
      xREMOVE_BIT( ch->act, PLR_ATTACKER );
   save_char_obj( ch );
   return;
}

/*
 * See if an attack justifies a ATTACKER flag.
 */
void check_attacker( CHAR_DATA * ch, CHAR_DATA * victim )
{

/* 
 * Made some changes to this function Apr 6/96 to reduce the prolifiration
 * of attacker flags in the realms. -Narn
 */
   /*
    * NPC's are fair game.
    * So are killers and thieves.
    */
   if( IS_NPC( victim ) || xIS_SET( victim->act, PLR_KILLER ) || xIS_SET( victim->act, PLR_THIEF ) )
      return;

   /*
    * deadly char check 
    */
   if( !IS_NPC( ch ) && !IS_NPC( victim ) && CAN_PKILL( ch ) && CAN_PKILL( victim ) )
      return;

/* Pkiller versus pkiller will no longer ever make an attacker flag
    { if ( !(ch->pcdata->clan && victim->pcdata->clan
      && ch->pcdata->clan == victim->pcdata->clan ) )  return; }
*/

   /*
    * Charm-o-rama.
    */
   if( IS_AFFECTED( ch, AFF_CHARM ) )
   {
      if( !ch->master )
      {
         bug( "Check_attacker: %s bad AFF_CHARM", IS_NPC( ch ) ? ch->short_descr : ch->name );
         affect_strip( ch, gsn_charm_person );
         xREMOVE_BIT( ch->affected_by, AFF_CHARM );
         return;
      }

      /*
       * Won't have charmed mobs fighting give the master an attacker 
       * flag.  The killer flag stays in, and I'll put something in 
       * do_murder. -Narn 
       */
      /*
       * xSET_BIT(ch->master->act, PLR_ATTACKER);
       */
      /*
       * stop_follower( ch ); 
       */
      return;
   }

   /*
    * NPC's are cool of course (as long as not charmed).
    * Hitting yourself is cool too (bleeding).
    * So is being immortal (Alander's idea).
    * And current killers stay as they are.
    */
   if( IS_NPC( ch )
       || ch == victim || ch->level >= LEVEL_IMMORTAL || xIS_SET( ch->act, PLR_ATTACKER ) || xIS_SET( ch->act, PLR_KILLER ) )
      return;

   xSET_BIT( ch->act, PLR_ATTACKER );
   save_char_obj( ch );
   return;
}


/*
 * Set position of a victim.
 */
void update_pos( CHAR_DATA * victim )
{
   if( !victim )
   {
      bug( "%s", "update_pos: null victim" );
      return;
   }

   if( victim->hit > 0 )
   {
      if( victim->position <= POS_STUNNED )
         victim->position = POS_STANDING;
      if( IS_AFFECTED( victim, AFF_PARALYSIS ) )
         victim->position = POS_STUNNED;
      return;
   }

   if( IS_NPC( victim ) || victim->hit <= -11 )
   {
      if( victim->mount )
      {
         act( AT_ACTION, "$n falls from $N.", victim, NULL, victim->mount, TO_ROOM );
         xREMOVE_BIT( victim->mount->act, ACT_MOUNTED );
         victim->mount = NULL;
      }
      victim->position = POS_DEAD;
      return;
   }

   if( victim->hit <= -6 )
      victim->position = POS_MORTAL;
   else if( victim->hit <= -3 )
      victim->position = POS_INCAP;
   else
      victim->position = POS_STUNNED;

   if( victim->position > POS_STUNNED && IS_AFFECTED( victim, AFF_PARALYSIS ) )
      victim->position = POS_STUNNED;

   if( victim->mount )
   {
      act( AT_ACTION, "$n falls unconscious from $N.", victim, NULL, victim->mount, TO_ROOM );
      xREMOVE_BIT( victim->mount->act, ACT_MOUNTED );
      victim->mount = NULL;
   }
   return;
}


/*
 * Start fights.
 */
void set_fighting( CHAR_DATA * ch, CHAR_DATA * victim )
{
   FIGHT_DATA *fight;

   if( ch->fighting )
   {
      bug( "Set_fighting: %s -> %s (already fighting %s)", ch->name, victim->name, ch->fighting->who->name );
      return;
   }

   if( IS_AFFECTED( ch, AFF_SLEEP ) )
      affect_strip( ch, gsn_sleep );

   /*
    * Limit attackers -Thoric 
    */
   if( victim->num_fighting > MAX_FIGHT )
   {
      send_to_char( "There are too many people fighting for you to join in.\r\n", ch );
      return;
   }

   CREATE( fight, FIGHT_DATA, 1 );
   fight->who = victim;
   fight->xp = ( int )( xp_compute( ch, victim ) * 0.85 );
   fight->align = align_compute( ch, victim );
   if( !IS_NPC( ch ) && IS_NPC( victim ) )
      fight->timeskilled = times_killed( ch, victim );
   ch->num_fighting = 1;
   ch->fighting = fight;

   if( !ch->target )
      set_new_target( ch, get_target_2( ch, victim, -1 ) );
   if( !victim->target )
      set_new_target( victim, get_target_2( victim, ch, -1 ) );

   victim->num_fighting++;
   if( victim->switched && IS_AFFECTED( victim->switched, AFF_POSSESS ) )
   {
      send_to_char( "You are disturbed!\r\n", victim->switched );
      do_return( victim->switched, "" );
   }
   return;
}


CHAR_DATA *who_fighting( CHAR_DATA * ch )
{
   if( !ch )
   {
      bug( "%s", "who_fighting: null ch" );
      return NULL;
   }
   if( !ch->fighting )
      return NULL;
   return ch->fighting->who;
}

void free_fight( CHAR_DATA * ch )
{
   if( !ch )
   {
      bug( "%s", "Free_fight: null ch!" );
      return;
   }
   if( ch->fighting )
   {
      if( !char_died( ch->fighting->who ) )
         --ch->fighting->who->num_fighting;
      DISPOSE( ch->fighting );
   }
   ch->fighting = NULL;
   if( ch->mount )
      ch->position = POS_MOUNTED;
   else
      ch->position = POS_STANDING;
   /*
    * Berserk wears off after combat. -- Altrag 
    */
   if( IS_AFFECTED( ch, AFF_BERSERK ) )
   {
      affect_strip( ch, gsn_berserk );
      set_char_color( AT_WEAROFF, ch );
      send_to_char( skill_table[gsn_berserk]->msg_off, ch );
      send_to_char( "\r\n", ch );
   }
   return;
}


/*
 * Stop fights.
 */
void stop_fighting( CHAR_DATA * ch, bool fBoth )
{
   CHAR_DATA *fch;

   free_fight( ch );
   update_pos( ch );

   if( !fBoth )   /* major short cut here by Thoric */
      return;

   for( fch = first_char; fch; fch = fch->next )
   {
      if( who_fighting( fch ) == ch )
      {
         stop_hunting( fch );
         stop_hating( fch );
         stop_fearing( fch );
         free_fight( fch );
         update_pos( fch );
      }
   }
   return;
}

/* Vnums for the various bodyparts */
int part_vnums[] = { 12,   /* Head */
   14,   /* arms */
   15,   /* legs */
   13,   /* heart */
   44,   /* brains */
   16,   /* guts */
   45,   /* hands */
   46,   /* feet */
   47,   /* fingers */
   48,   /* ear */
   49,   /* eye */
   50,   /* long_tongue */
   51,   /* eyestalks */
   52,   /* tentacles */
   53,   /* fins */
   54,   /* wings */
   55,   /* tail */
   56,   /* scales */
   59,   /* claws */
   87,   /* fangs */
   58,   /* horns */
   57,   /* tusks */
   55,   /* tailattack */
   85,   /* sharpscales */
   84,   /* beak */
   86,   /* haunches */
   83,   /* hooves */
   82,   /* paws */
   81,   /* forelegs */
   80,   /* feathers */
   0, /* r1 */
   0  /* r2 */
};

/* Messages for flinging off the various bodyparts */
const char *part_messages[] = {
   "$n's severed head plops from its neck.",
   "$n's arm is sliced from $s dead body.",
   "$n's leg is sliced from $s dead body.",
   "$n's heart is torn from $s chest.",
   "$n's brains spill grotesquely from $s head.",
   "$n's guts spill grotesquely from $s torso.",
   "$n's hand is sliced from $s dead body.",
   "$n's foot is sliced from $s dead body.",
   "A finger is sliced from $n's dead body.",
   "$n's ear is sliced from $s dead body.",
   "$n's eye is gouged from its socket.",
   "$n's tongue is torn from $s mouth.",
   "An eyestalk is sliced from $n's dead body.",
   "A tentacle is severed from $n's dead body.",
   "A fin is sliced from $n's dead body.",
   "A wing is severed from $n's dead body.",
   "$n's tail is sliced from $s dead body.",
   "A scale falls from the body of $n.",
   "A claw is torn from $n's dead body.",
   "$n's fangs are torn from $s mouth.",
   "A horn is wrenched from the body of $n.",
   "$n's tusk is torn from $s dead body.",
   "$n's tail is sliced from $s dead body.",
   "A ridged scale falls from the body of $n.",
   "$n's beak is sliced from $s dead body.",
   "$n's haunches are sliced from $s dead body.",
   "A hoof is sliced from $n's dead body.",
   "A paw is sliced from $n's dead body.",
   "$n's foreleg is sliced from $s dead body.",
   "Some feathers fall from $n's dead body.",
   "r1 message.",
   "r2 message."
};

/*
 * Improved Death_cry contributed by Diavolo.
 * Additional improvement by Thoric (and removal of turds... sheesh!)  
 * Support for additional bodyparts by Fireblade
 */
void death_cry( CHAR_DATA * ch )
{
   ROOM_INDEX_DATA *was_in_room;
   const char *msg;
   EXIT_DATA *pexit;
   int vnum, shift, cindex, i;

   if( !ch )
   {
      bug( "%s: null ch!", __FUNCTION__ );
      return;
   }

   vnum = 0;
   msg = NULL;

   switch ( number_range( 0, 5 ) )
   {
      default:
         msg = "You hear $n's death cry.";
         break;
      case 0:
         msg = "$n screams furiously as $e falls to the ground in a heap!";
         break;
      case 1:
         msg = "$n hits the ground ... DEAD.";
         break;
      case 2:
         msg = "$n catches $s guts in $s hands as they pour through $s fatal wound!";
         break;
      case 3:
         msg = "$n splatters blood on your armor.";
         break;
      case 4:
         msg = "$n gasps $s last breath and blood spurts out of $s mouth and ears.";
         break;
      case 5:
         shift = number_range( 0, 31 );
         cindex = 1 << shift;

         for( i = 0; i < 32 && ch->xflags; i++ )
         {
            if( HAS_BODYPART( ch, cindex ) )
            {
               msg = part_messages[shift];
               vnum = part_vnums[shift];
               break;
            }
            else
            {
               shift = number_range( 0, 31 );
               cindex = 1 << shift;
            }
         }

         if( !msg )
            msg = "You hear $n's death cry.";
         break;
   }

   act( AT_CARNAGE, msg, ch, NULL, NULL, TO_ROOM );

   if( vnum )
   {
      char buf[MAX_STRING_LENGTH];
      OBJ_DATA *obj;
      const char *name;

      if( !get_obj_index( vnum ) )
      {
         bug( "%s: invalid vnum", __FUNCTION__ );
         return;
      }

      name = IS_NPC( ch ) ? ch->short_descr : ch->name;
      obj = create_object( get_obj_index( vnum ), 0 );
      obj->timer = number_range( 4, 7 );
      if( IS_AFFECTED( ch, AFF_POISON ) )
         obj->value[3] = 10;

      snprintf( buf, MAX_STRING_LENGTH, obj->short_descr, name );
      STRFREE( obj->short_descr );
      obj->short_descr = STRALLOC( buf );

      snprintf( buf, MAX_STRING_LENGTH, obj->description, name );
      STRFREE( obj->description );
      obj->description = STRALLOC( buf );

      obj = obj_to_room( obj, ch->in_room );
   }

   if( IS_NPC( ch ) )
      msg = "You hear something's death cry.";
   else
      msg = "You hear someone's death cry.";

   was_in_room = ch->in_room;
   for( pexit = was_in_room->first_exit; pexit; pexit = pexit->next )
   {
      if( pexit->to_room && pexit->to_room != was_in_room )
      {
         ch->in_room = pexit->to_room;
         act( AT_CARNAGE, msg, ch, NULL, NULL, TO_ROOM );
      }
   }
   ch->in_room = was_in_room;
}

OBJ_DATA *raw_kill( CHAR_DATA * ch, CHAR_DATA * victim )
{
   OBJ_DATA *corpse_to_return = NULL;

   if( !victim )
   {
      bug( "%s: null victim!", __FUNCTION__ );
      return NULL;
   }

   /*
    * backup in case hp goes below 1 
    */
   if( NOT_AUTHED( victim ) )
   {
      bug( "%s: killing unauthed", __FUNCTION__ );
      return NULL;
   }

   stop_fighting( victim, TRUE );

   /*
    * Take care of morphed characters 
    */
   if( victim->morph )
   {
      do_unmorph_char( victim );
      return raw_kill( ch, victim );
   }

   mprog_death_trigger( ch, victim );
   if( char_died( victim ) )
      return NULL;

   /*
    * death_cry( victim ); 
    */

   rprog_death_trigger( victim );
   if( char_died( victim ) )
      return NULL;

   corpse_to_return = make_corpse( victim, ch );
   if( victim->in_room->sector_type == SECT_OCEANFLOOR
       || victim->in_room->sector_type == SECT_UNDERWATER
       || victim->in_room->sector_type == SECT_WATER_SWIM || victim->in_room->sector_type == SECT_WATER_NOSWIM )
      act( AT_BLOOD, "$n's blood slowly clouds the surrounding water.", victim, NULL, NULL, TO_ROOM );
   else if( victim->in_room->sector_type == SECT_AIR )
      act( AT_BLOOD, "$n's blood sprays wildly through the air.", victim, NULL, NULL, TO_ROOM );
   else
      make_blood( victim );

   if( IS_NPC( victim ) )
   {
      victim->pIndexData->killed++;
      extract_char( victim, TRUE );
      victim = NULL;
      return corpse_to_return;
   }

   set_char_color( AT_DIEMSG, victim );
   if( victim->pcdata->mdeaths + victim->pcdata->pdeaths < 3 )
      do_help( victim, "new_death" );
   else
      do_help( victim, "_DIEMSG_" );

   extract_char( victim, FALSE );
   if( !victim )
   {
      bug( "%s: oops! extract_char destroyed pc char", __FUNCTION__ );
      return NULL;
   }
   while( victim->first_affect )
      affect_remove( victim, victim->first_affect );
   victim->affected_by = race_table[victim->race]->affected;
   victim->resistant = 0;
   victim->susceptible = 0;
   victim->immune = 0;
   victim->carry_weight = 0;
   victim->armor = 100;
   victim->armor += race_table[victim->race]->ac_plus;
   victim->attacks = race_table[victim->race]->attacks;
   victim->defenses = race_table[victim->race]->defenses;
   victim->mod_str = 0;
   victim->mod_dex = 0;
   victim->mod_wis = 0;
   victim->mod_int = 0;
   victim->mod_con = 0;
   victim->mod_cha = 0;
   victim->mod_pas = 0;
   victim->attack = 0;
   victim->mental_state = -10;
   victim->alignment = URANGE( -1000, victim->alignment, 1000 );
/*  victim->alignment		= race_table[victim->race]->alignment;
-- switched lines just for now to prevent mortals from building up
days of bellyaching about their angelic or satanic humans becoming
neutral when they die given the difficulting of changing align */

   victim->saving_poison_death = race_table[victim->race]->saving_poison_death;
   victim->saving_wand = race_table[victim->race]->saving_wand;
   victim->saving_para_petri = race_table[victim->race]->saving_para_petri;
   victim->saving_breath = race_table[victim->race]->saving_breath;
   victim->saving_spell_staff = race_table[victim->race]->saving_spell_staff;
   victim->position = POS_RESTING;
   victim->hit = UMAX( 1, victim->hit );
   /*
    * Shut down some of those naked spammer killers - Blodkai 
    */
   if( victim->level < LEVEL_AVATAR )
      victim->mana = UMAX( 1, victim->mana );
   else
      victim->mana = 1;
   victim->move = UMAX( 1, victim->move );

   /*
    * Pardon crimes...                -Thoric
    */
   if( xIS_SET( victim->act, PLR_KILLER ) )
   {
      xREMOVE_BIT( victim->act, PLR_KILLER );
      send_to_char( "The gods have pardoned you for your murderous acts.\r\n", victim );
   }
   if( xIS_SET( victim->act, PLR_THIEF ) )
   {
      xREMOVE_BIT( victim->act, PLR_THIEF );
      send_to_char( "The gods have pardoned you for your thievery.\r\n", victim );
   }
   victim->pcdata->condition[COND_FULL] = 12;
   victim->pcdata->condition[COND_THIRST] = 12;

   if( IS_SET( sysdata.save_flags, SV_DEATH ) )
      save_char_obj( victim );
   return corpse_to_return;
}

void group_gain( CHAR_DATA * ch, CHAR_DATA * victim )
{
   CHAR_DATA *gch, *gch_next;
   CHAR_DATA *lch;
   int xp;
   int members;

   /*
    * Monsters don't get kill xp's or alignment changes.
    * Dying of mortal wounds or poison doesn't give xp to anyone!
    */
   if( IS_NPC( ch ) || victim == ch )
      return;

   members = 0;
   for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
   {
      if( is_same_group( gch, ch ) )
         members++;
   }

   if( members == 0 )
   {
      bug( "%s: members %d", __FUNCTION__, members );
      members = 1;
   }

   lch = ch->leader ? ch->leader : ch;

   for( gch = ch->in_room->first_person; gch; gch = gch_next )
   {
      OBJ_DATA *obj;
      OBJ_DATA *obj_next;

      gch_next = gch->next_in_room;

      if( !is_same_group( gch, ch ) )
         continue;

      if( gch->level - lch->level > 8 )
      {
         send_to_char( "You are too high for this group.\r\n", gch );
         continue;
      }

      if( gch->level - lch->level < -8 )
      {
         send_to_char( "You are too low for this group.\r\n", gch );
         continue;
      }

      xp = ( int )( xp_compute( gch, victim ) * 0.1765 ) / members;
      if( !gch->fighting )
         xp /= 2;
      gch->alignment = align_compute( gch, victim );
      ch_printf( gch, "You receive %d experience points.\r\n", xp );
      gain_exp( gch, xp );

      for( obj = gch->first_carrying; obj; obj = obj_next )
      {
         obj_next = obj->next_content;
         if( obj->wear_loc == WEAR_NONE )
            continue;

         if( ( IS_OBJ_STAT( obj, ITEM_ANTI_EVIL ) && IS_EVIL( gch ) ) ||
             ( IS_OBJ_STAT( obj, ITEM_ANTI_GOOD ) && IS_GOOD( gch ) ) ||
             ( IS_OBJ_STAT( obj, ITEM_ANTI_NEUTRAL ) && IS_NEUTRAL( gch ) ) )
         {
            act( AT_MAGIC, "You are zapped by $p.", gch, obj, NULL, TO_CHAR );
            act( AT_MAGIC, "$n is zapped by $p.", gch, obj, NULL, TO_ROOM );

            obj_from_char( obj );
            obj = obj_to_room( obj, gch->in_room );
            oprog_zap_trigger( gch, obj );   /* mudprogs */
            if( char_died( gch ) )
               break;
         }
      }
   }

   return;
}


int align_compute( CHAR_DATA * gch, CHAR_DATA * victim )
{
   int align, newalign, divalign;

   align = gch->alignment - victim->alignment;

   /*
    * slowed movement in good & evil ranges by a factor of 5, h 
    */
   /*
    * Added divalign to keep neutral chars shifting faster -- Blodkai 
    */
   /*
    * This is obviously gonna take a lot more thought 
    */

   if( gch->alignment > -350 && gch->alignment < 350 )
      divalign = 4;
   else
      divalign = 20;

   if( align > 500 )
      newalign = UMIN( gch->alignment + ( align - 500 ) / divalign, 1000 );
   else if( align < -500 )
      newalign = UMAX( gch->alignment + ( align + 500 ) / divalign, -1000 );
   else
      newalign = gch->alignment - ( int )( gch->alignment / divalign );

   return newalign;
}


/*
 * Calculate how much XP gch should gain for killing victim
 * Lots of redesigning for new exp system by Thoric
 */
int xp_compute( CHAR_DATA * gch, CHAR_DATA * victim )
{
   int align;
   int xp;
   int xp_ratio;
   int gchlev = gch->level;

   xp = ( get_exp_worth( victim ) * URANGE( 0, ( victim->level - gchlev ) + 10, 13 ) ) / 10;
   align = gch->alignment - victim->alignment;

   /*
    * bonus for attacking opposite alignment 
    */
   if( align > 990 || align < -990 )
      xp = ( xp * 5 ) >> 2;
   else
      /*
       * penalty for good attacking same alignment 
       */
   if( gch->alignment > 300 && align < 250 )
      xp = ( xp * 3 ) >> 2;

   xp = number_range( ( xp * 3 ) >> 2, ( xp * 5 ) >> 2 );

   /*
    * get 1/4 exp for players               -Thoric 
    */
   if( !IS_NPC( victim ) )
      xp /= 4;
   else
      /*
       * reduce exp for killing the same mob repeatedly    -Thoric 
       */
   if( !IS_NPC( gch ) )
   {
      int times = times_killed( gch, victim );

      if( times >= 20 )
         xp = 0;
      else if( times )
      {
         xp = ( xp * ( 20 - times ) ) / 20;
         if( times > 15 )
            xp /= 3;
         else if( times > 10 )
            xp >>= 1;
      }
   }

   /*
    * semi-intelligent experienced player vs. novice player xp gain
    * "bell curve"ish xp mod by Thoric
    * based on time played vs. level
    */
   if( !IS_NPC( gch ) && gchlev > 5 )
   {
      xp_ratio = ( int )gch->played / gchlev;
      if( xp_ratio > 20000 )  /* 5/4 */
         xp = ( xp * 5 ) >> 2;
      else if( xp_ratio > 16000 )   /* 3/4 */
         xp = ( xp * 3 ) >> 2;
      else if( xp_ratio > 10000 )   /* 1/2 */
         xp >>= 1;
      else if( xp_ratio > 5000 ) /* 1/4 */
         xp >>= 2;
      else if( xp_ratio > 3500 ) /* 1/8 */
         xp >>= 3;
      else if( xp_ratio > 2000 ) /* 1/16 */
         xp >>= 4;
   }

   /*
    * Level based experience gain cap.  Cannot get more experience for
    * a kill than the amount for your current experience level   -Thoric
    */
   return URANGE( 0, xp, exp_level( gch, gchlev + 1 ) - exp_level( gch, gchlev ) );
}

/*
 * Revamped to support Davenge's ranged fight system -Davenge
 */

void new_dam_message( CHAR_DATA * ch, CHAR_DATA * victim, int dam, unsigned int dt, int hit_wear, bool crit, EXT_BV damtype )
{
   CHAR_DATA *rch;
   char damtype_message[MAX_INPUT_LENGTH];
   int counter;
   /*
    * let's get our messages based on damage types out of the way -Davenge
    * Magical first -Davenge
    */
   for( counter = 0; counter < MAX_DAMTYPE; counter++ )
   {
      if( xIS_SET( damtype, counter ) )
      {
         if( counter >= DAM_PIERCE && counter <= DAM_BLUNT )
            sprintf( damtype_message, "%s %s ", damtype_message, damage_message[counter] );
         else
            sprintf( damtype_message, "%s %s ", damage_message[counter], damtype_message );
      }
   }

   /*
    * Doing it by DTs
    */
   if( dt >= TYPE_HIT && hit_wear >= 0 )
   {
      if( !IS_NPC( ch ) && !xIS_SET( ch->pcdata->fight_chatter, DAM_YOU_DO ) )
         ch_printf( ch, "Your %s strikes %s on the %s dealing %d damage.\r\n", damtype_message, victim->name, w_flags[hit_wear], dam );
      if( !IS_NPC( victim ) && !xIS_SET( victim->pcdata->fight_chatter, DAM_YOU_TAKE ) )
         ch_printf( ch, "%s's %s strikes you on the %s dealing %d damage.\r\n", ch->name, damtype_message, w_flags[hit_wear], dam );
      for( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
      {
         if( rch == victim || rch == ch )
            continue;
         if( IS_NPC( rch ) )
            continue;
         if( is_same_group( rch, ch ) && xIS_SET( rch->pcdata->fight_chatter, DAM_PARTY_DOES ) )
            continue;
         if( !is_same_group( rch, ch ) && xIS_SET( rch->pcdata->fight_chatter, DAM_OTHER_DOES ) )
            continue;
         if( is_same_group( rch, victim ) && xIS_SET( rch->pcdata->fight_chatter, DAM_PARTY_TAKES ) )
            continue;
         if( !is_same_group( rch, victim ) && xIS_SET( rch->pcdata->fight_chatter, DAM_OTHER_TAKES ) )
            continue;
         ch_printf( rch, "%s's %s strikes %s on the %s dealing %d damage.\r\n", ch->name, damtype_message, victim->name, w_flags[hit_wear], dam );
      }
      if( ch->in_room != victim->in_room )
      {
         for( rch = victim->in_room->first_person; rch; rch = rch->next_in_room )
         {
            if( rch == victim || rch == ch )
               continue;
            if( IS_NPC( rch ) )
               continue;
            if( is_same_group( rch, ch ) && xIS_SET( rch->pcdata->fight_chatter, DAM_PARTY_DOES ) )
               continue;
            if( !is_same_group( rch, ch ) && xIS_SET( rch->pcdata->fight_chatter, DAM_OTHER_DOES ) )
               continue;
            if( is_same_group( rch, victim ) && xIS_SET( rch->pcdata->fight_chatter, DAM_PARTY_TAKES ) )
               continue;
            if( !is_same_group( rch, victim ) && xIS_SET( rch->pcdata->fight_chatter, DAM_OTHER_TAKES ) )
               continue;
            ch_printf( rch, "%s's %s strikes %s on the %s dealing %d damage.\r\n", ch->name, damtype_message, victim->name, w_flags[hit_wear], dam );
         }
      }
   }
   return;
}

void do_target( CHAR_DATA *ch, const char *argument )
{
   CHAR_DATA *victim;
   TARGET_DATA *target;
   char arg[MAX_INPUT_LENGTH];

   argument = one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "You need to specify a target and possibly a direction.\r\n", ch );
      return;
   }
   if( !str_cmp( strlower( arg ), "none" ) )
   {
      send_to_char( "Clearing target.\r\n", ch );
      clear_target( ch );
      return;
   }

   if( argument[0] == '\0' && ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "That person is not in the room, try specifying the direction they are in.\r\n", ch );
      return;
   }

   if( victim )
      target = make_new_target( victim, 0, -1 );
   else
      target = get_target( ch, arg, get_dir( argument ) );

   if( !target )
   {
      send_to_char( "That person is not here nor there for targetting.\r\n", ch );
      return;
   }
   ch_printf( ch, "Targetting: %s...\r\n", target->victim->name );
   set_new_target( ch, target );
   return;
}

void do_stop( CHAR_DATA *ch, const char *argument )
{
   if( ch->position != POS_FIGHTING )
   {
      send_to_char( "Stop what? You aren't fighting anyone.\r\n", ch );
      return;
   }
   send_to_char( "You relax from your fighting stance and stop auto-attacking.\r\n", ch ); 
   ch->stopkill = TRUE;
   ch->position = POS_STANDING;
   return;

}

void do_kill( CHAR_DATA* ch, const char* argument)
{
   TARGET_DATA *target;
   CHAR_DATA *victim;
   const char *orig_argument;
   char arg[MAX_INPUT_LENGTH];
   int range, dir;

   orig_argument = str_dup( argument );
   argument = one_argument( argument, arg );

   /* If you have recently stopped, gotta wait for it to clear before you can kill again. -Davenge */

   if( ch->stopkill )
   {
      send_to_char( "Not so fast... you must wait to kill again after typing stop so soon.\r\n", ch );
      return;
   }

   /* If player doesn't already have a target -Davenge */

   if( !ch->target )
   {
      /* No argument is entered, drop to a fighting stance -Davenge */
      if( arg[0] == '\0' )
      {
         send_to_char( "You drop into a fighting stance, kill who? Try picking a target!\r\nKill <victim> or kill <victim> <direction>\r\n", ch );
         ch->position = POS_FIGHTING;
         return;
      }

      /* If no direction is specified, check the room */
      if( argument[0] == '\0' )
      {
         if( ( victim = get_char_room( ch, arg ) ) == NULL )
         {
            ch_printf( ch, "There is no %s in the room.\r\n", arg );
            return;
         }
         target = make_new_target( victim, 0, -1 );
      }
      else
      {
         if( ( dir = get_door( argument ) ) == -1 )
         {
            send_to_char( "That's not a valid direction.\r\n", ch );
            return;
         }
         if( ( target = get_target( ch, arg, dir ) ) == NULL )
         {
            ch_printf( ch, "There is no %s to the %s.\r\n", arg, dir_name[dir] );
            return;
         }
      }
   }
   else
   {
      if( arg[0] == '\0' || !strcmp( arg, ch->target->victim->name ) )
         send_to_char( "You drop into a fighting stance!\r\n", ch );
      else
      {
         clear_target( ch );
         do_kill( ch, orig_argument );
         return;
      }
      CREATE( target, TARGET_DATA, 1 );
      target->victim = ch->target->victim;
      target->range = ch->target->range;
      target->dir = ch->target->dir;
   }

   range = get_max_range( ch );

   if( range < target->range )
   {
      ch_printf( ch, "%s is too far away.\r\n", capitalize( target->victim->name ) );
      return;
   }

   if( target->victim == ch )
   {
      send_to_char( "You hit yourself.  Ouch!\r\n", ch );
      return;
   }

   if( is_safe( ch, target->victim, TRUE ) )
      return;

   if( IS_AFFECTED( ch, AFF_CHARM ) && ch->master == target->victim )
   {
      act( AT_PLAIN, "$N is your beloved master.", ch, NULL, target->victim, TO_CHAR );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "You're already fighting something, just switch targets!\r\n", ch );
      return;
   }
 
   ch_printf( ch, "You begin targeting %s and drop to a fighting stance.\r\n", target->victim->name );
   ch->position = POS_FIGHTING;
   set_new_target( ch, target );
   check_attacker( ch, target->victim );
   multi_hit( ch, target, TYPE_UNDEFINED );
   return;
}

void do_murde( CHAR_DATA* ch, const char* argument)
{
   send_to_char( "If you want to MURDER, spell it out.\r\n", ch );
   return;
}

void do_murder( CHAR_DATA* ch, const char* argument)
{
   send_to_char( "Murder currently disabled.\r\n", ch );
   return;
}
/*
   char buf[MAX_STRING_LENGTH];
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Murder whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "Suicide is a mortal sin.\r\n", ch );
      return;
   }

   if( is_safe( ch, victim, TRUE ) )
      return;

   if( IS_AFFECTED( ch, AFF_CHARM ) )
   {
      if( ch->master == victim )
      {
         act( AT_PLAIN, "$N is your beloved master.", ch, NULL, victim, TO_CHAR );
         return;
      }
      else
      {
         if( ch->master )
            xSET_BIT( ch->master->act, PLR_ATTACKER );
      }
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "You do the best you can!\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) && xIS_SET( ch->act, PLR_NICE ) )
   {
      send_to_char( "You feel too nice to do that!\r\n", ch );
      return;
   }
*
    if ( !IS_NPC( victim ) && xIS_SET(victim->act, PLR_PK ) )
*

   if( !IS_NPC( victim ) )
   {
      log_printf_plus( LOG_NORMAL, ch->level, "%s: murder %s.", ch->name, victim->name );
   }

   WAIT_STATE( ch, 1 * PULSE_VIOLENCE );
   snprintf( buf, MAX_STRING_LENGTH, "Help!  I am being attacked by %s!", IS_NPC( ch ) ? ch->short_descr : ch->name );
   if( IS_PKILL( victim ) )
      do_wartalk( victim, buf );
   else
      do_yell( victim, buf );
   check_illegal_pk( ch, victim );
   check_attacker( ch, victim );
   multi_hit( ch, victim, TYPE_UNDEFINED );
   return;
} */

/*
 * Check to see if the player is in an "Arena".
 */
bool in_arena( CHAR_DATA * ch )
{
   if( xIS_SET( ch->in_room->room_flags, ROOM_ARENA ) )
      return TRUE;
   if( IS_SET( ch->in_room->area->flags, AFLAG_FREEKILL ) )
      return TRUE;
   if( ch->in_room->vnum >= 29 && ch->in_room->vnum <= 43 )
      return TRUE;
   if( !str_cmp( ch->in_room->area->filename, "arena.are" ) )
      return TRUE;

   return FALSE;
}

bool check_illegal_pk( CHAR_DATA * ch, CHAR_DATA * victim )
{
   char buf[MAX_STRING_LENGTH];
   char buf2[MAX_STRING_LENGTH];
   char log_buf[MAX_STRING_LENGTH];

   if( !IS_NPC( victim ) && !IS_NPC( ch ) )
   {
      if( ( !IS_SET( victim->pcdata->flags, PCFLAG_DEADLY )
            || ch->level - victim->level > 10
            || !IS_SET( ch->pcdata->flags, PCFLAG_DEADLY ) )
          && !in_arena( ch ) && ch != victim && !( IS_IMMORTAL( ch ) && IS_IMMORTAL( victim ) ) )
      {
         if( IS_NPC( ch ) )
            snprintf( buf, MAX_STRING_LENGTH, " (%s)", ch->name );
         if( IS_NPC( victim ) )
            snprintf( buf2, MAX_STRING_LENGTH, " (%s)", victim->name );

         snprintf( log_buf, MAX_STRING_LENGTH, "&p%s on %s%s in &W***&rILLEGAL PKILL&W*** &pattempt at %d",
                   ( lastplayercmd ),
                   ( IS_NPC( victim ) ? victim->short_descr : victim->name ),
                   ( IS_NPC( victim ) ? buf2 : "" ), victim->in_room->vnum );
         last_pkroom = victim->in_room->vnum;
         log_string( log_buf );
         to_channel( log_buf, CHANNEL_MONITOR, "Monitor", LEVEL_IMMORTAL );
         return TRUE;
      }
   }
   return FALSE;
}

void do_flee( CHAR_DATA* ch, const char* argument)
{
   ROOM_INDEX_DATA *was_in;
   ROOM_INDEX_DATA *now_in;
   char buf[MAX_STRING_LENGTH];
   int attempt, los;
   short door;
   EXIT_DATA *pexit;

   if( !who_fighting( ch ) )
   {
      if( ch->position == POS_FIGHTING )
      {
         if( ch->mount )
            ch->position = POS_MOUNTED;
         else
            ch->position = POS_STANDING;
      }
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }
   if( IS_AFFECTED( ch, AFF_BERSERK ) )
   {
      send_to_char( "Flee while berserking?  You aren't thinking very clearly...\r\n", ch );
      return;
   }
   if( ch->move <= 0 )
   {
      send_to_char( "You're too exhausted to flee from combat!\r\n", ch );
      return;
   }
   /*
    * No fleeing while more aggressive than standard or hurt. - Haus 
    */
   if( !IS_NPC( ch ) && ch->position < POS_FIGHTING )
   {
      send_to_char( "You can't flee in an aggressive stance...\r\n", ch );
      return;
   }
   if( IS_NPC( ch ) && ch->position <= POS_SLEEPING )
      return;
   was_in = ch->in_room;
   for( attempt = 0; attempt < 8; attempt++ )
   {
      door = number_door(  );
      if( ( pexit = get_exit( was_in, door ) ) == NULL
          || !pexit->to_room
          || IS_SET( pexit->exit_info, EX_NOFLEE )
          || ( IS_SET( pexit->exit_info, EX_CLOSED )
               && !IS_AFFECTED( ch, AFF_PASS_DOOR ) )
          || ( IS_NPC( ch ) && xIS_SET( pexit->to_room->room_flags, ROOM_NO_MOB ) ) )
         continue;
      affect_strip( ch, gsn_sneak );
      xREMOVE_BIT( ch->affected_by, AFF_SNEAK );
      if( ch->mount && ch->mount->fighting )
         stop_fighting( ch->mount, TRUE );
      move_char( ch, pexit, 0 );
      if( ( now_in = ch->in_room ) == was_in )
         continue;
      ch->in_room = was_in;
      act( AT_FLEE, "$n flees head over heels!", ch, NULL, NULL, TO_ROOM );
      ch->in_room = now_in;
      act( AT_FLEE, "$n glances around for signs of pursuit.", ch, NULL, NULL, TO_ROOM );
      if( !IS_NPC( ch ) )
      {
         CHAR_DATA *wf = who_fighting( ch );
         act( AT_FLEE, "You flee head over heels from combat!", ch, NULL, NULL, TO_CHAR );
         los = ( int )( ( exp_level( ch, ch->level + 1 ) - exp_level( ch, ch->level ) ) * 0.2 );
         if( ch->level < LEVEL_AVATAR )
         {
            snprintf( buf, MAX_STRING_LENGTH, "Curse the gods, you've lost %d experience!", los );
            act( AT_FLEE, buf, ch, NULL, NULL, TO_CHAR );
            gain_exp( ch, 0 - los );
         }
         if( wf && ch->pcdata->deity )
         {
            int level_ratio = URANGE( 1, wf->level / ch->level, MAX_LEVEL );

            if( wf && wf->race == ch->pcdata->deity->npcrace )
               adjust_favor( ch, 1, level_ratio );
            else if( wf && wf->race == ch->pcdata->deity->npcfoe )
               adjust_favor( ch, 16, level_ratio );
            else
               adjust_favor( ch, 0, level_ratio );
         }
      }
      stop_fighting( ch, TRUE );
      return;
   }
   los = ( int )( ( exp_level( ch, ch->level + 1 ) - exp_level( ch, ch->level ) ) * 0.1 );
   act( AT_FLEE, "You attempt to flee from combat but can't escape!", ch, NULL, NULL, TO_CHAR );
   if( ch->level < LEVEL_AVATAR && number_bits( 3 ) == 1 )
   {
      snprintf( buf, MAX_STRING_LENGTH, "Curse the gods, you've lost %d experience!\r\n", los );
      act( AT_FLEE, buf, ch, NULL, NULL, TO_CHAR );
      gain_exp( ch, 0 - los );
   }
   return;
}

void do_sla( CHAR_DATA* ch, const char* argument)
{
   send_to_char( "If you want to SLAY, spell it out.\r\n", ch );
   return;
}

void do_slay( CHAR_DATA* ch, const char* argument)
{
   CHAR_DATA *victim;
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];

   argument = one_argument( argument, arg );
   one_argument( argument, arg2 );
   if( arg[0] == '\0' )
   {
      send_to_char( "Slay whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( ch == victim )
   {
      send_to_char( "Suicide is a mortal sin.\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) && get_trust( victim ) >= get_trust( ch ) )
   {
      send_to_char( "You failed.\r\n", ch );
      return;
   }

   if( !str_cmp( arg2, "immolate" ) )
   {
      act( AT_FIRE, "Your fireball turns $N into a blazing inferno.", ch, NULL, victim, TO_CHAR );
      act( AT_FIRE, "$n releases a searing fireball in your direction.", ch, NULL, victim, TO_VICT );
      act( AT_FIRE, "$n points at $N, who bursts into a flaming inferno.", ch, NULL, victim, TO_NOTVICT );
   }

   else if( !str_cmp( arg2, "shatter" ) )
   {
      act( AT_LBLUE, "You freeze $N with a glance and shatter the frozen corpse into tiny shards.", ch, NULL, victim,
           TO_CHAR );
      act( AT_LBLUE, "$n freezes you with a glance and shatters your frozen body into tiny shards.", ch, NULL, victim,
           TO_VICT );
      act( AT_LBLUE, "$n freezes $N with a glance and shatters the frozen body into tiny shards.", ch, NULL, victim,
           TO_NOTVICT );
   }

   else if( !str_cmp( arg2, "demon" ) )
   {
      act( AT_IMMORT, "You gesture, and a slavering demon appears.  With a horrible grin, the", ch, NULL, victim, TO_CHAR );
      act( AT_IMMORT, "foul creature turns on $N, who screams in panic before being eaten alive.", ch, NULL, victim,
           TO_CHAR );
      act( AT_IMMORT, "$n gestures, and a slavering demon appears.  The foul creature turns on", ch, NULL, victim, TO_VICT );
      act( AT_IMMORT, "you with a horrible grin.   You scream in panic before being eaten alive.", ch, NULL, victim,
           TO_VICT );
      act( AT_IMMORT, "$n gestures, and a slavering demon appears.  With a horrible grin, the", ch, NULL, victim,
           TO_NOTVICT );
      act( AT_IMMORT, "foul creature turns on $N, who screams in panic before being eaten alive.", ch, NULL, victim,
           TO_NOTVICT );
   }

   else if( !str_cmp( arg2, "pounce" ) )
   {
      act( AT_BLOOD, "Leaping upon $N with bared fangs, you tear open $S throat and toss the corpse to the ground...", ch,
           NULL, victim, TO_CHAR );
      act( AT_BLOOD,
           "In a heartbeat, $n rips $s fangs through your throat!  Your blood sprays and pours to the ground as your life ends...",
           ch, NULL, victim, TO_VICT );
      act( AT_BLOOD,
           "Leaping suddenly, $n sinks $s fangs into $N's throat.  As blood sprays and gushes to the ground, $n tosses $N's dying body away.",
           ch, NULL, victim, TO_NOTVICT );
   }

   else if( !str_cmp( arg2, "slit" ) )
   {
      act( AT_BLOOD, "You calmly slit $N's throat.", ch, NULL, victim, TO_CHAR );
      act( AT_BLOOD, "$n reaches out with a clawed finger and calmly slits your throat.", ch, NULL, victim, TO_VICT );
      act( AT_BLOOD, "$n calmly slits $N's throat.", ch, NULL, victim, TO_NOTVICT );
   }

   else if( !str_cmp( arg2, "dog" ) )
   {
      act( AT_BLOOD, "You order your dogs to rip $N to shreds.", ch, NULL, victim, TO_CHAR );
      act( AT_BLOOD, "$n orders $s dogs to rip you apart.", ch, NULL, victim, TO_VICT );
      act( AT_BLOOD, "$n orders $s dogs to rip $N to shreds.", ch, NULL, victim, TO_NOTVICT );
   }

   else
   {
      act( AT_IMMORT, "You slay $N in cold blood!", ch, NULL, victim, TO_CHAR );
      act( AT_IMMORT, "$n slays you in cold blood!", ch, NULL, victim, TO_VICT );
      act( AT_IMMORT, "$n slays $N in cold blood!", ch, NULL, victim, TO_NOTVICT );
   }

   set_cur_char( victim );
   raw_kill( ch, victim );
   return;
}

bool range_check( CHAR_DATA *ch, TARGET_DATA *target, int dt, bool CastStart )
{
   int range;

   range = target->range;

   if( dt > TYPE_UNDEFINED && dt < TYPE_HIT )
   {
      switch( skill_table[dt]->type )
      {
         case SKILL_SPELL:
            if( CastStart && range >= get_skill_range( ch, dt ) )
               return FALSE;
            if( !CastStart && range >= ( get_skill_range( ch, dt ) + 3 ) )
               return FALSE;
            break;
         case SKILL_SKILL:
            if( CastStart && range >= get_skill_range( ch, dt ) )
               return FALSE;
            if( !CastStart && range >= ( get_skill_range( ch, dt ) + 1 ) )
               return FALSE;
            break;
      }
   }
   else if( dt == TYPE_UNDEFINED && range >= get_max_range( ch ) )
      return FALSE;
   return TRUE;
}

int res_pen( CHAR_DATA *ch, CHAR_DATA *victim, int dam, EXT_BV damtype )
{
   double mod, mod_pen, mod_res;
   int counter, split_dam;
   int num_damtype = 0;
   int progress = 0;

   for( counter = 0; counter < MAX_DAMTYPE; counter++ )
      if( xIS_SET( damtype, counter ) )
         num_damtype++;

   if( num_damtype <= 0 )
   {
      bug( "res_pen being called with no damtypes" );
      return dam;
   }

   split_dam = dam / num_damtype;
   dam = 0;

   for( counter = DAM_PIERCE; counter < MAX_DAMTYPE; counter++ )
   {
      if( xIS_SET( damtype, counter ) )
      {
         mod_pen = ch->penetration[DAM_ALL];
         mod_res = victim->resistance[DAM_ALL];

         if( counter >= DAM_PIERCE && counter <= DAM_BLUNT )
         {
            mod_pen += ch->penetration[DAM_PHYSICAL] + ch->penetration[counter];
            mod_res += victim->resistance[DAM_PHYSICAL] + victim->resistance[counter];

            mod = (100 + URANGE( -95, ( mod_pen - mod_res ), 95 )) / 100;
            dam += (int)( split_dam * mod );
         }
         if( counter >= DAM_WIND && counter <= DAM_DARK )
         {
            mod_pen += ch->penetration[DAM_MAGIC] + ch->penetration[counter];
            mod_res += victim->resistance[DAM_PHYSICAL] + victim->resistance[counter];

            mod = (100 +URANGE( -95, ( mod_pen - mod_res ), 95 )) / 100;
            dam += (int)( split_dam * mod );
         }
         if( ++progress == num_damtype )
            break;
      }
   }
   return dam; 
}

int get_fist_weight( CHAR_DATA * ch )
{
   OBJ_DATA *obj;
   int fist_weight;

   /*
    * Base body part weight, the hand -Davenge 
    */
   fist_weight = body_part_weight[WEAR_HANDS];

   /*
    * Add weight of any gloves -Davenge
    */
   if( ( obj = get_eq_char( ch, WEAR_HANDS ) ) != NULL )  
      fist_weight += obj->weight;

   /*
    * Add the weight of a weapon -Davenge
    */
   if( used_weapon )
      fist_weight += used_weapon->weight;

   /*
    * Augment points to heavy_handed -Davenge
    *
    * Not in yet, will add with augment system
    */
   return fist_weight;
}

int get_wear_loc_weight( CHAR_DATA * ch, int hit_wear )
{
   OBJ_DATA *obj;
   int loc_weight;

   /*
    * Base bofdy part weight -Davenge
    */

   loc_weight = body_part_weight[hit_wear];

   /*
    * If armored at that location -Davenge
    */
   if( ( obj = get_eq_char( ch, hit_wear ) ) != NULL )
      loc_weight += obj->weight;

   /*
    * Augment points spent on a body_part -Davenge
    *
    * Not in yet, will add with augment system
    */
    return loc_weight;
}

int calc_weight_mod( CHAR_DATA *ch, CHAR_DATA *victim, int hit_wear, int dam, bool crit )
{
   int armor_weight, weapon_weight;
   double mod;

   /*
    * Get the weights of each -Davenge
    */

   armor_weight = get_wear_loc_weight( victim, hit_wear );
   weapon_weight = get_fist_weight( ch );

   /*
    * Do math, basically floor at 95% damage reduction/increase to the damage
    * -Davenge
    */
   mod = (double)URANGE( (int)( armor_weight * .05), (armor_weight + ( armor_weight - weapon_weight )), (int)( armor_weight * 1.95 ));

   /*
    * convert out mod into proper percentage -Davenge
    */
   mod = mod / armor_weight;

   /*
    * If we crit, and we are already doing extra damage, add to it
    * If we crit and we are getting penalized, just make no mod -Davenge
    */
   if( crit && mod > 1 )
      mod += .25;
   else if( crit && mod < 1 )
      return dam;
   /*
    * Multiply the dam passed by our mod
    * -Davenge
    */
   dam = (int)( dam * mod );

   return dam;
}

int attack_ac_mod( CHAR_DATA *ch, CHAR_DATA *victim, int dam )
{
   int atkac_mod;

   /*
    * Basically make 1 attack equal to 10 armor for 1% damage increase/redux
    * -Davenge
    */
   atkac_mod = 100 + ( GET_ATTACK( ch ) - ( GET_AC( victim ) / 10 ) );

   /*
    * Apply the mod after turning it into an appropriate percentage capping at
    * %5 to 95% increase respectively -Davenge
    */
   dam = (int)( dam * ( (double)URANGE( 5, atkac_mod, 195 ) / 100 ) );

   return dam;
}

int mattack_mdefense_mod( CHAR_DATA *ch, CHAR_DATA *victim, int dam )
{
   int matkmdef_mod;

   /*
    * Basically make 1 magic attack increase damage by 1.5% and 
    * 10 magic defense decrease magic damage by 1%
    */

   matkmdef_mod = 100 + ( ( GET_MAGICATTACK( ch ) * 1.5 ) - ( GET_MAGICDEFENSE( victim ) / 10 ) );

   /*
    * Apply the mod after turning it into an approriate percentage capping at
    * 5% and 250% increse resepctively -Davenge
    */

   dam = (int)( dam * ( (double)URANGE(5, matkmdef_mod, 250 ) / 100 ) );

   return dam;
}

bool get_crit( CHAR_DATA *ch, int dt )
{
   double chance;
   int counter;

   if( dt >= TYPE_HIT || ( dt < TYPE_HIT && skill_table[dt]->type == SKILL_SKILL ) )
   {
      for( counter = 0; counter < get_curr_dex( ch ); counter++ )
      {
         if( counter >= 0 && counter <= 15 )
            chance += .4;
         if( counter > 15 && counter <= 30 )
            chance += .25;
         if( counter > 30 && counter <= 50 )
            chance += .1;
         if( counter > 50 )
            chance += .05;
      }
      chance = URANGE( 0, (int)chance, 50 );
   }
   else if( skill_table[dt]->type == SKILL_SPELL )
   {
      for( counter = 0; counter < get_curr_pas( ch ); counter++ )
      {
         if( counter >= 0 && counter <= 15 )
            chance += .65;
         if( counter > 15 && counter <= 30 )
            chance += .45;
         if( counter > 30 && counter <= 50 )
            chance += .20;
         if( counter > 50 )
            chance += .08;
      }
      chance = URANGE( 0, (int) chance, 75 );
   }

   if( number_percent( ) > chance )
      return FALSE;
   return TRUE;
}
