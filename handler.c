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
 *                     Main structure manipulation module                   *
 ****************************************************************************/

#include <stdio.h>
#include <string.h>
#include <math.h>
#include "mud.h"

extern int top_exit;
extern int top_ed;
extern int top_affect;
extern int cur_qobjs;
extern int cur_qchars;
extern REL_DATA *first_relation;
extern REL_DATA *last_relation;

CHAR_DATA *cur_char;
ROOM_INDEX_DATA *cur_room;
bool cur_char_died;
ch_ret global_retcode;

int cur_obj;
int cur_obj_serial;
bool cur_obj_extracted;
obj_ret global_objcode;

OBJ_DATA *group_object( OBJ_DATA * obj1, OBJ_DATA * obj2 );
void update_room_reset( CHAR_DATA *ch, bool setting );
bool in_magic_container( OBJ_DATA * obj );
void delete_reset( RESET_DATA * pReset );

/* 50 nested loops should be enough till the end of time,
   unless someone forgot to deallocate :P - Luc 06/2007 */
#define TRW_MAXHEAP 50

int trw_loops = 0;
TRV_WORLD trw_heap[TRW_MAXHEAP];

TRV_DATA *trvch_create( CHAR_DATA * ch, trv_type tp )
{
   CHAR_DATA *first, *ptr;
   TRV_DATA *New;
   int count = 0;

   if( tp == TR_CHAR_ROOM_FORW || tp == TR_CHAR_ROOM_BACK )
   {
      if( !ch )
      {
         bug( "%s: NULL ch.", __FUNCTION__ );
         return NULL;
      }
      else if( !ch->in_room )
      {
         bug( "%s: type %d with NULL room.", __FUNCTION__, tp );
         return NULL;
      }
   }
   switch ( tp )
   {
      case TR_CHAR_ROOM_FORW:
         first = ptr = ch->in_room->first_person;
         while( ptr )
         {
            ptr = ptr->next_in_room;
            count++;
         }
         break;
      case TR_CHAR_ROOM_BACK:
         first = ptr = ch->in_room->last_person;
         while( ptr )
         {
            ptr = ptr->prev_in_room;
            count++;
         }
         break;
/*    case TR_CHAR_WORLD_FORW:
         first = ptr = first_char;
         while ( ptr ) {
            ptr = ptr->next;
            count++;
            }
         break;
      case TR_CHAR_WORLD_BACK:
         first = ptr = last_char;
         while ( ptr ) {
            ptr = ptr->prev;
            count++;
            }
         break; */
      default:
         bug( "%s: bad type (%d).", __FUNCTION__, tp );
         return NULL;
   }

   New = ( TRV_DATA * ) malloc( sizeof( TRV_DATA ) );
   if( !New )
   {
      bug( "%s: malloc() failure for %d nodes.", __FUNCTION__, count );
      return NULL;
   }
   New->el = (void**) malloc( count * sizeof(void*) );
   if( !New->el )
   {
      bug( "%s: malloc() failure for %d nodes.", __FUNCTION__, count );
      return NULL;
   }
   New->type = tp;
   New->count = count;
   New->current = 1;
   New->ext_mark = extracted_char_queue;
   count = 0;
   ptr = first;
   switch ( tp )
   {
      case TR_CHAR_ROOM_FORW:
         for( ; ptr; ptr = ptr->next_in_room )
            New->el[count++] = ptr;
         New->where = first->in_room;
         break;
      case TR_CHAR_ROOM_BACK:
         for( ; ptr; ptr = ptr->prev_in_room )
            New->el[count++] = ptr;
         New->where = first->in_room;
         break;
      case TR_CHAR_WORLD_FORW:
         for( ; ptr; ptr = ptr->next )
            New->el[count++] = ptr;
         New->where = NULL;
         break;
      case TR_CHAR_WORLD_BACK:
         for( ; ptr; ptr = ptr->prev )
            New->el[count++] = ptr;
         New->where = NULL;
         break;
      default:
         break;
   }
   return New;
}

TRV_DATA *trvobj_create( OBJ_DATA * obj, trv_type tp )
{
   OBJ_DATA *first, *ptr;
   void *ground_zero;
   TRV_DATA *New;
   int count = 0;

   if( !obj && tp >= TR_OBJ_ROOM_FORW && tp <= TR_OBJ_OBJ_BACK )
   {
      bug( "%s: NULL obj.", __FUNCTION__ );
      return NULL;
   }
   if( ( tp == TR_OBJ_ROOM_FORW || tp == TR_OBJ_ROOM_BACK ) && !obj->in_room )
   {
      bug( "%s: type %d in NULL room.", __FUNCTION__, tp );
      return NULL;
   }
   if( ( tp == TR_OBJ_CHAR_FORW || tp == TR_OBJ_CHAR_BACK ) && !obj->carried_by )
   {
      bug( "%s: type %d in NULL carrier.", __FUNCTION__, tp );
      return NULL;
   }
   if( ( tp == TR_OBJ_OBJ_FORW || tp == TR_OBJ_OBJ_BACK ) && !obj->in_obj )
   {
      bug( "%s: type %d in NULL container.", __FUNCTION__, tp );
      return NULL;
   }
   switch ( tp )
   {
      case TR_OBJ_ROOM_FORW:
         ground_zero = obj->in_room;
         first = ptr = obj->in_room->first_content;
         while( ptr )
         {
            ptr = ptr->next_content;
            count++;
         }
         break;
      case TR_OBJ_ROOM_BACK:
         ground_zero = obj->in_room;
         first = ptr = obj->in_room->last_content;
         while( ptr )
         {
            ptr = ptr->prev_content;
            count++;
         }
         break;
      case TR_OBJ_CHAR_FORW:
         ground_zero = obj->carried_by;
         first = ptr = obj->carried_by->first_carrying;
         while( ptr )
         {
            ptr = ptr->next_content;
            count++;
         }
         break;
      case TR_OBJ_CHAR_BACK:
         ground_zero = obj->carried_by;
         first = ptr = obj->carried_by->last_carrying;
         while( ptr )
         {
            ptr = ptr->prev_content;
            count++;
         }
         break;
      case TR_OBJ_OBJ_FORW:
         ground_zero = obj->in_obj;
         first = ptr = obj->in_obj->first_content;
         while( ptr )
         {
            ptr = ptr->next_content;
            count++;
         }
         break;
      case TR_OBJ_OBJ_BACK:
         ground_zero = obj->in_obj;
         first = ptr = obj->in_obj->last_content;
         while( ptr )
         {
            ptr = ptr->prev_content;
            count++;
         }
         break;

         /*
          * Hmmm.  Trouble is, on big muds there may be LOTS of objects,
          * meaning lots memory (about 800k for a 200k objs mud) and lots
          * of iterations.  Maybe these two should be redone with a different
          * algorithm, considering that the world lists operate on simpler
          * conditions: elements are only extracted for good or appended at
          * the end, not moved back and forth between lists at this level 
          */
/*    case TR_OBJ_WORLD_FORW:
         ground_zero = NULL;
         first = ptr = first_object;
         while ( ptr ) {
            ptr = ptr->next;
            count++;
            }
         break;
      case TR_OBJ_WORLD_BACK:
         ground_zero = NULL;
         first = ptr = last_object;
         while ( ptr ) {
            ptr = ptr->prev;
            count++;
            }
         break; */
      default:
         bug( "%s: bad type (%d).", __FUNCTION__, tp );
         return NULL;
   }

   New = ( TRV_DATA * ) malloc( sizeof( TRV_DATA ) );
   if( !New )
   {
      bug( "%s: malloc() failure, %d nodes, type %d.", __FUNCTION__, count, tp );
      return NULL;
   }
   New->el = (void**) malloc(count * sizeof(void*) );
   if( !New->el )
   {
      bug( "%s: malloc() failure, %d nodes, type %d.", __FUNCTION__, count, tp );
      return NULL;
   }
   New->type = tp;
   New->count = count;
   New->current = 1;
   New->ext_mark = extracted_obj_queue;
   New->where = ground_zero;
   count = 0;
   ptr = first;
   switch ( tp )
   {
      case TR_OBJ_ROOM_FORW:
      case TR_OBJ_CHAR_FORW:
      case TR_OBJ_OBJ_FORW:
         for( ; ptr; ptr = ptr->next_content )
            New->el[count++] = ptr;
         break;
      case TR_OBJ_ROOM_BACK:
      case TR_OBJ_CHAR_BACK:
      case TR_OBJ_OBJ_BACK:
         for( ; ptr; ptr = ptr->prev_content )
            New->el[count++] = ptr;
         break;
      case TR_OBJ_WORLD_FORW:
         for( ; ptr; ptr = ptr->next )
            New->el[count++] = ptr;
         break;
      case TR_OBJ_WORLD_BACK:
         for( ; ptr; ptr = ptr->prev )
            New->el[count++] = ptr;
         break;
      default:
         break;
   }
   return New;
}

void trv_dispose( TRV_DATA ** p )
{
   if( *p )
   {
      free( (*p)->el );
      free( *p );
      *p = NULL;
   }
   else
      bug( "%s: NULL pointer.", __FUNCTION__ );
}

CHAR_DATA *trvch_next( TRV_DATA * lc )
{
   EXTRACT_CHAR_DATA *extr;
   CHAR_DATA *nx;

   if( !lc )
      return NULL;
   if( lc->type < TR_CHAR_WORLD_FORW || lc->type > TR_CHAR_ROOM_BACK )
   {
      bug( "%s: called on a type %d structure.", __FUNCTION__, lc->type );
      return NULL;
   }
   while( lc->current < lc->count )
   {
      nx = ( CHAR_DATA * ) lc->el[lc->current++];

      /*
       * Check for list membership. Doesn't apply to world loops. 
       */
      if( lc->where && nx->in_room && nx->in_room != lc->where )
         continue;

      /*
       * Check for extracted chars.  Either an NPC or a quitting PC... 
       */
      if( !nx->in_room )
         continue;

      /*
       * It may still be a "revived" PC... 
       */
      for( extr = extracted_char_queue; extr != lc->ext_mark; extr = extr->next )
         if( extr->ch == nx )
            break;
      if( extr != lc->ext_mark )
         continue;

      return nx;
   }
   /*
    * No more chars in the list, endgame 
    */
   return NULL;
}

OBJ_DATA *trvobj_next( TRV_DATA * lc )
{
   OBJ_DATA *extr;
   void *where;
   OBJ_DATA *nx;

   if( !lc )
      return NULL;
   if( lc->type < TR_OBJ_WORLD_FORW || lc->type > TR_OBJ_OBJ_BACK )
   {
      bug( "%s: called on a type %d structure.", __FUNCTION__, lc->type );
      return NULL;
   }
   while( lc->current < lc->count )
   {
      nx = ( OBJ_DATA * ) lc->el[lc->current++];

      /*
       * Check for list membership 
       */
      if( nx->in_room )
         where = nx->in_room;
      else if( nx->carried_by )
         where = nx->carried_by;
      else
         where = nx->in_obj;
      if( lc->where && where && where != lc->where )
         continue;

      /*
       * Skip extracted and auctioned objs 
       */
      if( !where )
         continue;
      /*
       * Do we need this if it's ok to skip auctions? 
       */
      for( extr = extracted_obj_queue; extr != lc->ext_mark; extr = extr->next )
         if( extr == nx )
            break;
      if( extr != lc->ext_mark )
         continue;

      return nx;
   }
   return NULL;
}

TRV_WORLD *trworld_create( trv_type tp )
{
   TRV_WORLD *tnew;

   if( tp != TR_CHAR_WORLD_FORW && tp != TR_CHAR_WORLD_BACK && tp != TR_OBJ_WORLD_FORW && tp != TR_OBJ_WORLD_BACK )
   {
      bug( "%s: invalid type %d.", __FUNCTION__, tp );
      return NULL;
   }
   if( trw_loops >= TRW_MAXHEAP )
   {
      bug( "%s: heap limit exceeded.", __FUNCTION__ );
      return NULL;
   }
   tnew = trw_heap + trw_loops++;
   tnew->type = tp;
   if( tp == TR_CHAR_WORLD_FORW )
   {
      tnew->limit = last_char;
      tnew->next = first_char->next;
   }
   else if( tp == TR_CHAR_WORLD_BACK )
   {
      tnew->limit = first_char;
      tnew->next = last_char->prev;
   }
   else if( tp == TR_OBJ_WORLD_FORW )
   {
      tnew->limit = last_object;
      tnew->next = first_object->next;
   }
   else
   {  /* TR_OBJ_WORLD_BACK */
      tnew->limit = first_object;
      tnew->next = last_object->prev;
   }
   return tnew;
}

void trworld_dispose( TRV_WORLD ** trash )
{

   if( ( trw_heap + --trw_loops ) != *trash )
   {
      bug( "%s: midlist control block (%zd).", __FUNCTION__, *trash - trw_heap );
      ++trw_loops;
   }
   else
      *trash = NULL;
}

CHAR_DATA *trvch_wnext( TRV_WORLD * lc )
{
   CHAR_DATA *nx;

   if( lc->type != TR_CHAR_WORLD_FORW && lc->type != TR_CHAR_WORLD_BACK )
   {
      bug( "%s: invalid type (%d).", __FUNCTION__, lc->type );
      return NULL;
   }
   nx = ( CHAR_DATA * ) lc->next;
   if( !nx )
      return NULL;
   if( nx == lc->limit )
      lc->next = NULL;
   else
      lc->next = lc->type == TR_CHAR_WORLD_FORW ? nx->next : nx->prev;
   return nx;
}

OBJ_DATA *trvobj_wnext( TRV_WORLD * lc )
{
   OBJ_DATA *nx;

   if( lc->type != TR_OBJ_WORLD_FORW && lc->type != TR_OBJ_WORLD_BACK )
   {
      bug( "%s: invalid type (%d).", __FUNCTION__, lc->type );
      return NULL;
   }
   nx = ( OBJ_DATA * ) lc->next;
   if( !nx )
      return NULL;
   if( nx == lc->limit )
      lc->next = NULL;
   else
      lc->next = lc->type == TR_OBJ_WORLD_FORW ? nx->next : nx->prev;
   return nx;
}

/* For node removal. Add wherever you remove nodes from the world lists,
   like in extract_char() and extract_obj()  */
void trworld_char_check( CHAR_DATA * rmv )
{
   TRV_WORLD *sp;

   if( !rmv )
      return;
   for( sp = trw_heap + trw_loops - 1; sp >= trw_heap; --sp )
   {

      if( sp->type < TR_CHAR_WORLD_FORW || sp->type > TR_CHAR_WORLD_BACK )
         continue;
      if( sp->limit == rmv )
         sp->limit = sp->type == TR_CHAR_WORLD_FORW ? rmv->prev : rmv->next;
      if( sp->next == rmv )
         sp->next = sp->type == TR_CHAR_WORLD_FORW ?
            ( ( CHAR_DATA * ) ( sp->next ) )->next : ( ( CHAR_DATA * ) ( sp->next ) )->prev;
   }
}

void trworld_obj_check( OBJ_DATA * rmv )
{
   TRV_WORLD *sp;

   if( !rmv )
      return;
   for( sp = trw_heap + trw_loops - 1; sp >= trw_heap; --sp )
   {

      if( sp->type < TR_OBJ_WORLD_FORW || sp->type > TR_OBJ_WORLD_BACK )
         continue;
      if( sp->limit == rmv )
         sp->limit = sp->type == TR_OBJ_WORLD_FORW ? rmv->prev : rmv->next;
      if( sp->next == rmv )
         sp->next = sp->type == TR_OBJ_WORLD_FORW ?
            ( ( CHAR_DATA * ) ( sp->next ) )->next : ( ( CHAR_DATA * ) ( sp->next ) )->prev;
   }
}

int umin( int check, int ncheck )
{
   if( check < ncheck )
      return check;
   return ncheck;
}

int umax( int check, int ncheck )
{
   if( check > ncheck )
      return check;
   return ncheck;
}

int urange( int mincheck, int check, int maxcheck )
{
   if( check < mincheck )
      return mincheck;
   if( check > maxcheck )
      return maxcheck;
   return check;
}

/*
 * Return how much exp a char has
 */
int get_exp( CHAR_DATA * ch )
{
   return ch->experience[ch->Class];
}

/*
 * Calculate roughly how much experience a character is worth
 */
int get_exp_worth( CHAR_DATA * ch )
{
   int wexp;

   wexp = ch->level * ch->level * ch->level * 5;
   wexp += ch->max_hit;
   wexp -= ( ch->armor - 50 ) * 2;
   wexp += ( ch->barenumdie * ch->baresizedie + GET_ATTACK( ch ) ) * 50;
   if( IS_AFFECTED( ch, AFF_SANCTUARY ) )
      wexp += ( int )( wexp * 1.5 );
   if( IS_AFFECTED( ch, AFF_FIRESHIELD ) )
      wexp += ( int )( wexp * 1.2 );
   if( IS_AFFECTED( ch, AFF_SHOCKSHIELD ) )
      wexp += ( int )( wexp * 1.2 );
   wexp = URANGE( MIN_EXP_WORTH, wexp, MAX_EXP_WORTH );

   return wexp;
}

short get_exp_base( CHAR_DATA * ch )
{
   if( IS_NPC( ch ) )
      return 1000;
   return class_table[ch->Class]->exp_base;
}

/*								-Thoric
 * Return how much experience is required for ch to get to a certain level
 */
int exp_level( CHAR_DATA * ch, short level )
{
   int lvl;

   lvl = UMAX( 0, level - 1 );
   return ( lvl * lvl * lvl * get_exp_base( ch ) );
}

/*
 * Get what level ch is based on exp
 */
short level_exp( CHAR_DATA * ch, int cexp )
{
   int x, lastx, y, tmp;

   x = LEVEL_SUPREME;
   lastx = x;
   y = 0;
   while( !y )
   {
      tmp = exp_level( ch, x );
      lastx = x;
      if( tmp > cexp )
         x /= 2;
      else if( lastx != x )
         x += ( x / 2 );
      else
         y = x;
   }
   if( y < 1 )
      y = 1;
   if( y > LEVEL_SUPREME )
      y = LEVEL_SUPREME;
   return y;
}

/*
 * Retrieve a character's trusted level for permission checking.
 */
short get_trust( CHAR_DATA * ch )
{
   if( ch->desc && ch->desc->original )
      ch = ch->desc->original;

   if( ch->trust != 0 )
      return ch->trust;

   if( IS_NPC( ch ) && ch->top_level >= LEVEL_AVATAR )
      return LEVEL_AVATAR;

   if( ch->top_level >= LEVEL_NEOPHYTE && IS_RETIRED( ch ) )
      return LEVEL_NEOPHYTE;

   return ch->top_level;
}

/* One hopes this will do as planned and determine how old a PC is based on the birthdate
   we record at creation. - Samson 10-25-99 */
short calculate_age( CHAR_DATA * ch )
{
   short age, num_days, ch_days;

   if( IS_NPC( ch ) )
      return -1;

   num_days = ( time_info.month + 1 ) * sysdata.dayspermonth;
   num_days += time_info.day;

   ch_days = ( ch->pcdata->month + 1 ) * sysdata.dayspermonth;
   ch_days += ch->pcdata->day;

   age = time_info.year - ch->pcdata->year;

   if( ch_days - num_days > 0 )
      age -= 1;

   return age;
}

/*
 * Retrieve character's current strength.
 */
short get_curr_str( CHAR_DATA * ch )
{
   return ( ch->perm_str + ch->mod_str );
}

/*
 * Retrieve character's current intelligence.
 */
short get_curr_int( CHAR_DATA * ch )
{
   return ( ch->perm_int + ch->mod_int );
}

/*
 * Retrieve character's current wisdom.
 */
short get_curr_wis( CHAR_DATA * ch )
{
   return ( ch->perm_wis + ch->mod_wis );
}

/*
 * Retrieve character's current dexterity.
 */
short get_curr_dex( CHAR_DATA * ch )
{
   return ( ch->perm_dex + ch->mod_dex );
}

/*
 * Retrieve character's current constitution.
 */
short get_curr_con( CHAR_DATA * ch )
{
   return ( ch->perm_con + ch->mod_con );
}

/*
 * Retrieve character's current charisma.
 */
short get_curr_cha( CHAR_DATA * ch )
{
   return ( ch->perm_cha + ch->mod_cha );
}

/*
 * Retrieve character's current passion.
 */
short get_curr_pas( CHAR_DATA * ch )
{
   return ( ch->perm_pas + ch->mod_pas );
}

/*
 * Retrieve a character's carry capacity.
 * Vastly reduced (finally) due to containers		-Thoric
 */
int can_carry_n( CHAR_DATA * ch )
{
   int penalty = 0;

   if( !IS_NPC( ch ) && ch->level >= LEVEL_IMMORTAL )
      return get_trust( ch ) * 200;

   if( IS_NPC( ch ) && xIS_SET( ch->act, ACT_IMMORTAL ) )
      return ch->level * 200;

   if( get_eq_char( ch, WEAR_WIELD ) )
      ++penalty;
   if( get_eq_char( ch, WEAR_DUAL_WIELD ) )
      ++penalty;
   if( get_eq_char( ch, WEAR_MISSILE_WIELD ) )
      ++penalty;
   if( get_eq_char( ch, WEAR_HOLD ) )
      ++penalty;
   if( get_eq_char( ch, WEAR_SHIELD ) )
      ++penalty;
   return URANGE( 5, ( ch->level + 15 ) / 5 + get_curr_dex( ch ) - 13 - penalty, 20 );
}

/*
 * Retrieve a character's carry capacity.
 */
int can_carry_w( CHAR_DATA * ch )
{
   if( !IS_NPC( ch ) && ch->level >= LEVEL_IMMORTAL )
      return 1000000;

   if( IS_NPC( ch ) && xIS_SET( ch->act, ACT_IMMORTAL ) )
      return 1000000;

   return str_app[get_curr_str( ch )].carry;
}

/*
 * See if a player/mob can take a piece of prototype eq		-Thoric
 */
bool can_take_proto( CHAR_DATA * ch )
{
   if( IS_IMMORTAL( ch ) )
      return TRUE;
   else if( IS_NPC( ch ) && xIS_SET( ch->act, ACT_PROTOTYPE ) )
      return TRUE;
   else
      return FALSE;
}

/*
 * See if a string is one of the names of an object.
 */
bool is_name( const char *str, const char *namelist )
{
   char name[MAX_INPUT_LENGTH];

   for( ;; )
   {
      namelist = one_argument( namelist, name );
      if( name[0] == '\0' )
         return FALSE;
      if( !str_cmp( str, name ) )
         return TRUE;
   }
}

bool is_name_prefix( const char *str, char *namelist )
{
   char name[MAX_INPUT_LENGTH];

   for( ;; )
   {
      namelist = one_argument( namelist, name );
      if( name[0] == '\0' )
         return FALSE;
      if( !str_prefix( str, name ) )
         return TRUE;
   }
}

/*
 * See if a string is one of the names of an object.		-Thoric
 * Treats a dash as a word delimiter as well as a space
 */
bool is_name2( const char *str, const char *namelist )
{
   char name[MAX_INPUT_LENGTH];

   for( ;; )
   {
      namelist = one_argument2( namelist, name );
      if( name[0] == '\0' )
         return FALSE;
      if( !str_cmp( str, name ) )
         return TRUE;
   }
}

bool is_name2_prefix( const char *str, const char *namelist )
{
   char name[MAX_INPUT_LENGTH];

   for( ;; )
   {
      namelist = one_argument2( namelist, name );
      if( name[0] == '\0' )
         return FALSE;
      if( !str_prefix( str, name ) )
         return TRUE;
   }
}

/* Rewrote the 'nifty' functions since they mistakenly allowed for all objects
   to be selected by specifying an empty list like -, '', "", ', " etc,
   example: ofind -, c loc ''  - Luc 08/2000 */
bool nifty_is_name( const char *str, const char *namelist )
{
   char name[MAX_INPUT_LENGTH];
   bool valid = FALSE;

   if( !str || !str[0] )
      return FALSE;

   for( ;; )
   {
      str = one_argument2( str, name );
      if( *name )
      {
         valid = TRUE;
         if( !is_name2( name, namelist ) )
            return FALSE;
      }
      if( !*str )
         return valid;
   }
}

bool nifty_is_name_prefix( const char *str, const char *namelist )
{
   char name[MAX_INPUT_LENGTH];
   bool valid = FALSE;

   if( !str || !str[0] )
      return FALSE;

   for( ;; )
   {
      str = one_argument2( str, name );
      if( *name )
      {
         valid = TRUE;
         if( !is_name2_prefix( name, namelist ) )
            return FALSE;
      }
      if( !*str )
         return valid;
   }
}

void room_affect( ROOM_INDEX_DATA * pRoomIndex, AFFECT_DATA * paf, bool fAdd )
{
   if( fAdd )
   {
      switch ( paf->location )
      {
         case APPLY_ROOMFLAG:
         case APPLY_SECTORTYPE:
            break;
         case APPLY_ROOMLIGHT:
            pRoomIndex->light += paf->modifier;
            break;
         case APPLY_TELEVNUM:
         case APPLY_TELEDELAY:
            break;
      }
   }
   else
   {
      switch ( paf->location )
      {
         case APPLY_ROOMFLAG:
         case APPLY_SECTORTYPE:
            break;
         case APPLY_ROOMLIGHT:
            pRoomIndex->light -= paf->modifier;
            break;
         case APPLY_TELEVNUM:
         case APPLY_TELEDELAY:
            break;
      }
   }
}

/*
 * Modify a skill (hopefully) properly			-Thoric
 *
 * On "adding" a skill modifying affect, the value set is unimportant
 * upon removing the affect, the skill it enforced to a proper range.
 */
void modify_skill( CHAR_DATA * ch, int sn, int mod, bool fAdd )
{
   if( !IS_NPC( ch ) )
   {
      if( fAdd )
         ch->pcdata->learned[sn] += mod;
      else
         ch->pcdata->learned[sn] = URANGE( 0, ch->pcdata->learned[sn] + mod, GET_ADEPT( ch, sn ) );
   }
}

