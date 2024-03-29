// pkg_grouppolicy.cc
//
//  Copyright 1999-2005, 2007 Daniel Burrows
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; see the file COPYING.  If not, write to
//  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
//  Boston, MA 02111-1307, USA.
//
//  Implementation of stuff that needs implementing.

#include "aptitude.h"

#include "pkg_grouppolicy.h"
#include "pkg_item.h"
#include "pkg_subtree.h"

#include <vscreen/transcode.h>

#include <generic/apt/apt.h>
#include <generic/apt/matchers.h>

#include <generic/util/util.h>

#include <map>
#include <set>

#include <cwctype>

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/version.h>

#include <sigc++/functors/mem_fun.h>
#include <sigc++/trackable.h>

using namespace std;

pkg_grouppolicy_factory::~pkg_grouppolicy_factory()
{
}

// This special tree munges its tag to allow an integer to be prepended to it.
// Ok, it's a dreadful hack.  I admit it.
class pkg_subtree_with_order:public pkg_subtree
{
  wstring my_tag;
public:
  pkg_subtree_with_order(wstring name, wstring description,
			 sigc::signal1<void, std::wstring> *_info_signal,
			 unsigned char order, bool expand=false)
    :pkg_subtree(name, description, _info_signal, expand)
  {
    my_tag+=order;
    my_tag+=pkg_subtree::tag();
  }

  pkg_subtree_with_order(wstring name, unsigned char order, bool expand=false)
    :pkg_subtree(name, expand)
  {
    my_tag+=order;
    my_tag+=pkg_subtree::tag();
  }

  virtual const wchar_t *tag() override
  {
    return my_tag.c_str();
  }
};

// The following class is a special policy which is used to terminate a
// policy chain:
class pkg_grouppolicy_end:public pkg_grouppolicy
{
public:
  pkg_grouppolicy_end(pkg_signal *_sig, desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig) {}

  virtual void add_package(const pkgCache::PkgIterator &i, pkg_subtree *root) override
    {
      root->add_child(new pkg_item(i, get_sig()));
    }
};

pkg_grouppolicy *pkg_grouppolicy_end_factory::instantiate(pkg_signal *_sig,
							  desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_end(_sig, _desc_sig);
}

/*****************************************************************************/

// Groups packages so that the latest version of the package is shown, sorted
// by section.

class pkg_grouppolicy_section:public pkg_grouppolicy
{
  typedef std::map<string, pair<pkg_grouppolicy *, pkg_subtree *> > section_map;
  section_map sections;

  pkg_grouppolicy_factory *chain;

  // As in the factory
  int split_mode;
  bool passthrough;

  // The descriptions are in the cw::style used by package descriptions.
  static std::map<string, wstring> section_descriptions;
  static void init_section_descriptions();
public:
  pkg_grouppolicy_section(int _split_mode,
			  bool _passthrough,
			  pkg_grouppolicy_factory *_chain,
			  pkg_signal *_sig,
			  desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig), chain(_chain),
     split_mode(_split_mode), passthrough(_passthrough)
  {
    init_section_descriptions();
  }

  virtual void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override;

  virtual ~pkg_grouppolicy_section()
    {
      for(section_map::iterator i=sections.begin(); i!=sections.end(); i++)
        delete i->second.first;
    }
};

pkg_grouppolicy *pkg_grouppolicy_section_factory::instantiate(pkg_signal *_sig,
							      desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_section(split_mode, passthrough, chain,
				     _sig, _desc_sig);
}

std::map<string, wstring> pkg_grouppolicy_section::section_descriptions;

// Should this be externally configurable?
void pkg_grouppolicy_section::init_section_descriptions()
{
  static bool already_done=false;

  if(already_done)
    return;

  section_descriptions["Unknown"]=transcode(_("Packages with no declared section\n No section is given for these packages. Perhaps there is an error in the Packages file?"));
  section_descriptions["virtual"]=transcode(_("Virtual packages\n These packages do not exist; they are names other packages use to require or provide some functionality."));

  already_done=true;
}

