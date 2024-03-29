// apt.cc
//
//  Copyright 1999-2006 Daniel Burrows
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
//  Handles basic apt bookkeeping.

#include "apt.h"

#include <aptitude.h>


#include "config_signal.h"
#include "rev_dep_iterator.h"

#include <generic/util/undo.h>
#include <generic/util/util.h>
#include <vscreen/transcode.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/depcache.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/pkgcachegen.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/version.h>

#include <fstream>

#include <generic/util/eassert.h>
#include <csignal>
#include <unistd.h>
#include <sys/stat.h>
#include <cerrno>

using namespace std;

enum interesting_state {uncached = 0, uninteresting, interesting};
static interesting_state *cached_deps_interesting = NULL;

// Memoization of surrounding_or.  To save space, the low bit of each
// pointer in the following table is set to 1 when a result is cached:
static pkgCache::Dependency **cached_surrounding_or = NULL;

aptitudeCacheFile *apt_cache_file=NULL;
signalling_config *aptcfg=NULL;
pkgRecords *apt_package_records=NULL;
pkgSourceList *apt_source_list=NULL;
undo_list *apt_undos=NULL;

string *pendingerr=NULL;
bool erroriswarning=false;

static Configuration *theme_config;
static Configuration *user_config;

sigc::signal0<void> cache_closed, cache_reloaded, cache_reload_failed;
sigc::signal0<void> consume_errors;

static void reset_interesting_dep_memoization()
{
  delete[] cached_deps_interesting;
  cached_deps_interesting = NULL;
}

static void reset_surrounding_or_memoization()
{
  delete[] cached_surrounding_or;
  cached_surrounding_or = NULL;
}

void apt_preinit()
{
  signal(SIGPIPE, SIG_IGN);

  theme_config=new Configuration;
  user_config=new Configuration;

  ReadConfigFile(*theme_config, PKGDATADIR "/aptitude-defaults");

  pkgInitConfig(*_config);
  pkgInitSystem(*_config, _system);

  // Allow a user-specific customization file.
  const char *HOME = getenv("HOME");

  string cfgloc;

  if(HOME != NULL && *HOME != '\0' &&
     access((string(HOME) + "/.aptitude").c_str(), R_OK | X_OK) == 0)
    cfgloc = string(HOME) + "/.aptitude/config";
  else
    {
      cfgloc = get_homedir();
      if(!cfgloc.empty())
	cfgloc += "/.aptitude/config";
    }

  if(!cfgloc.empty() && access(cfgloc.c_str(), R_OK) == 0)
    {
      ReadConfigFile(*user_config, cfgloc);
      ReadConfigFile(*_config, cfgloc);
    }

  aptcfg=new signalling_config(user_config, _config, theme_config);

  aptcfg->connect(PACKAGE "::Recommends-Important",
		  sigc::ptr_fun(&reset_interesting_dep_memoization));

  cache_closed.connect(sigc::ptr_fun(&reset_interesting_dep_memoization));

  cache_closed.connect(sigc::ptr_fun(&reset_surrounding_or_memoization));

  apt_dumpcfg("Aptitude");

  apt_undos=new undo_list;
}

void apt_dumpcfg(const char *root)
{
  string cfgloc;

  const char *HOME = getenv("HOME");
  if(HOME != NULL && *HOME != '\0')
    {
      string tmp(HOME);
      tmp += "/.aptitude";
      if(access(tmp.c_str(), W_OK) == 0)
	cfgloc = tmp + "/config";
      else if(access(tmp.c_str(), R_OK | X_OK) == 0)
	{
	  // The squashed-root case.
	  _error->Error(_("%s is readable but not writable; unable to write configuration file."),
			tmp.c_str());
	  return;
	}
    }

  if(cfgloc.empty())
    {
      cfgloc = get_homedir();

      if(cfgloc.empty())
	return;

      cfgloc += "/.aptitude";

      if(mkdir(cfgloc.c_str(), 0700)<0 && errno != EEXIST)
	{
	  _error->Errno("mkdir", "%s", cfgloc.c_str());
	  return;
	}

      cfgloc += "/config";
    }

  ofstream f((cfgloc+".new").c_str());

  if(!f)
    {
      _error->Errno("apt_init", _("Unable to open %s for writing"), cfgloc.c_str());
      return;
    }

  aptcfg->Dump(f);

  f.close();

  if(rename((cfgloc+".new").c_str(), cfgloc.c_str())!=0)
    {
      _error->Errno("apt_init", _("Unable to replace %s with new configuration file"), cfgloc.c_str());
      return;
    }
}