/*
 * Apply or remove an affect to a character.
 */
void affect_modify( CHAR_DATA * ch, AFFECT_DATA * paf, bool fAdd )
{
   OBJ_DATA *wield;
   int mod, sn;
   struct skill_type *skill;
   ch_ret retcode;

   mod = paf->modifier;

   if( fAdd )
   {
      xSET_BITS( ch->affected_by, paf->bitvector );
      if( xIS_SET( paf->bitvector, AFF_STUN ) )
         interrupt( ch );
      if( paf->location % REVERSE_APPLY == APPLY_RECURRINGSPELL )
      {
         mod = abs( mod );
         if( IS_VALID_SN( mod ) && ( skill = skill_table[mod] ) != NULL && skill->type == SKILL_SPELL )
            xSET_BIT( ch->affected_by, AFF_RECURRINGSPELL );
         else
            bug( "affect_modify(%s) APPLY_RECURRINGSPELL with bad sn %d", ch->name, mod );
         return;
      }
   }
   else
   {
      xREMOVE_BITS( ch->affected_by, paf->bitvector );
      /*
       * might be an idea to have a duration removespell which returns
       * the spell after the duration... but would have to store
       * the removed spell's information somewhere...    -Thoric
       * (Though we could keep the affect, but disable it for a duration)
       */

      if( paf->location % REVERSE_APPLY == APPLY_RECURRINGSPELL )
      {
         mod = abs( mod );
         if( !IS_VALID_SN( mod ) || ( skill = skill_table[mod] ) == NULL || skill->type != SKILL_SPELL )
            bug( "affect_modify(%s) APPLY_RECURRINGSPELL with bad sn %d", ch->name, mod );
         xREMOVE_BIT( ch->affected_by, AFF_RECURRINGSPELL );
         return;
      }

      switch ( paf->location % REVERSE_APPLY )
      {
         case APPLY_AFFECT:
            REMOVE_BIT( ch->affected_by.bits[0], mod );
            return;
         case APPLY_EXT_AFFECT:
            xREMOVE_BIT( ch->affected_by, mod );
            return;
         case APPLY_RESISTANT:
            REMOVE_BIT( ch->resistant, mod );
            return;
         case APPLY_IMMUNE:
            REMOVE_BIT( ch->immune, mod );
            return;
         case APPLY_SUSCEPTIBLE:
            REMOVE_BIT( ch->susceptible, mod );
            return;
         case APPLY_REMOVE:
            SET_BIT( ch->affected_by.bits[0], mod );
            return;
         default:
            break;
      }
      mod = 0 - mod;
   }

   switch ( paf->location % REVERSE_APPLY )
   {
      default:
         bug( "Affect_modify: unknown location %d.", paf->location );
         return;

      case APPLY_NONE:
         break;
      case APPLY_STR:
         adjust_stat( ch, STAT_STRENGTH, mod );
         break;
      case APPLY_DEX:
         adjust_stat( ch, STAT_DEXTERITY, mod );
         break;
      case APPLY_INT:
         adjust_stat( ch, STAT_INTELLIGENCE, mod );
         break;
      case APPLY_WIS:
         adjust_stat( ch, STAT_WISDOM, mod );
         break;
      case APPLY_CON:
         adjust_stat( ch, STAT_CONSTITUTION, mod );
         break;
      case APPLY_CHA:
         ch->mod_cha += mod;
         break;
      case APPLY_PAS:
         adjust_stat( ch, STAT_PASSION, mod );
         break;
      case APPLY_SEX:
         ch->sex = ( ch->sex + mod ) % 3;
         if( ch->sex < 0 )
            ch->sex += 2;
         ch->sex = URANGE( 0, ch->sex, 2 );
         break;

         /*
          * These are unused due to possible problems.  Enable at your own risk.
          */
      case APPLY_CLASS:
         break;
      case APPLY_LEVEL:
         break;
      case APPLY_AGE:
         break;
      case APPLY_GOLD:
         break;
      case APPLY_EXP:
         break;

         /*
          * Regular apply types
          */
      case APPLY_HEIGHT:
         ch->height += mod;
         break;
      case APPLY_WEIGHT:
         ch->weight += mod;
         break;
      case APPLY_MANA:
         adjust_stat( ch, STAT_MAXMANA, mod );
         break;
      case APPLY_HIT:
         adjust_stat( ch, STAT_MAXHIT, mod );
         break;
      case APPLY_MOVE:
         adjust_stat( ch, STAT_MAXMOVE, mod );
         break;
      case APPLY_ARMOR:
         adjust_stat( ch, STAT_DEFENSE, mod );
         break;
      case APPLY_ATTACK:
         adjust_stat( ch, STAT_ATTACK, mod );
         break;
      case APPLY_SAVING_POISON:
         ch->saving_poison_death += mod;
         break;
      case APPLY_SAVING_ROD:
         ch->saving_wand += mod;
         break;
      case APPLY_SAVING_PARA:
         ch->saving_para_petri += mod;
         break;
      case APPLY_SAVING_BREATH:
         ch->saving_breath += mod;
         break;
      case APPLY_SAVING_SPELL:
         ch->saving_spell_staff += mod;
         break;

         /*
          * Bitvector modifying apply types
          */
      case APPLY_AFFECT:
         SET_BIT( ch->affected_by.bits[0], mod );
         break;
      case APPLY_EXT_AFFECT:
         xSET_BIT( ch->affected_by, mod );
         break;
      case APPLY_RESISTANT:
         SET_BIT( ch->resistant, mod );
         break;
      case APPLY_IMMUNE:
         SET_BIT( ch->immune, mod );
         break;
      case APPLY_SUSCEPTIBLE:
         SET_BIT( ch->susceptible, mod );
         break;
      case APPLY_WEAPONSPELL:   /* see fight.c */
         break;
      case APPLY_REMOVE:
         REMOVE_BIT( ch->affected_by.bits[0], mod );
         break;

         /*
          * Player condition modifiers
          */
      case APPLY_FULL:
         if( !IS_NPC( ch ) )
            ch->pcdata->condition[COND_FULL] = URANGE( 0, ch->pcdata->condition[COND_FULL] + mod, 48 );
         break;

      case APPLY_THIRST:
         if( !IS_NPC( ch ) )
            ch->pcdata->condition[COND_THIRST] = URANGE( 0, ch->pcdata->condition[COND_THIRST] + mod, 48 );
         break;

      case APPLY_DRUNK:
         if( !IS_NPC( ch ) )
            ch->pcdata->condition[COND_DRUNK] = URANGE( 0, ch->pcdata->condition[COND_DRUNK] + mod, 48 );
         break;

      case APPLY_BLOOD:
         if( !IS_NPC( ch ) )
            ch->pcdata->condition[COND_BLOODTHIRST] =
               URANGE( 0, ch->pcdata->condition[COND_BLOODTHIRST] + mod, ch->level + 10 );
         break;

      case APPLY_MENTALSTATE:
         ch->mental_state = URANGE( -100, ch->mental_state + mod, 100 );
         break;
      case APPLY_EMOTION:
         ch->emotional_state = URANGE( -100, ch->emotional_state + mod, 100 );
         break;


         /*
          * Specialty modfiers
          */
      case APPLY_CONTAGIOUS:
         break;
      case APPLY_ODOR:
         break;
      case APPLY_STRIPSN:
         if( IS_VALID_SN( mod ) )
            affect_strip( ch, mod );
         else
            bug( "affect_modify: APPLY_STRIPSN invalid sn %d", mod );
         break;

         /*
          * spell cast upon wear/removal of an object -Thoric 
          */
      case APPLY_WEARSPELL:
      case APPLY_REMOVESPELL:
         if( xIS_SET( ch->in_room->room_flags, ROOM_NO_MAGIC ) || IS_SET( ch->immune, RIS_MAGIC )
          || ( ( paf->location % REVERSE_APPLY ) == APPLY_WEARSPELL && !fAdd )
          || ( ( paf->location % REVERSE_APPLY ) == APPLY_REMOVESPELL && fAdd )
          || saving_char == ch   /* so save/quit doesn't trigger */
          || loading_char == ch )   /* so loading doesn't trigger */
            return;

         mod = abs( mod );
         if( IS_VALID_SN( mod ) && ( skill = skill_table[mod] ) != NULL && skill->type == SKILL_SPELL )
         {
            if( skill->target == TAR_IGNORE || skill->target == TAR_OBJ_INV )
            {
               bug( "APPLY_WEARSPELL trying to apply bad target spell.  SN is %d.", mod );
               return;
            }
            if( ( retcode = ( *skill->spell_fun ) ( mod, ch->level, ch, ch ) ) == rCHAR_DIED || char_died( ch ) )
               return;
         }
         break;


         /*
          * Skill apply types
          */
      case APPLY_PALM: /* not implemented yet */
         break;
      case APPLY_TRACK:
         modify_skill( ch, gsn_track, mod, fAdd );
         break;
      case APPLY_HIDE:
         modify_skill( ch, gsn_hide, mod, fAdd );
         break;
      case APPLY_STEAL:
         modify_skill( ch, gsn_steal, mod, fAdd );
         break;
      case APPLY_SNEAK:
         modify_skill( ch, gsn_sneak, mod, fAdd );
         break;
      case APPLY_PICK:
         modify_skill( ch, gsn_pick_lock, mod, fAdd );
         break;
      case APPLY_BACKSTAB:
         modify_skill( ch, gsn_backstab, mod, fAdd );
         break;
      case APPLY_DETRAP:
         modify_skill( ch, gsn_detrap, mod, fAdd );
         break;
      case APPLY_PEEK:
         modify_skill( ch, gsn_peek, mod, fAdd );
         break;
      case APPLY_SCAN:
         modify_skill( ch, gsn_scan, mod, fAdd );
         break;
      case APPLY_GOUGE:
         modify_skill( ch, gsn_gouge, mod, fAdd );
         break;
      case APPLY_SEARCH:
         modify_skill( ch, gsn_search, mod, fAdd );
         break;
      case APPLY_DIG:
         modify_skill( ch, gsn_dig, mod, fAdd );
         break;
      case APPLY_MOUNT:
         modify_skill( ch, gsn_mount, mod, fAdd );
         break;
      case APPLY_DISARM:
         modify_skill( ch, gsn_disarm, mod, fAdd );
         break;
      case APPLY_KICK:
         modify_skill( ch, gsn_kick, mod, fAdd );
         break;
      case APPLY_BASH:
         modify_skill( ch, gsn_bash, mod, fAdd );
         break;
      case APPLY_STUN:
         modify_skill( ch, gsn_stun, mod, fAdd );
         break;
      case APPLY_PUNCH:
         modify_skill( ch, gsn_punch, mod, fAdd );
         break;
      case APPLY_CLIMB:
         modify_skill( ch, gsn_climb, mod, fAdd );
         break;
      case APPLY_GRIP:
         modify_skill( ch, gsn_grip, mod, fAdd );
         break;
      case APPLY_SCRIBE:
         modify_skill( ch, gsn_scribe, mod, fAdd );
         break;
      case APPLY_BREW:
         modify_skill( ch, gsn_brew, mod, fAdd );
         break;
      case APPLY_COOK:
         modify_skill( ch, gsn_cook, mod, fAdd );
         break;

         /*
          * Room apply types
          */
      case APPLY_ROOMFLAG:
      case APPLY_SECTORTYPE:
      case APPLY_ROOMLIGHT:
      case APPLY_TELEVNUM:
         break;
      case APPLY_PENETRATION:
         adjust_stat( ch, STAT_PENETRATION, mod );
         break;
      case APPLY_RESISTANCE:
         adjust_stat( ch, STAT_RESISTANCE, mod );
         break;
      case APPLY_DTYPEPOTENCY:
         adjust_stat( ch, STAT_DTYPEPOTENCY, mod );
         break;
      case APPLY_SKILLPOTENCY:
         int amount;
         sn = get_value_one( mod );
         amount = get_value_two( mod );
         ch->pcdata->potency[sn] += amount;
         break;
      case APPLY_SKILLRANGE:
         sn = get_value_one( mod );
         amount = get_value_two( mod );
         ch->pcdata->range[sn] += amount;
         break;
      case APPLY_SKILLCOOLDOWN:
         sn = get_value_one( mod );
         amount = get_value_two( mod );
         ch->pcdata->cooldown[sn] += amount;
         break;
      case APPLY_SKILLDURATION:
         sn = get_value_one( mod );
         amount = get_value_two( mod );
         ch->pcdata->cooldown[sn] += amount;
         break;
      case APPLY_SKILLHITS:
         sn = get_value_one( mod );
         amount = get_value_two( mod );
         ch->pcdata->hits[sn] += amount;
      case APPLY_GRANTSKILL:
         if( IS_VALID_SN( mod ) )
            xSET_BIT( ch->granted_skills, mod );
         break;
      case APPLY_BARENUMDIE:
         adjust_stat( ch, STAT_BARENUMDIE, mod );
         break;
      case APPLY_BARESIZEDIE:
         adjust_stat( ch, STAT_BARESIZEDIE, mod );
         break;
      case APPLY_WEPNUMDIE:
         adjust_stat( ch, STAT_WEPNUMDIE, mod );
         break;
      case APPLY_WEPSIZEDIE:
         adjust_stat( ch, STAT_WEPSIZEDIE, mod );
         break;
      case APPLY_MAGICATTACK:
         adjust_stat( ch, STAT_MAGICATTACK, mod );
         break;
      case APPLY_HASTE:
         adjust_stat( ch, STAT_HASTE, mod );
         break;
      case APPLY_HASTEFROMMAGIC:
         adjust_stat( ch, STAT_HASTEFROMMAGIC, mod );
         break;
      case APPLY_MAGICDEFENSE:
         adjust_stat( ch, STAT_MAGICDEFENSE, mod );
         break;
      case APPLY_THREAT:
         adjust_stat( ch, STAT_THREAT, mod );
         break;
      case APPLY_PERMSTR:
         adjust_stat( ch, STAT_PERMSTR, mod );
         break;
      case APPLY_PERMDEX:
         adjust_stat( ch, STAT_PERMDEX, mod );
         break;
      case APPLY_PERMCON:
         adjust_stat( ch, STAT_PERMCON, mod );
         break;
      case APPLY_PERMINT:
         adjust_stat( ch, STAT_PERMINT, mod );
         break;
      case APPLY_PERMWIS:
         adjust_stat( ch, STAT_PERMWIS, mod );
         break;
      case APPLY_PERMPAS:
         adjust_stat( ch, STAT_PERMPAS, mod );
         break;
      case APPLY_POTENCY:
         adjust_stat( ch, STAT_POTENCY, mod );
         break;
      case APPLY_RANGE:
         adjust_stat( ch, STAT_RANGE, mod );
         break;
      case APPLY_COOLDOWNS:
         adjust_stat( ch, STAT_COOLDOWNS, mod );
         break;
      case APPLY_DURATIONS:
         adjust_stat( ch, STAT_DURATIONS, mod );
         break;
      case APPLY_REGEN:
         adjust_stat( ch, STAT_REGEN, mod );
         break;
      case APPLY_REFRESH:
         adjust_stat( ch, STAT_REFRESH, mod );
         break;
      case APPLY_FEEDBACKPOTENCY:
         adjust_stat( ch, STAT_FEEDBACKPOTENCY, mod );
         break;
      case APPLY_DOUBLEATTACK:
         adjust_stat( ch, STAT_DOUBLEATTACK, mod );
         break;
      case APPLY_CRITCHANCE:
         adjust_stat( ch, STAT_CRITCHANCE, mod );
         break;
      case APPLY_CRITDAMAGE:
         adjust_stat( ch, STAT_CRITDAM, mod );
         break;
      case APPLY_COUNTER:
         adjust_stat( ch, STAT_COUNTER, mod );
         break;
      case APPLY_PHASE:
         adjust_stat( ch, STAT_PHASE, mod );
         break;
      case APPLY_BLOCK:
         adjust_stat( ch, STAT_BLOCK, mod );
         break;
      case APPLY_DODGE:
         adjust_stat( ch, STAT_DODGE, mod );
         break;
      case APPLY_PARRY:
         adjust_stat( ch, STAT_PARRY, mod );
         break;
      case APPLY_COMBODMG:
         adjust_stat( ch, STAT_COMBODMG, mod );
         break;
      case APPLY_CHARMEDDMGBOOST:
         adjust_stat( ch, STAT_CHARMEDDMG, mod );
         break;
      case APPLY_CHARMEDDEFBOOST:
         adjust_stat( ch, STAT_CHARMEDDEF, mod );
         break;
      case APPLY_GRAVITY:
         adjust_stat( ch, STAT_GRAVITY, mod );
         break;
         /*
          * Object apply types
          */
   }

   /*
    * Check for weapon wielding.
    * Guard against recursion (for weapons with affects).
    */
   if( !IS_NPC( ch )
       && saving_char != ch
       && ( wield = get_eq_char( ch, WEAR_WIELD ) ) != NULL && get_obj_weight( wield ) > str_app[get_curr_str( ch )].wield )
   {
      static int depth;

      if( depth == 0 )
      {
         depth++;
         act( AT_ACTION, "You are too weak to wield $p any longer.", ch, wield, NULL, TO_CHAR );
         act( AT_ACTION, "$n stops wielding $p.", ch, wield, NULL, TO_ROOM );
         unequip_char( ch, wield );
         depth--;
      }
   }

   return;
}

/*
 * Give an affect to a char.
 */
void affect_to_char( CHAR_DATA *ch, AFFECT_DATA *paf )
{
   affect_to_char( ch, NULL, paf );
   return;
}
void affect_to_char( CHAR_DATA * ch, CHAR_DATA *from, AFFECT_DATA * paf )
{
   AFFECT_DATA *paf_new;

   if( !ch )
   {
      bug( "%s (NULL, %d)", __FUNCTION__, paf ? paf->type : 0 );
      return;
   }

   if( !paf )
   {
      bug( "%s (%s, NULL)", __FUNCTION__, ch->name );
      return;
   }

   CREATE( paf_new, AFFECT_DATA, 1 );
   LINK( paf_new, ch->first_affect, ch->last_affect, next, prev );
   paf_new->type = paf->type;
   paf_new->duration = paf->duration;
   paf_new->location = paf->location;
   paf_new->modifier = paf->modifier;
   paf_new->bitvector = paf->bitvector;

   affect_modify( ch, paf_new, TRUE );

   /*
    * SMAUG's support (not complete in stock SMAUG) for player affects
    * affecting a room (only "light" is supported).
    */
   if( ch->in_room )
      room_affect( ch->in_room, paf_new, TRUE );

   if( !is_queued( ch, AFFECT_TIMER ) )
      add_queue( ch, AFFECT_TIMER );

   if( from )
   {
      if( skill_table[paf_new->type]->target == TAR_CHAR_DEFENSIVE )
         generate_buff_threat( from, ch, get_threat( from, paf_new->type ) );
      else if( skill_table[paf_new->type]->target == TAR_CHAR_OFFENSIVE )
         generate_threat( from, ch, get_threat( from, paf_new->type ) );
      paf_new->affect_from = from;
   }
   return;
}

/*
 * Remove an affect from a char.
 */
void affect_remove( CHAR_DATA * ch, AFFECT_DATA * paf )
{
   if( !ch->first_affect )
   {
      bug( "Affect_remove(%s, %d): no affect.", ch->name, paf ? paf->type : 0 );
      return;
   }

   affect_modify( ch, paf, FALSE );

   /*
    * SMAUG's support (not complete in stock SMAUG) for player affects
    * affecting a room (only "light" is supported).
    */
   if( ch->in_room )
      room_affect( ch->in_room, paf, FALSE );

   UNLINK( paf, ch->first_affect, ch->last_affect, next, prev );
   DISPOSE( paf );
   return;
}

/*
 * Strip all affects of a given sn.
 */
void affect_strip( CHAR_DATA * ch, int sn )
{
   AFFECT_DATA *paf;
   AFFECT_DATA *paf_next;

   for( paf = ch->first_affect; paf; paf = paf_next )
   {
      paf_next = paf->next;
      if( paf->type == sn )
         affect_remove( ch, paf );
   }

   return;
}



/*
 * Return true if a char is affected by a spell.
 */
bool is_affected( CHAR_DATA * ch, int sn )
{
   AFFECT_DATA *paf;

   for( paf = ch->first_affect; paf; paf = paf->next )
      if( paf->type == sn )
         return TRUE;

   return FALSE;
}


/*
 * Add or enhance an affect.
 * Limitations put in place by Thoric, they may be high... but at least
 * they're there :)
 */
void affect_join( CHAR_DATA * ch, AFFECT_DATA * paf )
{
   AFFECT_DATA *paf_old;

   for( paf_old = ch->first_affect; paf_old; paf_old = paf_old->next )
      if( paf_old->type == paf->type )
      {
         paf->duration = UMIN( 1000000, (int)paf->duration + (int)paf_old->duration );
         if( paf->modifier )
            paf->modifier = UMIN( 5000, paf->modifier + paf_old->modifier );
         else
            paf->modifier = paf_old->modifier;
         affect_remove( ch, paf_old );
         break;
      }

   affect_to_char( ch, paf );
   return;
}


/*
 * Apply only affected and RIS on a char
 */
void aris_affect( CHAR_DATA * ch, AFFECT_DATA * paf )
{
   xSET_BITS( ch->affected_by, paf->bitvector );
   switch ( paf->location % REVERSE_APPLY )
   {
      case APPLY_AFFECT:
         SET_BIT( ch->affected_by.bits[0], paf->modifier );
         break;
      case APPLY_RESISTANT:
         SET_BIT( ch->resistant, paf->modifier );
         break;
      case APPLY_IMMUNE:
         SET_BIT( ch->immune, paf->modifier );
         break;
      case APPLY_SUSCEPTIBLE:
         SET_BIT( ch->susceptible, paf->modifier );
         break;
   }
}

/*
 * Update affecteds and RIS for a character in case things get messed.
 * This should only really be used as a quick fix until the cause
 * of the problem can be hunted down. - FB
 * Last modified: June 30, 1997
 *
 * Quick fix?  Looks like a good solution for a lot of problems.
 */

/* Temp mod to bypass immortals so they can keep their mset affects,
 * just a band-aid until we get more time to look at it -- Blodkai */
void update_aris( CHAR_DATA * ch )
{
   AFFECT_DATA *paf;
   OBJ_DATA *obj;
   int hiding;

   if( IS_NPC( ch ) || IS_IMMORTAL( ch ) )
      return;

   /*
    * So chars using hide skill will continue to hide 
    */
   hiding = IS_AFFECTED( ch, AFF_HIDE );

   xCLEAR_BITS( ch->affected_by );
   ch->resistant = 0;
   ch->immune = 0;
   ch->susceptible = 0;
   xCLEAR_BITS( ch->no_affected_by );
   ch->no_resistant = 0;
   ch->no_immune = 0;
   ch->no_susceptible = 0;

   /*
    * Add in effects from race 
    */
   xSET_BITS( ch->affected_by, race_table[ch->race]->affected );
   SET_BIT( ch->resistant, race_table[ch->race]->resist );
   SET_BIT( ch->susceptible, race_table[ch->race]->suscept );

   /*
    * Add in effects from class 
    */
   xSET_BITS( ch->affected_by, class_table[ch->Class]->affected );
   SET_BIT( ch->resistant, class_table[ch->Class]->resist );
   SET_BIT( ch->susceptible, class_table[ch->Class]->suscept );

   /*
    * Add in effects from deities 
    */
   if( ch->pcdata->deity )
   {
      if( ch->pcdata->favor > ch->pcdata->deity->affectednum )
         xSET_BITS( ch->affected_by, ch->pcdata->deity->affected );
      if( ch->pcdata->favor > ch->pcdata->deity->elementnum )
         SET_BIT( ch->resistant, ch->pcdata->deity->element );
      if( ch->pcdata->favor < ch->pcdata->deity->susceptnum )
         SET_BIT( ch->susceptible, ch->pcdata->deity->suscept );
   }

   /*
    * Add in effect from spells 
    */
   for( paf = ch->first_affect; paf; paf = paf->next )
      aris_affect( ch, paf );

   /*
    * Add in effect from room 
    */
   if( ch->in_room )
   {
      for( paf = ch->in_room->first_affect; paf; paf = paf->next )
         aris_affect( ch, paf );
      for( paf = ch->in_room->first_permaffect; paf; paf = paf->next )
         aris_affect( ch, paf );
   }

   /*
    * Add in effects from equipment 
    */
   for( obj = ch->first_carrying; obj; obj = obj->next_content )
   {
      if( obj->wear_loc != WEAR_NONE )
      {
         for( paf = obj->first_affect; paf; paf = paf->next )
            aris_affect( ch, paf );

         for( paf = obj->pIndexData->first_affect; paf; paf = paf->next )
            aris_affect( ch, paf );
      }
   }

   /*
    * Add in effects from the room 
    */
   if( ch->in_room ) /* non-existant char booboo-fix --TRI */
      for( paf = ch->in_room->first_affect; paf; paf = paf->next )
         aris_affect( ch, paf );

   /*
    * Add in effects for polymorph 
    */
   if( ch->morph )
   {
      xSET_BITS( ch->affected_by, ch->morph->affected_by );
      SET_BIT( ch->immune, ch->morph->immune );
      SET_BIT( ch->resistant, ch->morph->resistant );
      SET_BIT( ch->susceptible, ch->morph->suscept );
      /*
       * Right now only morphs have no_ things --Shaddai 
       */
      xSET_BITS( ch->no_affected_by, ch->morph->no_affected_by );
      SET_BIT( ch->no_immune, ch->morph->no_immune );
      SET_BIT( ch->no_resistant, ch->morph->no_resistant );
      SET_BIT( ch->no_susceptible, ch->morph->no_suscept );
   }

   /*
    * If they were hiding before, make them hiding again 
    */
   if( hiding )
      xSET_BIT( ch->affected_by, AFF_HIDE );

   return;
}

/*
 * Move a char out of a room.
 */