void pkg_grouppolicy_section::add_package(const pkgCache::PkgIterator &pkg,
					  pkg_subtree *root)
{
  const char *section=pkg.VersionList().Section();
  bool maypassthrough=false; // FIXME: HACK!

  if(!section)
    {
      maypassthrough=true;
      section=_("Unknown");
    }

  if(pkg.VersionList().end())
    {
      maypassthrough=true;
      section=_("virtual");
    }

  const char *split=strchr(section, '/');
  const char *subdir=split?split+1:section;

  string tag;

  if(split_mode==pkg_grouppolicy_section_factory::split_none)
    tag=section;
  else if(split_mode==pkg_grouppolicy_section_factory::split_topdir)
    tag=(split?string(section,split-section):section);
  else if(split_mode==pkg_grouppolicy_section_factory::split_subdir)
    tag=subdir;

  section_map::iterator found=sections.find(tag);

  if((split == NULL || maypassthrough) && passthrough)
    {
      if(found==sections.end())
	sections[tag].first=chain->instantiate(get_sig(), get_desc_sig());

      sections[tag].first->add_package(pkg, root);
    }
  else
    {
      if(found==sections.end())
	{
	  pkg_subtree *newtree;

	  if(section_descriptions.find(tag)!=section_descriptions.end())
	    {
	      wstring desc=section_descriptions[tag];

	      if(desc.find(L'\n')!=desc.npos)
		newtree=new pkg_subtree(transcode(tag)+L" - "+wstring(desc, 0, desc.find('\n')),
					desc,
					get_desc_sig());
	      else
		newtree=new pkg_subtree(transcode(tag)+desc);
	    }
	  else
	    newtree=new pkg_subtree(transcode(tag));

	  sections[tag].first=chain->instantiate(get_sig(), get_desc_sig());
	  sections[tag].second=newtree;

	  root->add_child(newtree);
	}

      sections[tag].first->add_package(pkg, sections[tag].second);
    }
}

/*****************************************************************************/

class pkg_grouppolicy_status:public pkg_grouppolicy
{
  static const int numgroups=7;
  pkg_grouppolicy_factory *chain;

  enum states {security_upgradable, upgradable, newpkg, installed, not_installed, obsolete_pkg, virtual_pkg};

  static const char * const state_titles[numgroups];
  // FIXME: need better titles :)

  pair<pkg_grouppolicy *, pkg_subtree *> children[numgroups];
public:
  pkg_grouppolicy_status(pkg_grouppolicy_factory *_chain,
			 pkg_signal *_sig,
			 desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig), chain(_chain)
    {
      for(int i=0; i<( (int) (sizeof(children)/sizeof(children[0]))); i++)
	{
	  children[i].first=NULL;
	  children[i].second=NULL;
	}
    }

  virtual void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override;

  virtual ~pkg_grouppolicy_status()
    {
      for(int i=0; i<numgroups; i++)
	delete children[i].first;
    }
};

const char * const pkg_grouppolicy_status::state_titles[numgroups] =
{
  N_("Security Updates\n Security updates for these packages are available from security.debian.org."),
  N_("Upgradable Packages\n A newer version of these packages is available."),
  N_("New Packages\n These packages have been added to Debian since the last time you cleared the list of \"new\" packages (choose \"Forget new packages\" from the Actions menu to empty this list)."),
  N_("Installed Packages\n These packages are currently installed on your computer."),
  N_("Not Installed Packages\n These packages are not installed on your computer."),
  N_("Obsolete and Locally Created Packages\n These packages are currently installed on your computer, but they are not available from any apt source.  They may be obsolete and removed from the archive, or you may have built a private version of them yourself."),
  N_("Virtual Packages\n These packages do not exist; they are names other packages use to require or provide some functionality.")
};

pkg_grouppolicy *pkg_grouppolicy_status_factory::instantiate(pkg_signal *_sig,
							     desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_status(chain, _sig, _desc_sig);
}

// Stolen from apt-watch:

/** Tests whether a particular version is security-related.
 *
 *  \return \b true iff the given package version comes from security.d.o
 */
