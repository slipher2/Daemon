/*
===========================================================================

Daemon GPL Source Code
Copyright (C) 2012 Unvanquished Developers

This file is part of the Daemon GPL Source Code (Daemon Source Code).

Daemon Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Daemon Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Daemon Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Daemon Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following the
terms and conditions of the GNU General Public License which accompanied the Daemon
Source Code.  If not, please request a copy in writing from id Software at the address
below.

If you have questions concerning this license or the applicable additional terms, you
may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville,
Maryland 20850 USA.

===========================================================================
*/

#ifndef ROCKETCHATFIELD_H
#define ROCKETCHATFIELD_H

#include <Rocket/Core.h>
#include <Rocket/Core/Element.h>
#include <Rocket/Core/ElementUtilities.h>
#include <Rocket/Core/GeometryUtilities.h>
#include "client.h"
#include "rocket.h"
#include "../framework/CommandSystem.h"

class RocketChatField : public Rocket::Core::Element, Rocket::Core::EventListener
{
public:
	RocketChatField( const Rocket::Core::String &tag ) : Rocket::Core::Element( tag ), cursor_timer( 0 ), last_update_time( 0 ), init( false ), focus( false ), cursor_character_index( 0 ), text_element( NULL )
	{
		// Spawn text container
		text_element = Rocket::Core::Factory::InstanceElement( this, "span", "#text", Rocket::Core::XMLAttributes() );

		// Add it to this element
		AppendChild( text_element );
		AddEventListener( "exec", this );
		init = false;
	}
	virtual ~RocketChatField( void )
	{
	}

	void OnRender( void )
	{
		UpdateCursorPosition();
		cursor_geometry.Render( cursor_position );
	}

	void OnUpdate( void )
	{
		if ( !init )
		{
			init = true;

			GetContext()->GetRootElement()->AddEventListener( "show", this );
			GetContext()->GetRootElement()->AddEventListener( "hide", this );
			GetContext()->GetRootElement()->AddEventListener( "blur", this );
			GetContext()->GetRootElement()->AddEventListener( "mousemove", this );

		}

		// Ensure mouse follow cursor
		if ( focus )
		{
			GetContext()->ProcessMouseMove( cursor_position.x, cursor_position.y, 0 );
		}

		// Make sure this element is in focus if visible
		if ( GetContext()->GetFocusElement() != this )
		{
			this->Click();
			this->Focus();
		}
	}

	void ProcessEvent( Rocket::Core::Event &event )
	{
		// Cannot move mouse while this element is in view
		if ( focus && event == "mousemove" )
		{
			event.StopPropagation();
		}

		if ( event.GetTargetElement() == this )
		{
			if ( event == "resize" )
			{
				GetContext()->ShowMouseCursor( false );
				focus = true;
				GenerateCursor();
			}

			// Handle key presses
			else if ( event == "keydown" )
			{
				Rocket::Core::Input::KeyIdentifier key_identifier = ( Rocket::Core::Input::KeyIdentifier ) event.GetParameter<int>( "key_identifier", 0 );

				switch ( key_identifier )
				{
					case Rocket::Core::Input::KI_BACK:
						text.Erase( cursor_character_index - 1, 1 );
						UpdateText();
						MoveCursor( -1 );
						break;

					case Rocket::Core::Input::KI_DELETE:
						if ( cursor_character_index < text.Length() )
						{
							text.Erase( cursor_character_index, 1 );
							UpdateText();
						}

						break;

					case Rocket::Core::Input::KI_LEFT:
						MoveCursor( -1 );
						break;

					case Rocket::Core::Input::KI_RIGHT:
						MoveCursor( 1 );
						break;

					case Rocket::Core::Input::KI_RETURN:
					case Rocket::Core::Input::KI_NUMPADENTER:
					{
						Rocket::Core::String cmd = GetAttribute<Rocket::Core::String>( "exec", "" );

						if ( !cmd.Empty() )
						{
							Cmd::BufferCommandText( va( "%s %s\n", cmd.CString(), text.CString() ) );
							text.Clear();
							DispatchEvent( "exec", Rocket::Core::Dictionary() );
						}
					}
						break;

					default:
						break;
				}
			}

			else if ( event == "textinput" )
			{
				if ( event.GetParameter< int >( "ctrl_key", 0 ) == 0 &&
				        event.GetParameter< int >( "alt_key", 0 ) == 0 &&
				        event.GetParameter< int >( "meta_key", 0 ) == 0 )
				{
					Rocket::Core::word character = event.GetParameter< Rocket::Core::word >( "data", 0 );

					if ( text.Length() == cursor_character_index )
					{
						text.Append( ( char )character );
					}

					else
					{
						text.Insert( cursor_character_index, character );
					}

					UpdateText();
					MoveCursor( 1 );
				}
			}

			// We are in focus, let the element know
			else if ( event == "show" )
			{
				focus = true;
			}

			else if ( event == "blur" || event == "hide" )
			{
				focus =  false;
				GetContext()->ShowMouseCursor( true );
				text.Clear();
			}
		}
	}

