// reason_fragment.cc
//
//  Copyright 2004 Daniel Burrows

#include "reason_fragment.h"

#include "aptitude.h"
#include "pkg_item.h"
#include "pkg_ver_item.h"
#include "ui.h"

#include <generic/apt/apt.h>
#include <generic/apt/infer_reason.h>

#include <vscreen/config/colors.h>
#include <vscreen/fragment.h>

#include <functional>
#include <set>

using namespace std;

/** Returns a fragment describing a dependency (as in "depends
 *  on" or "suggests" rather than "Depends" or "Suggests")
 */
fragment *depname_frag(pkgCache::DepIterator dep)
{
  switch(dep->Type)
    {
    case pkgCache::Dep::Depends: return text_fragment(_("depends on"),
						      style_attrs_on(A_BOLD));
    case pkgCache::Dep::PreDepends: return text_fragment(_("pre-depends on"),
							 style_attrs_on(A_BOLD));
    case pkgCache::Dep::Suggests: return text_fragment(_("suggests"));
    case pkgCache::Dep::Recommends: return text_fragment(_("recommends"),
							 style_attrs_on(A_BOLD));
    case pkgCache::Dep::Conflicts: return text_fragment(_("conflicts with"),
							style_attrs_on(A_BOLD));
    case pkgCache::Dep::Replaces: return text_fragment(_("replaces"));
    case pkgCache::Dep::Obsoletes: return text_fragment(_("obsoletes"));
    }

  // Untranslated (internal error that will only happen if things go
  // entirely wonky, and I want to be able to understand it if it
  // appears)
  return text_fragment("has an invalid dependency type!", get_style("Error"));
}

/** Compare two packages by name */
struct pkg_name_cmp
{
  bool operator()(pkgCache::PkgIterator P1,
		  pkgCache::PkgIterator P2)
  {
    return strcmp(P1.Name(), P2.Name())<0;
  }
};

/** Compare two versions by memory location (useful for inserting into
 *  maps when the particular order is uninteresting)
 */
struct ver_ptr_cmp
{
  bool operator()(pkgCache::VerIterator V1,
		   pkgCache::VerIterator V2)
  {
    return less<void*>()(&*V1, &*V2);
  }
};

/** Generate a fragment describing the packages providing a given package.
 *
 *  \param pkg the package whose providers will be described.
 *  \param ignpkg a package which should not be shown as a provider.
 *
 *  \param installed a filter: if \b true, only providees which are
 *  installed, or which will be installed, are shown; otherwise, all
 *  providees are shown.
 */
fragment *prvfrag(pkgCache::PkgIterator pkg,
		  pkgCache::PkgIterator ignpkg,
		  bool installed)
{
  // All packages providing the given package name are listed.
  //
  // For each package, we check whether both its current and
  // candidate versions (if any) provide the name; if so, the
  // package name is listed, colorized for the package.  If only
  // one provides the name, its version is listed, colorized for
  // the version.

  vector<fragment*> fragments;

  set<pkgCache::VerIterator, ver_ptr_cmp> providing_versions;
  set<pkgCache::PkgIterator, pkg_name_cmp> providing_packages;

  pkgCache::VerIterator candver=(*apt_cache_file)[pkg].CandidateVerIter(*apt_cache_file);	

  for(pkgCache::PrvIterator P=pkg.ProvidesList();
      !P.end(); ++P)
    {
      if(P.OwnerPkg()!=ignpkg)
	{
	  providing_versions.insert(P.OwnerVer());
	  providing_packages.insert(P.OwnerPkg());
	}
    }

  for(set<pkgCache::PkgIterator>::const_iterator P=providing_packages.begin();
      P!=providing_packages.end(); ++P)
    {
      bool provided_curr=false, provided_cand=false;
      pkgCache::VerIterator currver=P->CurrentVer();
      pkgCache::VerIterator instver=(*apt_cache_file)[*P].InstVerIter(*apt_cache_file);	

      if(installed)
	{
	  if(instver.end() && currver.end())
	    continue;
	}
#if 0
      else
	{
	  if(!(!currver.end() && instver.end()))
	    continue;
	}
#endif

      if(!currver.end() &&
	 providing_versions.find(currver)!=providing_versions.end())
	provided_curr=true;
      if(!candver.end() &&
	 providing_versions.find(candver)!=providing_versions.end())
	provided_cand=true;

      if((currver.end() || provided_cand) &&
	 (currver.end() || provided_curr) &&
	 (provided_cand || provided_curr))
	fragments.push_back(text_fragment(P->Name(),
					  pkg_item::pkg_style(*P, false)));
      else if(provided_cand || provided_curr)
	{
	  pkgCache::VerIterator &pv=provided_cand?candver:currver;

	  fragments.push_back(style_fragment(fragf("%s %s",
						   P->Name(),
						   pv.VerStr()),
					     pkg_ver_item::ver_style(pv, false)));
	}
      else
	// Bail and print EVERYTHING IN SIGHT...not very efficiently, either.
	{
	  for(set<pkgCache::VerIterator>::const_iterator i=providing_versions.begin();
	      i!=providing_versions.end(); ++i)
	    if(i->ParentPkg()==*P)
	      fragments.push_back(style_fragment(fragf("%s %s",
						       P->Name(),
						       i->VerStr()),
						 pkg_ver_item::ver_style(*i, false)));
	}
    }

  if(fragments.size()==0)
    return fragf("");
  else
    return fragf(_(" (provided by %F)"),
		 join_fragments(fragments, L", "));
}