static bool version_is_security(const pkgCache::VerIterator &ver)
{
  for(pkgCache::VerFileIterator F=ver.FileList(); !F.end(); ++F)
    if(string(F.File().Site())=="security.debian.org")
      return true;

  return false;
}

void pkg_grouppolicy_status::add_package(const pkgCache::PkgIterator &pkg,
					 pkg_subtree *root)
{
  states section;
  pkgDepCache::StateCache &state=(*apt_cache_file)[pkg];

  if(pkg.VersionList().end())
    section=virtual_pkg;
  else
    {
      if(pkg_obsolete(pkg))
	section=obsolete_pkg;
      else if(state.Status != 2 && state.Upgradable())
	{
	  pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);
	  if(version_is_security(candver))
	    section=security_upgradable;
	  else
	    section=upgradable;
	}
      else if((*apt_cache_file)->get_ext_state(pkg).new_package)
	section=newpkg;
      else if(state.Status==2)
	section=not_installed;
      else
	section=installed;
    }

  if(!children[section].second)
    {
      wstring desc=transcode(_(state_titles[section]));
      wstring shortdesc(desc, 0, desc.find('\n'));

      pkg_subtree *newtree=new pkg_subtree_with_order(shortdesc, desc,
						      get_desc_sig(), section);
      children[section].first=chain->instantiate(get_sig(), get_desc_sig());
      children[section].second=newtree;
      root->add_child(newtree);
    }

  children[section].first->add_package(pkg, children[section].second);
}

/*****************************************************************************/
class pkg_grouppolicy_filter:public pkg_grouppolicy
{
  pkg_matcher *filter;

  pkg_grouppolicy *chain;
public:
  pkg_grouppolicy_filter(pkg_grouppolicy_factory *_chain, pkg_matcher *_filter,
			 pkg_signal *_sig, desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig),
     filter(_filter),
     chain(_chain->instantiate(_sig, _desc_sig))
  {
  }

  virtual void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override
  {
    if(filter->matches(pkg))
      chain->add_package(pkg, root);
  }

  virtual ~pkg_grouppolicy_filter() {delete chain;}
};

pkg_grouppolicy *pkg_grouppolicy_filter_factory::instantiate(pkg_signal *_sig,
							     desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_filter(chain, filter, _sig, _desc_sig);
}

pkg_grouppolicy_filter_factory::~pkg_grouppolicy_filter_factory()
{
  delete chain;
  delete filter;
}

/*****************************************************************************/

class pkg_grouppolicy_mode:public pkg_grouppolicy
{
  const static char * const child_names[];
  pair<pkg_grouppolicy *, pkg_subtree *> children[num_pkg_action_states];
  pair<pkg_grouppolicy *, pkg_subtree *> suggested_child;
  pair<pkg_grouppolicy *, pkg_subtree *> recommended_child;
  pkg_grouppolicy_factory *chain;
public:
  pkg_grouppolicy_mode(pkg_grouppolicy_factory *_chain,
		       pkg_signal *_sig, desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig), chain(_chain)
  {
    for(int i=0; i<num_pkg_action_states; i++)
      {
	children[i].first=NULL;
	children[i].second=NULL;
      }
  }

