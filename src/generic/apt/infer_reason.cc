// infer_reason.cc
//
// Copyright 2004 Daniel Burrows

#include "infer_reason.h"

#include "apt.h"
#include "config_signal.h"

#include "rev_dep_iterator.h"

#include <apt-pkg/pkgsystem.h>
#include <apt-pkg/version.h>

#include <set>

using namespace std;

// Report dependencies in the order:
//   PreDepends, Depends, Recommends, Conflicts, Suggests, Replaces, Obsoletes
static int cmp_dep_types(unsigned char A, unsigned char B)
{
  if(A==B)
    return 0;

  switch(A)
    {
    case pkgCache::Dep::PreDepends:
      return -1;
    case pkgCache::Dep::Depends:
      if(B==pkgCache::Dep::PreDepends)
	return 1;
      else
	return -1;
    case pkgCache::Dep::Recommends:
      switch(B)
	{
	case pkgCache::Dep::PreDepends:
	case pkgCache::Dep::Depends:
	  return 1;
	default:
	  return -1;
	}
    case pkgCache::Dep::Conflicts:
      switch(B)
	{
	case pkgCache::Dep::PreDepends:
	case pkgCache::Dep::Depends:
	case pkgCache::Dep::Recommends:
	  return 1;
	default:
	  return -1;
	}
    case pkgCache::Dep::Suggests:
      switch(B)
	{
	case pkgCache::Dep::Replaces:
	case pkgCache::Dep::Obsoletes:
	  return -1;
	default:
	  return 1;
	}
    case pkgCache::Dep::Replaces:
      if(B==pkgCache::Dep::Obsoletes)
	return -1;
      else
	return 1;
    case pkgCache::Dep::Obsoletes:
      return 1;

    default:
      return 1;
    }
}

bool operator<(const reason &a, const reason &b)
{
  int d=cmp_dep_types(a.dep->Type, b.dep->Type);

  return d<0 || (d==0 && strcmp(a.pkg.Name(), b.pkg.Name())<0);
}


static bool relevant_dep(pkgCache::VerIterator ver, pkgCache::DepIterator d)
{
  // Check whether the package doing the depending is going
  // to be installed.
  pkgCache::PkgIterator depender=d.ParentPkg();
  pkgDepCache::StateCache &depstate=(*apt_cache_file)[depender];
  pkgCache::VerIterator depinstver=depstate.InstVerIter(*apt_cache_file);

  if(!depinstver.end() &&
     _system->VS->CheckDep(ver.VerStr(),
			   d->CompareOp, d.TargetVer()))
    {
      if(d.ParentVer()==depinstver)
	return true;
    }

  return false;
}

/** Returns \b true if the given dependency is an indirect
 *  self-conflict on a pure virtual package that is not provided by
 *  any other package which is to be installed. (the last condition is
 *  because packages which will not be installed are uninteresting for
 *  a Conflicts)
 *
 *  \param dep the dependency to test.
 */
static bool is_simple_self_conflict(pkgCache::DepIterator dep) {
  pkgCache::PkgIterator target=dep.TargetPkg();
  pkgDepCache::StateCache depstate=(*apt_cache_file)[target];

  if(!((target.CurrentVer().end() && depstate.Keep()) ||
       depstate.Delete()))
    return false;

  if(dep.TargetPkg().ProvidesList().end())
    return false;
  else
    for(pkgCache::PrvIterator P=dep.TargetPkg().ProvidesList();
	!P.end(); ++P)
      {
	pkgCache::PkgIterator pkg=P.OwnerPkg();
	pkgDepCache::StateCache state=(*apt_cache_file)[pkg];

	if(P.OwnerPkg()!=dep.ParentPkg() &&
	   !((pkg.CurrentVer().end() && state.Keep()) ||
	     state.Delete()))
	  return false;
      }

  return true;
}