/** Generate a fragment describing the given dependency iterator. */
fragment *dep_singlefrag(pkgCache::PkgIterator pkg,
			 pkgCache::DepIterator dep)
{
  fragment *verfrag;

  pkgCache::VerIterator instver=(*apt_cache_file)[pkg].InstVerIter(*apt_cache_file);

  if(!dep.TargetVer())
    verfrag=text_fragment("");
  else
    {
      // Figure out the state of the versioned dep.
      //
      // Display it as "uninstalled" if it is not satisfied and won't
      // be; "installed" if it is satisfied and will be; "removed" if
      // it is satisfied but won't be, and "installing" if it is not
      // satisfied and will be.

      style verstyle;

      bool matches_now=!pkg.CurrentVer().end() &&
	_system->VS->CheckDep(pkg.CurrentVer().VerStr(),
			      dep->CompareOp,
			      dep.TargetVer());
      bool matches_inst=!instver.end() &&
	_system->VS->CheckDep(instver.VerStr(),
			      dep->CompareOp,
			      dep.TargetVer());

      if(matches_now)
	{
	  if(matches_inst)
	    verstyle=style_attrs_on(A_BOLD);
	  else
	    verstyle=get_style("PkgToRemove");
	}
      else
	{
	  if(matches_inst)
	    verstyle=get_style("PkgToInstall");
	}

      verfrag=fragf(" (%s %F)",
		    dep.CompType(),
		    text_fragment(dep.TargetVer(), verstyle));
    }

  bool available=false;

  for(pkgCache::VerIterator i=dep.TargetPkg().VersionList(); !i.end(); i++)
    if(_system->VS->CheckDep(i.VerStr(), dep->CompareOp, dep.TargetVer()))
      available=true;

  for(pkgCache::PrvIterator i=dep.TargetPkg().ProvidesList(); !i.end(); i++)
    if(_system->VS->CheckDep(i.ProvideVersion(), dep->CompareOp, dep.TargetVer()))
      available=true;

  return fragf("%F%F%F%s",
	       text_fragment(dep.TargetPkg().Name(),
			     pkg_item::pkg_style(dep.TargetPkg(), false)),
	       verfrag,
	       prvfrag(dep.TargetPkg(),
		       dep.ParentPkg(),
		       dep->Type==pkgCache::Dep::Conflicts),
	       available?"":(string(" [")+_("UNAVAILABLE")+"]").c_str());
}

/** Generate a fragment describing the OR group that contains the
 *  given dependency, assuming that we are examining pkg.  Assumes
 *  that duplicate OR dependencies are already dealt with in some way.
 */
fragment *dep_or_frag(pkgCache::PkgIterator pkg,
		      pkgCache::DepIterator dep)
{
  vector<fragment*> fragments;

  pkgCache::DepIterator or_begin, or_end;

  surrounding_or(dep, or_begin, or_end);

  for(pkgCache::DepIterator D=or_begin; D!=or_end; ++D)
    if(D->CompareOp&pkgCache::Dep::Or)
      fragments.push_back(fragf("%F | ",
				dep_singlefrag(pkg, D)));
    else
      fragments.push_back(dep_singlefrag(pkg, D));

  return fragf(_("%F %F %F"),
	       text_fragment(dep.ParentPkg().Name(),
			     pkg_item::pkg_style(dep.ParentPkg(), false)),
	       depname_frag(dep),
	       sequence_fragment(fragments));
}

typedef pair<pkgCache::DepIterator, pkgCache::DepIterator> deppair;

/** Return a fragment describing the reasons in the given vector. */
fragment *reasonsfrag(pkgCache::PkgIterator pkg, set<reason> &reasons)
{
  vector<fragment*> fragments;

  // Used to exclude dependencies from the same OR that show up twice.
  // If this is too expensive, switch to a set.
  vector<deppair> seen_ors;

  for(set<reason>::const_iterator i=reasons.begin(); i!=reasons.end(); ++i)
    {
      pkgCache::DepIterator depbegin, depend;
      surrounding_or(i->dep, depbegin, depend);

      bool seen=false;
      for(vector<deppair>::const_iterator j=seen_ors.begin();
	  j!=seen_ors.end(); ++j)
	if(j->first==depbegin && j->second==depend)
	  seen=true;

      if(!seen)
	{
	  seen_ors.push_back(deppair(depbegin, depend));

	  fragment *itemtext=dep_or_frag(pkg, i->dep);

	  fragments.push_back(sequence_fragment(text_fragment("  * ",
							      get_style("Bullet")),
						indentbox(0, 4, flowbox(itemtext)),
						NULL));
	}
    }

  return sequence_fragment(fragments);
}