	bool GetIntrinsicDimensions( Rocket::Core::Vector2f &dimension )
	{
		const Rocket::Core::Property *property;
		property = GetProperty( "width" );

		// Absolute unit. We can use it as is
		if ( property->unit & Rocket::Core::Property::ABSOLUTE_UNIT )
		{
			dimensions.x = property->value.Get<float>();
		}

		else
		{
			float base_size = 0;
			Rocket::Core::Element *parent = this;

			while ( ( parent = parent->GetParentNode() ) )
			{
				if ( ( base_size = parent->GetOffsetWidth() ) != 0 )
				{
					dimensions.x = ResolveProperty( "width", base_size );
					break;
				}
			}
		}

		dimensions.y = Rocket::Core::ElementUtilities::GetLineHeight( this );

		dimension = dimensions;

		return true;
	}


protected:
	void GenerateCursor( void )
	{
		// Generates the cursor.
		cursor_geometry.Release();

		Rocket::Core::Container::vector< Rocket::Core::Vertex >::Type &vertices = cursor_geometry.GetVertices();
		vertices.resize( 4 );

		Rocket::Core::Container::vector< int >::Type &indices = cursor_geometry.GetIndices();
		indices.resize( 6 );

		cursor_size.x = 1;
		cursor_size.y = ( float ) Rocket::Core::ElementUtilities::GetLineHeight( text_element ) + 2;
		Rocket::Core::GeometryUtilities::GenerateQuad( &vertices[0], &indices[0], Rocket::Core::Vector2f( 0, 0 ), cursor_size, GetProperty< Rocket::Core::Colourb >( "color" ) );
	}

	void MoveCursor( int amt )
	{
		cursor_character_index += amt;

		cursor_character_index = Rocket::Core::Math::Clamp<int>( cursor_character_index, 0, text.Length() );
	}

	void UpdateCursorPosition( void )
	{
		if ( text_element->GetFontFaceHandle() == NULL )
		{
			return;
		}

		cursor_position = GetAbsoluteOffset();

		cursor_position.x += ( float ) Rocket::Core::ElementUtilities::GetStringWidth( text_element, text.Substring( 0, cursor_character_index ) );
	}

	void UpdateText( void )
	{
		if ( text_element->GetNumChildren() )
		{
			text_element->RemoveAllChildren();
		}

		q2rml( text.CString(), text_element );
	}

	// Special q -> rml conversion function that preserves carets and codes
	void q2rml( const char *in, Rocket::Core::Element *parent )
	{
		const char *p;
		Rocket::Core::String out;
		Rocket::Core::Element *child = NULL;
		bool span = false;

		if ( !*in )
		{
			return;
		}

		for ( p = in; p && *p; ++p )
		{
			if ( *p == '<' )
			{
				out.Append( "&lt;" );
			}

			else if ( *p == '>' )
			{
				out.Append( "&gt;" );
			}

			else if ( *p == '&' )
			{
				out.Append( "&amp;" );
			}

			else if ( *p == '\n' )
			{
				// Child element initialized.
				if ( span )
				{
					span = false;
				}

				// If not intialized, probably the first one, and should be white.
				else
				{
					Rocket::Core::XMLAttributes xml;
					child = Rocket::Core::Factory::InstanceElement( parent, "#text", "span", xml );
					child->SetProperty( "color", "#FFFFFF" );
				}

				rocket_dynamic_cast<Rocket::Core::ElementText *>( child )->SetText( out );
				parent->AppendChild( child );
				parent->AppendChild( Rocket::Core::Factory::InstanceElement( parent, "*", "br", Rocket::Core::XMLAttributes() ) );
				out.Clear();
			}

			// Convert ^^ to ^
			else if ( *p == '^' && *( p + 1 ) == '^' )
			{
				p++;
				out.Append( "^" );
			}

			else if ( Q_IsColorString( p ) )
			{
				Rocket::Core::XMLAttributes xml;
				int code = ColorIndex( *++p );
				char c = *p;

				// Child element initialized
				if ( span )
				{
					span = false;
					rocket_dynamic_cast<Rocket::Core::ElementText *>( child )->SetText( out );
					parent->AppendChild( child );
					out.Clear();
				}

				// If not intialized, probably the first one, and should be white.
				else if ( !out.Empty() )
				{
					Rocket::Core::XMLAttributes xml;
					child = Rocket::Core::Factory::InstanceElement( parent, "#text", "span", xml );
					child->SetProperty( "color", "#FFFFFF" );
					rocket_dynamic_cast<Rocket::Core::ElementText *>( child )->SetText( out );
					parent->AppendChild( child );
					out.Clear();
				}


				child = Rocket::Core::Factory::InstanceElement( parent, "#text", "span", xml );
				child->SetProperty( "color", va( "#%02X%02X%02X",
				                                 ( int )( g_color_table[ code ][ 0 ] * 255 ),
				                                 ( int )( g_color_table[ code ][ 1 ] * 255 ),
				                                 ( int )( g_color_table[ code ][ 2 ] * 255 ) ) );
				out.Append( va( "^%c", c ) );
				span = true;
			}

			else
			{
				out.Append( *p );
			}
		}

		if ( span && child && !out.Empty() )
		{
			rocket_dynamic_cast<Rocket::Core::ElementText *>( child )->SetText( out );
			parent->AppendChild( child );
			span = false;
		}

		else if ( !span && !child && !out.Empty() )
		{
			child = Rocket::Core::Factory::InstanceElement( parent, "#text", "span", Rocket::Core::XMLAttributes() );
			rocket_dynamic_cast<Rocket::Core::ElementText *>( child )->SetText( out );
			parent->AppendChild( child );
		}
	}

private:
	Rocket::Core::Vector2f cursor_position;
	float cursor_timer;
	float last_update_time;
	bool init;
	bool focus;
	int cursor_character_index;
	Rocket::Core::Element *text_element;

	Rocket::Core::Geometry cursor_geometry;
	Rocket::Core::Vector2f cursor_size;
	Rocket::Core::Vector2f dimensions;
	Rocket::Core::String text;

};
#endif