// view_changelog.cc
//
//   Copyright (C) 2004-2005, 2007 Daniel Burrows
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

#include <vscreen/config/colors.h>
#include <vscreen/config/keybindings.h>
#include <vscreen/transcode.h>
#include <vscreen/vs_pager.h>
#include <vscreen/vs_scrollbar.h>
#include <vscreen/vs_table.h>
#include <vscreen/vs_text_layout.h>

#include "changelog_parse.h"
#include "download_bar.h"
#include "menu_redirect.h"
#include "menu_text_layout.h"
#include "ui.h"

#include <aptitude.h>

#include <generic/apt/apt.h>
#include <generic/apt/config_signal.h>

#include <generic/util/util.h>

#include <apt-pkg/pkgrecords.h>

#include <sigc++/adaptors/bind.h>
#include <sigc++/functors/mem_fun.h>

using namespace std;

class pkg_changelog_screen : public vs_pager, public menu_redirect
{
  bool last_search_forwards;

  void do_search()
  {
    last_search_forwards = true;

    prompt_string(transcode(_("Search for: ")),
		  get_last_search(),
		  arg(sigc::mem_fun(*this, &vs_pager::search_for)),
		  NULL,
		  NULL,
		  NULL);
  }

  void do_search_back()
  {
    last_search_forwards = false;

    prompt_string(transcode(_("Search backwards for: ")),
		  get_last_search(),
		  arg(sigc::mem_fun(*this, &vs_pager::search_back_for)),
		  NULL,
		  NULL,
		  NULL);
  }

  void do_repeat_search()
  {
    if(last_search_forwards)
      search_for(L"");
    else
      search_back_for(L"");
  }

  void do_repeat_search_back()
  {
    if(!last_search_forwards)
      search_for(L"");
    else
      search_back_for(L"");
  }

protected:
  pkg_changelog_screen(const string &changelog,
		       int x = 0, int y = 0,
		       int width = 0, int height = 0):
    vs_pager(changelog), last_search_forwards(true)
  {
    connect_key("Search", &global_bindings,
		sigc::mem_fun(*this, &pkg_changelog_screen::do_search));
    connect_key("SearchBack", &global_bindings,
		sigc::mem_fun(*this, &pkg_changelog_screen::do_search_back));
    connect_key("ReSearch", &global_bindings,
		sigc::mem_fun(*this, &pkg_changelog_screen::do_repeat_search));
    connect_key("RepeatSearchBack", &global_bindings,
		sigc::mem_fun(*this, &pkg_changelog_screen::do_repeat_search_back));
  }

public:
  static ref_ptr<pkg_changelog_screen>
  create(const string &changelog,
	 int x = 0, int y = 0, int width = 0, int height = 0)
  {
    ref_ptr<pkg_changelog_screen>
      rval(new pkg_changelog_screen(changelog, x, y, width, height));
    rval->decref();
    return rval;
  }

  bool find_search_enabled() override
  {
    return true;
  }

  bool find_search() override
  {
    do_search();
    return true;
  }

  bool find_search_back_enabled() override
  {
    return true;
  }

  bool find_search_back() override
  {
    do_search_back();
    return true;
  }

  bool find_research_enabled() override
  {
    return !get_last_search().empty();
  }

  bool find_research() override
  {
    do_repeat_search();
    return true;
  }
};
typedef ref_ptr<pkg_changelog_screen> pkg_changelog_screen_ref;


static void do_view_changelog(string changelog,
			      string pkgname,
			      string curverstr)
{
  string menulabel =
    ssprintf(_("ChangeLog of %s"), pkgname.c_str());
  string tablabel = ssprintf(_("%s changes"), pkgname.c_str());
  string desclabel = _("View the list of changes made to this Debian package.");

  fragment *f = make_changelog_fragment(changelog, curverstr);

  vs_table_ref           t = vs_table::create();
  if(f != NULL)
    {
      vs_scrollbar_ref   s = vs_scrollbar::create(vs_scrollbar::VERTICAL);
      menu_text_layout_ref l = menu_text_layout::create();


      l->location_changed.connect(sigc::mem_fun(s.unsafe_get_ref(), &vs_scrollbar::set_slider));
      s->scrollbar_interaction.connect(sigc::mem_fun(l.unsafe_get_ref(), &vs_text_layout::scroll));
      l->set_fragment(f);

      t->add_widget_opts(l, 0, 0, 1, 1,
			 vs_table::EXPAND|vs_table::SHRINK, vs_table::EXPAND);
      t->add_widget_opts(s, 0, 1, 1, 1, 0,
			 vs_table::EXPAND | vs_table::FILL);

      create_menu_bindings(l.unsafe_get_ref(), t);
    }
  else
    {
      pkg_changelog_screen_ref cs = pkg_changelog_screen::create(changelog);
      vs_scrollbar_ref          s = vs_scrollbar::create(vs_scrollbar::VERTICAL);

      cs->line_changed.connect(sigc::mem_fun(s.unsafe_get_ref(), &vs_scrollbar::set_slider));
      s->scrollbar_interaction.connect(sigc::mem_fun(cs.unsafe_get_ref(), &pkg_changelog_screen::scroll_page));
      cs->scroll_top();

      t->add_widget_opts(cs, 0, 0, 1, 1,
			 vs_table::EXPAND|vs_table::SHRINK, vs_table::EXPAND);
      t->add_widget_opts(s, 0, 1, 1, 1, 0,
			 vs_table::EXPAND | vs_table::FILL);

      create_menu_bindings(cs.unsafe_get_ref(), t);
    }

  t->show_all();

  insert_main_widget(t, menulabel, desclabel, tablabel);
}

void view_changelog(pkgCache::VerIterator ver)
{
  string pkgname = ver.ParentPkg().Name();
  pkgRecords::Parser &rec=apt_package_records->Lookup(ver.FileList());
  string changelog = rec.Changelog();

  pkgCache::VerIterator curver = ver.ParentPkg().CurrentVer();
  string curverstr;
  if(!curver.end() && curver.VerStr() != NULL)
    curverstr = curver.VerStr();

  do_view_changelog(changelog, pkgname, curverstr);
}
