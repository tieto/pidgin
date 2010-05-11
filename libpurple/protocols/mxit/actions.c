/*
 *					MXit Protocol libPurple Plugin
 *
 *					-- handle MXit plugin actions --
 *
 *				Pieter Loubser	<libpurple@mxit.com>
 *
 *			(C) Copyright 2009	MXit Lifestyle (Pty) Ltd.
 *				<http://www.mxitlifestyle.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02111-1301  USA
 */

#include    "internal.h"
#include	"purple.h"

#include	"protocol.h"
#include	"mxit.h"
#include	"roster.h"
#include	"actions.h"
#include	"splashscreen.h"
#include	"cipher.h"
#include	"profile.h"


/*------------------------------------------------------------------------
 * The user has selected to change their profile.
 *
 *  @param gc		The connection object
 *  @param fields	The fields from the request pop-up
 */
static void mxit_cb_set_profile( PurpleConnection* gc, PurpleRequestFields* fields )
{
	struct MXitSession*		session	= (struct MXitSession*) gc->proto_data;
	PurpleRequestField*		field	= NULL;
	const char*				pin		= NULL;
	const char*				pin2	= NULL;
	const char*				name	= NULL;
	const char*				bday	= NULL;
	const char*				err		= NULL;
	int						len;
	int						i;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_cb_set_profile\n" );

	if ( !PURPLE_CONNECTION_IS_VALID( gc ) ) {
		purple_debug_error( MXIT_PLUGIN_ID, "Unable to update profile; account offline.\n" );
		return;
	}

	/* validate pin */
	pin = purple_request_fields_get_string( fields, "pin" );
	if ( !pin ) {
		err = _( "The PIN you entered is invalid." );
		goto out;
	}
	len = strlen( pin );
	if ( ( len < 4 ) || ( len > 10 ) ) {
		err = _( "The PIN you entered has an invalid length [4-10]." );
		goto out;
	}
	for ( i = 0; i < len; i++ ) {
		if ( !g_ascii_isdigit( pin[i] ) ) {
			err = _( "The PIN is invalid. It should only consist of digits [0-9]." );
			goto out;
		}
	}
	pin2 = purple_request_fields_get_string( fields, "pin2" );
	if ( ( !pin2 ) || ( strcmp( pin, pin2 ) != 0 ) ) {
		err = _( "The two PINs you entered do not match." );
		goto out;
	}

	/* validate name */
	name = purple_request_fields_get_string( fields, "name" );
	if ( ( !name ) || ( strlen( name ) < 3 ) ) {
		err = _( "The name you entered is invalid." );
		goto out;
	}

	/* validate birthdate */
	bday = purple_request_fields_get_string( fields, "bday" );
	if ( ( !bday ) || ( strlen( bday ) < 10 ) || ( !validateDate( bday ) ) ) {
		err = _( "The birthday you entered is invalid. The correct format is: 'YYYY-MM-DD'." );
		goto out;
	}

out:
	if ( !err ) {
		struct MXitProfile*	profile		= session->profile;
		GString*			attributes	= g_string_sized_new( 128 );
		char				attrib[512];
		unsigned int		acount		= 0;

		/* all good, so we can now update the profile */

		/* update pin */
		purple_account_set_password( session->acc, pin );
		g_free( session->encpwd );
		session->encpwd = mxit_encrypt_password( session );

		/* update name */
		g_strlcpy( profile->nickname, name, sizeof( profile->nickname ) );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_FULLNAME, CP_PROF_TYPE_UTF8, profile->nickname );
		g_string_append( attributes, attrib );
		acount++;

		/* update hidden */
		field = purple_request_fields_get_field( fields, "hidden" );
		profile->hidden = purple_request_field_bool_get_value( field );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_HIDENUMBER, CP_PROF_TYPE_BOOL, ( profile->hidden ) ? "1" : "0" );
		g_string_append( attributes, attrib );
		acount++;

		/* update birthday */
		strcpy( profile->birthday, bday );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_BIRTHDATE, CP_PROF_TYPE_UTF8, profile->birthday );
		g_string_append( attributes, attrib );
		acount++;

		/* update gender */
		profile->male = ( purple_request_fields_get_choice( fields, "male" ) != 0 );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_GENDER, CP_PROF_TYPE_BOOL, ( profile->male ) ? "1" : "0" );
		g_string_append( attributes, attrib );
		acount++;

		/* update title */
		name = purple_request_fields_get_string( fields, "title" );
		if ( !name )
			profile->title[0] = '\0';
		else
			strcpy( profile->title, name );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_TITLE, CP_PROF_TYPE_UTF8, profile->title );
		g_string_append( attributes, attrib );
		acount++;

		/* update firstname */
		name = purple_request_fields_get_string( fields, "firstname" );
		if ( !name )
			profile->firstname[0] = '\0';
		else
			strcpy( profile->firstname, name );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_FIRSTNAME, CP_PROF_TYPE_UTF8, profile->firstname );
		g_string_append( attributes, attrib );
		acount++;

		/* update lastname */
		name = purple_request_fields_get_string( fields, "lastname" );
		if ( !name )
			profile->lastname[0] = '\0';
		else
			strcpy( profile->lastname, name );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_LASTNAME, CP_PROF_TYPE_UTF8, profile->lastname );
		g_string_append( attributes, attrib );
		acount++;

		/* update email address */
		name = purple_request_fields_get_string( fields, "email" );
		if ( !name )
			profile->email[0] = '\0';
		else
			strcpy( profile->email, name );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_EMAIL, CP_PROF_TYPE_UTF8, profile->email );
		g_string_append( attributes, attrib );
		acount++;

		/* update mobile number */
		name = purple_request_fields_get_string( fields, "mobilenumber" );
		if ( !name )
			profile->mobilenr[0] = '\0';
		else
			strcpy( profile->mobilenr, name );
		g_snprintf( attrib, sizeof( attrib ), "\01%s\01%i\01%s", CP_PROFILE_MOBILENR, CP_PROF_TYPE_UTF8, profile->mobilenr );
		g_string_append( attributes, attrib );
		acount++;

		/* send the profile update to MXit */
		mxit_send_extprofile_update( session, session->encpwd, acount, attributes->str );
		g_string_free( attributes, TRUE );
	}
	else {
		/* show error to user */
		mxit_popup( PURPLE_NOTIFY_MSG_ERROR, _( "Profile Update Error" ), err );
	}
}