// Revert back to the default set of options.  Is it a hack?  mHmm...
void apt_revertoptions()
{
  Configuration *old_user_config=user_config;
  Configuration *old_config=_config;

  _config=new Configuration;

  user_config=new Configuration;

  // ick?
  pkgInitConfig(*_config);
  pkgInitSystem(*_config, _system);

  aptcfg->setcfg(user_config, _config, theme_config);

  delete old_user_config;
  delete old_config;
}

void apt_init(OpProgress *progress_bar, bool do_initselections,
	      const char *status_fname)
{
  if(!apt_cache_file)
    apt_reload_cache(progress_bar, do_initselections, status_fname);
}

void apt_close_cache()
{
  cache_closed();

  if(apt_package_records)
    {
      delete apt_package_records;
      apt_package_records=NULL;
    }

  if(apt_cache_file)
    {
      delete apt_cache_file;
      apt_cache_file=NULL;
    }

  if(apt_source_list)
    {
      delete apt_source_list;
      apt_source_list=NULL;
    }
}

void apt_load_cache(OpProgress *progress_bar, bool do_initselections,
		    const char * status_fname)
{
  if(apt_cache_file != NULL)
    return;

  aptitudeCacheFile *new_file=new aptitudeCacheFile;

  apt_source_list=new pkgSourceList;
  apt_source_list->ReadMainList();

  bool simulate = aptcfg->FindB(PACKAGE "::Simulate", false);

  bool open_failed=!new_file->Open(*progress_bar, do_initselections,
				   (getuid() == 0) && !simulate,
				   status_fname)
    || _error->PendingError();

  if(open_failed && getuid() == 0)
    {
      // Don't discard errors, make sure they get displayed instead.
      consume_errors();

      open_failed=!new_file->Open(*progress_bar, do_initselections,
				  false, status_fname);

      if(!open_failed)
	_error->Warning(_("Could not lock the cache file.  Opening in read-only mode; any changes you make to the states of packages will NOT be preserved!"));
    }

  if(open_failed)
    {
      delete new_file;
      cache_reload_failed();
      return;
    }

  apt_cache_file=new_file;

  // *If we were loading the global list of states*, dump immediate
  // changes back to it.  This reduces the chance that the user will
  // ^C and lose important changes (like the new dselect states of
  // packages).  Note, though, that we don't fail if this fails.
  if(!status_fname && apt_cache_file->is_locked())
    (*apt_cache_file)->save_selection_list(*progress_bar);

  apt_package_records=new pkgRecords(*apt_cache_file);

  // Um, good time to clear our undo info.
  apt_undos->clear_items();

  cache_reloaded();
}

void apt_reload_cache(OpProgress *progress_bar, bool do_initselections,
		      const char * status_fname)
{
  apt_close_cache();
  apt_load_cache(progress_bar, do_initselections, status_fname);
}