void char_from_room( CHAR_DATA * ch )
{
   OBJ_DATA *obj;
   AFFECT_DATA *paf;

   if( !ch->in_room )
   {
      bug( "%s", "Char_from_room: NULL." );
      return;
   }

   if( !IS_NPC( ch ) )
      --ch->in_room->area->nplayer;

   if( ( obj = get_eq_char( ch, WEAR_LIGHT ) ) != NULL
       && obj->item_type == ITEM_LIGHT && obj->value[2] != 0 && ch->in_room->light > 0 )
      --ch->in_room->light;

   /*
    * Character's affect on the room
    */
   for( paf = ch->first_affect; paf; paf = paf->next )
      room_affect( ch->in_room, paf, FALSE );

   /*
    * Remove room's affects on char.
    * Do this even if the char died.  If ch is a player, then they are
    * simply awaiting resurrection at the temple, in which case we
    * still must remove these affects from them, lest they be corrupted.
    */
   for( paf = ch->in_room->first_affect; paf; paf = paf->next )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_REMOVESPELL && paf->location != APPLY_STRIPSN )
         affect_modify( ch, paf, FALSE );
   }

   for( paf = ch->in_room->first_permaffect; paf; paf = paf->next )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_REMOVESPELL && paf->location != APPLY_STRIPSN )
         affect_modify( ch, paf, FALSE );
   }

   UNLINK( ch, ch->in_room->first_person, ch->in_room->last_person, next_in_room, prev_in_room );
   ch->was_in_room = ch->in_room;
   ch->in_room = NULL;
   ch->next_in_room = NULL;
   ch->prev_in_room = NULL;

   if( !IS_NPC( ch ) && get_timer( ch, TIMER_SHOVEDRAG ) > 0 )
      remove_timer( ch, TIMER_SHOVEDRAG );

   return;
}

/*
 * Move a char into a room.
 */
void char_to_room( CHAR_DATA * ch, ROOM_INDEX_DATA * pRoomIndex )
{
   OBJ_DATA *obj;
   AFFECT_DATA *paf;

   if( !ch )
   {
      bug( "%s: NULL ch!", __FUNCTION__ );
      return;
   }

   if( !pRoomIndex || !get_room_index( pRoomIndex->vnum ) )
   {
      bug( "%s: %s -> NULL room!  Putting char in limbo (%d)", __FUNCTION__, ch->name, ROOM_VNUM_LIMBO );
      /*
       * This used to just return, but there was a problem with crashing
       * and I saw no reason not to just put the char in limbo.  -Narn
       */
      pRoomIndex = get_room_index( ROOM_VNUM_LIMBO );
   }

   ch->in_room = pRoomIndex;
   if( ch->home_vnum < 1 )
      ch->home_vnum = ch->in_room->vnum;
   LINK( ch, pRoomIndex->first_person, pRoomIndex->last_person, next_in_room, prev_in_room );

   if( !IS_NPC( ch ) )
      if( ++pRoomIndex->area->nplayer > pRoomIndex->area->max_players )
         pRoomIndex->area->max_players = pRoomIndex->area->nplayer;

   if( ( obj = get_eq_char( ch, WEAR_LIGHT ) ) != NULL && obj->item_type == ITEM_LIGHT && obj->value[2] != 0 )
      ++pRoomIndex->light;

   /*
    * Add the room's affects to the char.
    * Even if the char died, we must do this, because the char
    * is removed from the room on death, which causes the room
    * affects to be removed from the char, and we must balance
    * that out.
    */
   for( paf = pRoomIndex->first_affect; paf; paf = paf->next )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_REMOVESPELL && paf->location != APPLY_STRIPSN )
         affect_modify( ch, paf, TRUE );
   }

   for( paf = pRoomIndex->first_permaffect; paf; paf = paf->next )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_REMOVESPELL && paf->location != APPLY_STRIPSN )
         affect_modify( ch, paf, TRUE );
   }

   /*
    * Character's effect on the room
    */
   for( paf = ch->first_affect; paf; paf = paf->next )
      room_affect( pRoomIndex, paf, TRUE );


   if( !IS_NPC( ch ) && xIS_SET( pRoomIndex->room_flags, ROOM_SAFE ) && get_timer( ch, TIMER_SHOVEDRAG ) <= 0 )
      add_timer( ch, TIMER_SHOVEDRAG, 10, NULL, 0 );
                                                 /*-30 Seconds-*/

   /*
    * Delayed Teleport rooms             -Thoric
    * Should be the last thing checked in this function
    */
   if( xIS_SET( pRoomIndex->room_flags, ROOM_TELEPORT ) && pRoomIndex->tele_delay > 0 )
   {
      TELEPORT_DATA *tele;

      for( tele = first_teleport; tele; tele = tele->next )
         if( tele->room == pRoomIndex )
            return;

      CREATE( tele, TELEPORT_DATA, 1 );
      LINK( tele, first_teleport, last_teleport, next, prev );
      tele->room = pRoomIndex;
      tele->timer = pRoomIndex->tele_delay;
   }
   if( !ch->was_in_room )
      ch->was_in_room = ch->in_room;
   return;
}

void free_teleports( void )
{
   TELEPORT_DATA *tele, *tele_next;

   for( tele = first_teleport; tele; tele = tele_next )
   {
      tele_next = tele->next;

      UNLINK( tele, first_teleport, last_teleport, next, prev );
      DISPOSE( tele );
   }
}

/*
 * Give an obj to a char.
 */
OBJ_DATA *obj_to_char( OBJ_DATA * obj, CHAR_DATA * ch )
{
   OBJ_DATA *otmp;
   OBJ_DATA *oret = obj;
   bool skipgroup, grouped;
   int oweight = get_obj_weight( obj );
   int onum = get_obj_number( obj );
   int wear_loc = obj->wear_loc;
   EXT_BV extra_flags = obj->extra_flags;

   skipgroup = FALSE;
   grouped = FALSE;

   if( IS_OBJ_STAT( obj, ITEM_PROTOTYPE ) )
   {
      if( !IS_IMMORTAL( ch ) && ( !IS_NPC( ch ) || !xIS_SET( ch->act, ACT_PROTOTYPE ) ) )
         return obj_to_room( obj, ch->in_room );
   }

   if( loading_char == ch )
   {
      int x, y;
      for( x = 0; x < MAX_WEAR; x++ )
      {
         for( y = 0; y < MAX_LAYERS; y++ )
         {
            if( IS_NPC( ch ) )
            {
               if( mob_save_equipment[x][y] == obj )
               {
                  skipgroup = TRUE;
                  break;
               }
            }
            else
            {
               if( save_equipment[x][y] == obj )
               {
                  skipgroup = TRUE;
                  break;
               }
            }
         }
      }
   }

   if( IS_NPC( ch ) && ch->pIndexData->pShop )
      skipgroup = TRUE;

   if( !skipgroup )
      for( otmp = ch->first_carrying; otmp; otmp = otmp->next_content )
         if( ( oret = group_object( otmp, obj ) ) == otmp )
         {
            grouped = TRUE;
            break;
         }
   if( !grouped )
   {
      if( !IS_NPC( ch ) || !ch->pIndexData->pShop )
      {
         LINK( obj, ch->first_carrying, ch->last_carrying, next_content, prev_content );
         obj->carried_by = ch;
         obj->in_room = NULL;
         obj->in_obj = NULL;
      }
      else
      {
         /*
          * If ch is a shopkeeper, add the obj using an insert sort 
          */
         for( otmp = ch->first_carrying; otmp; otmp = otmp->next_content )
         {
            if( obj->level > otmp->level )
            {
               INSERT( obj, otmp, ch->first_carrying, next_content, prev_content );
               break;
            }
            else if( obj->level == otmp->level && strcmp( obj->short_descr, otmp->short_descr ) < 0 )
            {
               INSERT( obj, otmp, ch->first_carrying, next_content, prev_content );
               break;
            }
         }

         if( !otmp )
         {
            LINK( obj, ch->first_carrying, ch->last_carrying, next_content, prev_content );
         }

         obj->carried_by = ch;
         obj->in_room = NULL;
         obj->in_obj = NULL;
      }
   }
   if( wear_loc == WEAR_NONE )
   {
      ch->carry_number += onum;
      ch->carry_weight += oweight;
   }
   else if( !xIS_SET( extra_flags, ITEM_MAGIC ) )
      ch->carry_weight += oweight;
   return ( oret ? oret : obj );
}

/*
 * Take an obj from its character.
 */
void obj_from_char( OBJ_DATA * obj )
{
   CHAR_DATA *ch;

   if( ( ch = obj->carried_by ) == NULL )
   {
      bug( "%s: null ch.", __FUNCTION__ );
      return;
   }

   if( obj->wear_loc != WEAR_NONE )
      unequip_char( ch, obj );

   /*
    * obj may drop during unequip... 
    */
   if( !obj->carried_by )
      return;

   UNLINK( obj, ch->first_carrying, ch->last_carrying, next_content, prev_content );

   if( IS_OBJ_STAT( obj, ITEM_COVERING ) && obj->first_content )
      empty_obj( obj, NULL, NULL );

   obj->in_room = NULL;
   obj->carried_by = NULL;
   ch->carry_number -= get_obj_number( obj );
   ch->carry_weight -= get_obj_weight( obj );
   return;
}

/*
 * Find the ac value of an obj, including position effect.
 */
int apply_ac( OBJ_DATA * obj, int iWear )
{
   if( obj->item_type != ITEM_ARMOR )
      return 0;

   switch ( iWear )
   {
      case WEAR_BODY:
         return 3 * obj->value[0];
      case WEAR_HEAD:
         return 2 * obj->value[0];
      case WEAR_LEGS:
         return 2 * obj->value[0];
      case WEAR_FEET:
         return obj->value[0];
      case WEAR_HANDS:
         return obj->value[0];
      case WEAR_ARMS:
         return obj->value[0];
      case WEAR_SHIELD:
         return obj->value[0];
      case WEAR_FINGER_L:
         return obj->value[0];
      case WEAR_FINGER_R:
         return obj->value[0];
      case WEAR_NECK_1:
         return obj->value[0];
      case WEAR_NECK_2:
         return obj->value[0];
      case WEAR_ABOUT:
         return 2 * obj->value[0];
      case WEAR_WAIST:
         return obj->value[0];
      case WEAR_WRIST_L:
         return obj->value[0];
      case WEAR_WRIST_R:
         return obj->value[0];
      case WEAR_HOLD:
         return obj->value[0];
      case WEAR_EYES:
         return obj->value[0];
      case WEAR_FACE:
         return obj->value[0];
      case WEAR_BACK:
         return obj->value[0];
      case WEAR_ANKLE_L:
         return obj->value[0];
      case WEAR_ANKLE_R:
         return obj->value[0];
   }

   return 0;
}



/*
 * Find a piece of eq on a character.
 * Will pick the top layer if clothing is layered.		-Thoric
 */
OBJ_DATA *get_eq_char( CHAR_DATA * ch, int iWear )
{
   OBJ_DATA *obj, *maxobj = NULL;

   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->wear_loc == iWear )
      {
         if( !obj->pIndexData->layers )
            return obj;
         else if( !maxobj || obj->pIndexData->layers > maxobj->pIndexData->layers )
            maxobj = obj;
      }

   return maxobj;
}



/*
 * Equip a char with an obj.
 */
void equip_char( CHAR_DATA * ch, OBJ_DATA * obj, int iWear )
{
   AFFECT_DATA *paf;
   OBJ_DATA *otmp;

   if( obj->carried_by != ch )
   {
      bug( "equip_char: obj not being carried by ch!" );
      return;
   }

   if( ( otmp = get_eq_char( ch, iWear ) ) != NULL && ( !otmp->pIndexData->layers || !obj->pIndexData->layers ) )
   {
      bug( "Equip_char: already equipped (%d).", iWear );
      return;
   }

   separate_obj( obj ); /* just in case */
   if( ( IS_OBJ_STAT( obj, ITEM_ANTI_EVIL ) && IS_EVIL( ch ) )
       || ( IS_OBJ_STAT( obj, ITEM_ANTI_GOOD ) && IS_GOOD( ch ) )
       || ( IS_OBJ_STAT( obj, ITEM_ANTI_NEUTRAL ) && IS_NEUTRAL( ch ) ) )
   {
      /*
       * Thanks to Morgenes for the bug fix here!
       */
      if( loading_char != ch )
      {
         act( AT_MAGIC, "You are zapped by $p and drop it.", ch, obj, NULL, TO_CHAR );
         act( AT_MAGIC, "$n is zapped by $p and drops it.", ch, obj, NULL, TO_ROOM );
      }
      if( obj->carried_by )
         obj_from_char( obj );
      obj_to_room( obj, ch->in_room );
      oprog_zap_trigger( ch, obj );
      if( IS_SET( sysdata.save_flags, SV_ZAPDROP ) && !char_died( ch ) )
         save_char_obj( ch );
      return;
   }

   ch->armor -= apply_ac( obj, iWear );
   obj->wear_loc = iWear;

   ch->carry_number -= get_obj_number( obj );
   if( IS_OBJ_STAT( obj, ITEM_MAGIC ) )
      ch->carry_weight -= get_obj_weight( obj );

   for( paf = obj->pIndexData->first_affect; paf; paf = paf->next )
      affect_modify( ch, paf, TRUE );
   for( paf = obj->first_affect; paf; paf = paf->next )
      affect_modify( ch, paf, TRUE );

   if( obj->item_type == ITEM_LIGHT && obj->value[2] != 0 && ch->in_room )
      ++ch->in_room->light;

   return;
}



/*
 * Unequip a char with an obj.
 */
void unequip_char( CHAR_DATA * ch, OBJ_DATA * obj )
{
   AFFECT_DATA *paf;

   if( obj->wear_loc == WEAR_NONE )
   {
      bug( "%s", "Unequip_char: already unequipped." );
      return;
   }

   ch->carry_number += get_obj_number( obj );
   if( IS_OBJ_STAT( obj, ITEM_MAGIC ) )
      ch->carry_weight += get_obj_weight( obj );

   ch->armor += apply_ac( obj, obj->wear_loc );
   obj->wear_loc = -1;

   for( paf = obj->pIndexData->first_affect; paf; paf = paf->next )
      affect_modify( ch, paf, FALSE );
   if( obj->carried_by )
      for( paf = obj->first_affect; paf; paf = paf->next )
         affect_modify( ch, paf, FALSE );

   update_aris( ch );

   if( !obj->carried_by )
      return;

   if( obj->item_type == ITEM_LIGHT && obj->value[2] != 0 && ch->in_room && ch->in_room->light > 0 )
      --ch->in_room->light;

   return;
}

/*
 * Move an obj out of a room.
 */
void write_corpses args( ( CHAR_DATA * ch, const char *name, OBJ_DATA * objrem ) );

int falling;

void obj_from_room( OBJ_DATA * obj )
{
   ROOM_INDEX_DATA *in_room;
   AFFECT_DATA *paf;

   if( ( in_room = obj->in_room ) == NULL )
   {
      bug( "%s", "obj_from_room: NULL." );
      return;
   }

   for( paf = obj->first_affect; paf; paf = paf->next )
      room_affect( in_room, paf, FALSE );

   for( paf = obj->pIndexData->first_affect; paf; paf = paf->next )
      room_affect( in_room, paf, FALSE );

   UNLINK( obj, in_room->first_content, in_room->last_content, next_content, prev_content );

   /*
    * uncover contents 
    */
   if( IS_OBJ_STAT( obj, ITEM_COVERING ) && obj->first_content )
      empty_obj( obj, NULL, obj->in_room );

   if( obj->item_type == ITEM_FIRE )
      obj->in_room->light -= obj->count;

   obj->carried_by = NULL;
   obj->in_obj = NULL;
   obj->in_room = NULL;
   if( obj->pIndexData->vnum == OBJ_VNUM_CORPSE_PC && falling < 1 )
      write_corpses( NULL, obj->short_descr + 14, obj );
   return;
}

/*
 * Move an obj into a room.
 */
OBJ_DATA *obj_to_room( OBJ_DATA * obj, ROOM_INDEX_DATA * pRoomIndex )
{
   OBJ_DATA *otmp, *oret;
   short count = obj->count;
   short item_type = obj->item_type;
   AFFECT_DATA *paf;

   for( paf = obj->first_affect; paf; paf = paf->next )
      room_affect( pRoomIndex, paf, TRUE );

   for( paf = obj->pIndexData->first_affect; paf; paf = paf->next )
      room_affect( pRoomIndex, paf, TRUE );

   for( otmp = pRoomIndex->first_content; otmp; otmp = otmp->next_content )
      if( ( oret = group_object( otmp, obj ) ) == otmp )
      {
         if( item_type == ITEM_FIRE )
            pRoomIndex->light += count;
         return oret;
      }

   LINK( obj, pRoomIndex->first_content, pRoomIndex->last_content, next_content, prev_content );
   obj->in_room = pRoomIndex;
   obj->carried_by = NULL;
   obj->in_obj = NULL;
   obj->room_vnum = pRoomIndex->vnum;  /* hotboot tracker */
   if( item_type == ITEM_FIRE )
      pRoomIndex->light += count;
   falling++;
   obj_fall( obj, FALSE );
   falling--;
   if( obj->pIndexData->vnum == OBJ_VNUM_CORPSE_PC && falling < 1 )
      write_corpses( NULL, obj->short_descr + 14, NULL );
   return obj;
}

/*
 * Who's carrying an item -- recursive for nested objects	-Thoric
 */
CHAR_DATA *carried_by( OBJ_DATA * obj )
{
   if( obj->in_obj )
      return carried_by( obj->in_obj );

   return obj->carried_by;
}

/*
 * Move an object into an object.
 */
OBJ_DATA *obj_to_obj( OBJ_DATA * obj, OBJ_DATA * obj_to )
{
   OBJ_DATA *otmp, *oret;
   CHAR_DATA *who;

   if( obj == obj_to )
   {
      bug( "Obj_to_obj: trying to put object inside itself: vnum %d", obj->pIndexData->vnum );
      return obj;
   }

   if( !in_magic_container( obj_to ) && ( who = carried_by( obj_to ) ) != NULL )
      who->carry_weight += get_obj_weight( obj );

   for( otmp = obj_to->first_content; otmp; otmp = otmp->next_content )
      if( ( oret = group_object( otmp, obj ) ) == otmp )
         return oret;

   LINK( obj, obj_to->first_content, obj_to->last_content, next_content, prev_content );

   obj->in_obj = obj_to;
   obj->in_room = NULL;
   obj->carried_by = NULL;

   return obj;
}


/*
 * Move an object out of an object.
 */
void obj_from_obj( OBJ_DATA * obj )
{
   OBJ_DATA *obj_from;
   bool magic;

   if( ( obj_from = obj->in_obj ) == NULL )
   {
      bug( "%s", "Obj_from_obj: null obj_from." );
      return;
   }

   magic = in_magic_container( obj_from );

   UNLINK( obj, obj_from->first_content, obj_from->last_content, next_content, prev_content );

   /*
    * uncover contents 
    */
   if( IS_OBJ_STAT( obj, ITEM_COVERING ) && obj->first_content )
      empty_obj( obj, obj->in_obj, NULL );

   obj->in_obj = NULL;
   obj->in_room = NULL;
   obj->carried_by = NULL;

   if( !magic )
      for( ; obj_from; obj_from = obj_from->in_obj )
         if( obj_from->carried_by )
            obj_from->carried_by->carry_weight -= get_obj_weight( obj );

   return;
}

/*
 * Extract an obj from the world.
 */
void extract_obj( OBJ_DATA * obj )
{
   OBJ_DATA *obj_content;
   REL_DATA *RQueue, *rq_next;

   if( obj_extracted( obj ) )
   {
      bug( "%s: obj %d already extracted!", __FUNCTION__, obj->pIndexData->vnum );
      return;
   }

   if( obj->item_type == ITEM_PORTAL )
      remove_portal( obj );

   if( auction->item && auction->item == obj )
   {
      char buf[MAX_STRING_LENGTH];

      snprintf( buf, MAX_STRING_LENGTH, "Sale of %s has been stopped by a system action.", auction->item->short_descr );
      talk_auction( buf );

      auction->item = NULL;
      if( auction->buyer != NULL && auction->buyer != auction->seller ) /* return money to the buyer */
      {
         auction->buyer->gold += auction->bet;
         send_to_char( "Your money has been returned.\r\n", auction->buyer );
      }
   }

   if( obj->carried_by )
      obj_from_char( obj );
   else if( obj->in_room )
      obj_from_room( obj );
   else if( obj->in_obj )
      obj_from_obj( obj );

   while( ( obj_content = obj->last_content ) != NULL )
      extract_obj( obj_content );

   /*
    * remove affects 
    */
   {
      AFFECT_DATA *paf;
      AFFECT_DATA *paf_next;

      for( paf = obj->first_affect; paf; paf = paf_next )
      {
         paf_next = paf->next;
         DISPOSE( paf );
      }
      obj->first_affect = obj->last_affect = NULL;
   }

   /*
    * remove extra descriptions 
    */
   {
      EXTRA_DESCR_DATA *ed;
      EXTRA_DESCR_DATA *ed_next;

      for( ed = obj->first_extradesc; ed; ed = ed_next )
      {
         ed_next = ed->next;
         STRFREE( ed->description );
         STRFREE( ed->keyword );
         DISPOSE( ed );
      }
      obj->first_extradesc = obj->last_extradesc = NULL;
   }

   trworld_obj_check( obj );

   for( RQueue = first_relation; RQueue; RQueue = rq_next )
   {
      rq_next = RQueue->next;
      if( RQueue->Type == relOSET_ON )
      {
         if( obj == RQueue->Subject )
            ( ( CHAR_DATA * ) RQueue->Actor )->dest_buf = NULL;
         else
            continue;
         UNLINK( RQueue, first_relation, last_relation, next, prev );
         DISPOSE( RQueue );
      }
   }

   UNLINK( obj, first_object, last_object, next, prev );

   /*
    * shove onto extraction queue 
    */
   queue_extracted_obj( obj );

   obj->pIndexData->count -= obj->count;
   numobjsloaded -= obj->count;
   --physicalobjects;
   if( obj->serial == cur_obj )
   {
      cur_obj_extracted = TRUE;
      if( global_objcode == rNONE )
         global_objcode = rOBJ_EXTRACTED;
   }
   return;
}

/*
 * Extract a char from the world.
 */
void extract_char( CHAR_DATA * ch, bool fPull )
{
   CHAR_DATA *wch, *next_wch;
   GTHREAT_DATA *gthreat, *gthreat_next;
   OBJ_DATA *obj;
   char buf[MAX_STRING_LENGTH];
   ROOM_INDEX_DATA *location;
   REL_DATA *RQueue, *rq_next;

   if( !ch )
   {
      bug( "%s: NULL ch.", __FUNCTION__ );
      return;
   }

   if( !ch->in_room )
   {
      bug( "%s: %s in NULL room.", __FUNCTION__, ch->name ? ch->name : "???" );
      return;
   }

   if( ch == supermob )
   {
      bug( "%s: ch == supermob!", __FUNCTION__ );
      return;
   }

   if( char_died( ch ) )
   {
      bug( "%s: %s already died!", __FUNCTION__, ch->name );
      return;
   }

   if( ch == cur_char )
      cur_char_died = TRUE;

   /*
    * shove onto extraction queue 
    */
   queue_extracted_char( ch, fPull );

   for( RQueue = first_relation; RQueue; RQueue = rq_next )
   {
      rq_next = RQueue->next;
      if( fPull && RQueue->Type == relMSET_ON )
      {
         if( ch == RQueue->Subject )
            ( ( CHAR_DATA * ) RQueue->Actor )->dest_buf = NULL;
         else if( ch != RQueue->Actor )
            continue;
         UNLINK( RQueue, first_relation, last_relation, next, prev );
         DISPOSE( RQueue );
      }
   }

   trworld_char_check( ch );

   if( fPull )
      die_follower( ch );

   stop_fighting( ch, TRUE );

   if( ch->target )
      clear_target( ch, NORMAL_TARGET );
   if( ch->charge_target )
      clear_target( ch, CHARGE_TARGET );

   if( ch->first_targetedby )
      for( wch = ch->first_targetedby; wch; wch = next_wch )
      {
         next_wch = wch->next_person_targetting_your_target;
         clear_target( wch, NORMAL_TARGET );
      }
   if( ch->first_charge_targetedby )
      for( wch = ch->first_charge_targetedby; wch; wch = next_wch )
      {
         next_wch = wch->next_person_charge_targetting_your_target;
         interrupt( wch );
         clear_target( wch, CHARGE_TARGET );
      }

   free_threat( ch );

   for( gthreat = first_gthreat; gthreat; gthreat = gthreat_next )
   {
      gthreat_next = gthreat->next;
      if( gthreat->threat_attacker == ch )
         free_threat( gthreat->threat_owner, gthreat->threat );
   }

   if( ch->mount )
   {
      update_room_reset( ch, TRUE );
      xREMOVE_BIT( ch->mount->act, ACT_MOUNTED );
      ch->mount = NULL;
      ch->position = POS_STANDING;
   }

   /*
    * check if this NPC was a mount or a pet
    */
   if( IS_NPC( ch ) )
   {
      update_room_reset( ch, TRUE );
      xREMOVE_BIT( ch->act, ACT_MOUNTED );
      for( wch = first_char; wch; wch = wch->next )
      {
         if( wch->mount == ch )
         {
            wch->mount = NULL;
            wch->position = POS_STANDING;
            if( wch->in_room == ch->in_room )
            {
               act( AT_SOCIAL, "Your faithful mount, $N collapses beneath you...", wch, NULL, ch, TO_CHAR );
               act( AT_SOCIAL, "Sadly you dismount $M for the last time.", wch, NULL, ch, TO_CHAR );
               act( AT_PLAIN, "$n sadly dismounts $N for the last time.", wch, NULL, ch, TO_ROOM );
            }
         }
         if( wch->pcdata && wch->pcdata->pet == ch )
         {
            wch->pcdata->pet = NULL;
            if( wch->in_room == ch->in_room )
               act( AT_SOCIAL, "You mourn for the loss of $N.", wch, NULL, ch, TO_CHAR );
         }
      }
   }

   while( ( obj = ch->last_carrying ) != NULL )
      extract_obj( obj );

   char_from_room( ch );

   if( !fPull )
   {
      location = NULL;

      if( !IS_NPC( ch ) && ch->pcdata->clan )
         location = get_room_index( ch->pcdata->clan->recall );

      if( !location )
         location = get_room_index( ROOM_VNUM_ALTAR );

      if( !location )
         location = get_room_index( 1 );

      char_to_room( ch, location );
      /*
       * Make things a little fancier           -Thoric
       */
      if( ( wch = get_char_room( ch, "healer" ) ) != NULL )
      {
         act( AT_MAGIC, "$n mutters a few incantations, waves $s hands and points $s finger.", wch, NULL, NULL, TO_ROOM );
         act( AT_MAGIC, "$n appears from some strange swirling mists!", ch, NULL, NULL, TO_ROOM );
         snprintf( buf, MAX_STRING_LENGTH, "Welcome back to the land of the living, %s", capitalize( ch->name ) );
         do_say( wch, buf );
      }
      else
         act( AT_MAGIC, "$n appears from some strange swirling mists!", ch, NULL, NULL, TO_ROOM );
      ch->position = POS_RESTING;
      return;
   }

   if( IS_NPC( ch ) )
   {
      --ch->pIndexData->count;
      --nummobsloaded;
   }

   if( ch->desc && ch->desc->original )
      do_return( ch, "" );

   if( ch->switched && ch->switched->desc )
      do_return( ch->switched, "" );

   for( wch = first_char; wch; wch = wch->next )
   {
      if( wch->reply == ch )
         wch->reply = NULL;
      if( wch->retell == ch )
         wch->retell = NULL;
   }

   UNLINK( ch, first_char, last_char, next, prev );

   if( ch->desc )
   {
      if( ch->desc->character != ch )
         bug( "%s: char's descriptor points to another char", __FUNCTION__ );
      else
      {
         ch->desc->character = NULL;
         close_socket( ch->desc, FALSE );
         ch->desc = NULL;
      }
   }
   return;
}