/*------------------------------------------------------------------------
 * Display and update the user's profile.
 *
 *  @param action	The action object
 */
static void mxit_cb_action_profile( PurplePluginAction* action )
{
	PurpleConnection*			gc		= (PurpleConnection*) action->context;
	struct MXitSession*			session	= (struct MXitSession*) gc->proto_data;
	struct MXitProfile*			profile	= session->profile;

	PurpleRequestFields*		fields	= NULL;
	PurpleRequestFieldGroup*	group	= NULL;
	PurpleRequestField*			field	= NULL;

	purple_debug_info( MXIT_PLUGIN_ID, "mxit_cb_action_profile\n" );

	/* ensure that we actually have the user's profile information */
	if ( !profile ) {
		/* no profile information yet, so we cannot update */
		mxit_popup( PURPLE_NOTIFY_MSG_WARNING, _( "Profile" ), _( "Your profile information is not yet retrieved. Please try again later." ) );
		return;
	}

	fields = purple_request_fields_new();
	group = purple_request_field_group_new( NULL );
	purple_request_fields_add_group( fields, group );

	/* pin */
	field = purple_request_field_string_new( "pin", _( "PIN" ), session->acc->password, FALSE );
	purple_request_field_string_set_masked( field, TRUE );
	purple_request_field_group_add_field( group, field );
	field = purple_request_field_string_new( "pin2", _( "Verify PIN" ), session->acc->password, FALSE );
	purple_request_field_string_set_masked( field, TRUE );
	purple_request_field_group_add_field( group, field );

	/* display name */
	field = purple_request_field_string_new( "name", _( "Display Name" ), profile->nickname, FALSE );
	purple_request_field_group_add_field( group, field );

	/* birthday */
	field = purple_request_field_string_new( "bday", _( "Birthday" ), profile->birthday, FALSE );
	purple_request_field_group_add_field( group, field );

	/* gender */
	field = purple_request_field_choice_new( "male", _( "Gender" ), ( profile->male ) ? 1 : 0 );
	purple_request_field_choice_add( field, _( "Female" ) );		/* 0 */
	purple_request_field_choice_add( field, _( "Male" ) );			/* 1 */
	purple_request_field_group_add_field( group, field );

	/* hidden */
	field = purple_request_field_bool_new( "hidden", _( "Hide my number" ), profile->hidden );
	purple_request_field_group_add_field( group, field );

	/* title */
	field = purple_request_field_string_new( "title", _( "Job Title" ), profile->title, FALSE );
	purple_request_field_group_add_field( group, field );

	/* first name */
	field = purple_request_field_string_new( "firstname", _( "First Name" ), profile->firstname, FALSE );
	purple_request_field_group_add_field( group, field );

	/* last name */
	field = purple_request_field_string_new( "lastname", _( "Last Name" ), profile->lastname, FALSE );
	purple_request_field_group_add_field( group, field );

	/* email */
	field = purple_request_field_string_new( "email", _( "Email" ), profile->email, FALSE );
	purple_request_field_group_add_field( group, field );

	/* mobile number */
	field = purple_request_field_string_new( "mobilenumber", _( "Mobile Number" ), profile->mobilenr, FALSE );
	purple_request_field_group_add_field( group, field );

	/* (reference: "libpurple/request.h") */
	purple_request_fields( gc, _( "Profile" ), _( "Update your Profile" ), _( "Here you can update your MXit profile" ), fields, _( "Set" ),
			G_CALLBACK( mxit_cb_set_profile ), _( "Cancel" ), NULL, purple_connection_get_account( gc ), NULL, NULL, gc );
}


