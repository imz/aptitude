// cmdline_changelog.cc
//
//  Copyright 2004 Daniel Burrows

#include "cmdline_changelog.h"

#include "cmdline_common.h"
#include "cmdline_util.h"

#include <aptitude.h>

#include <generic/util/temp.h>
#include <generic/apt/apt.h>
#include <generic/apt/config_signal.h>

#include <apt-pkg/error.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/srcrecords.h>

#include <sigc++/adaptors/bind.h>

#include <stdlib.h>

using namespace std;

bool do_cmdline_changelog(const vector<string> &packages)
{
  const char *pager;

  pager=getenv("PAGER");

  if(pager==NULL)
    pager="more";

  string default_release = aptcfg->Find("APT::Default-Release");

  for(vector<string>::const_iterator i=packages.begin(); i!=packages.end(); ++i)
    {
      string input=*i;

      cmdline_version_source source;
      string package, sourcestr;

      if(!cmdline_parse_source(input, source, package, sourcestr))
	continue;

      if(source == cmdline_version_cand && !default_release.empty())
	{
	  source    = cmdline_version_archive;
	  sourcestr = default_release;
	}

      pkgCache::PkgIterator pkg=(*apt_cache_file)->FindPkg(package);

      temp::dir tempdir;
      temp::name tempname;

      // For real packages/versions, we can do a sanity check.
      if(!pkg.end())
	{

	  pkgCache::VerIterator ver=cmdline_find_ver(pkg,
						     source, sourcestr);
	  pkgRecords::Parser &rec=apt_package_records->Lookup(ver.FileList());
	  string changelog = rec.Changelog();

	  if (changelog.size() > 0)
	    {
	      try
		{
		  tempdir = temp::dir("aptitude");
		  tempname = temp::name(tempdir, "changelog");
		}
		catch(temp::TemporaryCreationFailure e)
		{
		  _error->Error("%s", e.errmsg().c_str());
		  continue;
		}

	      FileFd fd;

	      fd.Open(tempname.get_name(), FileFd::WriteEmpty, 0644);

	      if (!fd.IsOpen())
		{
		  _error->Error(_("Cannot open Aptitude temporary file"));
		  continue;
		}
	      else
		{
		  if (fd.Failed() || !fd.Write(changelog.c_str(), changelog.size()))
		    {
		      _error->Error(_("Couldn't write shangelog"));
		      fd.Close();

		      continue;
		    }
		  fd.Close();
		}
	    }
	}

      if(!tempname.valid())
	_error->Error(_("Couldn't find a changelog for %s"), input.c_str());
      else
	// Run the user's pager.
	system((string(pager) + " " + tempname.get_name()).c_str());
    }

  return true;
}

// TODO: fetch them all in one go.
int cmdline_changelog(int argc, char *argv[])
{
  _error->DumpErrors();

  OpProgress progress;
  apt_init(&progress, false);

  if(_error->PendingError())
    {
      _error->DumpErrors();
      return -1;
    }

  vector<string> packages;
  for(int i=1; i<argc; ++i)
    packages.push_back(argv[i]);

  do_cmdline_changelog(packages);

  _error->DumpErrors();

  return 0;
}