/*
 * Find a char in the room.
 */
CHAR_DATA *get_char_room( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *rch;
   int number, count, vnum;

   number = number_argument( argument, arg );
   if( !str_cmp( arg, "self" ) )
      return ch;

   if( get_trust( ch ) >= LEVEL_SAVIOR && is_number( arg ) )
      vnum = atoi( arg );
   else
      vnum = -1;

   count = 0;

   for( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
      if( can_see( ch, rch ) && ( nifty_is_name( arg, rch->name ) || ( IS_NPC( rch ) && vnum == rch->pIndexData->vnum ) ) )
      {
         if( number == 0 && !IS_NPC( rch ) )
            return rch;
         else if( ++count == number )
            return rch;
      }

   if( vnum != -1 )
      return NULL;

   /*
    * If we didn't find an exact match, run through the list of characters
    * again looking for prefix matching, ie gu == guard.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
   {
      if( !can_see( ch, rch ) || !nifty_is_name_prefix( arg, rch->name ) )
         continue;
      if( number == 0 && !IS_NPC( rch ) )
         return rch;
      else if( ++count == number )
         return rch;
   }

   return NULL;
}




/*
 * Find a char in the world.
 */
CHAR_DATA *get_char_world( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *wch;
   int number, count, vnum;

   number = number_argument( argument, arg );
   count = 0;
   if( !str_cmp( arg, "self" ) )
      return ch;

   /*
    * Allow reference by vnum for saints+         -Thoric
    */
   if( get_trust( ch ) >= LEVEL_SAVIOR && is_number( arg ) )
      vnum = atoi( arg );
   else
      vnum = -1;

   /*
    * check the room for an exact match 
    */
   for( wch = ch->in_room->first_person; wch; wch = wch->next_in_room )
      if( can_see( ch, wch ) && ( nifty_is_name( arg, wch->name ) || ( IS_NPC( wch ) && vnum == wch->pIndexData->vnum ) ) )
      {
         if( number == 0 && !IS_NPC( wch ) )
            return wch;
         else if( ++count == number )
            return wch;
      }

   count = 0;



   /*
    * check the world for an exact match 
    */
   for( wch = first_char; wch; wch = wch->next )
      if( can_see( ch, wch ) && ( nifty_is_name( arg, wch->name ) || ( IS_NPC( wch ) && vnum == wch->pIndexData->vnum ) ) )
      {
         if( number == 0 && !IS_NPC( wch ) )
            return wch;
         else if( ++count == number )
            return wch;
      }

   /*
    * bail out if looking for a vnum match 
    */
   if( vnum != -1 )
      return NULL;

   /*
    * If we didn't find an exact match, check the room for
    * for a prefix match, ie gu == guard.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( wch = ch->in_room->first_person; wch; wch = wch->next_in_room )
   {
      if( !can_see( ch, wch ) || !nifty_is_name_prefix( arg, wch->name ) )
         continue;
      if( number == 0 && !IS_NPC( wch ) )
         return wch;
      else if( ++count == number )
         return wch;
   }

   /*
    * If we didn't find a prefix match in the room, run through the full list
    * of characters looking for prefix matching, ie gu == guard.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( wch = first_char; wch; wch = wch->next )
   {
      if( !can_see( ch, wch ) || !nifty_is_name_prefix( arg, wch->name ) )
         continue;
      if( number == 0 && !IS_NPC( wch ) )
         return wch;
      else if( ++count == number )
         return wch;
   }

   return NULL;
}

/*
 * Find an obj in a list.
 */
OBJ_DATA *get_obj_list( CHAR_DATA * ch, char *argument, OBJ_DATA * list )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   int number;
   int count;

   number = number_argument( argument, arg );
   count = 0;
   for( obj = list; obj; obj = obj->next_content )
      if( can_see_obj( ch, obj ) && nifty_is_name( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   /*
    * If we didn't find an exact match, run through the list of objects
    * again looking for prefix matching, ie swo == sword.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( obj = list; obj; obj = obj->next_content )
      if( can_see_obj( ch, obj ) && nifty_is_name_prefix( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   return NULL;
}

/*
 * Find an obj in a list...going the other way			-Thoric
 */
OBJ_DATA *get_obj_list_rev( CHAR_DATA * ch, const char *argument, OBJ_DATA * list )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   int number;
   int count;

   number = number_argument( argument, arg );
   count = 0;
   for( obj = list; obj; obj = obj->prev_content )
      if( can_see_obj( ch, obj ) && nifty_is_name( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   /*
    * If we didn't find an exact match, run through the list of objects
    * again looking for prefix matching, ie swo == sword.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( obj = list; obj; obj = obj->prev_content )
      if( can_see_obj( ch, obj ) && nifty_is_name_prefix( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   return NULL;
}

/*
 * Find an obj in player's inventory or wearing via a vnum -Shaddai
 */

OBJ_DATA *get_obj_vnum( CHAR_DATA * ch, int vnum )
{
   OBJ_DATA *obj;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
      if( can_see_obj( ch, obj ) && obj->pIndexData->vnum == vnum )
         return obj;
   return NULL;
}


/*
 * Find an obj in player's inventory.
 */
OBJ_DATA *get_obj_carry( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   int number, count, vnum;

   number = number_argument( argument, arg );
   if( get_trust( ch ) >= LEVEL_SAVIOR && is_number( arg ) )
      vnum = atoi( arg );
   else
      vnum = -1;

   count = 0;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
      if( obj->wear_loc == WEAR_NONE
          && can_see_obj( ch, obj ) && ( nifty_is_name( arg, obj->name ) || obj->pIndexData->vnum == vnum ) )
         if( ( count += obj->count ) >= number )
            return obj;

   if( vnum != -1 )
      return NULL;

   /*
    * If we didn't find an exact match, run through the list of objects
    * again looking for prefix matching, ie swo == sword.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
      if( obj->wear_loc == WEAR_NONE && can_see_obj( ch, obj ) && nifty_is_name_prefix( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   return NULL;
}



/*
 * Find an obj in player's equipment.
 */
OBJ_DATA *get_obj_wear( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   int number, count, vnum;

   number = number_argument( argument, arg );

   if( get_trust( ch ) >= LEVEL_SAVIOR && is_number( arg ) )
      vnum = atoi( arg );
   else
      vnum = -1;

   count = 0;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
      if( obj->wear_loc != WEAR_NONE
          && can_see_obj( ch, obj ) && ( nifty_is_name( arg, obj->name ) || obj->pIndexData->vnum == vnum ) )
         if( ++count == number )
            return obj;

   if( vnum != -1 )
      return NULL;

   /*
    * If we didn't find an exact match, run through the list of objects
    * again looking for prefix matching, ie swo == sword.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
      if( obj->wear_loc != WEAR_NONE && can_see_obj( ch, obj ) && nifty_is_name_prefix( arg, obj->name ) )
         if( ++count == number )
            return obj;

   return NULL;
}



/*
 * Find an obj in the room or in inventory.
 */
OBJ_DATA *get_obj_here( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *obj;

   obj = get_obj_list_rev( ch, argument, ch->in_room->last_content );
   if( obj )
      return obj;

   if( ( obj = get_obj_carry( ch, argument ) ) != NULL )
      return obj;

   if( ( obj = get_obj_wear( ch, argument ) ) != NULL )
      return obj;

   return NULL;
}



/*
 * Find an obj in the world.
 */
OBJ_DATA *get_obj_world( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   int number, count, vnum;

   if( ( obj = get_obj_here( ch, argument ) ) != NULL )
      return obj;

   number = number_argument( argument, arg );

   /*
    * Allow reference by vnum for saints+         -Thoric
    */
   if( get_trust( ch ) >= LEVEL_SAVIOR && is_number( arg ) )
      vnum = atoi( arg );
   else
      vnum = -1;

   count = 0;
   for( obj = first_object; obj; obj = obj->next )
      if( can_see_obj( ch, obj ) && ( nifty_is_name( arg, obj->name ) || vnum == obj->pIndexData->vnum ) )
         if( ( count += obj->count ) >= number )
            return obj;

   /*
    * bail out if looking for a vnum 
    */
   if( vnum != -1 )
      return NULL;

   /*
    * If we didn't find an exact match, run through the list of objects
    * again looking for prefix matching, ie swo == sword.
    * Added by Narn, Sept/96
    */
   count = 0;
   for( obj = first_object; obj; obj = obj->next )
      if( can_see_obj( ch, obj ) && nifty_is_name_prefix( arg, obj->name ) )
         if( ( count += obj->count ) >= number )
            return obj;

   return NULL;
}


/*
 * How mental state could affect finding an object		-Thoric
 * Used by get/drop/put/quaff/recite/etc
 * Increasingly freaky based on mental state and drunkeness
 */
bool ms_find_obj( CHAR_DATA * ch )
{
   int ms = ch->mental_state;
   int drunk = IS_NPC( ch ) ? 0 : ch->pcdata->condition[COND_DRUNK];
   const char *t;

   /*
    * we're going to be nice and let nothing weird happen unless
    * you're a tad messed up
    */
   drunk = UMAX( 1, drunk );
   if( abs( ms ) + ( drunk / 3 ) < 30 )
      return FALSE;
   if( ( number_percent(  ) + ( ms < 0 ? 15 : 5 ) ) > abs( ms ) / 2 + drunk / 4 )
      return FALSE;
   if( ms > 15 )  /* range 1 to 20 -- feel free to add more */
      switch ( number_range( UMAX( 1, ( ms / 5 - 15 ) ), ( ms + 4 ) / 5 ) )
      {
         default:
         case 1:
            t = "As you reach for it, you forgot what it was...\r\n";
            break;
         case 2:
            t = "As you reach for it, something inside stops you...\r\n";
            break;
         case 3:
            t = "As you reach for it, it seems to move out of the way...\r\n";
            break;
         case 4:
            t = "You grab frantically for it, but can't seem to get a hold of it...\r\n";
            break;
         case 5:
            t = "It disappears as soon as you touch it!\r\n";
            break;
         case 6:
            t = "You would if it would stay still!\r\n";
            break;
         case 7:
            t = "Whoa!  It's covered in blood!  Ack!  Ick!\r\n";
            break;
         case 8:
            t = "Wow... trails!\r\n";
            break;
         case 9:
            t = "You reach for it, then notice the back of your hand is growing something!\r\n";
            break;
         case 10:
            t = "As you grasp it, it shatters into tiny shards which bite into your flesh!\r\n";
            break;
         case 11:
            t = "What about that huge dragon flying over your head?!?!?\r\n";
            break;
         case 12:
            t = "You stratch yourself instead...\r\n";
            break;
         case 13:
            t = "You hold the universe in the palm of your hand!\r\n";
            break;
         case 14:
            t = "You're too scared.\r\n";
            break;
         case 15:
            t = "Your mother smacks your hand... 'NO!'\r\n";
            break;
         case 16:
            t = "Your hand grasps the worst pile of revoltingness that you could ever imagine!\r\n";
            break;
         case 17:
            t = "You stop reaching for it as it screams out at you in pain!\r\n";
            break;
         case 18:
            t = "What about the millions of burrow-maggots feasting on your arm?!?!\r\n";
            break;
         case 19:
            t = "That doesn't matter anymore... you've found the true answer to everything!\r\n";
            break;
         case 20:
            t = "A supreme entity has no need for that.\r\n";
            break;
      }
   else
   {
      int sub = URANGE( 1, abs( ms ) / 2 + drunk, 60 );
      switch ( number_range( 1, sub / 10 ) )
      {
         default:
         case 1:
            t = "In just a second...\r\n";
            break;
         case 2:
            t = "You can't find that...\r\n";
            break;
         case 3:
            t = "It's just beyond your grasp...\r\n";
            break;
         case 4:
            t = "...but it's under a pile of other stuff...\r\n";
            break;
         case 5:
            t = "You go to reach for it, but pick your nose instead.\r\n";
            break;
         case 6:
            t = "Which one?!?  I see two... no three...\r\n";
            break;
      }
   }
   send_to_char( t, ch );
   return TRUE;
}


/*
 * Generic get obj function that supports optional containers.	-Thoric
 * currently only used for "eat" and "quaff".
 */
OBJ_DATA *find_obj( CHAR_DATA * ch, const char *argument, bool carryonly )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   OBJ_DATA *obj = NULL;

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( !str_cmp( arg2, "from" ) && argument[0] != '\0' )
      argument = one_argument( argument, arg2 );

   if( arg2[0] == '\0' )
   {
      if( carryonly && ( obj = get_obj_carry( ch, arg1 ) ) == NULL )
      {
         send_to_char( "You do not have that item.\r\n", ch );
         return NULL;
      }
      else if( !carryonly && ( obj = get_obj_here( ch, arg1 ) ) == NULL )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, arg1, TO_CHAR );
         return NULL;
      }
      return obj;
   }
   else
   {
      OBJ_DATA *container = NULL;

      if( carryonly
          && ( container = get_obj_carry( ch, arg2 ) ) == NULL && ( container = get_obj_wear( ch, arg2 ) ) == NULL )
      {
         send_to_char( "You do not have that item.\r\n", ch );
         return NULL;
      }
      if( !carryonly && ( container = get_obj_here( ch, arg2 ) ) == NULL )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR );
         return NULL;
      }

      if( !IS_OBJ_STAT( container, ITEM_COVERING ) && IS_SET( container->value[1], CONT_CLOSED ) )
      {
         act( AT_PLAIN, "The $d is closed.", ch, NULL, container->name, TO_CHAR );
         return NULL;
      }

      obj = get_obj_list( ch, arg1, container->first_content );
      if( !obj )
         act( AT_PLAIN, IS_OBJ_STAT( container, ITEM_COVERING ) ?
              "I see nothing like that beneath $p." : "I see nothing like that in $p.", ch, container, NULL, TO_CHAR );
      return obj;
   }
}

int get_obj_number( OBJ_DATA * obj )
{
   return obj->count;
}

/*
 * Return TRUE if an object is, or nested inside a magic container
 */
bool in_magic_container( OBJ_DATA * obj )
{
   if( obj->item_type == ITEM_CONTAINER && IS_OBJ_STAT( obj, ITEM_MAGIC ) )
      return TRUE;
   if( obj->in_obj )
      return in_magic_container( obj->in_obj );
   return FALSE;
}

/*
 * Return weight of an object, including weight of contents (unless magic).
 */
int get_obj_weight( OBJ_DATA * obj )
{
   int weight;

   weight = obj->count * obj->weight;

   /*
    * magic containers 
    */
   if( obj->item_type != ITEM_CONTAINER || !IS_OBJ_STAT( obj, ITEM_MAGIC ) )
      for( obj = obj->first_content; obj; obj = obj->next_content )
         weight += get_obj_weight( obj );

   return weight;
}

/*
 * Return real weight of an object, including weight of contents.
 */
int get_real_obj_weight( OBJ_DATA * obj )
{
   int weight;

   weight = obj->count * obj->weight;

   for( obj = obj->first_content; obj; obj = obj->next_content )
      weight += get_real_obj_weight( obj );

   return weight;
}

/*
 * True if room is dark.
 */
bool room_is_dark( ROOM_INDEX_DATA * pRoomIndex )
{
   if( !pRoomIndex )
   {
      bug( "%s:: NULL pRoomIndex", __FUNCTION__ );
      return TRUE;
   }

   if( pRoomIndex->light > 0 )
      return FALSE;

   if( xIS_SET( pRoomIndex->room_flags, ROOM_DARK ) )
      return TRUE;

   if( pRoomIndex->sector_type == SECT_INSIDE || pRoomIndex->sector_type == SECT_CITY )
      return FALSE;

   if( time_info.sunlight == SUN_SET || time_info.sunlight == SUN_DARK )
      return TRUE;

   return FALSE;
}

/*
 * If room is "do not disturb" return the pointer to the imm with dnd flag
 * NULL if room is not "do not disturb".
 */
CHAR_DATA *room_is_dnd( CHAR_DATA * ch, ROOM_INDEX_DATA * pRoomIndex )
{
   CHAR_DATA *rch;

   if( !pRoomIndex )
   {
      bug( "%s", "room_is_dnd: NULL pRoomIndex" );
      return NULL;
   }

   if( !xIS_SET( pRoomIndex->room_flags, ROOM_DND ) )
      return NULL;

   for( rch = pRoomIndex->first_person; rch; rch = rch->next_in_room )
   {
      if( !IS_NPC( rch ) && rch->pcdata && IS_IMMORTAL( rch )
          && IS_SET( rch->pcdata->flags, PCFLAG_DND ) && get_trust( ch ) < get_trust( rch ) && can_see( ch, rch ) )
         return rch;
   }
   return NULL;
}


/*
 * True if room is private.
 */
bool room_is_private( ROOM_INDEX_DATA * pRoomIndex )
{
   CHAR_DATA *rch;
   int count;

   if( !pRoomIndex )
   {
      bug( "%s", "room_is_private: NULL pRoomIndex" );
      return FALSE;
   }

   count = 0;
   for( rch = pRoomIndex->first_person; rch; rch = rch->next_in_room )
      count++;

   if( xIS_SET( pRoomIndex->room_flags, ROOM_PRIVATE ) && count >= 2 )
      return TRUE;

   if( xIS_SET( pRoomIndex->room_flags, ROOM_SOLITARY ) && count >= 1 )
      return TRUE;

   return FALSE;
}

/*
 * True if char can see victim.
 */
bool can_see( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( !victim )  /* Gorog - panicked attempt to stop crashes */
      return FALSE;
   if( !ch )
   {
      if( IS_AFFECTED( victim, AFF_INVISIBLE ) || IS_AFFECTED( victim, AFF_HIDE ) || xIS_SET( victim->act, PLR_WIZINVIS ) )
         return FALSE;
      else
         return TRUE;
   }

   if( ch == victim )
      return TRUE;

   if( !IS_NPC( victim ) && xIS_SET( victim->act, PLR_WIZINVIS ) && get_trust( ch ) < victim->pcdata->wizinvis )
      return FALSE;

   /*
    * SB 
    */
   if( IS_NPC( victim ) && xIS_SET( victim->act, ACT_MOBINVIS ) && get_trust( ch ) < victim->mobinvis )
      return FALSE;

/* Deadlies link-dead over 2 ticks aren't seen by mortals -- Blodkai */
   if( !IS_IMMORTAL( ch ) && !IS_NPC( ch ) && !IS_NPC( victim ) && IS_PKILL( victim ) && victim->timer > 1 && !victim->desc )
      return FALSE;

   if( !IS_NPC( ch ) && xIS_SET( ch->act, PLR_HOLYLIGHT ) )
      return TRUE;

   /*
    * The miracle cure for blindness? -- Altrag 
    */
   if( !IS_AFFECTED( ch, AFF_TRUESIGHT ) )
   {
      if( IS_AFFECTED( ch, AFF_BLIND ) )
         return FALSE;

      if( room_is_dark( ch->in_room ) && !IS_AFFECTED( ch, AFF_INFRARED ) )
         return FALSE;

      if( IS_AFFECTED( victim, AFF_INVISIBLE ) && !IS_AFFECTED( ch, AFF_DETECT_INVIS ) )
         return FALSE;

      if( IS_AFFECTED( victim, AFF_HIDE )
          && !IS_AFFECTED( ch, AFF_DETECT_HIDDEN )
          && !victim->fighting && ( IS_NPC( ch ) ? !IS_NPC( victim ) : IS_NPC( victim ) ) )
         return FALSE;
   }

   /*
    * Redone by Narn to let newbie council members see pre-auths. 
    */
   if( NOT_AUTHED( victim ) )
   {
      if( NOT_AUTHED( ch ) || IS_IMMORTAL( ch ) || IS_NPC( ch ) )
         return TRUE;

      if( ch->pcdata->council && !str_cmp( ch->pcdata->council->name, "Newbie Council" ) )
         return TRUE;

      return FALSE;
   }

/* Commented out for who list purposes 
    if (!NOT_AUTHED(victim) && NOT_AUTHED(ch) && !IS_IMMORTAL(victim) 
    && !IS_NPC(victim))
   	return FALSE;*/
   return TRUE;
}

/*
 * True if char can see obj.
 */
bool can_see_obj( CHAR_DATA * ch, OBJ_DATA * obj )
{
   if( !IS_NPC( ch ) && xIS_SET( ch->act, PLR_HOLYLIGHT ) )
      return TRUE;

   if( IS_NPC( ch ) && ch->pIndexData->vnum == MOB_VNUM_SUPERMOB )
      return TRUE;

   if( IS_OBJ_STAT( obj, ITEM_BURIED ) )
      return FALSE;

   if( IS_OBJ_STAT( obj, ITEM_HIDDEN ) )
      return FALSE;

   if( IS_AFFECTED( ch, AFF_TRUESIGHT ) )
      return TRUE;

   if( IS_AFFECTED( ch, AFF_BLIND ) )
      return FALSE;

   /*
    * can see lights in the dark 
    */
   if( obj->item_type == ITEM_LIGHT && obj->value[2] != 0 )
      return TRUE;

   if( room_is_dark( ch->in_room ) )
   {
      /*
       * can see glowing items in the dark... invisible or not 
       */
      if( IS_OBJ_STAT( obj, ITEM_GLOW ) )
         return TRUE;
      if( !IS_AFFECTED( ch, AFF_INFRARED ) )
         return FALSE;
   }

   if( IS_OBJ_STAT( obj, ITEM_INVIS ) && !IS_AFFECTED( ch, AFF_DETECT_INVIS ) )
      return FALSE;

   return TRUE;
}

/*
 * True if char can drop obj.
 */
bool can_drop_obj( CHAR_DATA * ch, OBJ_DATA * obj )
{
   if( !IS_OBJ_STAT( obj, ITEM_NODROP ) )
      return TRUE;

   if( !IS_NPC( ch ) && ch->level >= LEVEL_IMMORTAL )
      return TRUE;

   if( IS_NPC( ch ) && ch->pIndexData->vnum == MOB_VNUM_SUPERMOB )
      return TRUE;

   return FALSE;
}

/*
 * Return ascii name of an item type.
 */
const char *item_type_name( OBJ_DATA * obj )
{
   if( obj->item_type < 1 || obj->item_type > MAX_ITEM_TYPE )
   {
      bug( "Item_type_name: unknown type %d.", obj->item_type );
      return "(unknown)";
   }

   return o_types[obj->item_type];
}

/*
 * Return ascii name of an affect location.
 */
const char *affect_loc_name( int location )
{
   switch ( location )
   {
      case APPLY_NONE:
         return "none";
      case APPLY_STR:
      case APPLY_PERMSTR:
         return "strength";
      case APPLY_DEX:
      case APPLY_PERMDEX:
         return "dexterity";
      case APPLY_INT:
      case APPLY_PERMINT:
         return "intelligence";
      case APPLY_WIS:
      case APPLY_PERMWIS:
         return "wisdom";
      case APPLY_CON:
      case APPLY_PERMCON:
         return "constitution";
      case APPLY_CHA:
         return "charisma";
      case APPLY_PAS:
      case APPLY_PERMPAS:
         return "passion";
      case APPLY_SEX:
         return "sex";
      case APPLY_CLASS:
         return "class";
      case APPLY_LEVEL:
         return "level";
      case APPLY_AGE:
         return "age";
      case APPLY_MANA:
         return "mana";
      case APPLY_HIT:
         return "hp";
      case APPLY_MOVE:
         return "moves";
      case APPLY_GOLD:
         return "gold";
      case APPLY_EXP:
         return "experience";
      case APPLY_ARMOR:
         return "armor class";
      case APPLY_ATTACK:
         return "attack";
      case APPLY_SAVING_POISON:
         return "save vs poison";
      case APPLY_SAVING_ROD:
         return "save vs rod";
      case APPLY_SAVING_PARA:
         return "save vs paralysis";
      case APPLY_SAVING_BREATH:
         return "save vs breath";
      case APPLY_SAVING_SPELL:
         return "save vs spell";
      case APPLY_HEIGHT:
         return "height";
      case APPLY_WEIGHT:
         return "weight";
      case APPLY_AFFECT:
         return "affected_by";
      case APPLY_RESISTANT:
         return "resistant";
      case APPLY_IMMUNE:
         return "immune";
      case APPLY_SUSCEPTIBLE:
         return "susceptible";
      case APPLY_BACKSTAB:
         return "backstab";
      case APPLY_PICK:
         return "pick";
      case APPLY_TRACK:
         return "track";
      case APPLY_STEAL:
         return "steal";
      case APPLY_SNEAK:
         return "sneak";
      case APPLY_HIDE:
         return "hide";
      case APPLY_PALM:
         return "palm";
      case APPLY_DETRAP:
         return "detrap";
      case APPLY_DODGE:
         return "dodge";
      case APPLY_PEEK:
         return "peek";
      case APPLY_SCAN:
         return "scan";
      case APPLY_GOUGE:
         return "gouge";
      case APPLY_SEARCH:
         return "search";
      case APPLY_MOUNT:
         return "mount";
      case APPLY_DISARM:
         return "disarm";
      case APPLY_KICK:
         return "kick";
      case APPLY_PARRY:
         return "parry";
      case APPLY_BASH:
         return "bash";
      case APPLY_STUN:
         return "stun";
      case APPLY_PUNCH:
         return "punch";
      case APPLY_CLIMB:
         return "climb";
      case APPLY_GRIP:
         return "grip";
      case APPLY_SCRIBE:
         return "scribe";
      case APPLY_BREW:
         return "brew";
      case APPLY_COOK:
         return "cook";
      case APPLY_WEAPONSPELL:
         return "weapon spell";
      case APPLY_WEARSPELL:
         return "wear spell";
      case APPLY_REMOVESPELL:
         return "remove spell";
      case APPLY_MENTALSTATE:
         return "mental state";
      case APPLY_EMOTION:
         return "emotional state";
      case APPLY_STRIPSN:
         return "dispel";
      case APPLY_REMOVE:
         return "remove";
      case APPLY_DIG:
         return "dig";
      case APPLY_FULL:
         return "hunger";
      case APPLY_THIRST:
         return "thirst";
      case APPLY_DRUNK:
         return "drunk";
      case APPLY_BLOOD:
         return "blood";
      case APPLY_RECURRINGSPELL:
         return "recurring spell";
      case APPLY_CONTAGIOUS:
         return "contagious";
      case APPLY_ODOR:
         return "odor";
      case APPLY_ROOMFLAG:
         return "roomflag";
      case APPLY_SECTORTYPE:
         return "sectortype";
      case APPLY_ROOMLIGHT:
         return "roomlight";
      case APPLY_TELEVNUM:
         return "teleport vnum";
      case APPLY_TELEDELAY:
         return "teleport delay";
      case APPLY_ALIGN:
         return "alignment";
      case APPLY_BARENUMDIE:
         return "bare hand numdie";
      case APPLY_BARESIZEDIE:
         return "bare hand sizedie";
      case APPLY_WEPNUMDIE:
         return "weapon numdie";
      case APPLY_WEPSIZEDIE:
         return "weapon sizedie";
      case APPLY_MAGICATTACK:
         return "magic attack";
      case APPLY_HASTE:
         return "haste";
      case APPLY_MAGICDEFENSE:
         return "magic defense";
      case APPLY_THREAT:
         return "threat";
      case APPLY_POTENCY:
         return "all ability potencies";
      case APPLY_COOLDOWNS:
         return "all cooldowns";
      case APPLY_RANGE:
         return "all auto-attack range";
      case APPLY_REGEN:
         return "regen";
      case APPLY_REFRESH:
         return "refresh";
      case APPLY_FEEDBACKPOTENCY:
         return "sorceror feedback potency";
      case APPLY_DURATIONS:
         return "all ability durations";
      case APPLY_DOUBLEATTACK:
         return "double attack";
      case APPLY_CRITCHANCE:
         return "critical chance(percent)";
      case APPLY_CRITDAMAGE:
         return "critical hit damage(percent)";
      case APPLY_COUNTER:
         return "counter";
      case APPLY_PHASE:
         return "phase";
      case APPLY_BLOCK:
         return "block";
      case APPLY_COMBODMG:
         return "increases combo damage";
      case APPLY_CHARMEDDMGBOOST:
         return "charmed damage";
      case APPLY_CHARMEDDEFBOOST:
         return "charmed defense";

   };

   bug( "Affect_location_name: unknown location %d.", location );
   return "(unknown)";
}



/*
 * Return ascii name of an affect bit vector.
 */
const char *affect_bit_name( EXT_BV * vector )
{
   static char buf[512];

   buf[0] = '\0';
   if( xIS_SET( *vector, AFF_BLIND ) )
      mudstrlcat( buf, " blind", 512 );
   if( xIS_SET( *vector, AFF_INVISIBLE ) )
      mudstrlcat( buf, " invisible", 512 );
   if( xIS_SET( *vector, AFF_DETECT_EVIL ) )
      mudstrlcat( buf, " detect_evil", 512 );
   if( xIS_SET( *vector, AFF_DETECT_INVIS ) )
      mudstrlcat( buf, " detect_invis", 512 );
   if( xIS_SET( *vector, AFF_DETECT_MAGIC ) )
      mudstrlcat( buf, " detect_magic", 512 );
   if( xIS_SET( *vector, AFF_DETECT_HIDDEN ) )
      mudstrlcat( buf, " detect_hidden", 512 );
   if( xIS_SET( *vector, AFF_HOLD ) )
      mudstrlcat( buf, " hold", 512 );
   if( xIS_SET( *vector, AFF_SANCTUARY ) )
      mudstrlcat( buf, " sanctuary", 512 );
   if( xIS_SET( *vector, AFF_FAERIE_FIRE ) )
      mudstrlcat( buf, " faerie_fire", 512 );
   if( xIS_SET( *vector, AFF_INFRARED ) )
      mudstrlcat( buf, " infrared", 512 );
   if( xIS_SET( *vector, AFF_CURSE ) )
      mudstrlcat( buf, " curse", 512 );
   if( xIS_SET( *vector, AFF_FLAMING ) )
      mudstrlcat( buf, " flaming", 512 );
   if( xIS_SET( *vector, AFF_POISON ) )
      mudstrlcat( buf, " poison", 512 );
   if( xIS_SET( *vector, AFF_PROTECT ) )
      mudstrlcat( buf, " protect", 512 );
   if( xIS_SET( *vector, AFF_PARALYSIS ) )
      mudstrlcat( buf, " paralyzed", 512 );
   if( xIS_SET( *vector, AFF_STUN ) )
      mudstrlcat( buf, " stunned", 512 );
   if( xIS_SET( *vector, AFF_SLEEP ) )
      mudstrlcat( buf, " sleep", 512 );
   if( xIS_SET( *vector, AFF_SNEAK ) )
      mudstrlcat( buf, " sneak", 512 );
   if( xIS_SET( *vector, AFF_HIDE ) )
      mudstrlcat( buf, " hide", 512 );
   if( xIS_SET( *vector, AFF_CHARM ) )
      mudstrlcat( buf, " charm", 512 );
   if( xIS_SET( *vector, AFF_POSSESS ) )
      mudstrlcat( buf, " possess", 512 );
   if( xIS_SET( *vector, AFF_FLYING ) )
      mudstrlcat( buf, " flying", 512 );
   if( xIS_SET( *vector, AFF_PASS_DOOR ) )
      mudstrlcat( buf, " pass_door", 512 );
   if( xIS_SET( *vector, AFF_FLOATING ) )
      mudstrlcat( buf, " floating", 512 );
   if( xIS_SET( *vector, AFF_TRUESIGHT ) )
      mudstrlcat( buf, " true_sight", 512 );
   if( xIS_SET( *vector, AFF_DETECTTRAPS ) )
      mudstrlcat( buf, " detect_traps", 512 );
   if( xIS_SET( *vector, AFF_SCRYING ) )
      mudstrlcat( buf, " scrying", 512 );
   if( xIS_SET( *vector, AFF_FIRESHIELD ) )
      mudstrlcat( buf, " fireshield", 512 );
   if( xIS_SET( *vector, AFF_ACIDMIST ) )
      mudstrlcat( buf, " acidmist", 512 );
   if( xIS_SET( *vector, AFF_VENOMSHIELD ) )
      mudstrlcat( buf, " venomshield", 512 );
   if( xIS_SET( *vector, AFF_SHOCKSHIELD ) )
      mudstrlcat( buf, " shockshield", 512 );
   if( xIS_SET( *vector, AFF_ICESHIELD ) )
      mudstrlcat( buf, " iceshield", 512 );
   if( xIS_SET( *vector, AFF_BERSERK ) )
      mudstrlcat( buf, " berserk", 512 );
   if( xIS_SET( *vector, AFF_AQUA_BREATH ) )
      mudstrlcat( buf, " aqua_breath", 512 );
   return ( buf[0] != '\0' ) ? buf + 1 : ( char * )"none";
}

/*
 * Return ascii name of extra flags vector.
 */
const char *extra_bit_name( EXT_BV * extra_flags )
{
   static char buf[512];

   buf[0] = '\0';
   if( xIS_SET( *extra_flags, ITEM_GLOW ) )
      mudstrlcat( buf, " glow", 512 );
   if( xIS_SET( *extra_flags, ITEM_HUM ) )
      mudstrlcat( buf, " hum", 512 );
   if( xIS_SET( *extra_flags, ITEM_DARK ) )
      mudstrlcat( buf, " dark", 512 );
   if( xIS_SET( *extra_flags, ITEM_LOYAL ) )
      mudstrlcat( buf, " loyal", 512 );
   if( xIS_SET( *extra_flags, ITEM_EVIL ) )
      mudstrlcat( buf, " evil", 512 );
   if( xIS_SET( *extra_flags, ITEM_INVIS ) )
      mudstrlcat( buf, " invis", 512 );
   if( xIS_SET( *extra_flags, ITEM_MAGIC ) )
      mudstrlcat( buf, " magic", 512 );
   if( xIS_SET( *extra_flags, ITEM_NODROP ) )
      mudstrlcat( buf, " nodrop", 512 );
   if( xIS_SET( *extra_flags, ITEM_BLESS ) )
      mudstrlcat( buf, " bless", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_GOOD ) )
      mudstrlcat( buf, " anti-good", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_EVIL ) )
      mudstrlcat( buf, " anti-evil", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_NEUTRAL ) )
      mudstrlcat( buf, " anti-neutral", 512 );
   if( xIS_SET( *extra_flags, ITEM_NOREMOVE ) )
      mudstrlcat( buf, " noremove", 512 );
   if( xIS_SET( *extra_flags, ITEM_INVENTORY ) )
      mudstrlcat( buf, " inventory", 512 );
   if( xIS_SET( *extra_flags, ITEM_DEATHROT ) )
      mudstrlcat( buf, " deathrot", 512 );
   if( xIS_SET( *extra_flags, ITEM_GROUNDROT ) )
      mudstrlcat( buf, " groundrot", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_MAGE ) )
      mudstrlcat( buf, " anti-mage", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_THIEF ) )
      mudstrlcat( buf, " anti-thief", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_WARRIOR ) )
      mudstrlcat( buf, " anti-warrior", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_CLERIC ) )
      mudstrlcat( buf, " anti-cleric", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_DRUID ) )
      mudstrlcat( buf, " anti-druid", 512 );
   if( xIS_SET( *extra_flags, ITEM_ANTI_VAMPIRE ) )
      mudstrlcat( buf, " anti-vampire", 512 );
   if( xIS_SET( *extra_flags, ITEM_ORGANIC ) )
      mudstrlcat( buf, " organic", 512 );
   if( xIS_SET( *extra_flags, ITEM_METAL ) )
      mudstrlcat( buf, " metal", 512 );
   if( xIS_SET( *extra_flags, ITEM_DONATION ) )
      mudstrlcat( buf, " donation", 512 );
   if( xIS_SET( *extra_flags, ITEM_CLANOBJECT ) )
      mudstrlcat( buf, " clan", 512 );
   if( xIS_SET( *extra_flags, ITEM_CLANCORPSE ) )
      mudstrlcat( buf, " clanbody", 512 );
   if( xIS_SET( *extra_flags, ITEM_PROTOTYPE ) )
      mudstrlcat( buf, " prototype", 512 );
   return ( buf[0] != '\0' ) ? buf + 1 : ( char * )"none";
}

