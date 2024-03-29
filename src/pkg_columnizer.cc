// pkg_columnizer.cc
//
//  The pkg_columnizer class.
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

#include "aptitude.h"

#include "pkg_columnizer.h"

#include <generic/apt/apt.h>
#include <generic/apt/config_signal.h>

#include <apt-pkg/error.h>
#include <apt-pkg/pkgrecords.h>
#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/policy.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/version.h>

#ifndef HAVE_LIBAPT_PKG3
#define pkgCheckDep _system->checkDep
#endif

#include <vscreen/transcode.h>

#include <unistd.h>

column_definition_list *pkg_item::pkg_columnizer::columns=NULL;
const char *pkg_item::pkg_columnizer::default_pkgdisplay="%c%a%M%S %p %Z %v %V";

// NOTE: the default widths here will be overridden by the initialization
//      code.
column_type_defaults pkg_item::pkg_columnizer::defaults[pkg_columnizer::numtypes]={
  {30, true, true},     // name
  {6, false, false},    // installed_size
  {6, false, false},    // debsize
  {1, false, false},    // stateflag
  {1, false, false},    // actionflag
  {40, true, true},     // description
  {10, false, false},   // currver
  {10, false, false},   // candver
  {11, false, false},   // longstate
  {10, false, false},   // longaction
  {35, true, true},     // maintainer
  {9, false, true},     // priority
  {3, false, true},     // shortpriority
  {10, false, true},    // section
  {2, false, false},    // revdepcount
  {1, false, false},    // autoset
  {1, false, false},    // tagged
  {10, true, true},     // archive
  {7, false, false},    // sizechange
  {strlen(PACKAGE), false, false},  // progname
  {strlen(VERSION), false, false},  // progver
  {12, false, true},    // brokencount
  {30, false, true},    // diskusage
  {15, false, true},    // downloadsize
  {4, false, false},    // pin_priority
  {15, false, true},    // hostname
  {1, false, false}     // trust_state
};

// Default widths for:
// name, installed_size, debsize, stateflag, actionflag, description, currver,
// candver, longstate, longaction, maintainer, priority, section, revdepcount,
// brokencount, diskusage, downloadsize.
//
// You can't set default widths for the program name and version here (those
// strings aren't affected by translation, for one thing)
const char *default_widths=N_("30 6 6 1 1 40 10 10 11 10 35 9 10 2 1 1 10 12 30 15 15");

const char *pkg_item::pkg_columnizer::column_names[pkg_columnizer::numtypes]=
  {N_("Package"),
   N_("InstSz"),
   N_("DebSz"),
   N_("State"),
   N_("Action"),
   N_("Description"),
   N_("InstVer"),
   N_("CandVer"),
   N_("LongState"),
   N_("LongAction"),
   N_("Maintainer"),
   N_("Priority"),
   N_("Section"),
   N_("RC"),
   N_("Auto"),
   N_("Tag"),

   // These don't make sense with headers, but whatever:
   N_("ProgName"),
   N_("ProgVer"),
   N_("#Broken"),
   N_("DiskUsage"),
   N_("DownloadSize")
  };

