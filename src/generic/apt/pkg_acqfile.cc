// pkg_acqfile.cc
//
//  Copyright 2002 Daniel Burrows
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

// (based on pkg_changelog)

#include "pkg_acqfile.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <aptitude.h>

#include <cstdio>
#include <cerrno>
#include <unistd.h>
#include <fcntl.h>
#include <csignal>
#include <sys/stat.h>

#include <apt-pkg/error.h>
#include <apt-pkg/configuration.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/sourcelist.h>
#include <apt-pkg/strutl.h>

pkgAcqFileSane::pkgAcqFileSane(pkgAcquire *Owner, string URI,
			       string Description, string ShortDesc,
			       string filename):
  Item(Owner)
{
  Retries=_config->FindI("Acquire::Retries",0);
  DestFile=filename;

  Desc.URI=URI;
  Desc.Description=Description;
  Desc.Owner=this;
  Desc.ShortDesc=ShortDesc;

  QueueURI(Desc);
}

// Straight from acquire-item.cc
/* Here we try other sources */
void pkgAcqFileSane::Failed(const string Message,pkgAcquire::MethodConfig *Cnf)
{
  ErrorText = LookupTag(Message,"Message");

  // This is the retry counter
  if (Retries != 0 &&
      Cnf->LocalOnly == false &&
      StringToBool(LookupTag(Message,"Transient-Failure"),false) == true)
    {
      Retries--;
      QueueURI(Desc);
      return;
    }

  Item::Failed(Message,Cnf);
}

// Mostly copied from pkgAcqArchive.
bool get_archive(pkgAcquire *Owner, pkgSourceList *Sources,
		 pkgRecords *Recs, pkgCache::VerIterator const &Version,
		 string directory, string &StoreFilename)
{
  pkgCache::VerFileIterator Vf=Version.FileList();

  if(Version.Arch()==0)
    return _error->Error(_("I wasn't able to locate a file for the %s package. "
			   "This might mean you need to manually fix this package. (due to missing arch)"),
			 Version.ParentPkg().Name());

  /* We need to find a filename to determine the extension. We make the
     assumption here that all the available sources for this version share
     the same extension.. */
  // Skip not source sources, they do not have file fields.
  for (; Vf.end() == false; Vf++)
    {
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) != 0)
	continue;
      break;
    }

  // Does not really matter here.. we are going to fail out below
  if (Vf.end() != true)
    {     
      // If this fails to get a file name we will bomb out below.
      pkgRecords::Parser &Parse = Recs->Lookup(Vf);
      if (_error->PendingError() == true)
	return false;
            
      // Generate the final file name as: package_version_arch.foo
      StoreFilename = QuoteString(Version.ParentPkg().Name(),"_:") + '_' +
	QuoteString(Version.VerStr(),"_:") + '_' +
	QuoteString(Version.Arch(),"_:.") + 
	"." + flExtension(Parse.FileName());
    }

   for (; Vf.end() == false; Vf++)
   {
      // Ignore not source sources
      if ((Vf.File()->Flags & pkgCache::Flag::NotSource) != 0)
         continue;

      // Try to cross match against the source list
      pkgIndexFile *Index;
      if (Sources->FindIndex(Vf.File(),Index) == false)
            continue;
      
      // Grab the text package record
      pkgRecords::Parser &Parse = Recs->Lookup(Vf);
      if (_error->PendingError() == true)
         return false;

      string PkgFile = Parse.FileName();
      if (PkgFile.empty() == true)
         return _error->Error(_("The package index files are corrupted. No Filename: "
                              "field for package %s."),
                              Version.ParentPkg().Name());

      string DestFile = directory + "/" + flNotDir(StoreFilename);

      // Create the item
      new pkgAcqFileSane(Owner, Index->ArchiveURI(PkgFile),
			 Index->ArchiveInfo(Version),
			 Version.ParentPkg().Name(), DestFile);

      Vf++;
      return true;
   }
   return false;
}