/*
 * Return ascii name of magic flags vector. - Scryn
 */
const char *magic_bit_name( int magic_flags )
{
   static char buf[512];

   buf[0] = '\0';
   if( magic_flags & ITEM_RETURNING )
      mudstrlcat( buf, " returning", 512 );
   return ( buf[0] != '\0' ) ? buf + 1 : ( char * )"none";
}

/*
 * Return ascii name of pulltype exit setting.
 */
const char *pull_type_name( int pulltype )
{
   if( pulltype >= PT_FIRE )
      return ex_pfire[pulltype - PT_FIRE];
   if( pulltype >= PT_AIR )
      return ex_pair[pulltype - PT_AIR];
   if( pulltype >= PT_EARTH )
      return ex_pearth[pulltype - PT_EARTH];
   if( pulltype >= PT_WATER )
      return ex_pwater[pulltype - PT_WATER];
   if( pulltype < 0 )
      return "ERROR";

   return ex_pmisc[pulltype];
}

/*
 * Return ascii name of damage types.
 * -Davenge
 */

const char *damage_type_name( EXT_BV * damtype )
{
   static char buf[512];

   buf[0] = '\0';

   if( xIS_SET( *damtype, DAM_ALL ) )
      mudstrlcat( buf, " All", 512 );
   if( xIS_SET( *damtype, DAM_MAGIC ) )
      mudstrlcat( buf, " All_Magic", 512 );
   if( xIS_SET( *damtype, DAM_PHYSICAL ) )
      mudstrlcat( buf, " All_Physical", 512 );
   if( xIS_SET( *damtype, DAM_PIERCE ) )
      mudstrlcat( buf, " Piercing", 512 );
   if( xIS_SET( *damtype, DAM_SLASH ) )
      mudstrlcat( buf, " Slashing", 512 );
   if( xIS_SET( *damtype, DAM_BLUNT ) )
      mudstrlcat( buf, " Blunt", 512 );
   if( xIS_SET( *damtype, DAM_WIND ) )
      mudstrlcat( buf, " Wind", 512 );
   if( xIS_SET( *damtype, DAM_EARTH ) )
      mudstrlcat( buf, " Earth", 512 );
   if( xIS_SET( *damtype, DAM_FIRE ) )
      mudstrlcat( buf, " Fire", 512 );
   if( xIS_SET( *damtype, DAM_ICE ) )
      mudstrlcat( buf, " Ice", 512 );
   if( xIS_SET( *damtype, DAM_WATER ) )
      mudstrlcat( buf, " Water", 512 );
   if( xIS_SET( *damtype, DAM_LIGHTNING ) )
      mudstrlcat( buf, " Lightning", 512 );
   if( xIS_SET( *damtype, DAM_LIGHT ) )
      mudstrlcat( buf, " Light", 512 );
   if( xIS_SET( *damtype, DAM_DARK ) )
      mudstrlcat( buf, " Darkness", 512 );
   if( xIS_SET( *damtype, DAM_INHERITED ) )
      mudstrlcat( buf, " Inherited", 512 );
   return ( buf[0] != '\0' ) ? buf + 1 : ( char * )"none";
}

/*
 * Set off a trap (obj) upon character (ch) -Thoric
 */
