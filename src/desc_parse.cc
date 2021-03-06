// desc_parse.cc
//
//  Copyright 2004-2006 Daniel Burrows
//
//   This program is free software; you can redistribute it and/or
//   modify it under the terms of the GNU General Public License as
//   published by the Free Software Foundation; either version 2 of
//   the License, or (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//   General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program; see the file COPYING.  If not, write to
//   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//   Boston, MA 02111-1307, USA.
//
//  Parses a description into a fragment.

#include "desc_parse.h"

#include "aptitude.h"
#include "ui.h"

#include <generic/apt/apt.h>
#include <generic/apt/config_signal.h>

#include <vscreen/fragment.h>
#include <vscreen/transcode.h>
#include <vscreen/config/colors.h>

using namespace std;

// Notes on bulletting:
//
//   A line beginning with two or more spaces, followed by a bullet
// character and a space, is considered to be an element of a bulleted
// list.  Every succeeding line that begins with N+1 or more spaces,
// where N is the number of spaces preceding the bullet, is considered
// to be part of the same bulleted list, and is processed as if N+1
// spaces had been stripped from its left margin, with two exceptions:
//
//   * if the N+2nd character is not a space, the line is formatted as
//     if a leading space were present.  (see below)
//   * lines containing a space followed by a full stop terminate a
//     paragraph but do NOT break the list.
//   * Full stops at the beginning of a line of the list are ignored
//     (even though shifting them left would put them at the front
//     of a line).
//
//   Bullet characters are (maybe) "-", "+", and "*"; the
// frontend may render them literally or modify their appearence as it
// deems appropriate.

/** Scans for text at a single indent level and returns a fragment
 * corresponding to it.
 *
 *  \param desc the string from which the description should be built.
 *  \param level how many list indent levels have been entered; used
 *  to choose bullet style.
 *
 *  \param indent the number of spaces to strip from the left-hand
 *  side of each line.  If a line with less than this many spaces is
 *  encountered, start is placed at the beginning of the line and we
 *  return.
 *
 *  \param start the current location in the string; will be updated
 *  to reflect how many characters were consumed.  This should be
 *  placed after the "nspaces" indentation -- but it will be updated
 *  to be placed at the beginning of a line (0 indentation).
 *
 *  \return the new fragment.
 */
static fragment *make_level_fragment(const wstring &desc,
				     unsigned int level,
				     wstring::size_type indent,
				     wstring::size_type &start,
				     bool recognize_bullets)
{
  vector<fragment*> fragments;
  bool first=true;

  while(start<desc.size())
    {
      wstring::size_type loc=start;
      unsigned int nspaces;

      if(!first)
	{
	  nspaces=0;

	  while(loc<desc.size() && (desc[loc]==L' ' || desc[loc]==L'\t') && nspaces<indent)
	    {
	      ++loc;
	      ++nspaces;
	    }

	  // This handles the case of " .\n" breaking list paragraphs.
	  // I arbitrarily put the paragraph break inside the indented
	  // text even when it actually terminates the list.
	  if(nspaces == 1 && loc < desc.size() && desc[loc] == L'.')
	    ; // Handled uniformly below for both this case and the
	      // case of a leading blank line.
	  else if(nspaces < indent) // Anything but a " .\n" that has
				    // the wrong indent will break the
				    // list.
	    break;
	}
      else
	{
	  nspaces=indent;
	  first=false;
	}

      // Only insert blank lines for full stops that have exactly one
      // space; other full stops are treated as part of a paragraph.
      if(nspaces == 1 && desc[loc] == '.')
	{
	  fragments.push_back(newline_fragment());

	  while(loc < desc.size() && desc[loc] != L'\n')
	    ++loc;

	  if(loc < desc.size())
	    ++loc;

	  start=loc;

	  continue; // Skip the switch statement below.
	}

      switch(desc[loc])
	{
	case L' ':
	case L'\t':
	  // preformatted:
	  {
	    // Stores the number of spaces up to a bullet, if any.
	    unsigned int nspaces2=nspaces+1;

	    ++loc;

	    // Provisionally check if it's a bulletted line --
	    // *ignoring leading spaces*.
	    wstring::size_type loc2=loc;

	    while(loc2<desc.size() && desc[loc2] == L' ')
	      {
		++loc2;
		++nspaces2;
	      }

	    if(recognize_bullets &&
	       loc2 + 1 < desc.size() &&
	       (desc[loc2] == L'+' ||
		desc[loc2] == L'-' ||
		desc[loc2] == L'*') &&
	       desc[loc2 + 1] == L' ')
	      {
		// Start a list item (i.e., an indented region).

		wstring bullet;
		bullet+=(L"*+-"[level%3]);

		start = loc2 + 2;

		fragment *item_contents=make_level_fragment(desc,
							    level+1,
							    nspaces2 + 2,
							    start,
							    recognize_bullets);

		fragments.push_back(style_fragment(text_fragment(bullet),
						   get_style("Bullet")));
		fragments.push_back(indentbox(1,
					      (level+1)*2,
					      item_contents));

	      }
	    else
	      {
		int amt=0;
		while(loc+amt<desc.size() && desc[loc+amt]!=L'\n')
		  ++amt;

		// Hard-wrap AS REQUIRED BY POLICY.
		fragments.push_back(hardwrapbox(text_fragment(wstring(desc, loc, amt))));

		loc+=amt;
		if(loc<desc.size())
		  ++loc;

		start=loc;
	      }
	  }

	  break;
	default:
	  // It's a paragraph.
	  {
	    bool cont=true;
	    wstring::size_type amt=0;
	    wstring par=L"";

	    do {
	      amt=0;
	      while(loc+amt<desc.size() && desc[loc+amt]!=L'\n')
		++amt;

	      par=par+wstring(desc, loc, amt);

	      loc+=amt;

	      // If we hit a newline and didn't just output a whitespace
	      // character, insert one.
	      if(loc<desc.size() && par.size()>0 && par[par.size()-1]!=L' ')
		par+=L" ";

	      // Skip the newline
	      if(loc<desc.size())
		++loc;

	      // Update start.
	      start=loc;

	      // Find how much indentation this line has.
	      nspaces=0;
	      while(loc<desc.size() && (desc[loc]==L' ' || desc[loc]==L'\t'))
		{
		  ++loc;
		  ++nspaces;
		}

	      // Check if we should continue (if not, we back up and
	      // start parsing again from "start").  Note that *any*
	      // change in the indentation requires us to restart --
	      // more indentation is a literal line, while less means
	      // we should exit this indent level.
	      if(nspaces != indent)
		cont=false;
	      else if(loc>=desc.size())
		cont=false;
	      else if(nspaces == 1 && desc[loc] == '.')
		cont=false;
	    } while(cont);

	    fragments.push_back(wrapbox(text_fragment(par)));
	  }
	}
    }

  return sequence_fragment(fragments);
}

fragment *make_desc_fragment(const wstring &desc)
{
  wstring::size_type loc=0;
  vector<fragment*> fragments;

  return make_level_fragment(desc, 0, 1, loc,
			     aptcfg->FindB(PACKAGE "::Parse-Description-Bullets",
					   true));
}