column_disposition pkg_item::pkg_columnizer::setup_column(int type)
{
  switch(type)
    {
    case name:
      if(!pkg.end())
	return column_disposition(pkg.Name(), basex);
      else
	return column_disposition("", 0);

      break;
    case installed_size:
      if(!visible_ver.end())
	{
	  if(visible_ver->InstalledSize>0)
	    return column_disposition(SizeToStr(visible_ver->InstalledSize)+'B', 0);
	  else
	    return column_disposition(_("<N/A>"), 0);
	}
      else
	return column_disposition("", 0);	

      break;
    case debsize:
      if(!visible_ver.end())
	{
	  if(visible_ver->Size>0)
	    return column_disposition(SizeToStr(visible_ver->Size)+'B', 0);
	  else
	    return column_disposition(_("<N/A>"), 0);
	}
      else
	return column_disposition("", 0);
      break;
    case sizechange:
      if(pkg.end())
	return column_disposition("", 0);
      else
	{
	  pkgCache::VerIterator inst_ver=(*apt_cache_file)[pkg].InstVerIter(*apt_cache_file);

	  // TODO: move this to a generic file (common with cmdline.cc)
	  int dsize=(inst_ver.end()?0:inst_ver->InstalledSize)
	    -(pkg.CurrentVer().end()?0:pkg.CurrentVer()->InstalledSize);

	  if(dsize==0)
	    return column_disposition("", 0);
	  else if(dsize>0)
	    return column_disposition("+"+SizeToStr(dsize)+"B", 0);
	  else
	    return column_disposition("-"+SizeToStr(dsize)+"B", 0);
	}
      break;
    case currver:
      if(pkg.end())
	return column_disposition("", 0);
      else if(!pkg.CurrentVer().end())
	return column_disposition(pkg.CurrentVer().VerStr(), 0);
      else
	return column_disposition(_("<none>"), 0);

      break;
    case candver:
      if(!pkg.end())
	{
	  pkgCache::VerIterator cand_ver=(*apt_cache_file)[pkg].CandidateVerIter(*apt_cache_file);

	  if(!cand_ver.end() && !pkg_obsolete(pkg))
	    return column_disposition(cand_ver.VerStr(), 0);
	  else
	    return column_disposition(_("<none>"), 0);
	}
      else
	return column_disposition("", 0);

      break;
    case stateflag:
      if(pkg.end())
	return column_disposition("", 0);
      else if(pkg.VersionList().end())
	return column_disposition("v", 0);
      else if((*apt_cache_file)[pkg].NowBroken())
	return column_disposition("B", 0);
      else
	switch(pkg->CurrentState)
	  {
	  case pkgCache::State::NotInstalled:
	    return column_disposition("p", 0);
	    // Assume it's purged if we're in this state.  Is this correct?
	    // If it's removed but config-files are present it should be
	    // in the ConfigFiles state.
	  case pkgCache::State::UnPacked:
	    return column_disposition("u", 0);
	  case pkgCache::State::HalfConfigured:
	    return column_disposition("C", 0);
	  case pkgCache::State::HalfInstalled:
	    return column_disposition("H", 0);
	  case pkgCache::State::ConfigFiles:
	    return column_disposition("c", 0);
	  case pkgCache::State::Installed:
	    return column_disposition("i", 0);
	  default:
	    return column_disposition("E", 0);
	  }

      break;
    case longstate:
      if(pkg.end())
	return column_disposition("", 0);
      else if(pkg.VersionList().end())
	return column_disposition(_("virtual"), 0);
      else if((*apt_cache_file)[pkg].NowBroken())
	return column_disposition("B", 0);
      else
	switch(pkg->CurrentState)
	  {
	  case pkgCache::State::NotInstalled:
	    return column_disposition(_("purged"), 0);
	    // Assume it's purged if we're in this state.  Is this correct?
	    // If it's removed but config-files are present it should be
	    // in the ConfigFiles state.
	  case pkgCache::State::UnPacked:
	    return column_disposition(_("unpacked"), 0);
	  case pkgCache::State::HalfConfigured:
	    return column_disposition(_("half-config"), 0);
	  case pkgCache::State::HalfInstalled:
	    return column_disposition(_("half-install"), 0);
	  case pkgCache::State::ConfigFiles:
	    return column_disposition(_("config-files"), 0);
	  case pkgCache::State::Installed:
	    return column_disposition(_("installed"), 0);
	  default:
	    return column_disposition(_("ERROR"), 0);
	  }

      break;
    case actionflag:
      {
	if(pkg.end())
	  return column_disposition("", 0);

	aptitudeDepCache::StateCache &state=(*apt_cache_file)[pkg];
	aptitudeDepCache::aptitude_state &estate=(*apt_cache_file)->get_ext_state(pkg);
	pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);

	if(state.Status!=2 &&
	   (*apt_cache_file)->get_ext_state(pkg).selection_state==pkgCache::State::Hold &&
	   !state.InstBroken())
	  return column_disposition("h", 0);
	else if(state.Upgradable() && !pkg.CurrentVer().end() &&
		!candver.end() && candver.VerStr() == estate.forbidver)
	  return column_disposition("F", 0);
	else if(state.Delete())
	  return column_disposition("d", 0);
	else if(state.InstBroken())
	  return column_disposition("B", 0);
	else if(state.NewInstall())
	  return column_disposition("i", 0);
	else if(state.iFlags&pkgDepCache::ReInstall)
	  return column_disposition("r", 0);
	else if(state.Upgrade())
	  return column_disposition("u", 0);
	else if(pkg.CurrentVer().end())
	  return column_disposition(" ", 0);
	else
	  return column_disposition(" ", 0);
      }

      break;
    case longaction:
      {
	if(pkg.end())
	  return column_disposition("", 0);

	aptitudeDepCache::StateCache state=(*apt_cache_file)[pkg];
	aptitudeDepCache::aptitude_state &estate=(*apt_cache_file)->get_ext_state(pkg);
	pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);

	if(state.Status!=2 && (state.Held() || estate.selection_state==pkgCache::State::Hold) && !state.InstBroken())
	  return column_disposition(_("hold"), 0);
	else if(state.Upgradable() && !pkg.CurrentVer().end() &&
		!candver.end() && candver.VerStr() == estate.forbidver)
	  return column_disposition(_("forbidden upgrade"), 0);
	else if(state.Delete())
	  return column_disposition(_("delete"), 0);
	else if(state.InstBroken())
	  return column_disposition(_("broken"), 0);
	else if(state.NewInstall())
	  return column_disposition(_("install"), 0);
	else if(state.iFlags&pkgDepCache::ReInstall)
	  return column_disposition(_("reinstall"), 0);
	else if(state.Upgrade())
	  return column_disposition(_("upgrade"), 0);
	else if(pkg.CurrentVer().end())
	  return column_disposition(_("none"), 0);
	else
	  return column_disposition(_("none"), 0);
      }

      break;
    case description:
      return column_disposition(get_short_description(visible_ver), 0);

      break;
    case maintainer:
      if(!visible_ver.end() &&
	 !visible_ver.FileList().end() &&
	 apt_package_records)
	return column_disposition(apt_package_records->Lookup(visible_ver.FileList()).Maintainer(), 0);
      else
	return column_disposition("", 0);

      break;
    case priority:
      if(!visible_ver.end() &&
	 visible_ver.PriorityType() &&
	 visible_ver.PriorityType()[0])
	return column_disposition(visible_ver.PriorityType(), 0);
      else
	return column_disposition(_("Unknown"), 0);

      break;
    case shortpriority:
      if(!visible_ver.end())
	switch(visible_ver->Priority)
	  {
	  case pkgCache::State::Important:
	    return column_disposition(_("Imp"), 0);
	  case pkgCache::State::Required:
	    return column_disposition(_("Req"), 0);
	  case pkgCache::State::Standard:
	    return column_disposition(_("Std"), 0);
	  case pkgCache::State::Optional:
	    return column_disposition(_("Opt"), 0);
	  case pkgCache::State::Extra:
	    return column_disposition(_("Xtr"), 0);
	  default:
	    return column_disposition(_("ERR"), 0);
	  }
      else
	return column_disposition(_("Unknown"), 0);

      break;
    case section:
      if(!visible_ver.end() && visible_ver.Section())
	return column_disposition(visible_ver.Section(), 0);
      else
	return column_disposition(_("Unknown"), 0);

      break;
    case progname:
      return column_disposition(PACKAGE, 0);
    case progver:
      return column_disposition(VERSION, 0);
    case brokencount:
      if(apt_cache_file && (*apt_cache_file)->BrokenCount()>0)
	{
	  char buf[512];

	  snprintf(buf, 512, _("#Broken: %ld"),
		   (*apt_cache_file)->BrokenCount());
	  return column_disposition(buf, 0);
	}
      else
	return column_disposition("", 0);

      break;
    case diskusage:
      {
	char buf[256];
	if(apt_cache_file && (*apt_cache_file)->UsrSize()>0)
	  {
	    // Be translator-friendly -- breaking messages up like that is not
	    // so good..
	    snprintf(buf, 256, _("Will use %sB of disk space"), SizeToStr((*apt_cache_file)->UsrSize()).c_str());
	    return column_disposition(buf, 0);
	  }
	else if(apt_cache_file &&
		(*apt_cache_file)->UsrSize()<0)
	  {
	    // Be translator-friendly -- breaking messages up like that is not
	    // so good..
	    snprintf(buf, 256, _("Will free %sB of disk space"), SizeToStr(-(*apt_cache_file)->UsrSize()).c_str());
	    return column_disposition(buf, 0);
	  }
	else
	  return column_disposition("", 0);
      }

      break;
    case downloadsize:
      {
	if(apt_cache_file && (*apt_cache_file)->DebSize()!=0)
	  {
	    char buf[256];
	    snprintf(buf, 256,
		     _("DL Size: %sB"), SizeToStr((*apt_cache_file)->DebSize()).c_str());
	    return column_disposition(buf, 0);
	  }
	else
	  return column_disposition("", 0);
      }

      break;
    case pin_priority:
      {
	if(apt_cache_file && !pkg.end())
	  {
	    char buf[256];

	    pkgPolicy *policy=dynamic_cast<pkgPolicy *>(&(*apt_cache_file)->GetPolicy());

	    if(!policy)
	      return column_disposition("", 0);

	    // Not quite sure what this indicates
	    signed short priority=policy->GetPriority(pkg);

	    if(priority==0)
	      return column_disposition("", 0);

	    snprintf(buf, 256, "%d", priority);

	    return column_disposition(buf, 0);
	  }
	else
	  return column_disposition("", 0);
      }
    case autoset:
      // Display the "auto" flag UNLESS the package has been removed
      // or purged already and is not presently being installed.
      if((!pkg.CurrentVer().end() ||
	  (*apt_cache_file)[pkg].Install()) &&
	 (*apt_cache_file)->get_ext_state(pkg).install_reason!=aptitudeDepCache::manual)
	return column_disposition("A", 0);
      else
	return column_disposition("", 0);

      break;
    case tagged:
      if((*apt_cache_file)->get_ext_state(pkg).tagged)
	return column_disposition("*", 0);
      else
	return column_disposition("", 0);

      break;

    case archive:
      if(!visible_ver.end())
	{
	  string buf;
	  for(pkgCache::VerFileIterator verfile=visible_ver.FileList(); !verfile.end(); ++verfile)
	    {
	      pkgCache::PkgFileIterator pkgfile=verfile.File();
	      if(pkgfile.Archive() && strcmp(pkgfile.Archive(), "now"))
		buf+=string(buf.empty()?"":",")+pkgfile.Archive();
	    }
	  return column_disposition(buf,0);
	}

    case hostname:
      {
	char buf[256];
	buf[255]=0;
	if(gethostname(buf, 255)!=0)
	  // Panic
	  // ForTranslators: Hostname
	  return column_disposition(_("HN too long"), 0);
	else
	  return column_disposition(buf, 0);
      }

      break;
    case revdepcount:
      {
	int count=0;
	char buf[100];
	if(!visible_ver.end())
	  {
	    for(pkgCache::DepIterator D=pkg.RevDependsList(); !D.end(); D++)
	      if(D.IsCritical() &&
		 D->Type!=pkgCache::Dep::Conflicts &&
		 D.ParentVer()==D.ParentPkg().CurrentVer() &&
		 // That test is CORRECT; we want to see if the version
		 // providing the dependency is correct.
		 // (I'm writing this note because I just looked at the test
		 //  and couldn't remember what it did despite writing it
		 //  5 minutes ago. Maybe I should have my head examined :) )
		 _system->VS->CheckDep(visible_ver.VerStr(), D->CompareOp, D.TargetVer()))
		count++;
	  }

	for(pkgCache::PrvIterator i=pkg.ProvidesList(); !i.end(); i++)
	  for(pkgCache::DepIterator D=i.ParentPkg().RevDependsList(); !D.end(); D++)
	    {
	      if(D.IsCritical() &&
		 D->Type!=pkgCache::Dep::Conflicts &&
		 D.ParentVer()==D.ParentPkg().CurrentVer() &&
		 _system->VS->CheckDep(i.ProvideVersion(), D->CompareOp, D.TargetVer()))
		count++;
	    }
	snprintf(buf, 100, "%i", count);
	return column_disposition(buf, 0);
      }
      break;
    case trust_state:
      {
	if(visible_ver.end())
	  return column_disposition(" ", 0);
	else if(package_trusted(visible_ver))
	  return column_disposition(" ", 0);
	else
	  return column_disposition("U", 0);
      }
    default:
      return column_disposition(_("ERROR"), 0);
    }
}