ch_ret spring_trap( CHAR_DATA * ch, OBJ_DATA * obj )
{
   int dam, typ, lev;
   const char *txt;
   char buf[MAX_STRING_LENGTH];
   ch_ret retcode;

   typ = obj->value[1];
   lev = obj->value[2];

   retcode = rNONE;

   switch ( typ )
   {
      default:
         txt = "hit by a trap";
         break;
      case TRAP_TYPE_POISON_GAS:
         txt = "surrounded by a green cloud of gas";
         break;
      case TRAP_TYPE_POISON_DART:
         txt = "hit by a dart";
         break;
      case TRAP_TYPE_POISON_NEEDLE:
         txt = "pricked by a needle";
         break;
      case TRAP_TYPE_POISON_DAGGER:
         txt = "stabbed by a dagger";
         break;
      case TRAP_TYPE_POISON_ARROW:
         txt = "struck with an arrow";
         break;
      case TRAP_TYPE_BLINDNESS_GAS:
         txt = "surrounded by a red cloud of gas";
         break;
      case TRAP_TYPE_SLEEPING_GAS:
         txt = "surrounded by a yellow cloud of gas";
         break;
      case TRAP_TYPE_FLAME:
         txt = "struck by a burst of flame";
         break;
      case TRAP_TYPE_EXPLOSION:
         txt = "hit by an explosion";
         break;
      case TRAP_TYPE_ACID_SPRAY:
         txt = "covered by a spray of acid";
         break;
      case TRAP_TYPE_ELECTRIC_SHOCK:
         txt = "suddenly shocked";
         break;
      case TRAP_TYPE_BLADE:
         txt = "sliced by a razor sharp blade";
         break;
      case TRAP_TYPE_SEX_CHANGE:
         txt = "surrounded by a mysterious aura";
         break;
   }

   dam = number_range( obj->value[2], obj->value[2] * 2 );
   snprintf( buf, MAX_STRING_LENGTH, "You are %s!", txt );
   act( AT_HITME, buf, ch, NULL, NULL, TO_CHAR );
   snprintf( buf, MAX_STRING_LENGTH, "$n is %s.", txt );
   act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
   --obj->value[0];
   if( obj->value[0] <= 0 )
      extract_obj( obj );
   switch ( typ )
   {
      default:
      case TRAP_TYPE_POISON_DART:
      case TRAP_TYPE_POISON_NEEDLE:
      case TRAP_TYPE_POISON_DAGGER:
      case TRAP_TYPE_POISON_ARROW:
         /*
          * hmm... why not use spell_poison() here? 
          */
         retcode = obj_cast_spell( gsn_poison, lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_POISON_GAS:
         retcode = obj_cast_spell( gsn_poison, lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_BLINDNESS_GAS:
         retcode = obj_cast_spell( gsn_blindness, lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_SLEEPING_GAS:
         retcode = obj_cast_spell( skill_lookup( "sleep" ), lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_ACID_SPRAY:
         retcode = obj_cast_spell( skill_lookup( "acid blast" ), lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_SEX_CHANGE:
         retcode = obj_cast_spell( skill_lookup( "change sex" ), lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_FLAME:
      case TRAP_TYPE_EXPLOSION:
         retcode = obj_cast_spell( gsn_fireball, lev, ch, ch, NULL );
         break;
      case TRAP_TYPE_ELECTRIC_SHOCK:
      case TRAP_TYPE_BLADE:
         break;
   }
   return retcode;
}

/*
 * Check an object for a trap					-Thoric
 */
ch_ret check_for_trap( CHAR_DATA * ch, OBJ_DATA * obj, int flag )
{
   OBJ_DATA *check;
   ch_ret retcode;

   if( !obj->first_content )
      return rNONE;

   retcode = rNONE;

   for( check = obj->first_content; check; check = check->next_content )
      if( check->item_type == ITEM_TRAP && IS_SET( check->value[3], flag ) )
      {
         retcode = spring_trap( ch, check );
         if( retcode != rNONE )
            return retcode;
      }
   return retcode;
}

/*
 * Check the room for a trap					-Thoric
 */
ch_ret check_room_for_traps( CHAR_DATA * ch, int flag )
{
   OBJ_DATA *check;
   ch_ret retcode;

   retcode = rNONE;

   if( !ch )
      return rERROR;
   if( !ch->in_room || !ch->in_room->first_content )
      return rNONE;

   for( check = ch->in_room->first_content; check; check = check->next_content )
   {
      if( check->item_type == ITEM_TRAP && IS_SET( check->value[3], flag ) )
      {
         retcode = spring_trap( ch, check );
         if( retcode != rNONE )
            return retcode;
      }
   }
   return retcode;
}

/*
 * return TRUE if an object contains a trap			-Thoric
 */
bool is_trapped( OBJ_DATA * obj )
{
   OBJ_DATA *check;

   if( !obj->first_content )
      return FALSE;

   for( check = obj->first_content; check; check = check->next_content )
      if( check->item_type == ITEM_TRAP )
         return TRUE;

   return FALSE;
}

/*
 * If an object contains a trap, return the pointer to the trap	-Thoric
 */
OBJ_DATA *get_trap( OBJ_DATA * obj )
{
   OBJ_DATA *check;

   if( !obj->first_content )
      return NULL;

   for( check = obj->first_content; check; check = check->next_content )
      if( check->item_type == ITEM_TRAP )
         return check;

   return NULL;
}

/*
 * Return a pointer to the first object of a certain type found that
 * a player is carrying/wearing
 */
OBJ_DATA *get_objtype( CHAR_DATA * ch, short type )
{
   OBJ_DATA *obj;

   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->item_type == type )
         return obj;

   return NULL;
}

/*
 * Remove an exit from a room					-Thoric
 */
void extract_exit( ROOM_INDEX_DATA * room, EXIT_DATA * pexit )
{
   UNLINK( pexit, room->first_exit, room->last_exit, next, prev );
   if( pexit->rexit )
      pexit->rexit->rexit = NULL;
   STRFREE( pexit->keyword );
   STRFREE( pexit->description );
   DISPOSE( pexit );
}

void clean_room( ROOM_INDEX_DATA * room )
{
   EXTRA_DESCR_DATA *ed, *ed_next;
   EXIT_DATA *pexit, *pexit_next;
   MPROG_DATA *mprog, *mprog_next;
   AFFECT_DATA *paf;

   STRFREE( room->description );
   STRFREE( room->name );
   for( mprog = room->mudprogs; mprog; mprog = mprog_next )
   {
      mprog_next = mprog->next;
      STRFREE( mprog->arglist );
      STRFREE( mprog->comlist );
      DISPOSE( mprog );
   }
   for( ed = room->first_extradesc; ed; ed = ed_next )
   {
      ed_next = ed->next;
      STRFREE( ed->description );
      STRFREE( ed->keyword );
      DISPOSE( ed );
      top_ed--;
   }
   room->first_extradesc = NULL;
   room->last_extradesc = NULL;
   for( pexit = room->first_exit; pexit; pexit = pexit_next )
   {
      pexit_next = pexit->next;
      extract_exit( room, pexit );
      top_exit--;
   }
   room->first_exit = NULL;
   room->last_exit = NULL;

   /*
    *  As part of cleaning the room, clean it's affects.
    *  But take care to avoid char corruption.
    */
   while( ( paf = room->first_affect ) != NULL )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_WEAPONSPELL )
      {
         CHAR_DATA *vch;

         for( vch = room->first_person; vch; vch = vch->next )
            affect_modify( vch, paf, FALSE );
      }
      UNLINK( paf, room->first_affect, room->last_affect, next, prev );
      DISPOSE( paf );
   }

   while( ( paf = room->first_permaffect ) != NULL )
   {
      if( paf->location != APPLY_WEARSPELL && paf->location != APPLY_WEAPONSPELL )
      {
         CHAR_DATA *vch;

         for( vch = room->first_person; vch; vch = vch->next )
            affect_modify( vch, paf, FALSE );
      }
      UNLINK( paf, room->first_permaffect, room->last_permaffect, next, prev );
      DISPOSE( paf );
   }

   xCLEAR_BITS( room->room_flags );
   room->sector_type = 0;
   room->light = 0;
}

void clean_obj( OBJ_INDEX_DATA * obj )
{
   AFFECT_DATA *paf, *paf_next;
   EXTRA_DESCR_DATA *ed, *ed_next;
   MPROG_DATA *mprog, *mprog_next;

   STRFREE( obj->name );
   STRFREE( obj->short_descr );
   STRFREE( obj->description );
   STRFREE( obj->action_desc );
   obj->item_type = 0;
   xCLEAR_BITS( obj->extra_flags );
   obj->wear_flags = 0;
   obj->count = 0;
   obj->weight = 0;
   obj->cost = 0;
   obj->value[0] = 0;
   obj->value[1] = 0;
   obj->value[2] = 0;
   obj->value[3] = 0;
   obj->value[4] = 0;
   obj->value[5] = 0;
   for( paf = obj->first_affect; paf; paf = paf_next )
   {
      paf_next = paf->next;
      DISPOSE( paf );
      top_affect--;
   }
   obj->first_affect = NULL;
   obj->last_affect = NULL;
   for( ed = obj->first_extradesc; ed; ed = ed_next )
   {
      ed_next = ed->next;
      STRFREE( ed->description );
      STRFREE( ed->keyword );
      DISPOSE( ed );
      top_ed--;
   }
   obj->first_extradesc = NULL;
   obj->last_extradesc = NULL;
   for( mprog = obj->mudprogs; mprog; mprog = mprog_next )
   {
      mprog_next = mprog->next;
      STRFREE( mprog->arglist );
      STRFREE( mprog->comlist );
      DISPOSE( mprog );
   }
}

/*
 * clean out a mobile (index) (leave list pointers intact )	-Thoric
 */
void clean_mob( MOB_INDEX_DATA * mob )
{
   MPROG_DATA *mprog, *mprog_next;

   STRFREE( mob->player_name );
   STRFREE( mob->short_descr );
   STRFREE( mob->long_descr );
   STRFREE( mob->description );
   mob->spec_fun = NULL;
   mob->pShop = NULL;
   mob->rShop = NULL;
   xCLEAR_BITS( mob->progtypes );

   for( mprog = mob->mudprogs; mprog; mprog = mprog_next )
   {
      mprog_next = mprog->next;
      STRFREE( mprog->arglist );
      STRFREE( mprog->comlist );
      DISPOSE( mprog );
   }
   mob->count = 0;
   mob->killed = 0;
   mob->sex = 0;
   mob->level = 0;
   xCLEAR_BITS( mob->act );
   xCLEAR_BITS( mob->affected_by );
   mob->alignment = 0;
   mob->mobthac0 = 0;
   mob->ac = 0;
   mob->hitnodice = 0;
   mob->hitsizedice = 0;
   mob->hitplus = 0;
   mob->damnodice = 0;
   mob->damsizedice = 0;
   mob->damplus = 0;
   mob->gold = 0;
   mob->experience = 0;
   mob->position = 0;
   mob->defposition = 0;
   mob->height = 0;
   mob->weight = 0;  /* mob->vnum      = 0;  */
   xCLEAR_BITS( mob->attacks );
   xCLEAR_BITS( mob->defenses );
}

extern int top_reset;

/*
 * Remove all resets from a room -Thoric
 */
void clean_resets( ROOM_INDEX_DATA * room )
{
   RESET_DATA *pReset, *pReset_next;

   for( pReset = room->first_reset; pReset; pReset = pReset_next )
   {
      pReset_next = pReset->next;
      delete_reset( pReset );
      --top_reset;
   }
   room->first_reset = NULL;
   room->last_reset = NULL;
}

/*
 * "Roll" players stats based on the character name -Thoric
 */
void name_stamp_stats( CHAR_DATA * ch )
{
   unsigned int x;
   int a, b, c;

   ch->perm_str = UMIN( 18, ch->perm_str );
   ch->perm_wis = UMIN( 18, ch->perm_wis );
   ch->perm_dex = UMIN( 18, ch->perm_dex );
   ch->perm_int = UMIN( 18, ch->perm_int );
   ch->perm_con = UMIN( 18, ch->perm_con );
   ch->perm_cha = UMIN( 18, ch->perm_cha );
   ch->perm_pas = UMIN( 18, ch->perm_pas );
   ch->perm_str = UMAX( 9, ch->perm_str );
   ch->perm_wis = UMAX( 9, ch->perm_wis );
   ch->perm_dex = UMAX( 9, ch->perm_dex );
   ch->perm_int = UMAX( 9, ch->perm_int );
   ch->perm_con = UMAX( 9, ch->perm_con );
   ch->perm_cha = UMAX( 9, ch->perm_cha );
   ch->perm_pas = UMAX( 9, ch->perm_pas );

   for( x = 0; x < strlen( ch->name ); x++ )
   {
      c = ch->name[x] + x;
      b = c % 14;
      a = ( c % 1 ) + 1;
      switch ( b )
      {
         case 0:
            ch->perm_str = UMIN( 18, ch->perm_str + a );
            break;
         case 1:
            ch->perm_dex = UMIN( 18, ch->perm_dex + a );
            break;
         case 2:
            ch->perm_wis = UMIN( 18, ch->perm_wis + a );
            break;
         case 3:
            ch->perm_int = UMIN( 18, ch->perm_int + a );
            break;
         case 4:
            ch->perm_con = UMIN( 18, ch->perm_con + a );
            break;
         case 5:
            ch->perm_cha = UMIN( 18, ch->perm_cha + a );
            break;
         case 6:
            ch->perm_pas = UMIN( 18, ch->perm_pas + a );
            break;
         case 7:
            ch->perm_str = UMAX( 9, ch->perm_str - a );
            break;
         case 8:
            ch->perm_dex = UMAX( 9, ch->perm_dex - a );
            break;
         case 9:
            ch->perm_wis = UMAX( 9, ch->perm_wis - a );
            break;
         case 10:
            ch->perm_int = UMAX( 9, ch->perm_int - a );
            break;
         case 11:
            ch->perm_con = UMAX( 9, ch->perm_con - a );
            break;
         case 12:
            ch->perm_cha = UMAX( 9, ch->perm_cha - a );
            break;
         case 13:
            ch->perm_pas = UMAX( 9, ch->perm_pas - a );
            break;
      }
   }
}

/*
 * "Fix" a character's stats -Thoric
 */
void fix_char( CHAR_DATA * ch )
{
   AFFECT_DATA *aff;
   OBJ_DATA *obj;

   de_equip_char( ch );

   for( aff = ch->first_affect; aff; aff = aff->next )
      affect_modify( ch, aff, FALSE );

   /*
    * As part of returning the char to their "natural naked state",
    * we must strip any room affects from them.
    */
   if( ch->in_room )
   {
      for( aff = ch->in_room->first_affect; aff; aff = aff->next )
      {
         if( aff->location != APPLY_WEARSPELL && aff->location != APPLY_REMOVESPELL && aff->location != APPLY_STRIPSN )
            affect_modify( ch, aff, FALSE );
      }

      for( aff = ch->in_room->first_permaffect; aff; aff = aff->next )
      {
         if( aff->location != APPLY_WEARSPELL && aff->location != APPLY_REMOVESPELL && aff->location != APPLY_STRIPSN )
            affect_modify( ch, aff, FALSE );
      }
   }

   xCLEAR_BITS( ch->affected_by );
   xSET_BITS( ch->affected_by, race_table[ch->race]->affected );
   ch->mental_state = -10;
   ch->hit = UMAX( 1, ch->hit );
   ch->mana = UMAX( 1, ch->mana );
   ch->move = UMAX( 1, ch->move );
   ch->armor = 100;
   ch->mod_str = 0;
   ch->mod_dex = 0;
   ch->mod_wis = 0;
   ch->mod_int = 0;
   ch->mod_con = 0;
   ch->mod_cha = 0;
   ch->mod_pas = 0;
   ch->attack = 0;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );
   ch->saving_breath = 0;
   ch->saving_wand = 0;
   ch->saving_para_petri = 0;
   ch->saving_spell_staff = 0;
   ch->saving_poison_death = 0;

   for( aff = ch->first_affect; aff; aff = aff->next )
      affect_modify( ch, aff, TRUE );

   /*
    * Now that the char is fixed, add the room's affects back on.
    */
   if( ch->in_room )
   {
      for( aff = ch->in_room->first_affect; aff; aff = aff->next )
      {
         if( aff->location != APPLY_WEARSPELL && aff->location != APPLY_REMOVESPELL && aff->location != APPLY_STRIPSN )
            affect_modify( ch, aff, TRUE );
      }

      for( aff = ch->in_room->first_permaffect; aff; aff = aff->next )
      {
         if( aff->location != APPLY_WEARSPELL && aff->location != APPLY_REMOVESPELL && aff->location != APPLY_STRIPSN )
            affect_modify( ch, aff, TRUE );
      }
   }

   ch->carry_weight = 0;
   ch->carry_number = 0;

   for( obj = ch->first_carrying; obj; obj = obj->next_content )
   {
      if( obj->wear_loc == WEAR_NONE )
         ch->carry_number += get_obj_number( obj );
      if( !xIS_SET( obj->extra_flags, ITEM_MAGIC ) )
         ch->carry_weight += get_obj_weight( obj );
   }

   re_equip_char( ch );
}


/*
 * Show an affect verbosely to a character			-Thoric
 */
void showaffect( CHAR_DATA * ch, AFFECT_DATA * paf )
{
   char buf[MAX_STRING_LENGTH];
   int x;

   if( !paf )
   {
      bug( "%s", "showaffect: NULL paf" );
      return;
   }
   if( paf->location != APPLY_NONE && paf->modifier != 0 )
   {
      switch ( paf->location )
      {
         default:
            snprintf( buf, MAX_STRING_LENGTH, "Affects %s by %d.\r\n", affect_loc_name( paf->location ), paf->modifier );
            break;
         case APPLY_AFFECT:
            snprintf( buf, MAX_STRING_LENGTH, "Affects %s by", affect_loc_name( paf->location ) );
            for( x = 0; x < 32; x++ )
               if( IS_SET( paf->modifier, 1 << x ) )
               {
                  mudstrlcat( buf, " ", MAX_STRING_LENGTH );
                  mudstrlcat( buf, a_flags[x], MAX_STRING_LENGTH );
               }
            mudstrlcat( buf, "\r\n", MAX_STRING_LENGTH );
            break;
         case APPLY_WEAPONSPELL:
         case APPLY_WEARSPELL:
         case APPLY_REMOVESPELL:
            snprintf( buf, MAX_STRING_LENGTH, "Casts spell '%s'\r\n",
                      IS_VALID_SN( paf->modifier ) ? skill_table[paf->modifier]->name : "unknown" );
            break;
         case APPLY_RESISTANT:
         case APPLY_IMMUNE:
         case APPLY_SUSCEPTIBLE:
            snprintf( buf, MAX_STRING_LENGTH, "Affects %s by", affect_loc_name( paf->location ) );
            for( x = 0; x < 32; x++ )
               if( IS_SET( paf->modifier, 1 << x ) )
               {
                  mudstrlcat( buf, " ", MAX_STRING_LENGTH );
                  mudstrlcat( buf, ris_flags[x], MAX_STRING_LENGTH );
               }
            mudstrlcat( buf, "\r\n", MAX_STRING_LENGTH );
            break;
         case APPLY_PENETRATION:
         case APPLY_RESISTANCE:
            int damtype, amount;

            damtype = get_value_one( paf->modifier );
            amount = get_value_two( paf->modifier );
            snprintf( buf, MAX_STRING_LENGTH,  "Affects %s %s by %d.\r\n", damage_table[damtype], a_types[paf->location],  amount );
            break;
         case APPLY_GRANTSKILL:
            snprintf( buf, MAX_STRING_LENGTH, "Grants wearer access to the '%s' skill.\r\n", skill_table[paf->modifier]->name );
            break;
         case APPLY_SKILLPOTENCY:
            snprintf( buf, MAX_STRING_LENGTH, "Boosts '%s' potency by %d percent.\r\n", skill_table[get_value_one( paf->modifier )]->name, get_value_two( paf->modifier ) );
            break;
         case APPLY_SKILLRANGE:
            snprintf( buf, MAX_STRING_LENGTH, "Affects '%s' range by %droom(s).\r\n", skill_table[get_value_one( paf->modifier )]->name, get_value_two( paf->modifier ) );
            break;
         case APPLY_SKILLCOOLDOWN:
            snprintf( buf, MAX_STRING_LENGTH, "Affects '%s' cooldown by %d &cseconds.\r\n", skill_table[get_value_one( paf->modifier )]->name, get_value_two( paf->modifier ) );
            break;
         case APPLY_SKILLDURATION:
            snprintf( buf, MAX_STRING_LENGTH, "Affects '%s' duration by %d &cseconds.\r\n", skill_table[get_value_one( paf->modifier )]->name, get_value_two( paf->modifier ) );
            break;
         case APPLY_SKILLHITS:
            snprintf( buf, MAX_STRING_LENGTH, "Affects '%s' hits per use by %d.\r\n", skill_table[get_value_one( paf->modifier )]->name, get_value_two( paf->modifier ) );
            break;
      }
      send_to_char( buf, ch );
   }
}

/*
 * Set the current global object to obj				-Thoric
 */
void set_cur_obj( OBJ_DATA * obj )
{
   cur_obj = obj->serial;
   cur_obj_extracted = FALSE;
   global_objcode = rNONE;
}

/*
 * Check the recently extracted object queue for obj		-Thoric
 */
bool obj_extracted( OBJ_DATA * obj )
{
   OBJ_DATA *cod;

   if( obj->serial == cur_obj && cur_obj_extracted )
      return TRUE;

   for( cod = extracted_obj_queue; cod; cod = cod->next )
      if( obj == cod )
         return TRUE;
   return FALSE;
}

/*
 * Stick obj onto extraction queue
 */
void queue_extracted_obj( OBJ_DATA * obj )
{

   ++cur_qobjs;
   obj->next = extracted_obj_queue;
   extracted_obj_queue = obj;
}

/* Deallocates the memory used by a single object after it's been extracted. */
void free_obj( OBJ_DATA * obj )
{
   AFFECT_DATA *paf, *paf_next;
   EXTRA_DESCR_DATA *ed, *ed_next;
   REL_DATA *RQueue, *rq_next;
   MPROG_ACT_LIST *mpact, *mpact_next;

   for( mpact = obj->mpact; mpact; mpact = mpact_next )
   {
      mpact_next = mpact->next;
      DISPOSE( mpact->buf );
      DISPOSE( mpact );
   }

   /*
    * remove affects 
    */
   for( paf = obj->first_affect; paf; paf = paf_next )
   {
      paf_next = paf->next;
      DISPOSE( paf );
   }
   obj->first_affect = obj->last_affect = NULL;

   /*
    * remove extra descriptions 
    */
   for( ed = obj->first_extradesc; ed; ed = ed_next )
   {
      ed_next = ed->next;
      STRFREE( ed->description );
      STRFREE( ed->keyword );
      DISPOSE( ed );
   }
   obj->first_extradesc = obj->last_extradesc = NULL;

   for( RQueue = first_relation; RQueue; RQueue = rq_next )
   {
      rq_next = RQueue->next;
      if( RQueue->Type == relOSET_ON )
      {
         if( obj == RQueue->Subject )
            ( ( CHAR_DATA * ) RQueue->Actor )->dest_buf = NULL;
         else
            continue;
         UNLINK( RQueue, first_relation, last_relation, next, prev );
         DISPOSE( RQueue );
      }
   }
   STRFREE( obj->name );
   STRFREE( obj->description );
   STRFREE( obj->short_descr );
   STRFREE( obj->action_desc );
   STRFREE( obj->owner );
   DISPOSE( obj );
   return;
}

/*
 * Clean out the extracted object queue
 */
void clean_obj_queue( void )
{
   OBJ_DATA *obj;

   while( extracted_obj_queue )
   {
      obj = extracted_obj_queue;
      extracted_obj_queue = extracted_obj_queue->next;
      free_obj( obj );
      --cur_qobjs;
   }
}

/*
 * Set the current global character to ch			-Thoric
 */
void set_cur_char( CHAR_DATA * ch )
{
   cur_char = ch;
   cur_char_died = FALSE;
   cur_room = ch->in_room;
   global_retcode = rNONE;
}

/*
 * Check to see if ch died recently				-Thoric
 */
bool char_died( CHAR_DATA * ch )
{
   EXTRACT_CHAR_DATA *ccd;

   if( ch == cur_char && cur_char_died )
      return TRUE;

   for( ccd = extracted_char_queue; ccd; ccd = ccd->next )
      if( ccd->ch == ch )
         return TRUE;
   return FALSE;
}

/*
 * Add ch to the queue of recently extracted characters		-Thoric
 */
void queue_extracted_char( CHAR_DATA * ch, bool extract )
{
   EXTRACT_CHAR_DATA *ccd;

   if( !ch )
   {
      bug( "%s", "queue_extracted char: ch = NULL" );
      return;
   }
   CREATE( ccd, EXTRACT_CHAR_DATA, 1 );
   ccd->ch = ch;
   ccd->room = ch->in_room;
   ccd->extract = extract;
   if( ch == cur_char )
      ccd->retcode = global_retcode;
   else
      ccd->retcode = rCHAR_DIED;
   ccd->next = extracted_char_queue;
   extracted_char_queue = ccd;
   cur_qchars++;
}

/*
 * clean out the extracted character queue
 */
void clean_char_queue(  )
{
   EXTRACT_CHAR_DATA *ccd;

   for( ccd = extracted_char_queue; ccd; ccd = extracted_char_queue )
   {
      extracted_char_queue = ccd->next;
      if( ccd->extract )
         free_char( ccd->ch );
      DISPOSE( ccd );
      --cur_qchars;
   }
}

void add_timer( CHAR_DATA *ch, short type, int count, DO_FUN * fun, int value )
{
   add_timer( ch, type, (double)count, fun, value );
   return; 
}

/*
 * Add a timer to ch						-Thoric
 * Support for "call back" time delayed commands
 */
void add_timer( CHAR_DATA * ch, short type, double count, DO_FUN * fun, int value )
{
   TIMER *timer;

   for( timer = ch->first_timer; timer; timer = timer->next )
      if( timer->type == type )
      {
         timer->count = count;
         timer->do_fun = fun;
         timer->value = value;
         break;
      }
   if( !timer )
   {
      CREATE( timer, TIMER, 1 );
      timer->count = count;
      timer->type = type;
      timer->do_fun = fun;
      timer->value = value;
      LINK( timer, ch->first_timer, ch->last_timer, next, prev );
   }
   if( !is_queued( ch, TIMER_TIMER ) )
      add_queue( ch, TIMER_TIMER );
}

TIMER *get_timerptr( CHAR_DATA * ch, short type )
{
   TIMER *timer;

   for( timer = ch->first_timer; timer; timer = timer->next )
      if( timer->type == type )
         return timer;
   return NULL;
}

double get_timer( CHAR_DATA * ch, short type )
{
   TIMER *timer;

   if( ( timer = get_timerptr( ch, type ) ) != NULL )
      return timer->count;
   else
      return 0;
}

void extract_timer( CHAR_DATA * ch, TIMER * timer )
{
   if( !timer )
   {
      bug( "%s", "extract_timer: NULL timer" );
      return;
   }

   UNLINK( timer, ch->first_timer, ch->last_timer, next, prev );
   DISPOSE( timer );
   return;
}

void remove_timer( CHAR_DATA * ch, short type )
{
   TIMER *timer;

   for( timer = ch->first_timer; timer; timer = timer->next )
      if( timer->type == type )
         break;

   if( timer )
      extract_timer( ch, timer );
}

bool in_soft_range( CHAR_DATA * ch, AREA_DATA * tarea )
{
   if( IS_IMMORTAL( ch ) )
      return TRUE;
   else if( IS_NPC( ch ) )
      return TRUE;
   else if( ch->level >= tarea->low_soft_range || ch->level <= tarea->hi_soft_range )
      return TRUE;
   else
      return FALSE;
}

bool can_astral( CHAR_DATA * ch, CHAR_DATA * victim )
{
   if( victim == ch
       || !victim->in_room
       || xIS_SET( victim->in_room->room_flags, ROOM_PRIVATE )
       || xIS_SET( victim->in_room->room_flags, ROOM_SOLITARY )
       || xIS_SET( victim->in_room->room_flags, ROOM_NO_ASTRAL )
       || xIS_SET( victim->in_room->room_flags, ROOM_DEATH )
       || xIS_SET( victim->in_room->room_flags, ROOM_PROTOTYPE )
       || xIS_SET( ch->in_room->room_flags, ROOM_NO_RECALL )
       || victim->level >= ch->level + 15
       || ( CAN_PKILL( victim ) && !IS_NPC( ch ) && !CAN_PKILL( ch ) )
       || ( IS_NPC( victim ) && xIS_SET( victim->act, ACT_PROTOTYPE ) )
       || ( IS_NPC( victim ) && saves_spell_staff( ch->level, victim ) )
       || ( IS_SET( victim->in_room->area->flags, AFLAG_NOPKILL ) && IS_PKILL( ch ) ) )
      return FALSE;
   else
      return TRUE;
}

bool in_hard_range( CHAR_DATA * ch, AREA_DATA * tarea )
{
   if( IS_IMMORTAL( ch ) )
      return TRUE;
   else if( IS_NPC( ch ) )
      return TRUE;
   else if( ch->level >= tarea->low_hard_range && ch->level <= tarea->hi_hard_range )
      return TRUE;
   else
      return FALSE;
}


/*
 * Scryn, standard luck check 2/2/96
 */
bool chance( CHAR_DATA * ch, short percent )
{
/*  short clan_factor, ms;*/
   short deity_factor, ms;

   if( !ch )
   {
      bug( "%s", "Chance: null ch!" );
      return FALSE;
   }

/* Code for clan stuff put in by Narn, Feb/96.  The idea is to punish clan
members who don't keep their alignment in tune with that of their clan by
making it harder for them to succeed at pretty much everything.  Clan_factor
will vary from 1 to 3, with 1 meaning there is no effect on the player's
change of success, and with 3 meaning they have half the chance of doing
whatever they're trying to do. 

Note that since the neutral clannies can only be off by 1000 points, their
maximum penalty will only be half that of the other clan types.

  if ( IS_CLANNED( ch ) )
    clan_factor = 1 + abs( ch->alignment - ch->pcdata->clan->alignment ) / 1000; 
  else
    clan_factor = 1;
*/
/* Mental state bonus/penalty:  Your mental state is a ranged value with
 * zero (0) being at a perfect mental state (bonus of 10).
 * negative values would reflect how sedated one is, and
 * positive values would reflect how stimulated one is.
 * In most circumstances you'd do best at a perfectly balanced state.
 */

   if( IS_DEVOTED( ch ) )
      deity_factor = ch->pcdata->favor / -500;
   else
      deity_factor = 0;

   ms = 10 - abs( ch->mental_state );

   if( ( number_percent(  ) - get_curr_pas( ch ) + 13 - ms ) + deity_factor <= percent )
      return TRUE;
   else
      return FALSE;
}

bool chance_attrib( CHAR_DATA * ch, short percent, short attrib )
{
/* Scryn, standard luck check + consideration of 1 attrib 2/2/96*/
   short deity_factor;

   if( !ch )
   {
      bug( "%s", "Chance: null ch!" );
      return FALSE;
   }

   if( IS_DEVOTED( ch ) )
      deity_factor = ch->pcdata->favor / -500;
   else
      deity_factor = 0;

   if( number_percent(  ) - get_curr_pas( ch ) + 13 - attrib + 13 + deity_factor <= percent )
      return TRUE;
   else
      return FALSE;

}

/*
 * Make a simple clone of an object (no extras...yet)		-Thoric
 */
OBJ_DATA *clone_object( OBJ_DATA * obj )
{
   OBJ_DATA *clone;

   CREATE( clone, OBJ_DATA, 1 );
   clone->pIndexData = obj->pIndexData;
   clone->name = QUICKLINK( obj->name );
   clone->short_descr = QUICKLINK( obj->short_descr );
   clone->description = QUICKLINK( obj->description );
   clone->action_desc = QUICKLINK( obj->action_desc );
   clone->owner = QUICKLINK( obj->owner );
   clone->item_type = obj->item_type;
   clone->extra_flags = obj->extra_flags;
   clone->magic_flags = obj->magic_flags;
   clone->wear_flags = obj->wear_flags;
   clone->wear_loc = obj->wear_loc;
   clone->weight = obj->weight;
   clone->cost = obj->cost;
   clone->level = obj->level;
   clone->timer = obj->timer;
   clone->value[0] = obj->value[0];
   clone->value[1] = obj->value[1];
   clone->value[2] = obj->value[2];
   clone->value[3] = obj->value[3];
   clone->value[4] = obj->value[4];
   clone->value[5] = obj->value[5];
   clone->count = 1;
   ++obj->pIndexData->count;
   ++numobjsloaded;
   ++physicalobjects;
   cur_obj_serial = UMAX( ( cur_obj_serial + 1 ) & ( BV30 - 1 ), 1 );
   clone->serial = clone->pIndexData->serial = cur_obj_serial;
   LINK( clone, first_object, last_object, next, prev );
   return clone;
}

/*
 * If possible group obj2 into obj1				-Thoric
 * This code, along with clone_object, obj->count, and special support
 * for it implemented throughout handler.c and save.c should show improved
 * performance on MUDs with players that hoard tons of potions and scrolls
 * as this will allow them to be grouped together both in memory, and in
 * the player files.
 */
OBJ_DATA *group_object( OBJ_DATA * obj1, OBJ_DATA * obj2 )
{
   if( !obj1 || !obj2 )
      return NULL;
   if( obj1 == obj2 )
      return obj1;

   if( obj1->pIndexData == obj2->pIndexData && !str_cmp( obj1->name, obj2->name ) && !str_cmp( obj1->short_descr, obj2->short_descr ) && !str_cmp( obj1->description, obj2->description ) && !str_cmp( obj1->action_desc, obj2->action_desc ) && !str_cmp( obj1->owner, obj2->owner ) && obj1->item_type == obj2->item_type && xSAME_BITS( obj1->extra_flags, obj2->extra_flags ) && obj1->magic_flags == obj2->magic_flags && obj1->wear_flags == obj2->wear_flags && obj1->wear_loc == obj2->wear_loc && obj1->weight == obj2->weight && obj1->cost == obj2->cost && obj1->level == obj2->level && obj1->timer == obj2->timer && obj1->value[0] == obj2->value[0] && obj1->value[1] == obj2->value[1] && obj1->value[2] == obj2->value[2] && obj1->value[3] == obj2->value[3] && obj1->value[4] == obj2->value[4] && obj1->value[5] == obj2->value[5] && !obj1->first_extradesc && !obj2->first_extradesc && !obj1->first_affect && !obj2->first_affect && !obj1->first_content && !obj2->first_content && obj1->count + obj2->count > 0 ) /* prevent count overflow */
   {
      obj1->count += obj2->count;
      obj1->pIndexData->count += obj2->count;   /* to be decremented in */
      numobjsloaded += obj2->count; /* extract_obj */
      extract_obj( obj2 );
      return obj1;
   }
   return obj2;
}

/*
 * Split off a grouped object					-Thoric
 * decreased obj's count to num, and creates a new object containing the rest
 */
void split_obj( OBJ_DATA * obj, int num )
{
   int count = obj->count;
   OBJ_DATA *rest;

   if( count <= num || num == 0 )
      return;

   rest = clone_object( obj );
   --obj->pIndexData->count;  /* since clone_object() ups this value */
   --numobjsloaded;
   rest->count = obj->count - num;
   obj->count = num;

   if( obj->carried_by )
   {
      LINK( rest, obj->carried_by->first_carrying, obj->carried_by->last_carrying, next_content, prev_content );
      rest->carried_by = obj->carried_by;
      rest->in_room = NULL;
      rest->in_obj = NULL;
   }
   else if( obj->in_room )
   {
      LINK( rest, obj->in_room->first_content, obj->in_room->last_content, next_content, prev_content );
      rest->carried_by = NULL;
      rest->in_room = obj->in_room;
      rest->in_obj = NULL;
   }
   else if( obj->in_obj )
   {
      LINK( rest, obj->in_obj->first_content, obj->in_obj->last_content, next_content, prev_content );
      rest->in_obj = obj->in_obj;
      rest->in_room = NULL;
      rest->carried_by = NULL;
   }
}

void separate_obj( OBJ_DATA * obj )
{
   split_obj( obj, 1 );
}

/*
 * Empty an obj's contents... optionally into another obj, or a room
 */
bool empty_obj( OBJ_DATA * obj, OBJ_DATA * destobj, ROOM_INDEX_DATA * destroom )
{
   OBJ_DATA *otmp, *otmp_next;
   CHAR_DATA *ch = obj->carried_by;
   bool movedsome = FALSE;

   if( !obj )
   {
      bug( "%s", "empty_obj: NULL obj" );
      return FALSE;
   }
   if( destobj || ( !destroom && !ch && ( destobj = obj->in_obj ) != NULL ) )
   {
      for( otmp = obj->first_content; otmp; otmp = otmp_next )
      {
         otmp_next = otmp->next_content;
         /*
          * only keys on a keyring 
          */
         if( destobj->item_type == ITEM_KEYRING && otmp->item_type != ITEM_KEY )
            continue;
         if( destobj->item_type == ITEM_QUIVER && otmp->item_type != ITEM_PROJECTILE )
            continue;
         if( ( destobj->item_type == ITEM_CONTAINER || destobj->item_type == ITEM_KEYRING
               || destobj->item_type == ITEM_QUIVER )
             && get_real_obj_weight( otmp ) + get_real_obj_weight( destobj ) > destobj->value[0] )
            continue;
         obj_from_obj( otmp );
         obj_to_obj( otmp, destobj );
         movedsome = TRUE;
      }
      return movedsome;
   }
   if( destroom || ( !ch && ( destroom = obj->in_room ) != NULL ) )
   {
      for( otmp = obj->first_content; otmp; otmp = otmp_next )
      {
         otmp_next = otmp->next_content;
         if( ch && HAS_PROG( otmp->pIndexData, DROP_PROG ) && otmp->count > 1 )
         {
            separate_obj( otmp );
            obj_from_obj( otmp );
            if( !otmp_next )
               otmp_next = obj->first_content;
         }
         else
            obj_from_obj( otmp );
         otmp = obj_to_room( otmp, destroom );
         if( ch )
         {
            oprog_drop_trigger( ch, otmp );  /* mudprogs */
            if( char_died( ch ) )
               ch = NULL;
         }
         movedsome = TRUE;
      }
      return movedsome;
   }
   if( ch )
   {
      for( otmp = obj->first_content; otmp; otmp = otmp_next )
      {
         otmp_next = otmp->next_content;
         obj_from_obj( otmp );
         obj_to_char( otmp, ch );
         movedsome = TRUE;
      }
      return movedsome;
   }
   bug( "empty_obj: could not determine a destination for vnum %d", obj->pIndexData->vnum );
   return FALSE;
}

/*
 * Improve mental state						-Thoric
 */
void better_mental_state( CHAR_DATA * ch, int mod )
{
   int c = URANGE( 0, abs( mod ), 20 );
   int con = get_curr_con( ch );

   c += number_percent(  ) < con ? 1 : 0;

   if( ch->mental_state < 0 )
      ch->mental_state = URANGE( -100, ch->mental_state + c, 0 );
   else if( ch->mental_state > 0 )
      ch->mental_state = URANGE( 0, ch->mental_state - c, 100 );
}

/*
 * Deteriorate mental state					-Thoric
 */
void worsen_mental_state( CHAR_DATA * ch, int mod )
{
   int c = URANGE( 0, abs( mod ), 20 );
   int con = get_curr_con( ch );

   c -= number_percent(  ) < con ? 1 : 0;
   if( c < 1 )
      return;

   /*
    * Nuisance flag makes state worsen quicker. --Shaddai 
    */
   if( !IS_NPC( ch ) && ch->pcdata->nuisance && ch->pcdata->nuisance->flags > 2 )
      c += ( int )( .4 * ( ( ch->pcdata->nuisance->flags - 2 ) * ch->pcdata->nuisance->power ) );

   if( ch->mental_state < 0 )
      ch->mental_state = URANGE( -100, ch->mental_state - c, 100 );
   else if( ch->mental_state > 0 )
      ch->mental_state = URANGE( -100, ch->mental_state + c, 100 );
   else
      ch->mental_state -= c;
}

/*
 * Add gold to an area's economy				-Thoric
 */
void boost_economy( AREA_DATA * tarea, int gold )
{
   while( gold >= 1000000000 )
   {
      ++tarea->high_economy;
      gold -= 1000000000;
   }
   tarea->low_economy += gold;
   while( tarea->low_economy >= 1000000000 )
   {
      ++tarea->high_economy;
      tarea->low_economy -= 1000000000;
   }
}

/*
 * Take gold from an area's economy				-Thoric
 */
void lower_economy( AREA_DATA * tarea, int gold )
{
   while( gold >= 1000000000 )
   {
      --tarea->high_economy;
      gold -= 1000000000;
   }
   tarea->low_economy -= gold;
   while( tarea->low_economy < 0 )
   {
      --tarea->high_economy;
      tarea->low_economy += 1000000000;
   }
}

/*
 * Check to see if economy has at least this much gold		   -Thoric
 */
bool economy_has( AREA_DATA * tarea, int gold )
{
   int hasgold = ( ( tarea->high_economy > 0 ) ? 1 : 0 ) * 1000000000 + tarea->low_economy;

   if( hasgold >= gold )
      return TRUE;
   return FALSE;
}

/*
 * Used in db.c when resetting a mob into an area		    -Thoric
 * Makes sure mob doesn't get more than 10% of that area's gold,
 * and reduces area economy by the amount of gold given to the mob
 */
void economize_mobgold( CHAR_DATA * mob )
{
   int gold;
   AREA_DATA *tarea;

   /*
    * make sure it isn't way too much 
    */
   mob->gold = UMIN( mob->gold, mob->level * mob->level * 400 );
   if( !mob->in_room )
      return;
   tarea = mob->in_room->area;

   gold = ( ( tarea->high_economy > 0 ) ? 1 : 0 ) * 1000000000 + tarea->low_economy;
   mob->gold = URANGE( 0, mob->gold, gold / 10 );
   if( mob->gold )
      lower_economy( tarea, mob->gold );
}


/*
 * Add another notch on that there belt... ;)
 * Keep track of the last so many kills by vnum			-Thoric
 */
void add_kill( CHAR_DATA * ch, CHAR_DATA * mob )
{
   int x, vnum;
   short track;

   if( IS_NPC( ch ) )
   {
      bug( "%s", "add_kill: trying to add kill to npc" );
      return;
   }
   if( !IS_NPC( mob ) )
   {
      bug( "%s", "add_kill: trying to add kill non-npc" );
      return;
   }
   vnum = mob->pIndexData->vnum;
   track = URANGE( 2, ( ( ch->level + 3 ) * MAX_KILLTRACK ) / LEVEL_AVATAR, MAX_KILLTRACK );
   for( x = 0; x < track; x++ )
      if( ch->pcdata->killed[x].vnum == vnum )
      {
         if( ch->pcdata->killed[x].count < 50 )
            ++ch->pcdata->killed[x].count;
         return;
      }
      else if( ch->pcdata->killed[x].vnum == 0 )
         break;
   memmove( ( char * )ch->pcdata->killed + sizeof( KILLED_DATA ),
            ch->pcdata->killed, ( track - 1 ) * sizeof( KILLED_DATA ) );
   ch->pcdata->killed[0].vnum = vnum;
   ch->pcdata->killed[0].count = 1;
   if( track < MAX_KILLTRACK )
      ch->pcdata->killed[track].vnum = 0;
}

/*
 * Return how many times this player has killed this mob	-Thoric
 * Only keeps track of so many (MAX_KILLTRACK), and keeps track by vnum
 */
int times_killed( CHAR_DATA * ch, CHAR_DATA * mob )
{
   int vnum, x;
   short track;

   if( IS_NPC( ch ) )
   {
      bug( "%s", "times_killed: ch is not a player" );
      return 0;
   }
   if( !IS_NPC( mob ) )
   {
      bug( "%s", "add_kill: mob is not a mobile" );
      return 0;
   }

   vnum = mob->pIndexData->vnum;
   track = URANGE( 2, ( ( ch->level + 3 ) * MAX_KILLTRACK ) / LEVEL_AVATAR, MAX_KILLTRACK );
   for( x = 0; x < track; x++ )
      if( ch->pcdata->killed[x].vnum == vnum )
         return ch->pcdata->killed[x].count;
      else if( ch->pcdata->killed[x].vnum == 0 )
         break;
   return 0;
}

/*
 * returns area with name matching input string
 * Last Modified : July 21, 1997
 * Fireblade
 */
AREA_DATA *get_area( char *name )
{
   AREA_DATA *pArea;

   if( !name )
   {
      bug( "get_area: NULL input string." );
      return NULL;
   }

   for( pArea = first_area; pArea; pArea = pArea->next )
   {
      if( nifty_is_name( name, pArea->name ) )
         break;
   }

   if( !pArea )
   {
      for( pArea = first_build; pArea; pArea = pArea->next )
      {
         if( nifty_is_name( name, pArea->name ) )
            break;
      }
   }

   return pArea;
}

AREA_DATA *get_area_obj( OBJ_INDEX_DATA * pObjIndex )
{
   AREA_DATA *pArea;

   if( !pObjIndex )
   {
      bug( "get_area_obj: pObjIndex is NULL." );
      return NULL;
   }
   for( pArea = first_area; pArea; pArea = pArea->next )
   {
      if( pObjIndex->vnum >= pArea->low_o_vnum && pObjIndex->vnum <= pArea->hi_o_vnum )
         break;
   }
   return pArea;
}

void check_switches( bool possess )
{
   CHAR_DATA *ch;

   for( ch = first_char; ch; ch = ch->next )
      check_switch( ch, possess );
}

void check_switch( CHAR_DATA * ch, bool possess )
{
   AFFECT_DATA *paf;
   CMDTYPE *cmd;
   int hash, trust = get_trust( ch );

   if( !ch->switched )
      return;

   if( !possess )
   {
      for( paf = ch->switched->first_affect; paf; paf = paf->next )
      {
         if( paf->duration == -1 )
            continue;
         if( paf->type != -1 && skill_table[paf->type]->spell_fun == spell_possess )
            return;
      }
   }

   for( hash = 0; hash < 126; hash++ )
   {
      for( cmd = command_hash[hash]; cmd; cmd = cmd->next )
      {
         if( cmd->do_fun != do_switch )
            continue;
         if( cmd->level <= trust )
            return;

         if( !IS_NPC( ch ) && ch->pcdata->bestowments && is_name( cmd->name, ch->pcdata->bestowments )
             && cmd->level <= trust + sysdata.bestow_dif )
            return;
      }
   }

   if( !possess )
   {
      set_char_color( AT_BLUE, ch->switched );
      send_to_char( "You suddenly forfeit the power to switch!\n\r", ch->switched );
   }
   do_return( ch->switched, "" );
}

/* Get the max possible "range" of a character's auto attacks -Davenge */

int get_max_range( CHAR_DATA * ch )
{
   OBJ_DATA *obj;
   OBJ_DATA *obj2;
   int range = 0;

   obj = get_eq_char( ch, WEAR_WIELD );
   obj2 = get_eq_char( ch, WEAR_DUAL_WIELD );

   if( obj && obj2 )
      if( obj2->range > obj->range )
         obj = obj2;

   if( obj )
      range += obj->range;
   else
      range += 1 + ch->range;
   return range;
}

/* Get target data when only a name is given. -Davenge */

TARGET_DATA *get_target( CHAR_DATA * ch, const char * argument, int dir )
{
   CHAR_DATA *rch;
   CHAR_DATA *victim;
   ROOM_INDEX_DATA *in_room;
   EXIT_DATA *pexit;
   short dist;
   char arg[MAX_INPUT_LENGTH];
   int number, count;
   bool found;

   found = FALSE;

   in_room = ch->in_room;
   number = number_argument( argument, arg );
   count = 0;

   if( dir == -1 )
   {
      if( ( victim = get_char_world( ch, argument ) ) == NULL )
         return NULL;
      dir = find_first_step( ch->in_room, victim->in_room, 10 );
   }

   if( dir == BFS_ALREADY_THERE )
   {
      if( ( victim = get_char_room( ch, argument ) ) == NULL || !can_see( ch, victim ) )
         return NULL;
      else
         return make_new_target( victim, 0, -1 );
   }

   if( ( pexit = get_exit( in_room, dir ) ) == NULL || IS_SET( pexit->exit_info, EX_SECRET ) )
      return NULL;

   for( dist = 1; ; dist++ )
   {
      if( IS_SET( pexit->exit_info, EX_SECRET ) )
         break;
      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
         break;

      in_room = pexit->to_room;

      for( rch = in_room->first_person; rch; rch = rch->next_in_room )
         if( can_see( ch, rch ) && nifty_is_name( arg, rch->name ) )
         {
            if( number == 0 && !IS_NPC( rch ) )
            {
               victim = rch;
               found = TRUE;
               break;
            }
            else if( ++count == number )
            {
               victim = rch;
               found = TRUE;
               break;
            }
         }

      if( found )
      {
         log_string( victim->name );
         return make_new_target( victim, dist, dir );
      }
      if( ( pexit = get_exit( in_room, dir ) ) == NULL )
         break;
   }
   return NULL;
}

/* Get target when specific CHAR_DATA *victim is known -Davenge */

TARGET_DATA *get_target_2( CHAR_DATA *ch, CHAR_DATA *victim, int dir )
{
   int range;

   if( ch->in_room == victim->in_room )
      return make_new_target( victim, 0, -1 );

   if( dir == -1 )
      dir = find_first_step( ch->in_room, victim->in_room, MAX_DISTANCE );

   if( dir != BFS_ERROR && dir != BFS_NO_PATH )
   {
      range = find_distance( ch, victim, dir );
      if( range > MAX_DISTANCE )
      {
         send_to_char( "Target distance greater than 20, losing target.\r\n", ch );
         return NULL;
      }
      if( range < 0 && range != BFS_ALREADY_THERE )
      {
         bug( "An error occured during the targeting of CH: %s\r\n", ch->name );
         return NULL;
      }
      return make_new_target( victim, range, dir );
   }
   return NULL;
}

int find_distance( CHAR_DATA *ch, CHAR_DATA *victim, int init_dir )
{
   ROOM_INDEX_DATA *current_room, *dest_room;
   EXIT_DATA *pexit;
   int dir, dist;

   current_room = ch->in_room;
   dest_room = victim->in_room;

   /*
    * If they are in the same room, return 0 - Davenge
    */

   if( current_room == dest_room )
      return 0;

   /*
    * Not in same room, what direction should we head in? -Davenge
    */

   if( init_dir > -1 )
      dir = init_dir;
   else
      dir = find_first_step( current_room, dest_room, MAX_DISTANCE );

   for( dist = 1; dist <= MAX_DISTANCE; dist ++ )
   {
      /*
       *If somehow an exit does not exist in this direction return -1 which is error for this function
       * -Davenge
       */
      if( ( pexit = get_exit( current_room, dir ) ) == NULL )
         return -1;
      /*
       * Change current_room to the exit's to_room, basically the next one in our tracking path
       * -Davenge
       */
      if( ( current_room = pexit->to_room ) == dest_room )
         return dist;
      /*
       * Haven't got their next, so find the direction we need for our next step -Davenge
       */
      dir = find_first_step( current_room, dest_room, MAX_DISTANCE );
   }
   return MAX_DISTANCE+1;
}

/* Function that checks Line of Sight -Davenge */

bool check_los( CHAR_DATA *ch, CHAR_DATA *victim )
{
   int ch_coord[3];
   int victim_coord[3];
   int counter, dif_x, dif_y, dif_z;
   int big_dif, dif_distance, distance;
   int inc_x, inc_y, inc_z;

   if( ch->in_room == victim->in_room) //Just incase, haha
      return TRUE;
   /*
    * Transfer coords into these arrays for simpler access -Davenge
    */

   for( counter = 0; counter < 3; counter++ )
      ch_coord[counter] = ch->in_room->coord[counter];
   for( counter = 0; counter < 3; counter++ )
      victim_coord[counter] = victim->in_room->coord[counter];

   dif_x = coord_dif( ch_coord[X], victim_coord[X] );
   dif_y = coord_dif( ch_coord[Y], victim_coord[Y] );
   dif_z = coord_dif( ch_coord[Z], victim_coord[Z] );

   distance = find_distance( ch, victim, -1 );
   dif_distance = distance_from_dif( dif_z, dif_y, dif_x );

   if( distance > dif_distance )
      return FALSE;

   big_dif = dif_x;
   if( dif_y > big_dif )
      big_dif = dif_y;
   if( dif_z > big_dif )
      big_dif = dif_z;

   inc_x = coord_inc( ch_coord[X], victim_coord[X], dif_x, big_dif );
   inc_y = coord_inc( ch_coord[Y], victim_coord[Y], dif_y, big_dif );
   inc_z = coord_inc( ch_coord[Z], victim_coord[Z], dif_z, big_dif );

   ROOM_INDEX_DATA *line[distance];
   EXIT_DATA *exit;

   line[0] = ch->in_room;
   line[distance-1] = victim->in_room;

   for( counter = 1; counter < ( distance - 1 ); counter++ )
      line[counter] = next_room_on_line( ch, counter, inc_x, inc_y, inc_z );

   /*
    * Check for a NULL room in our line of rooms -Davenge
    */
   for( counter = 0; counter < distance; counter++ )
      if( line[counter] == NULL )
         return FALSE;

   for( counter = 0; counter < ( distance - 1 ); counter++ )
   {
      exit = get_exit( line[counter], (find_first_step( line[counter], line[counter+1], 10 )) );
      if( !exit )
      {
         send_to_char( "The problem is here 2\r\n", ch );
         return FALSE;
      }
      if( IS_SET( exit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "The problem is here 3\r\n", ch );
         return FALSE;
      }
   }
   return TRUE;
}

ROOM_INDEX_DATA *next_room_on_line( CHAR_DATA *ch, int counter, int inc_x, int inc_y, int inc_z )
{
   ROOM_INDEX_DATA *room;
   int x, y, z;

   x = ch->in_room->coord[X] + (int)round(( counter * inc_x ));
   y = ch->in_room->coord[Y] + (int)round(( counter * inc_y ));
   z = ch->in_room->coord[Z] + (int)round(( counter * inc_z ));

   room = get_room_at_coord( ch, x, y, z );

   return room;
}
/*
 * Returns the distance from the coordinates in an amount of foot traveled distance
 * -Davenge
 */
int distance_from_dif( int dif_x, int dif_y, int dif_z )
{
   int distance = 0;

   distance += dif_x;
   if( dif_y != dif_x && dif_y != dif_z )
      distance += dif_y;
   if( dif_z != dif_x && dif_z != dif_y )
      distance += dif_z;

   return distance;
}

int coord_inc( int ch_coord, int vic_coord, int dif, int big_dif )
{
   int inc;

   inc = dif / big_dif;

   if( ch_coord > vic_coord )
      inc *= -1;

   return inc;
}
int coord_dif( int ch_coord, int vic_coord )
{
   int dif;
   dif = ch_coord - vic_coord;
   if( dif < 0 )
      dif *= -1;

   return dif;
}
/* Reverse a direction integer given to it's opposite, ie south = north, east = west
      -Davenge */

int reverse_dir( int dir )
{
   switch ( dir )
   {
      case 0:
      case 1:
         dir += 2;
         break;
      case 2:
      case 3:
         dir -= 2;
         break;
      case 4:
      case 7:
         dir += 1;
         break;
      case 5:
      case 8:
         dir -= 1;
         break;
      case 6:
         dir += 3;
         break;
      case 9:
         dir -= 3;
         break;
   }
   return dir;
}

/* Setting new targets -Davenge */

void set_new_target( CHAR_DATA *ch, TARGET_DATA *target, int type )
{
   if( !target )
   {
      bug( "%s: passed null target.", __FUNCTION__ );
      return;
   }

   switch( type )
   {
      case NORMAL_TARGET:
         if( !target->victim )
         {
            bug( "%s: somehow passed a NULL victim within target_data passed.", __FUNCTION__ );
            return;
         }
         if( ch->target )
            clear_target( ch, type );
         ch->target = target;
         LINK( ch, target->victim->first_targetedby, target->victim->last_targetedby, next_person_targetting_your_target, prev_person_targetting_your_target );
         break;

      case CHARGE_TARGET:
         if( !target->victim )
         {
            bug( "%s: somehow passed a NULL victim within charge target_data passed.", __FUNCTION__ );
            return;
         }
        if( ch->charge_target )
            free_target( ch, ch->charge_target );
          ch->charge_target = target;
         LINK( ch, target->victim->first_charge_targetedby, target->victim->last_charge_targetedby, next_person_charge_targetting_your_target, prev_person_charge_targetting_your_target );
         break;
   }
   return;
}

/*
 * Clear the characters target pointer
 */

void clear_target( CHAR_DATA *ch, int type )
{
   switch( type )
   {
      case NORMAL_TARGET:
         if( !ch->target )
         {
            bug( "%s: being called with ch having no target.", __FUNCTION__ );
            return;
         }
         if( !ch->target->victim )
         {
            bug( "%s: target being cleared has NULL victim.", __FUNCTION__ );
            free_target( ch, ch->target );
            return;
         }
         untarget( ch, ch->target->victim, type );
         free_target( ch, ch->target );
         break;
      case CHARGE_TARGET:
         if( !ch->charge_target )
         {
            bug( "%s: being called with ch having no charge_target.", __FUNCTION__ );
            return;
         }
         if( !ch->charge_target->victim )
         {
            bug( "%s: charge_target being cleared has NULL victim.", __FUNCTION__ );
            free_target( ch, ch->charge_target );
            return;
         }
         untarget( ch, ch->charge_target->victim, type );
         free_target( ch, ch->charge_target );
         break;
   }
   return;
}

/*
 * Free the data at said pointer
 * -Davenge
 */
void free_target( CHAR_DATA *ch, TARGET_DATA *target )
{
   if( ch->target == target )
      ch->target = NULL;

   if( ch->charge_target == target )
      ch->charge_target = NULL;

   target->victim = NULL;
   DISPOSE( target );
}

void untarget( CHAR_DATA *ch, CHAR_DATA *victim, int type )
{
   switch( type )
   {
      case NORMAL_TARGET:
         UNLINK( ch, victim->first_targetedby, victim->last_targetedby, next_person_targetting_your_target, prev_person_targetting_your_target );
         break;
      case CHARGE_TARGET:
         UNLINK( ch, victim->first_charge_targetedby, victim->last_charge_targetedby, next_person_charge_targetting_your_target, prev_person_charge_targetting_your_target );
         break;
   }
   return;
}

double get_skill_charge( CHAR_DATA *ch, int gsn )
{
   double charge = skill_table[gsn]->charge;

   if( is_affected( ch, gsn_augmentspell ) )
      return 1;

   return charge;
}

/* Return a skill's range -Davenge */

int get_skill_hits( CHAR_DATA *ch, int gsn )
{
   OBJ_DATA *obj;
   int hits;

   hits = skill_table[gsn]->hits;

   if( !IS_NPC( ch ) )
      hits += ch->pcdata->hits[gsn];

   if( skill_table[gsn]->type == SKILL_SKILL )
      if( ( obj = get_eq_char( ch, WEAR_DUAL_WIELD ) ) != NULL && obj->item_type == ITEM_WEAPON )
         hits++;

   return hits;
}

int get_skill_range( CHAR_DATA *ch, int gsn )
{
   OBJ_DATA *obj;
   int range = 0;

   if( skill_table[gsn]->range == 0 )
   {
      if( ( obj = get_eq_char( ch, WEAR_WIELD ) ) != NULL )
         range = obj->range;
   }
   else
      range = skill_table[gsn]->range;

   if( !IS_NPC( ch ) )
      range += ch->pcdata->range[gsn];

   return range;
}

bool is_skill( int dt )
{
   SKILLTYPE *skill = NULL;

   if( ( skill = get_skilltype( dt ) ) == NULL )
      return FALSE;

   return TRUE;
}

TARGET_DATA *make_new_target( CHAR_DATA *victim, int range, int dir )
{
   TARGET_DATA *target;

   if( victim == NULL )
   {
      bug( "Make new target called with bad victim" );
      return NULL;
   }
   CREATE( target, TARGET_DATA, 1 );
   target->victim = victim;
   target->range = range;
   target->dir = dir;
   return target;
}

REALM_DATA *get_realm( const char *argument )
{
   REALM_DATA *realm;

   if( argument[0] == '\0' )
      return NULL;

   for( realm = first_realm; realm; realm = realm->next )
      if( !strcmp( argument, realm->rfilename ) )
         return realm;
   return NULL;
}
AREA_DATA *get_area_file( const char *name )
{
   AREA_DATA *pArea;

   if( !name )
   {
      bug( "get_area: NULL input string." );
      return NULL;
   }

   for( pArea = first_area; pArea; pArea = pArea->next )
   {
      if( nifty_is_name( name, pArea->filename ) )
         break;
   }

   if( !pArea )
   {
      for( pArea = first_build; pArea; pArea = pArea->next )
      {
         if( nifty_is_name( name, pArea->filename ) )
            break;
      }
   }

   return pArea;
}

void update_target_ch_moved( CHAR_DATA *ch )
{
   CHAR_DATA *targeted_by;
   CHAR_DATA *victim;

   if( ch->target )
   {
      victim = ch->target->victim; // to be safe -Davenge
      set_new_target( ch, get_target_2( ch, victim, -1 ), NORMAL_TARGET );
   }

   if( ch->charge_target )
   {
      victim = ch->target->victim;
      set_new_target( ch, get_target_2( ch, victim, -1 ), CHARGE_TARGET );
   }

   if( ch->first_targetedby )
      for( targeted_by = ch->first_targetedby; targeted_by; targeted_by = targeted_by->next_person_targetting_your_target )
         set_new_target( targeted_by, get_target_2( targeted_by, ch, -1 ), NORMAL_TARGET );

   if( ch->first_charge_targetedby )
      for( targeted_by = ch->first_targetedby; targeted_by; targeted_by = targeted_by->next_person_charge_targetting_your_target )
          set_new_target( targeted_by, get_target_2( targeted_by, ch, -1 ), CHARGE_TARGET );
}

void add_queue( CHAR_DATA *ch, int type )
{
   QTIMER *queue;

   switch( type )
   {
      case COMBAT_LAG_TIMER:
         double lag;

         lag = base_class_lag[ch->Class] + ch->gravity;

         if( ch->combat_lag > 0 && is_queued( ch, COMBAT_LAG_TIMER ) ) //If we already have timer, just reset it -Davenge
         {
            ch->combat_lag = lag;
            return;
         }

         CREATE( queue, QTIMER, 1 );
         queue->timer_ch = ch;
         queue->type = type;

         ch->combat_lag = lag;

         LINK( queue, first_qtimer, last_qtimer, next, prev );
         break;
      case TIMER_TIMER:
      case COOLDOWN_TIMER:
      case AFFECT_TIMER:
         if( is_queued( ch, type) )
            break;
         CREATE( queue, QTIMER, 1 );
         queue->timer_ch = ch;
         queue->type = type;
         LINK( queue, first_qtimer, last_qtimer, next, prev );
         break;
      case COMBAT_ROUND:
         if( is_queued( ch, COMBAT_ROUND ) )
            break;

         if( ch->next_round <= 0 )
            ch->next_round = get_round( ch );

         CREATE( queue, QTIMER, 1 );
         queue->timer_ch = ch;
         queue->type = type;
         LINK( queue, first_qtimer, last_qtimer, next, prev );
         break;
   }
   return;
}

bool is_queued( CHAR_DATA *ch, int type )
{
   QTIMER *queue;

   for( queue = first_qtimer; queue; queue = queue->next )
      if( queue->timer_ch == ch && queue->type == type)
         return TRUE;
   return FALSE;
}

void extract_cooldown( CHAR_DATA * ch, CD_DATA * cdat )
{
   if( !cdat )
   {
      bug( "extrat_cooldown: NULL cdat" );
      return;
   }
   UNLINK( cdat, ch->first_cooldown, ch->last_cooldown, next, prev );
   DISPOSE( cdat->message );
   DISPOSE( cdat );
   return;
}

bool is_on_cooldown( CHAR_DATA *ch, int gsn )
{
   CD_DATA *cdat;

   if( !ch->first_cooldown )
      return FALSE;

   for( cdat = ch->first_cooldown; cdat; cdat = cdat->next )
   {
      if( gsn == cdat->sn )
      {
         ch_printf( ch, "%s is on cooldown for %d more seconds.\r\n", skill_table[gsn]->name, (int)cdat->time_remaining );
         return TRUE;
      }
   }
   return FALSE;
}

double get_skill_potency( CHAR_DATA *ch, int gsn )
{
   double potency;

   potency = ch->potency;

   if( !IS_NPC( ch ) )
      potency += ch->pcdata->potency[gsn];

   potency = ( potency / 100 ) + 1;

   if( is_affected( ch, gsn_potency ) )
      potency *= 2;

   return potency;
}

double get_skill_duration( CHAR_DATA *ch, int gsn )
{
   double duration;

   duration = skill_table[gsn]->duration + ch->durations;

   if( !IS_NPC( ch ) )
      duration += ch->pcdata->duration[gsn];

   if( is_affected( ch, gsn_extensioncombo ) )
      duration *= 2;

   return duration;;
}

double get_skill_cooldown( CHAR_DATA *ch, int gsn )
{
   double cooldown;

   cooldown = skill_table[gsn]->cooldown;

   if( !IS_NPC( ch ) )
      cooldown -= ch->pcdata->cooldown[gsn];

   cooldown *= ( 1 - ( get_haste( ch ) / 100 ) ) ;

   return cooldown;
}

void set_on_cooldown( CHAR_DATA *ch, int gsn )
{
   CD_DATA *cdat;
   double cooldown = get_skill_cooldown( ch, gsn );

   if( cooldown <= 0 )
      return;

   CREATE( cdat, CD_DATA, 1 );
   cdat->message = str_dup( skill_table[gsn]->cdmsg );
   cdat->sn = gsn;
   cdat->time_remaining = cooldown;
   LINK( cdat, ch->first_cooldown, ch->last_cooldown, next, prev );
   if( !is_queued( ch, COOLDOWN_TIMER ) )
      add_queue( ch, COOLDOWN_TIMER );
   return;
}

int weight_ratio_str( int str, int weight )
{
   int x;

   x = weight / ( str / 10 ) ;

   return URANGE( 1, x, 10 );
}
int weight_ratio_dex( int dex, int weight )
{
   int x;

   x = dex / abs(weight);

   return URANGE( 1, x, 10 );
}
HIT_DATA *init_hitdata( void )
{
   HIT_DATA *hit_data;

   CREATE( hit_data, HIT_DATA, 1 );
   hit_data->max_locations = 10;
   hit_data->hit_locs = 9;
   hit_data->miss_locs = 1;
   hit_data->locations[0] = HIT_HEAD;
   hit_data->locations[1] = HIT_BODY;
   hit_data->locations[2] = HIT_BODY;
   hit_data->locations[3] = HIT_WAIST;
   hit_data->locations[4] = HIT_WAIST;
   hit_data->locations[5] = HIT_ARMS;
   hit_data->locations[6] = HIT_HANDS;
   hit_data->locations[7] = HIT_LEGS;
   hit_data->locations[8] = HIT_FEET;
   hit_data->locations[9] = MISS_GENERAL;

   return hit_data;
}

HIT_DATA *generate_hit_data( CHAR_DATA *ch, CHAR_DATA *victim )
{
   OBJ_DATA *obj;
   HIT_DATA *hit_data;
   int str, dex, amount, counter, weight;

   str = get_curr_str( victim );
   dex = get_curr_dex( victim );

   hit_data = init_hitdata( );

   for( obj = victim->first_carrying; obj; obj = obj->next_content )
   {
      if( ( obj->wear_loc >= WEAR_BODY && obj->wear_loc <= WEAR_ARMS ) || obj->wear_loc == WEAR_WAIST )
      {
         weight = ( obj->weight + body_part_weight[obj->wear_loc] );
         if( obj->weight > 0 )
         {
            amount = weight_ratio_str( str, weight );
            for( counter = 0; counter < amount; counter++ )
            {
               hit_data->locations[hit_data->max_locations] = obj->wear_loc;
               hit_data->hit_locs++;
               hit_data->max_locations++;
            }
         }
         else if( obj->weight < 0 )
         {
            amount = weight_ratio_dex( dex, weight );
            for( counter = 0; counter < amount; counter++ )
            {
               hit_data->locations[hit_data->max_locations] = ( obj->wear_loc * -1);
               hit_data->miss_locs++;
               hit_data->max_locations++;
            }
         }
         if( IS_AFFECTED( ch, AFF_BLINDRUSH ) )
         {
            amount = hit_data->max_locations / 2;
            for( counter = 0; counter < amount; counter ++ )
            {
               hit_data->locations[hit_data->max_locations] = MISS_GENERAL;
               hit_data->miss_locs++;
               hit_data->max_locations++;
            }
         }
      }
   }
   return hit_data;
}

bool is_physical( EXT_BV *damtype )
{
   if( xIS_SET( *damtype, DAM_PIERCE ) || xIS_SET( *damtype, DAM_SLASH ) || xIS_SET( *damtype, DAM_BLUNT ) )
      return TRUE;
   return FALSE;
}

bool is_magical( EXT_BV *damtype )
{
   if( xIS_SET( *damtype, DAM_WIND ) || xIS_SET( *damtype, DAM_EARTH ) || xIS_SET( *damtype, DAM_FIRE ) || xIS_SET( *damtype, DAM_ICE )
      || xIS_SET( *damtype, DAM_WATER ) || xIS_SET( *damtype, DAM_LIGHTNING ) || xIS_SET( *damtype, DAM_LIGHT ) || xIS_SET( *damtype, DAM_DARK ) )
      return TRUE;
   return FALSE;
}

void do_beta( CHAR_DATA *ch, const char *argument )
{
   char arg[MAX_STRING_LENGTH];
   if( sysdata.beta )
   {
      sprintf( arg, "%s has taken the mud out of beta mode.\r\n", ch->name );
      sysdata.beta = FALSE;
   }
   else
   {
      sprintf( arg, "%s has put the mud in beta mode.\r\n", ch->name );
      sysdata.beta = TRUE;
   }
   do_echo( ch, arg );
   return;
}

int get_haste( CHAR_DATA *ch )
{
   int haste;
   int haste_from_magic;

   haste = UMIN( ch->haste, 20 );
   haste_from_magic = UMIN( ch->haste_from_magic, 30 );

   return ( haste + haste_from_magic );
}

double get_round( CHAR_DATA *ch )
{
   OBJ_DATA *obj;
   int counter;
   double round;

   round = 1.25;

   if( ( obj = get_eq_char( ch, WEAR_WIELD ) ) == NULL || obj->item_type != ITEM_WEAPON )
      return ( round * ( 1 - ( get_haste( ch ) / 10 ) ) );

   for( counter = 0; counter < obj->weight; counter++ )
   {
      if( counter <= 10 )
         round += .05;
      if( counter > 10 && counter <= 20 )
         round += .09;
      if( counter > 20 && counter <= 40 )
         round += .13;
      if( counter > 40 && counter <= 70 )
         round += .17;
      if( counter > 70 )
         round += .25;
   };

   round *= 1 + ( get_haste( ch ) / 100 );

   if( IS_AFFECTED( ch, AFF_STRONGBLOWS ) )
      round *= 1.5;

   return round;
}

void switch_class( CHAR_DATA *ch, int Class )
{
   OBJ_DATA *obj;
   int counter;

   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->wear_loc > -1 && obj->wear_loc < MAX_WEAR )
         unequip_char( ch, obj );

   ch->level = 1;
   ch->Class = Class;

   reset_stats( ch );

   for( counter = 1; counter < ch->class_data[Class]->level; counter++ )
   {
      ch->level++;
      advance_level( ch, TRUE );
   }
   return;
}

void apply_class_stats( CHAR_DATA *ch )
{
   ch->mod_str += ch->class_data[ch->Class]->stat[STAT_STR];
   ch->mod_dex += ch->class_data[ch->Class]->stat[STAT_DEX];
   ch->mod_con += ch->class_data[ch->Class]->stat[STAT_CON];
   ch->mod_int += ch->class_data[ch->Class]->stat[STAT_INT];
   ch->mod_wis += ch->class_data[ch->Class]->stat[STAT_WIS];
   ch->mod_pas += ch->class_data[ch->Class]->stat[STAT_PAS];
   return;
}


int get_class_num( const char *argument )
{
   int x;
   for( x = 0; x < MAX_CLASS; x++ )
   {
      if( nifty_is_name( class_table[x]->who_name, argument ) )
         return x;
   }
   return -1;
}

int get_questtype_num( const char *argument )
{
   int x;
   for( x = 0; x < MAX_QUEST_TYPE; x++ )
   {
      if( !str_cmp( quest_types[x], argument ) )
         return x;
   }
   return -1;
}

int get_triggertype_num( const char *argument )
{
   int x;
   for( x = 0; x < MAX_TRIGGER_TYPE; x++ )
   {
      if( !str_cmp( trigger_types[x], argument ) )
         return x;
   }
   return -1;
}

bool is_init_mob( CHAR_DATA *ch, CHAR_DATA *mob, QUEST_DATA *quest )
{
   if( mob->pIndexData == quest->init_mob && can_accept_quest( ch, quest ) )
      return TRUE;
   else
      return FALSE;
}

bool is_init_mob( CHAR_DATA *mob )
{
   QUEST_DATA *quest;

   for( quest = first_quest; quest; quest = quest->next )
      if( mob->pIndexData == quest->init_mob )
         return TRUE;
   return FALSE;
}

/*
bool involved_in_quest( CHAR_DATA *mob, QUEST_DATA *quest )
{

}

bool involved_in_quest( OBJ_DATA *obj, QUEST_DATA *quest )
{

}
*/
bool can_accept_quest( CHAR_DATA *ch, QUEST_DATA *quest )
{
   PREREQ_DATA *prereq;
   PLAYER_QUEST *pquest;
   bool can_accept = FALSE;

   if( quest->level_required[ch->Class] == 0 )
      return FALSE;

   if( ch->level >= quest->level_required[ch->Class] )
      can_accept = TRUE;

   /* If player is currently on quest */
   if( ( pquest = player_has_quest( ch, quest ) ) != NULL && pquest->stage > 0 )
      can_accept = FALSE;
   /* If player has completed it for one_time or once_per_class one time */
   else if( has_completed_quest( ch, quest ) )
      can_accept = FALSE;

   /* If the quest is repeatable... */
   if( has_completed_quest( ch, quest ) && quest->type == QUEST_REPEATABLE )
      can_accept = TRUE;

   for( prereq = quest->first_prereq; prereq; prereq = prereq->next )
   {
      if( !has_completed_quest( ch, prereq->prereq ) )
      {
         can_accept = FALSE;
         break;
      }
   }

   return can_accept;
}

bool has_completed_quest( CHAR_DATA *ch, QUEST_DATA *quest )
{
   PLAYER_QUEST *pquest;

   if( ( pquest = player_has_quest( ch, quest ) ) == NULL )
      return FALSE;

   if( ( quest->type == QUEST_ONE_TIME || quest->type == QUEST_REPEATABLE ) && pquest->stage == QUEST_COMPLETE )
      return TRUE;
   else if( quest->type == QUEST_ONCE_PER_CLASS && pquest->times_completed[ch->Class] > 0 )
         return TRUE;
   
   return FALSE;
}

void init_quest( CHAR_DATA *ch, QUEST_DATA *quest )
{
   PLAYER_QUEST *pquest;
   int x;

   if( ( pquest = player_has_quest( ch, quest ) ) == NULL )
   {
      CREATE( pquest, PLAYER_QUEST, 1 );
      pquest->quest = quest;
      pquest->stage = 1;
      pquest->on_path = quest->first_path;
      for( x = 0; x < MAX_CLASS; x++ )
         pquest->times_completed[x] = 0;
      LINK( pquest, ch->first_quest, ch->last_quest, next, prev );
   }
   pquest->stage = 1;
   pquest->on_path = quest->first_path;
   advance_quest( ch, pquest );
   return;
}

PLAYER_QUEST *player_has_quest( CHAR_DATA *ch, QUEST_DATA *quest )
{
   PLAYER_QUEST *pquest;

   for( pquest = ch->first_quest; pquest; pquest = pquest->next )
      if( pquest->quest == quest )
         return pquest;
   return NULL;
}

PATH_DATA *get_path( QUEST_DATA *quest, const char *argument )
{
   PATH_DATA *path;

   for( path = quest->first_path; path; path = path->next )
   {
      if( !str_cmp( path->name, argument ) )
         return path;
   }
   return NULL;
}

STAGE_DATA *get_stage( QUEST_DATA *quest, int num )
{
   STAGE_DATA *stage;
   int count = 0;

   if( num < 2 )
      return quest->first_stage;
   else
   {
      for( stage = quest->first_stage; stage; stage = stage->next )
      {
         if( ++count == num )
            return stage;
      }
   }
   return NULL;
}

TRIGGER_DATA *get_trigger( STAGE_DATA *stage, int num )
{
   TRIGGER_DATA *trigger;
   int count = 0;

   if( num < 2 )
      return stage->first_trigger;
   else
   {
      for( trigger = stage->first_trigger; trigger; trigger = trigger->next )
      {
         if( ++count == num )
            return trigger;
      }
   }
   return NULL;
}

OBJECTIVE_TRACKER *get_otracker( PLAYER_QUEST *pquest, int num )
{
   OBJECTIVE_TRACKER *objective;
   int count = 0;

   if( num < 2 )
      return pquest->first_objective_tracker;
   else
   {
      for( objective = pquest->first_objective_tracker; objective; objective = objective->next )
      {
         if( ++count == num )
            return objective;
      }
   }
   return NULL;
}

void create_trackers( PLAYER_QUEST *pquest, STAGE_DATA *stage )
{
   TRIGGER_DATA *trigger;
   OBJECTIVE_TRACKER *objective;

   log_string( "creating trackers" );

   for( trigger = stage->first_trigger; trigger; trigger = trigger->next )
   {
      CREATE( objective, OBJECTIVE_TRACKER, 1 );
      objective->objective = trigger;
      LINK( objective, pquest->first_objective_tracker, pquest->last_objective_tracker, next, prev );
      objective = NULL;
   }
   return;
}

void clear_trackers( PLAYER_QUEST *pquest )
{
   OBJECTIVE_TRACKER *objective, *objective_next;
   for( objective = pquest->first_objective_tracker; objective; objective = objective_next )
   {
      objective_next = objective->next;
      UNLINK( objective, pquest->first_objective_tracker, pquest->last_objective_tracker, next, prev );
      free_otracker( objective );
   }
   return;
}

void free_otracker( OBJECTIVE_TRACKER *objective )
{
   objective->objective = NULL;
   DISPOSE( objective );
   return;
}

int store_two_value( int v1, int v2 )
{
   int value = v1;

   value *= 10000;
   value += v2;

   return value;
}

int get_value_one( int value )
{
   return abs( value / 10000 );
}

int get_value_two( int value )
{
   return ( value % 10000 );
}

int get_available_stat_points( CHAR_DATA *ch )
{
   int total;
   int count;

   total = ch->level / 2;

   for( count = 0; count < MAX_STAT; count++ )
      total -= ch->class_data[ch->Class]->stat[count];

   return total;
}

int get_spent_stat_points( CHAR_DATA *ch )
{
   int spent;
   int count;

   spent = 0;

   for( count = 0; count < MAX_STAT; count++ )
      spent += ch->class_data[ch->Class]->stat[count];

   return spent;
}

void display_statallocation( CHAR_DATA * ch )
{
   int available_points, spent_points;
   int x;

   available_points = get_available_stat_points( ch );
   spent_points = get_spent_stat_points( ch );


   ch_printf( ch, "Avaialble Stat Points: %-2.2d Stat Points Spent: %-2.2d\r\n",  available_points, spent_points );
   send_to_char( "You have spent points in...:\r\n", ch );
   send_to_char( "-------------------------------------\r\n|", ch );
   for( x = 0; x < MAX_STAT; x++ )
      ch_printf( ch, " %-3.3s |", short_stat_names[x] );
   send_to_char( "\r\n-------------------------------------\r\n|", ch );
   for( x = 0; x < MAX_STAT; x++ )
      ch_printf( ch, " %3.3d |", ch->class_data[ch->Class]->stat[x] );
   send_to_char( "\r\n-------------------------------------\r\n", ch );
   return;
}

void reset_stats( CHAR_DATA *ch )
{
   AFFECT_DATA *paf, *paf_next;

   /* Remove all Affects */

   for( paf = ch->first_affect; paf; paf = paf_next )
   {
      paf_next = paf->next;
      affect_modify( ch, paf, FALSE );
      UNLINK( paf, ch->first_affect, ch->last_affect, next, prev );
   }

   /* Remove Quest Affects */
   for( paf = ch->class_data[ch->Class]->first_quest_affect; paf; paf = paf->next )
      affect_modify( ch, paf, FALSE );

   /* Re-Add any room affets */

   for( paf = ch->in_room->first_affect; paf; paf = paf->next )
   {
      affect_modify( ch, paf, TRUE );
      LINK( paf, ch->first_affect, ch->last_affect, next, prev );
   }

   ch->max_hit = base_hp[ch->Class];
   ch->max_mana = base_mana[ch->Class];
   ch->max_move= base_move[ch->Class];
   ch->hit = ch->max_hit;
   ch->mana = ch->max_mana;
   ch->move = ch->max_move;

   apply_class_base_stat_mod( ch );
   apply_class_stats( ch );

   /* Add Quest Affects from Quests done on the new class */
   for( paf = ch->class_data[ch->Class]->first_quest_affect; paf; paf = paf->next )
      affect_modify( ch, paf, TRUE );
   return;
}

int get_stat_num_from_short_name( const char *argument )
{
   int x;

   for( x = 0; x < MAX_STAT; x++ )
      if( !str_cmp( strlower( argument ), basic_short_stat_names[x] ) )
         return x;

   return -1;
}

void clear_stat_array( CHAR_DATA *ch )
{
   int x;

   for( x = 0; x < MAX_STAT; x++ )
      ch->class_data[ch->Class]->stat[x] = 0;

   return;
}

void adjust_stat( CHAR_DATA *ch, int type, int amount )
{
   int v1, v2;

   switch( type )
   {
      default:
         bug( "%s: Invalid type passed: %d", __FUNCTION__, type );
      case STAT_HIT:
         ch->hit += amount;
         break;
      case STAT_MAXHIT:
         ch->max_hit += amount;
         break;
      case STAT_MANA:
         ch->mana += amount;
         break;
      case STAT_MAXMANA:
         ch->max_mana += amount;
         break;
      case STAT_MOVE:
         ch->move += amount;
         break;
      case STAT_MAXMOVE:
         ch->max_move += amount;
         break;
      case STAT_ALIGN:
         ch->alignment += amount;
         break;
      case STAT_BARENUMDIE:
         ch->barenumdie += amount;
         break;
      case STAT_BARESIZEDIE:
         ch->baresizedie += amount;
         break;
      case STAT_ATTACK:
         ch->attack += amount;
         break;
      case STAT_MAGICATTACK:
         ch->magic_attack += amount;
         break;
      case STAT_DEFENSE:
         ch->armor += amount;
         break;
      case STAT_MAGICDEFENSE:
         ch->magic_defense += amount;
         break;
      case STAT_HASTE:
         ch->haste += amount;
         break;
      case STAT_HASTEFROMMAGIC:
         ch->haste_from_magic += amount;
         break;
      case STAT_THREAT:
         ch->threat += amount;
         break;
      case STAT_PERMSTR:
         ch->perm_str += amount;
         break;
      case STAT_PERMDEX:
         ch->perm_dex += amount;
         break;
      case STAT_PERMCON:
         ch->perm_con += amount;
         break;
      case STAT_PERMINT:
         ch->perm_int += amount;
         break;
      case STAT_PERMWIS:
         ch->perm_wis += amount;
         break;
      case STAT_PERMPAS:
         ch->perm_pas += amount;
         break;
      case STAT_STRENGTH:
         ch->mod_str += amount;
         break;
      case STAT_DEXTERITY:
         ch->mod_dex += amount;
         break;
      case STAT_CONSTITUTION:
         ch->mod_con += amount;
         break;
      case STAT_INTELLIGENCE:
         ch->mod_int += amount;
         break;
      case STAT_WISDOM:
         ch->mod_wis += amount;
         break;
      case STAT_PASSION:
         ch->mod_pas += amount;
         break;
      case STAT_RESISTANCE:
         v1 = get_value_one( amount );
         v2 = get_value_two( amount );
         ch->resistance[v1] += v2;
         break;
      case STAT_PENETRATION:
         v1 = get_value_one( amount );
         v2 = get_value_two( amount );
         ch->penetration[v1] += v2;
         break;
      case STAT_DTYPEPOTENCY:
         v1 = get_value_one( amount );
         v2 = get_value_two( amount );
         ch->damtype_potency[v1] += v2;
         break;
      case STAT_WEPNUMDIE:
         ch->wepnumdie += amount;
         break;
      case STAT_WEPSIZEDIE:
         ch->wepsizedie += amount;
         break;
      case STAT_POTENCY:
         ch->potency += amount;
         break;
      case STAT_COOLDOWNS:
         ch->cooldowns += amount;
         break;
      case STAT_RANGE:
         ch->range += amount;
         break;
      case STAT_DURATIONS:
         ch->durations += amount;
         break;
      case STAT_REGEN:
         ch->regen += amount;
         break;
      case STAT_REFRESH:
         ch->refresh += amount;
         break;
      case STAT_DOUBLEATTACK:
         ch->double_attack += amount;
         break;
      case STAT_CRITCHANCE:
         ch->crit_chance += amount;
         break;
      case STAT_CRITDAM:
         ch->crit_dam += amount;
         break;
      case STAT_DODGE:
         ch->dodge += amount;
         break;
      case STAT_PARRY:
         ch->parry += amount;
         break;
      case STAT_COUNTER:
         ch->counter += amount;
         break;
      case STAT_BLOCK:
         ch->counter += amount;
         break;
      case STAT_PHASE:
         ch->phase += amount;
         break;
      case STAT_COMBODMG:
         ch->combo_dmg += amount;
         break;
      case STAT_CHARMEDDMG:
         ch->charmed_dmg += amount;
         break;
      case STAT_CHARMEDDEF:
         ch->charmed_def += amount;
         break;
      case STAT_FEEDBACKPOTENCY:
         ch->feedback_potency += amount;
         break;
      case STAT_GRAVITY:
         ch->gravity += amount;
         break;
   }
}

int check_mana( CHAR_DATA *ch, int sn )
{
   int mana, increase, x;

   if( ( mana = skill_table[sn]->min_mana ) > 0 )
   {
      for( x = 0; x < ch->level; x++ )
      {
         increase = (int)( mana *.05 );
         mana += increase < 1 ? 1 : increase;
      }
   }
   return mana;
}

int check_move( CHAR_DATA *ch, int sn )
{
   int move, increase, x;

   if( ( move = skill_table[sn]->min_move ) > 0 )
   {
      for( x = 0; x < ch->level; x++ )
      {
         increase = (int)( move *.05 );
         move += increase < 1 ? 1: increase;
      }
   }
   return move;
}

int get_threat( CHAR_DATA *ch, int gsn )
{
   return skill_table[gsn]->threat * ch->level;
}

void interrupt( CHAR_DATA *ch ) 
{
   TIMER *timer;

   timer = get_timerptr( ch, TIMER_DO_FUN );
   if( timer )
   {
      int tempsub;

      tempsub = ch->substate;
      ch->substate = SUB_TIMER_DO_ABORT;
      ( timer->do_fun ) ( ch, "" );
      if( char_died( ch ) )
         return;
      if( ch->substate != SUB_TIMER_CANT_ABORT )
      {
         ch->substate = tempsub;
         extract_timer( ch, timer );
      }
      else
      {
         ch->substate = tempsub;
         return;
      }
   }
}