void infer_reason(pkgCache::PkgIterator pkg, set<reason> &reasons)
{
  pkg_action_state actionstate=find_pkg_state(pkg);

  pkgDepCache::StateCache &state=(*apt_cache_file)[pkg];
  pkgCache::VerIterator instver=state.InstVerIter(*apt_cache_file);
  pkgCache::VerIterator candver=state.CandidateVerIter(*apt_cache_file);

  if(actionstate==pkg_auto_install)
    {
      for(rev_dep_iterator d(instver); !d.end(); ++d)
	if((*d)->Type!=pkgCache::Dep::Conflicts &&
	   relevant_dep(instver, *d))
	  reasons.insert(reason((*d).ParentPkg(), *d));
    }
  else if(actionstate==pkg_unchanged && pkg.CurrentVer().end())
    // Add notes about packages that Recommend or Suggest this.
    {
      for(rev_dep_iterator d(pkg); !d.end(); ++d)
	if((*d)->Type==pkgCache::Dep::Suggests ||
	   (*d)->Type==pkgCache::Dep::Recommends)
	  if(!candver.end() && relevant_dep(candver, *d))
	    reasons.insert(reason((*d).ParentPkg(), *d));
    }
  // Non-unused removed packages: was one of their dependents
  //                             removed?  Maybe a conflicting package.
  //
  // FIXME: version numbers?
  else if(actionstate==pkg_auto_remove)
    {
      // Look for *other* packages that conflict with this one.
      for(rev_dep_iterator d(pkg.CurrentVer()); !d.end(); ++d)
	if(((*d)->Type==pkgCache::Dep::Conflicts &&
	   relevant_dep(pkg.CurrentVer(), *d) &&
	   !is_simple_self_conflict(*d)) ||
	   (*d)->Type==pkgCache::Dep::Obsoletes)
	  reasons.insert(reason((*d).ParentPkg(), *d));

      for(pkgCache::DepIterator d=pkg.CurrentVer().DependsList(); !d.end(); ++d)
	{
	  // FIXME: O(n^2) -- store the whole or list and iterate
	  // over it at the end.
	  pkgCache::DepIterator or_end=d;
	  while(or_end->CompareOp & pkgCache::Dep::Or)
	    ++or_end;

	  if((or_end.IsCritical() ||
	      or_end->Type==pkgCache::Dep::Recommends ||
	      or_end->Type==pkgCache::Dep::Suggests) &&
	     ((*apt_cache_file)[or_end]&pkgDepCache::DepGNow) &&
	     !((*apt_cache_file)[or_end]&pkgDepCache::DepGInstall))
	    reasons.insert(reason(d.TargetPkg(), d));
	}
    }
  else if(actionstate==pkg_unused_remove)
    {
      // Who used to depend on this package and is now being removed?
      //
      // FIXME: should I walk backwards up the dependency chain until
      //       finding something manually installed?
      for(pkgCache::DepIterator d=pkg.RevDependsList(); !d.end(); ++d)
	if(d.ParentVer()==d.ParentPkg().CurrentVer() &&
	   ((*apt_cache_file)[d.ParentPkg()].Delete() ||
	    (*apt_cache_file)[d.ParentPkg()].InstVerIter(*apt_cache_file)!=d.ParentVer()) &&
	   (d->Type==pkgCache::Dep::Depends ||
	    (aptcfg->FindB(PACKAGE "::Recommends-Important", true) &&
	     d->Type==pkgCache::Dep::Recommends)))
	  reasons.insert(reason(d.ParentPkg(), d));
    }
  else if(actionstate==pkg_auto_hold)
    {
      // hm, I doubt this is sufficient, but test cases are hard to
      // come by :(
      for(pkgCache::DepIterator d=instver.DependsList();
	  !d.end(); ++d)
	//	    if(d.IsCritical())
	{
	  pkgCache::DepIterator or_end=d;
	  while(or_end->CompareOp & pkgCache::Dep::Or)
	    ++or_end;

	  if(!((*apt_cache_file)[or_end]&pkgDepCache::DepGInstall))
	    reasons.insert(reason(d.TargetPkg(), d));
	}
    }
  else if(actionstate==pkg_broken)
    {
      if(state.InstBroken())
	for(pkgCache::DepIterator d=state.InstVerIter(*apt_cache_file).DependsList();
	    !d.end(); ++d)
	  if(d.IsCritical())
	    {
	      pkgCache::DepIterator or_end=d;
	      while(or_end->CompareOp & pkgCache::Dep::Or)
		++or_end;

	      if(!((*apt_cache_file)[or_end]&pkgDepCache::DepGInstall))
		reasons.insert(reason(d.TargetPkg(), d));
	    }
    }

}