  void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override
  {
    int group=find_pkg_state(pkg);
    if(group!=pkg_unchanged)
      {
	if(!children[group].second)
	  {
	    wstring desc=transcode(_(child_names[group]));
	    wstring shortdesc(desc, 0, desc.find('\n'));

	    pkg_subtree *newtree=new pkg_subtree_with_order(shortdesc,
							    desc,
							    get_desc_sig(),
							    group,
							    true);
	    root->add_child(newtree);
	    children[group].first=chain->instantiate(get_sig(),
						     get_desc_sig());
	    children[group].second=newtree;
	  }

	children[group].first->add_package(pkg, children[group].second);
      }
    else if(pkg.CurrentVer().end() && package_recommended(pkg))
      {
	if(!recommended_child.second)
	  {
	    const wstring desc=transcode(_("Packages which are recommended by other packages\n These packages are not strictly required, but they may be necessary to provide full functionality in some other programs that you are installing or upgrading."));
	    const wstring shortdesc(desc, 0, desc.find('\n'));

	    pkg_subtree *newtree=new pkg_subtree_with_order(shortdesc,
							    desc,
							    get_desc_sig(),
							    num_pkg_action_states,
							    true);
	    root->add_child(newtree);
	    recommended_child.first=chain->instantiate(get_sig(),
						       get_desc_sig());
	    recommended_child.second=newtree;
	  }

	recommended_child.first->add_package(pkg, recommended_child.second);
      }
    else if(pkg.CurrentVer().end() && package_suggested(pkg))
      {
	if(!suggested_child.second)
	  {
	    const wstring desc=transcode(_("Packages which are suggested by other packages\n These packages are not required in order to make your system function properly, but they may provide enhanced functionality for some programs that you are currently installing."));
	    const wstring shortdesc(desc, 0, desc.find('\n'));

	    pkg_subtree *newtree=new pkg_subtree_with_order(shortdesc,
							    desc,
							    get_desc_sig(),
							    num_pkg_action_states+1,
							    false);
	    root->add_child(newtree);
	    suggested_child.first=chain->instantiate(get_sig(),
						     get_desc_sig());
	    suggested_child.second=newtree;
	  }

	suggested_child.first->add_package(pkg, suggested_child.second);
      }
  }

  virtual ~pkg_grouppolicy_mode()
  {
    for(int i=0; i<num_pkg_action_states; i++)
      delete children[i].first;
  }
};

const char * const pkg_grouppolicy_mode::child_names[num_pkg_action_states]=
{
  N_("Packages with unsatisfied dependencies\n The dependency requirements of these packages will be unmet after the install is complete.\n .\n The presence of this tree probably indicates that something is broken, either on your system or in the Debian archive."),
  N_("Packages being removed because they are no longer used\n These packages are being deleted because they were automatically installed to fulfill dependencies, and the planned action will result in no installed package declaring an 'important' dependency on them.\n"),
  N_("Packages being automatically held in their current state\n These packages could be upgraded, but they have been kept in their current state to avoid breaking dependencies."),
  N_("Packages being automatically installed to satisfy dependencies\n These packages are being installed because they are required by another package you have chosen for installation."),
  N_("Packages being deleted due to unsatisfied dependencies\n These packages are being deleted because one or more of their dependencies is no longer available, or because another package conflicts with them."),
  N_("Packages to be downgraded\n An older version of these packages than is currently installed will be installed."),
  N_("Packages being held back\n These packages could be upgraded, but you have asked for them to be held at their current version."),
  N_("Packages to be reinstalled\n These packages will be reinstalled."),
  N_("Packages to be installed\n These packages have been manually selected for installation on your computer."),
  N_("Packages to be removed\n These packages have been manually selected for removal."),
  N_("Packages to be upgraded\n These packages will be upgraded to a newer version."),
};

pkg_grouppolicy *pkg_grouppolicy_mode_factory::instantiate(pkg_signal *_sig,
							   desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_mode(chain, _sig, _desc_sig);
}

/*****************************************************************************/

class pkg_grouppolicy_firstchar:public pkg_grouppolicy
{
  pkg_grouppolicy_factory *chain;

  typedef map<char, pair<pkg_grouppolicy *, pkg_subtree *> > childmap;
  // Store the child group policies and their associated subtrees.
  childmap children;
public:
  pkg_grouppolicy_firstchar(pkg_grouppolicy_factory *_chain,
			    pkg_signal *_sig, desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig), chain(_chain)
  {
  }

  ~pkg_grouppolicy_firstchar()
  {
    for(childmap::iterator i=children.begin(); i!=children.end(); i++)
      delete i->second.first;
  }