pkg_action_state find_pkg_state(pkgCache::PkgIterator pkg)
{
  aptitudeDepCache::StateCache &state=(*apt_cache_file)[pkg];
  aptitudeDepCache::aptitude_state &extstate=(*apt_cache_file)->get_ext_state(pkg);

  if(state.InstBroken())
    return pkg_broken;
  else if(state.Delete())
    {
      if(extstate.remove_reason==aptitudeDepCache::manual)
	return pkg_remove;
      else if(extstate.remove_reason==aptitudeDepCache::unused)
	return pkg_unused_remove;
      else
	return pkg_auto_remove;
    }
  else if(state.Install())
    {
      if(!pkg.CurrentVer().end())
	{
	  if(state.iFlags&pkgDepCache::ReInstall)
	    return pkg_reinstall;
	  else if(state.Downgrade())
	    return pkg_downgrade;
	  else if(state.Upgrade())
	    return pkg_upgrade;
	  else
	    // FOO!  Should I abort here?
	    return pkg_install;
	}
      else if(extstate.install_reason!=aptitudeDepCache::manual)
	return pkg_auto_install;
      else
	return pkg_install;
    }

  else if(state.Status==1 &&
	  state.Keep())
    {
      if(!(state.Flags & pkgDepCache::AutoKept))
	return pkg_hold;
      else
	return pkg_auto_hold;
    }

  else if(state.iFlags&pkgDepCache::ReInstall)
    return pkg_reinstall;

  return pkg_unchanged;
}

bool pkg_obsolete(pkgCache::PkgIterator pkg)
{
  if(pkg.CurrentVer().end())
    return false;
  else
    {
      pkgCache::VerIterator ver=pkg.VersionList();
      ver++;

      if(!ver.end())
	return false;
      else // Ok, there's only one version.  Good.
	{
	  pkgCache::VerFileIterator files=pkg.VersionList().FileList();
	  if(!files.end())
	    {
	      files++;
	      if(!files.end())
		return false; // Nope, more than one file
	    }
	}
    }

  return true;
}

// This does not assume that the dependency is the first elements of
// its OR group.
static void surrounding_or_internal(const pkgCache::DepIterator &dep,
				    pkgCache::DepIterator &start,
				    pkgCache::DepIterator &end)
{
  bool found=false;

  start=const_cast<pkgCache::DepIterator &>(dep).ParentVer().DependsList();
  end=start;

  while(!end.end() && !found)
    {
      start=end;

      while(end->CompareOp&pkgCache::Dep::Or)
	{
	  if(end==dep)
	    found=true;

	  ++end;
	}

      if(end==dep)
	found=true;

      ++end;
    }

  // If not, something is wrong with apt's cache.
  eassert(found);
}

void surrounding_or(pkgCache::DepIterator dep,
		    pkgCache::DepIterator &start,
		    pkgCache::DepIterator &end,
		    pkgCache *cache)
{
  if(cache == NULL)
    cache = *apt_cache_file;

  if(cached_surrounding_or == NULL)
    {
      cached_surrounding_or = new pkgCache::Dependency *[cache->Head().DependsCount];
      for(unsigned long i = 0; i<cache->Head().DependsCount; ++i)
	cached_surrounding_or[i] = 0;
    }

  pkgCache::Dependency *s = cached_surrounding_or[dep->ID];

  // Use the old trick of stuffing values into the low bits of a
  // pointer.
  if(((unsigned long) s & 0x1) != 0)
    {
      pkgCache::Dependency *unmunged
	= (pkgCache::Dependency *) (((unsigned long) s) & ~0x1UL);

      start = pkgCache::DepIterator(*cache, unmunged);
      end = start;

      while(end->CompareOp & pkgCache::Dep::Or)
	++end;

      ++end;
    }
  else
    {
      surrounding_or_internal(dep, start, end);

      cached_surrounding_or[dep->ID] = (pkgCache::Dependency *) ((unsigned long) ((pkgCache::Dependency *) start) | 0x1UL);
    }
}

bool package_suggested(const pkgCache::PkgIterator &pkg)
{
  pkgDepCache::StateCache &state=(*apt_cache_file)[pkg];
  pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);

  for(rev_dep_iterator d(pkg); !d.end(); ++d)
    if((*d)->Type==pkgCache::Dep::Suggests)
      {
	bool satisfied=false;

	pkgCache::DepIterator start,end;

	surrounding_or(*d, start, end);

	while(start!=end)
	  {
	    if(((*apt_cache_file)[start])&pkgDepCache::DepGInstall)
	      {
		satisfied=true;
		break;
	      }

	    ++start;
	  }

	if(!satisfied)
	  {
	    // Check whether the package doing the depending is going
	    // to be installed.
	    pkgCache::PkgIterator depender=(*d).ParentPkg();
	    pkgDepCache::StateCache &depstate=(*apt_cache_file)[depender];
	    pkgCache::VerIterator depinstver=depstate.InstVerIter(*apt_cache_file);

	    if(depender.CurrentVer().end() &&
	       depstate.Install() &&
	       !depinstver.end() &&
	       !candver.end() &&
	       _system->VS->CheckDep(candver.VerStr(),
				     (*d)->CompareOp, (*d).TargetVer()))
	      {
		if((*d).ParentVer()==depinstver)
		  return true;
	      }
	  }
      }

  return false;
}