int pkg_item::pkg_columnizer::parse_column_type(char id)
{
  switch(id)
    {
    case 'p':
      return name;
    case 'I':
      return installed_size;
    case 'D':
      return debsize;
    case 'c':
      // 'c' for CURRENTSTATE
      return stateflag;
    case 'a':
      return actionflag;
    case 'd':
      return description;
    case 'v':
      return currver;
    case 'V':
      return candver;
    case 'm':
      return maintainer;
    case 'C':
      return longstate;
    case 'A':
      return longaction;
    case 'P':
      return priority;
    case 'R':
      return shortpriority;
    case 'S':
      return trust_state;
    case 's':
      return section;
    case 'r':
      return revdepcount;
    case 'M': // autoMatic
      return autoset;
    case 'T':
      return tagged;
    case 't': // like apt-get -t
      return archive;

    case 'B':
      return brokencount;
    case 'N':
      return progname;
    case 'n':
      return progver;
    case 'u':
      return diskusage;
    case 'o':
      return downloadsize;
    case 'H':
      return hostname;
    case 'Z':
      return sizechange;
    case 'i':
      return pin_priority;
    default:
      return -1;
    }
}

class pkg_item::pkg_columnizer::pkg_genheaders:public column_generator
{
protected:
  column_disposition setup_column(int type) override
  {
    return column_disposition(_(pkg_columnizer::column_names[type]), 0);
  }
public:
  pkg_genheaders(const column_definition_list &_columns)
    :column_generator(_columns) {}
};

