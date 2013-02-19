/***************************************************************************
 *                                 _/                            _/        *
 *      _/_/_/  _/_/      _/_/_/  _/    _/_/    _/    _/    _/_/_/         *
 *     _/    _/    _/  _/        _/  _/    _/  _/    _/  _/    _/          *
 *    _/    _/    _/  _/        _/  _/    _/  _/    _/  _/    _/           *
 *   _/    _/    _/    _/_/_/  _/    _/_/      _/_/_/    _/_/_/            *
 ***************************************************************************
 * Mindcloud Copyright 2001-2003 by Jeff Boschee (Zarius),                 *
 * Additional credits are in the help file CODECREDITS                     *
 * All Rights Reserved.                                                    *
 ***************************************************************************/
/*--------------------------------------------------------------------------
 *  Changes snippet by: Xkilla. This is a snippet, you may modify it, but  *
 *  you must leave in Xkilla's credit.                                     *
 ***************************************************************************/

#define CHANGES_FILE    "../doc/changes.dat"

typedef struct  changes_data             CHANGE_DATA;

void load_changes args( (void) );
void save_changes args( (void) );
void delete_change args( (int num) );
 
struct changes_data
{
    const char *         change;
    const char *         coder;
    const char *         date;
    const char *         type;
    time_t         mudtime;
};

extern struct  changes_data * changes_table;