/*------------------------------------------------------------------------
 * Display the current splash-screen, or a notification pop-up if one is not available.
 *
 *  @param action	The action object
 */
static void mxit_cb_action_splash( PurplePluginAction* action )
{
	PurpleConnection*		gc		= (PurpleConnection*) action->context;
	struct MXitSession*		session	= (struct MXitSession*) gc->proto_data;

	if ( splash_current( session ) != NULL )
		splash_display( session );
	else
		mxit_popup( PURPLE_NOTIFY_MSG_INFO, _( "View Splash" ), _( "There is no splash-screen currently available" ) );
}


/*------------------------------------------------------------------------
 * Display info about the plugin.
 *
 *  @param action	The action object
 */
static void mxit_cb_action_about( PurplePluginAction* action )
{
	char	version[256];

	g_snprintf( version, sizeof( version ), "MXit libPurple Plugin v%s\n"
											"MXit Client Protocol v%s\n\n"
											"Author:\nPieter Loubser\n\n"
											"Contributors:\nAndrew Victor\n\n"
											"Testers:\nBraeme Le Roux\n\n",
											MXIT_PLUGIN_VERSION, MXIT_CP_RELEASE );

	mxit_popup( PURPLE_NOTIFY_MSG_INFO, _( "About" ), version );
}


/*------------------------------------------------------------------------
 * Associate actions with the MXit plugin.
 *
 *  @param plugin	The MXit protocol plugin
 *  @param context	The connection context (if available)
 *  @return			The list of plugin actions
 */
GList* mxit_actions( PurplePlugin* plugin, gpointer context )
{
	PurplePluginAction*		action	= NULL;
	GList*					m		= NULL;

	/* display / change profile */
	action = purple_plugin_action_new( _( "Change Profile..." ), mxit_cb_action_profile );
	m = g_list_append( m, action );

	/* display splash-screen */
	action = purple_plugin_action_new( _( "View Splash..." ), mxit_cb_action_splash );
	m = g_list_append( m, action );

	/* display plugin version */
	action = purple_plugin_action_new( _( "About..." ), mxit_cb_action_about );
	m = g_list_append( m, action );

	return m;
}

