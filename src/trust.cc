// trust.cc

#include "aptitude.h"
#include "trust.h"

#include <generic/apt/apt.h>

#include <vscreen/fragment.h>
#include <vscreen/config/colors.h>

fragment *make_untrusted_warning(const pkgCache::VerIterator &ver)
{
  if(package_trusted(ver))
    return NULL;
  else
    return indentbox(0,
		     strlen(_("WARNING"))+2,
		     flowbox(fragf(_("%F: This version of %s is from an %Buntrusted source%b!  Installing this package could allow a malicious individual to damage or take control of your system."),
				   text_fragment(_("WARNING"),
						 get_style("TrustWarning")),
				   ver.ParentPkg().Name())));
}
