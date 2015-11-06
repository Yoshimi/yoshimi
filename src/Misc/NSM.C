
/*******************************************************************************/
/* Copyright (C) 2012 Jonathan Moore Liles                                     */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/


#include "NSM.H"

#include "Config.h"
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "MasterUI.h"

extern int Pexitprogram;

extern Config Runtime;

extern NSM_Client *nsm;
extern char *instance_name;

extern bool music_is_active;
extern int start_music ( void );
extern int stop_music ( void );

NSM_Client::NSM_Client( SynthEngine* synth_, MasterUI* guiMaster_ )
{
    synth = synth_;
    guiMaster = guiMaster_;

    project_filename = 0;
    display_name = 0;
}

int command_open ( const char *name, const char *display_name, const char *client_id, char **out_msg );
int command_save ( char **out_msg );

int
NSM_Client::command_save ( char **out_msg )
{
    int r = ERR_OK;

    if ( project_filename )
        guiMaster->do_save_master_unconditional( project_filename );

    return r;
}

int
NSM_Client::command_open ( const char *name, const char *display_name, const char *client_id, char **out_msg )
{
    if ( instance_name )
        free( instance_name );
    
    instance_name = strdup( client_id );

    char *new_filename;
    
    asprintf( &new_filename, "%s.xmz", name );

    int r = ERR_OK;

    if ( project_filename )
        free( project_filename );

    if ( this->display_name )
        free( this->display_name );
        
    project_filename = new_filename;

    this->display_name = strdup( display_name );

    return r;
}

void
NSM_Client::command_active ( bool active )
{

}