void pkg_item::pkg_columnizer::init_formatting()
{
  sscanf(_(default_widths),
	 "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
	 &defaults[name].width,
	 &defaults[installed_size].width,
	 &defaults[debsize].width,
	 &defaults[stateflag].width,
	 &defaults[actionflag].width,
	 &defaults[description].width,
	 &defaults[currver].width,
	 &defaults[candver].width,
	 &defaults[longstate].width,
	 &defaults[longaction].width,
	 &defaults[maintainer].width,
	 &defaults[priority].width,
	 &defaults[section].width,
	 &defaults[revdepcount].width,
	 &defaults[autoset].width,
	 &defaults[tagged].width,
	 &defaults[archive].width,
	 &defaults[brokencount].width,
	 &defaults[diskusage].width,
	 &defaults[downloadsize].width,
	 &defaults[hostname].width);
}

void pkg_item::pkg_columnizer::setup_columns(bool force_update)
{
  // If we haven't been initialized yet, set up the translated formatting
  // stuff.
  if(!columns)
    init_formatting();

  if(force_update)
    delete columns;
  if(force_update || !columns)
    {
      std::wstring cfg;

      if(!transcode(aptcfg->Find(PACKAGE "::UI::Package-Display-Format",
				 default_pkgdisplay).c_str(),
		    cfg))
	_error->Errno("iconv", _("Unable to transcode package display format after \"%ls\""), cfg.c_str());
      else
	columns=parse_columns(cfg,
			      pkg_columnizer::parse_column_type,
			      pkg_columnizer::defaults);
      if(!columns)
	{
	  cfg.clear();
	  if(!transcode(default_pkgdisplay, cfg))
	    _error->Errno("iconv", _("Unable to transcode package display format after \"%ls\""), cfg.c_str());
	  else
	    columns=parse_columns(cfg,
				  pkg_columnizer::parse_column_type,
				  pkg_columnizer::defaults);
	  if(!columns)
	    {
	      _error->Warning(_("Internal error: Default column string is unparsable"));
	      columns->push_back(column_definition(column_definition::COLUMN_GENERATED, pkg_columnizer::name, pkg_columnizer::defaults[pkg_columnizer::name].width, true, true, false));
	    }
	}
    }
}

std::wstring pkg_item::pkg_columnizer::format_column_names(unsigned int width)
{
  setup_columns();

  empty_column_parameters p;
  return pkg_genheaders(*columns).layout_columns(width, p);
}