  void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override
  {
    eassert(pkg.Name());

    char firstchar=toupper(pkg.Name()[0]);

    childmap::iterator found=children.find(firstchar);
    if(found!=children.end())
      found->second.first->add_package(pkg, found->second.second);
    else
      {
	string treename;
	treename+=firstchar;

	pkg_subtree *newtree=new pkg_subtree(transcode(treename));
	pkg_grouppolicy *newchild=chain->instantiate(get_sig(),
						     get_desc_sig());
	children[firstchar].first=newchild;
	children[firstchar].second=newtree;
	root->add_child(newtree);

	newchild->add_package(pkg, newtree);
      }
  }
};

pkg_grouppolicy *pkg_grouppolicy_firstchar_factory::instantiate(pkg_signal *sig,
								desc_signal *desc_sig)
{
  return new pkg_grouppolicy_firstchar(chain, sig, desc_sig);
}

/*****************************************************************************/

// Groups packages by priority
class pkg_grouppolicy_priority:public pkg_grouppolicy
{
  // a map may be overkill, but I figure better safe than sorry..
  // who knows, maybe someone will change the number of priorities someday..
  typedef map<unsigned char,
	      pair<pkg_grouppolicy *, pkg_subtree *> > childmap;

  childmap children;
  pkg_grouppolicy_factory *chain;

  pkg_grouppolicy *spillover;
public:
  pkg_grouppolicy_priority(pkg_grouppolicy_factory *_chain,
			   pkg_signal *_sig, desc_signal *_desc_sig)
    :pkg_grouppolicy(_sig, _desc_sig),
     chain(_chain),
     spillover(_chain->instantiate(get_sig(), get_desc_sig()))
  {
  }

  ~pkg_grouppolicy_priority()
  {
    for(childmap::iterator i=children.begin(); i!=children.end(); i++)
      delete i->second.first;
  }

  void add_package(const pkgCache::PkgIterator &pkg, pkg_subtree *root) override
  {
    unsigned char priority;
    const char *pstr;

    if(!pkg.VersionList().end())
      {
	if(!pkg.CurrentVer().end())
	  priority=pkg.CurrentVer()->Priority;
	else
	  priority=pkg.VersionList()->Priority;

	pstr=(*apt_cache_file)->GetCache().Priority(priority);
      }
    else
      {
	pstr=NULL;
	priority=0;
      }

    // Some packages don't have a reasonable priority
    if(!pstr)
      {
	pstr=_("unknown");
	priority=0;
      }

    childmap::iterator found=children.find(priority);

    if(found!=children.end())
      found->second.first->add_package(pkg, found->second.second);
    else
      {
	char buf[512];
	snprintf(buf, 512, _("Priority %s"), pstr);
	int order;
	switch(priority)
	  {
	  case pkgCache::State::Required : order=1; break;
	  case pkgCache::State::Important: order=2; break;
	  case pkgCache::State::Standard : order=3; break;
	  case pkgCache::State::Optional : order=4; break;
	  case pkgCache::State::Extra    : order=5; break;
	  default                        : order=6; break;
	  };

	pkg_subtree *newtree=new pkg_subtree_with_order(transcode(buf), order);
	pkg_grouppolicy *newchild=chain->instantiate(get_sig(),
						     get_desc_sig());
	children[priority].first=newchild;
	children[priority].second=newtree;
	root->add_child(newtree);

	newchild->add_package(pkg, newtree);
      }
  }  
};

pkg_grouppolicy *pkg_grouppolicy_priority_factory::instantiate(pkg_signal *sig,
							       desc_signal *desc_sig)
{
  return new pkg_grouppolicy_priority(chain, sig, desc_sig);
}

/*****************************************************************************/

class pkg_grouppolicy_matchers : public pkg_grouppolicy
{
public:
  typedef pkg_grouppolicy_matchers_factory::match_pair match_pair;

  struct subtree_pair
  {
    pkg_grouppolicy *policy;
    pkg_subtree *tree;

    subtree_pair():policy(NULL), tree(NULL)
    {
    }