/** Infer reverse breakage information based on the given dependency. */
void infer_reverse_breakage(pkgCache::PkgIterator &pkg,
			    pkgCache::DepIterator &dep,
			    set<reason> &reasons)
{
  // Only look at dependencies whose source is installed.
  pkgCache::VerIterator parentinstver=(*apt_cache_file)[dep.ParentPkg()].InstVerIter(*apt_cache_file);
  if(parentinstver.end() || parentinstver!=dep.ParentVer())
    return;

  if(dep->Type==pkgCache::Dep::Depends || dep->Type==pkgCache::Dep::PreDepends)
    {
      // Find a forward dependency and move to the last element
      // of the OR group.
      pkgCache::DepIterator f(*apt_cache_file, &*dep);
      while(f->CompareOp & pkgCache::Dep::Or)
	++f;

      // If the dependency is broken, then this package is a
      // potential resolver (by definition)
      if(!((*apt_cache_file)[f]&pkgDepCache::DepGInstall))
	reasons.insert(reason(dep.ParentPkg(), dep));
    }
  // Look at any conflict that's not an immediate self-conflict
  else if(dep->Type==pkgCache::Dep::Conflicts &&
	  (dep.TargetPkg()!=pkg || dep.ParentPkg()!=pkg))
    {
      pkgCache::VerIterator instver=(*apt_cache_file)[pkg].InstVerIter(*apt_cache_file);

      if(!instver.end() &&
	 _system->VS->CheckDep(instver.VerStr(),
			       dep->CompareOp,
			       dep.TargetVer()))
	{
	  // exclude self-conflicts through reverse provides.
	  if(dep.ParentPkg()==pkg && !dep.TargetVer() && is_simple_self_conflict(dep))
	    return;

	  reasons.insert(reason(dep.ParentPkg(), dep));
	}
    }
}

struct cmp_pkg_mem
{
  bool operator()(const pkgCache::PkgIterator &P1,
		  const pkgCache::PkgIterator &P2) const
  {
    return less<const pkgCache::Package *>()(&*P1, &*P2);
  }
};

void infer_reverse_breakage(pkgCache::PkgIterator pkg,
			    set<reason> &reasons)
{
  for(pkgCache::DepIterator D=pkg.RevDependsList(); !D.end(); ++D)
    infer_reverse_breakage(pkg, D, reasons);

  set<pkgCache::PkgIterator, cmp_pkg_mem> seen;

  // Look at the list of stuff provided by any version.
  for(pkgCache::VerIterator V=pkg.VersionList(); !V.end(); ++V)
    for(pkgCache::PrvIterator P=V.ProvidesList(); !P.end(); ++P)
      {
	pkgCache::PkgIterator prv_pkg=P.ParentPkg();

	if(seen.find(prv_pkg)==seen.end())
	  {
	    seen.insert(prv_pkg);
	    for(pkgCache::DepIterator D=prv_pkg.RevDependsList(); !D.end(); ++D)
	      // Provides can't be versioned, ignore versioned
	      // dependencies on provided packages.
	      if(!D.TargetVer())
		infer_reverse_breakage(pkg, D, reasons);
	  }
      }
}