bool package_recommended(const pkgCache::PkgIterator &pkg)
{
  pkgDepCache::StateCache &state=(*apt_cache_file)[pkg];
  pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);

  for(rev_dep_iterator d(pkg); !d.end(); ++d)
    if((*d)->Type==pkgCache::Dep::Recommends)
      {
	bool satisfied=false;

	pkgCache::DepIterator start,end;

	surrounding_or(*d, start, end);

	while(start!=end)
	  {
	    if(((*apt_cache_file)[start])&pkgDepCache::DepGInstall)
	      {
		satisfied=true;
		break;
	      }

	    ++start;
	  }

	if(!satisfied)
	  {
	    // Check whether the package doing the depending is going
	    // to be installed or upgraded.
	    pkgCache::PkgIterator depender=(*d).ParentPkg();
	    pkgDepCache::StateCache &depstate=(*apt_cache_file)[depender];
	    pkgCache::VerIterator depinstver=depstate.InstVerIter(*apt_cache_file);

	    if(depstate.Install() &&
	       !candver.end() &&
	       _system->VS->CheckDep(candver.VerStr(),
				     (*d)->CompareOp, (*d).TargetVer()))
	      {
		if((*d).ParentVer()==depinstver)
		  return true;
	      }
	  }
      }

  return false;
}

bool package_trusted(const pkgCache::VerIterator &ver)
{
// Not yet implemented in apt 0.5
#if 0
  for(pkgCache::VerFileIterator i = ver.FileList(); !i.end(); ++i)
    {
      pkgIndexFile *index;

      if(!apt_source_list->FindIndex(i.File(), index))
	// Corresponds to the currently installed package, which is
	// always "trusted".
	return true;
      else if(index->IsTrusted())
	return true;
    }
#endif
  return true;
}

std::wstring get_short_description(const pkgCache::VerIterator &ver)
{
  if(ver.end() || ver.FileList().end() || apt_package_records == NULL)
    return std::wstring();

#ifndef HAVE_DDTP
  pkgCache::VerFileIterator vf = ver.FileList();

  if(vf.end())
    return std::wstring();
  else
    return transcode(apt_package_records->Lookup(vf).ShortDesc(), "UTF-8");
#else
  pkgCache::DescIterator d = ver.TranslatedDescription();

  if(d.end())
    return std::wstring();

  pkgCache::DescFileIterator df = d.FileList();

  if(df.end())
    return std::wstring();
  else
    // Since Packages files don't have a bundled description and it
    // just feels icky for the data in Packages to depend on the
    // encoding, I just force a sane encoding on it here.  Really,
    // though, Packages files should have an encapsulated encoding
    // somewhere.
    return transcode(apt_package_records->Lookup(df).ShortDesc(), "UTF-8");
#endif
}

std::wstring get_long_description(const pkgCache::VerIterator &ver)
{
  if(ver.end() || ver.FileList().end() || apt_package_records == NULL)
    return std::wstring();

#ifndef HAVE_DDTP
  pkgCache::VerFileIterator vf = ver.FileList();

  if(vf.end())
    return std::wstring();
  else
    return transcode(apt_package_records->Lookup(vf).LongDesc(), "UTF-8");
#else
  pkgCache::DescIterator d = ver.TranslatedDescription();

  if(d.end())
    return std::wstring();

  pkgCache::DescFileIterator df = d.FileList();

  if(df.end())
    return std::wstring();
  else
    return transcode(apt_package_records->Lookup(df).LongDesc(), "UTF-8");
#endif
}
