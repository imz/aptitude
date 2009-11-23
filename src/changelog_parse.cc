// changelog_parse.cc
//
//   Copyright (C) 2005 Daniel Burrows
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
// At the moment this code uses parsechangelog to convert changelogs
// into something easier to read.

#include "changelog_parse.h"

#include "desc_parse.h"

#include <apt-pkg/fileutl.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/tagfile.h>
#include <apt-pkg/version.h>

#include <generic/util/temp.h>
#include <generic/util/util.h>

#include <vscreen/fragment.h>
#include <vscreen/transcode.h>

static
fragment *change_text_fragment(const std::string &s)
{
  std::vector<fragment *> lines;

  std::string::size_type start = 0;
  std::string::size_type next_nl;

  bool was_nl = false;

  do
    {
      next_nl = s.find('\n', start);

      if(next_nl == start)
	{
	  if (!was_nl)
	    lines.push_back(newline_fragment());
	  start = next_nl + 1;
	  continue;
	}
      else
	was_nl = false;

      std::string this_line;
      if(next_nl != std::string::npos)
	this_line.assign(s, start, next_nl - start);
      else
	this_line.assign(s, start, std::string::npos);

      size_t first_nonspace = 0;
      while(first_nonspace < this_line.size() && isspace(this_line[first_nonspace]))
	++first_nonspace;

      bool has_bullet = false;
      if(first_nonspace < this_line.size())
	switch(this_line[first_nonspace])
	  {
	  case '*':
	  case '+':
	  case '-':
	    has_bullet = true;
	    break;
	  }

      if(has_bullet)
	lines.push_back(hardwrapbox(fragf("%s%F%s%n",
					  std::string(this_line, 0, first_nonspace).c_str(),
					  text_fragment(std::string(this_line, first_nonspace, 1).c_str(),
							get_style("Bullet")),
					  std::string(this_line, first_nonspace + 1).c_str())));
      else
	lines.push_back(hardwrapbox(fragf("%s%n", this_line.c_str())));

      start = next_nl + 1;
    } while(next_nl != std::string::npos);

  return sequence_fragment(lines);
}

fragment *make_changelog_fragment(const std::string &cl,
				  const std::string &curver)
{
  if(cl.size() > 0)
    {
      std::string::size_type start = 0;
      std::string::size_type next_nl;

      std::string version;
      std::string changes;
      std::string maintainer;
      std::string date;

      std::vector<fragment *> fragments;

      bool first = true;

      do
	{
	  next_nl = cl.find('\n', start);

	  if (cl[start] != '*' || next_nl == std::string::npos)
	    return sequence_fragment(fragments);

	  std::string::size_type ds, ms, vs;
	  vs = cl.rfind(' ', next_nl);
	  ds = start+1;

	  while (cl[ds] == ' ')
	    ++ds;

	  if ( vs < start || vs == std::string::npos)
	    return sequence_fragment(fragments);

	  version = cl.substr(vs+1, next_nl-vs-1);

	  while (cl[vs] == ' ')
	    --vs;

	  ms=ds;
	  for (int i = 4; i > 0; i--)
	    {
	      if (ms > vs)
		return sequence_fragment(fragments);

	      while (cl[ms] != ' ')
		  ++ms;
	      while (cl[ms] == ' ')
		  ++ms;
	    }
	  if (ms > vs)
	    return sequence_fragment(fragments);
	  --ms;

	  maintainer = cl.substr(ms+1, vs-ms);

	  while (cl[ms] == ' ')
	    --ms;

	  date = cl.substr(ds, ms-ds+1);

	  start=next_nl+1;
          next_nl = cl.find("\n*", start);

	  std::string::size_type ce;

	  ce = cl.find_last_not_of('\n', next_nl);

	  if (start > ce)
	    changes = cl.substr(start);
	  else
	    changes = cl.substr(start, ce-start+1);

	  start=next_nl+1;

	  fragment *f = fragf(first ? "%F%F" : "%n%F%F",
			      hardwrapbox(fragf("%F %F %F %F",
						text_fragment("*",
							      get_style("ChangelogStar")),
						text_fragment(date.c_str(),
							      get_style("ChangelogDate")),
						text_fragment(maintainer.c_str(),
							      get_style("ChangelogMaintainer")),
						text_fragment(version.c_str(),
							      get_style("ChangelogVersion")))),
			      change_text_fragment(changes));

	  first = false;

	  if(!curver.empty() && _system->VS->CmpVersion(version, curver) > 0)
	    {
	      style s = get_style("ChangelogNewerVersion");
	      fragments.push_back(style_fragment(f, s));
	    }
	  else
	    fragments.push_back(f);
	} while (next_nl != std::string::npos);

      return sequence_fragment(fragments);
    }
    return NULL;
}