/** Return a fragment describing the lack of a package. */
fragment *nopackage()
{
  return wrapbox(text_fragment(_("If you select a package, an explanation of its current state will appear in this space.")));
}

fragment *reason_fragment(const pkgCache::PkgIterator &pkg)
{
  bool dummy;

  return reason_fragment(pkg, dummy);
}

fragment *reason_fragment(const pkgCache::PkgIterator &pkg, bool &breakage)
{
  breakage=false;

  if(pkg.end())
    return nopackage();

  set<reason> reasons;

  infer_reason(pkg, reasons);

  vector<fragment *> fragments;
  pkg_action_state actionstate=find_pkg_state(pkg);

  aptitudeDepCache::StateCache &state=(*apt_cache_file)[pkg];
  aptitudeDepCache::aptitude_state &estate=(*apt_cache_file)->get_ext_state(pkg);
  pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);
  pkgCache::VerIterator instver=state.InstVerIter(*apt_cache_file);	

  // TODO: get non-lame text...some of these are just placeholders and
  // should be reworded before release.
  switch(actionstate)
    {
    case pkg_unused_remove:
      fragments.push_back(wrapbox(fragf(_("%B%s%b was installed automatically;  it is being removed because all of the packages which depend upon it are being removed:"),
					pkg.Name())));
      break;
    case pkg_auto_remove:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be automatically removed because of dependency errors:"),
					pkg.Name())));
      break;
    case pkg_auto_install:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be automatically installed to satisfy the following dependencies:"),
					pkg.Name())));
      break;
    case pkg_auto_hold:
      {
	if(candver.end() || candver==pkg.CurrentVer())
	  fragments.push_back(wrapbox(fragf(_("%B%s%b cannot be upgraded now, but if it could be, it would still be held at version %B%s%b."),
					    pkg.Name(), pkg.CurrentVer().VerStr())));
	else
	  fragments.push_back(wrapbox(fragf(_("%B%s%b will not be upgraded to version %B%s%b, to avoid breaking the following dependencies:"),
					    pkg.Name(),
					    candver.VerStr())));
	break;
      }
    case pkg_unchanged:
      if(!pkg.CurrentVer().end())
	{
	  if((*apt_cache_file)->is_held(pkg))
	    fragments.push_back(wrapbox(fragf(_("%B%s%b cannot be upgraded now, but if it could be, it would still be held at version %B%s%b."),
					      pkg.Name(), pkg.CurrentVer().VerStr())));
	  else
	    fragments.push_back(wrapbox(fragf(_("%B%s%b is currently installed."),
					      pkg.Name())));
	  break;
	}
      else
	{
	  fragments.push_back(wrapbox(fragf(_("%B%s%b is not currently installed."),
					    pkg.Name())));

	  break;
	}
    case pkg_broken:
      breakage=true;

      fragments.push_back(wrapbox(fragf(_("Some dependencies of %B%s%b are not satisfied:"),
					pkg.Name())));
      break;
    case pkg_downgrade:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be downgraded."),
					pkg.Name())));
      break;
    case pkg_hold:
      {
	if(estate.selection_state != pkgCache::State::Hold &&
	   !candver.end() && candver.VerStr() == estate.forbidver)
	  fragments.push_back(wrapbox(fragf(_("%B%s%b will not be upgraded to the forbidden version %B%s%b."),
					    pkg.Name(),
					    candver.VerStr())));
	else
	  fragments.push_back(wrapbox(fragf(_("%B%s%b could be upgraded to version %B%s%b, but it is being held at version %B%s%b."),
					    pkg.Name(),
					    candver.VerStr(),
					    pkg.CurrentVer().VerStr())));
      }
      break;
    case pkg_reinstall:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be re-installed."),
					pkg.Name())));
      break;
    case pkg_install:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be installed."),
					pkg.Name())));
      break;
    case pkg_remove:
      fragments.push_back(wrapbox(fragf(_("%B%s%b will be removed."),
					pkg.Name())));
      break;
    case pkg_upgrade:
      {
	fragments.push_back(wrapbox(fragf(_("%B%s%b will be upgraded from version %B%s%b to version %B%s%b."),
					  pkg.Name(),
					  pkg.CurrentVer().VerStr(),
					  candver.VerStr(), A_BOLD)));
      }
      break;
    default:
      // Another non-translatable internal error.
      fragments.push_back(wrapbox(fragf("Internal error: Unknown package state for %s!",
					pkg.Name())));
    }


  if(!reasons.empty())
    fragments.push_back(sequence_fragment(newline_fragment(),
					  newline_fragment(),
					  NULL));

  fragments.push_back(reasonsfrag(pkg, reasons));

  reasons.clear();

  infer_reverse_breakage(pkg, reasons);

  if(!reasons.empty())
    {
      breakage=true;

      fragments.push_back(sequence_fragment(newline_fragment(),
					    newline_fragment(),
					    NULL));

      // It will end up un-installed.
      if(instver.end())
	{
	  if(state.Delete())
	    fragments.push_back(wrapbox(fragf(_("The following packages depend on %B%s%b and will be broken by its removal:"),
					      pkg.Name())));
	  else
	    fragments.push_back(wrapbox(fragf(_("The following packages depend on %B%s%b and are broken:"),
					      pkg.Name())));
	}
      // It will end up installed.
      else
	{
	  if(pkg.CurrentVer().end())
	    fragments.push_back(wrapbox(fragf(_("The following packages conflict with %B%s%b and will be broken by its installation:"),
					      pkg.Name())));
	  else
	    // up/downgrade; could be either Depends or Conflicts
	    {
	      bool has_depends=false;
	      bool has_conflicts=false;
	      bool now_broken=false;
	      bool inst_broken=false;

	      for(set<reason>::const_iterator i=reasons.begin();
		  i!=reasons.end(); ++i)
		{
		  if(i->dep->Type == pkgCache::Dep::Conflicts)
		    has_conflicts=true;
		  else
		    has_depends=true;

		  // um, these are not entirely accurate :-(
		  //
		  // They show whether the whole dependency is ok, not
		  // whether this particular part is ok. (think ORed deps)
		  if(!((*apt_cache_file)[i->dep]&pkgDepCache::DepGNow))
		    now_broken=true;
		  if(!((*apt_cache_file)[i->dep]&pkgDepCache::DepGInstall))
		    inst_broken=true;
		}

	      if(state.Keep() || (now_broken && inst_broken))
		{
		  if(has_conflicts && has_depends)
		    {
		      if(state.Keep())
			fragments.push_back(wrapbox(fragf(_("The following packages depend on a version of %B%s%b other than the currently installed version of %B%s%b, or conflict with the currently installed version:"),
							  pkg.Name(),
							  pkg.CurrentVer().VerStr())));
		      else
			fragments.push_back(wrapbox(fragf(_("The following packages conflict with %B%s%b, or depend on a version of it which is not going to be installed."),
							  pkg.Name())));
		    }
		  else if(has_conflicts)
		    fragments.push_back(wrapbox(fragf(_("The following packages conflict with %B%s%b:"),
						      pkg.Name())));
		  else if(has_depends)
		    {
		      if(state.Keep())
			fragments.push_back(wrapbox(fragf(_("The following packages depend on a version of %B%s%b other than the currently installed version of %B%s%b:"),
							  pkg.Name(),
							  pkg.CurrentVer().VerStr())));
		      else
			fragments.push_back(wrapbox(fragf(_("The following packages depend on a version of %B%s%b which is not going to be installed."),
							  pkg.Name())));
		    }
		}
	      else
		{
		  const char *actionname=(actionstate==pkg_upgrade)?_("upgraded"):_("downgraded");

		  if(has_conflicts && has_depends)
		    // I hope this is ok for the translators :-/ --
		    // factoring out upgraded/downgraded in its two senses
		    // would be a royal pain even if gettext supported it.
		    fragments.push_back(wrapbox(fragf(_("The following packages depend on the currently installed version of %B%s%b (%B%s%b), or conflict with the version it will be %s to (%B%s%b), and will be broken if it is %s."),
						      pkg.Name(),
						      pkg.CurrentVer().VerStr(),
						      actionname,
						      instver.VerStr(),
						      actionname)));
		  else if(has_conflicts)
		    fragments.push_back(wrapbox(fragf(_("The following packages conflict with version %B%s%b of %B%s%b, and will be broken if it is %s."),
						      instver.VerStr(),
						      pkg.Name(),
						      actionname)));
		  else if(has_depends)
		    fragments.push_back(wrapbox(fragf(_("The following packages depend on version %B%s%b of %B%s%b, and will be broken if it is %s."),
						      pkg.CurrentVer().VerStr(),
						      pkg.Name(),
						      actionname)));
		}
	    }
	}

      fragments.push_back(sequence_fragment(newline_fragment(),
					    newline_fragment(),
					    NULL));

      fragments.push_back(reasonsfrag(pkg, reasons));
    }

  return sequence_fragment(fragments);
}