    subtree_pair(pkg_grouppolicy *_policy, pkg_subtree *_tree)
      :policy(_policy), tree(_tree)
    {
    }
  };
private:
  pkg_grouppolicy_factory *chain;
  const vector<match_pair> &subgroups;
  typedef map<wstring, subtree_pair> subtree_map;
  subtree_map subtrees;

  wstring substitute(const wstring &s,
		     pkg_match_result *res)
  {
    wstring rval;

    wstring::const_iterator i = s.begin();
    while(i != s.end())
      {
	while(i != s.end() && *i != L'\\')
	  {
	    rval += *i;
	    ++i;
	  }

	if(i != s.end())
	  {
	    ++i;
	    if(i == s.end())
	      rval += L'\\';
	    else if(*i == L'\\')
	      {
		rval += L'\\';
		++i;
	      }
	    else if(iswdigit(*i))
	      {
		wstring tocvt;

		while(i != s.end() && iswdigit(*i))
		  {
		    tocvt += *i;
		    ++i;
		  }

		wchar_t *endptr = NULL;
		unsigned long val = wcstoul(tocvt.c_str(), &endptr, 0);

		if(endptr && (*endptr) != L'\0')
		  {
		    wchar_t buf[512];

		    swprintf(buf, 512, transcode(_("Bad number in format string: %ls")).c_str(),
			     tocvt.c_str());

		    return buf;
		  }

		if(val < 1)
		  {
		    wchar_t buf[512];
		    swprintf(buf, 512, transcode(_("Match indices must be 1 or greater, not \"%s\"")).c_str(),
			     tocvt.c_str());
		    return buf;
		  }

		--val;

		if(val >= res->num_groups())
		  {
		    string group_values;
		    for(unsigned int i = 0; i<res->num_groups(); ++i)
		      {
			if(i > 0)
			  group_values += ",";
			group_values += res->group(i);
		      }

		    wchar_t buf[1024];
		    swprintf(buf, 1024, transcode(_("Match index %ls is too large; available groups are (%s)")).c_str(),
			     tocvt.c_str(), group_values.c_str());

		    return buf;
		  }

		rval += transcode(res->group(val));
	      }
	  }
      }

    return rval;
  }
public:
  pkg_grouppolicy_matchers(pkg_grouppolicy_factory *_chain,
			   pkg_signal *_sig, desc_signal *_desc_sig,
			   const vector<match_pair> &_subgroups)
        :pkg_grouppolicy(_sig, _desc_sig),
	 chain(_chain), subgroups(_subgroups)
  {
  }

  ~pkg_grouppolicy_matchers()
  {
    for(subtree_map::const_iterator i = subtrees.begin();
	i != subtrees.end(); ++i)
      delete i->second.policy;
  }

  void add_package(const pkgCache::PkgIterator &pkg,
		   pkg_subtree *root)
    override
  {
    for(vector<match_pair>::const_iterator i = subgroups.begin();
	i != subgroups.end(); ++i)
	{
	  pkg_match_result *res = i->matcher->get_match(pkg);
	  if(res != NULL)
	    {
	      wstring title = substitute(i->tree_name, res);
	      delete res;

	      subtree_map::const_iterator found =
		subtrees.find(title);

	      if(found != subtrees.end())
		found->second.policy->add_package(pkg, found->second.tree);
	      else
		{
		  pkg_subtree *tree = new pkg_subtree(title);
		  pkg_grouppolicy *policy = chain->instantiate(get_sig(),
							       get_desc_sig());
		  root->add_child(tree);

		  subtrees[title]=subtree_pair(policy, tree);

		  policy->add_package(pkg, tree);
		}
	    }
	}
  }
};

pkg_grouppolicy *pkg_grouppolicy_matchers_factory :: instantiate(pkg_signal *sig,
								 desc_signal *_desc_sig)
{
  return new pkg_grouppolicy_matchers(chain, sig, _desc_sig, subgroups);
}

pkg_grouppolicy_matchers_factory :: ~pkg_grouppolicy_matchers_factory()
{
  for(std::vector<match_pair>::const_iterator i = subgroups.begin();
      i != subgroups.end(); ++i)
    delete i->matcher;
  delete chain;
}
