// load_grouppolicy.cc
//
//  Copyright 2001 Daniel Burrows
//
//  Routines to parse grouping policies
//
// This uses a bunch of little functions to parse stuff.  Sue me.
//
// The parsers take an arglist and a chain argument, and return either a
// new pkg_grouppolicy_factory, or (if an error was encountered) NULL.
// (note: the chain argument will be NULL if the policy is the tail of a list)
//
// The astute observer will wonder why I don't just use bison or
// something similar.  The main reason is also the main reason I can't
// just do a straightforward parse: the matcher grammar.
// Unfortunately, it's hard to assemble grammars modularly in yacc,
// and I don't want to duplicate the matchers' grammar in several
// places; there's also the "minor" problem that the lexical analysis
// of matchers is radically different from the rest of the constructs.
//
// TODO: a lot of classes just exist to complain about having
// arguments and then creating a new instance of a policy0;
// commonalize this.

#include "load_grouppolicy.h"

#include <generic/apt/matchers.h>

#include <generic/util/util.h>

#include "aptitude.h"

#include "pkg_grouppolicy.h"
#include "pkg_ver_item.h"
#include "dep_item.h"

#include <generic/apt/apt.h>

#include <apt-pkg/error.h>

#include <map>

using namespace std;

class GroupParseException
{
  string msg;
public:
  GroupParseException(const char *format,
		      ...)
#ifdef __GNUG__
    __attribute__ ((format (printf, 2, 3)))
#endif
  {
    va_list args;

    va_start(args, format);

    msg = vssprintf(format, args);

    va_end(args);
  }

  const string &get_msg() const {return msg;}
};

// Since I fully parse grouping policies before instantiating them,
// it's necessary to represent the parse "tree" here:

/** Generic representation of a node in the grouping policy parse tree. */
class group_policy_parse_node
{
public:
  virtual ~group_policy_parse_node()
  {
  }

  /** If \b true, the policy must be at the end of a chain and is
   *  expected to properly handle a \b NULL chain value.  If \b false,
   *  it must not be at the end of a chain (chains will be
   *  automatically capped as necessary).
   *
   *  By default, this returns \b false.
   */
  virtual bool terminal() {return false;}


  /** Instantiates the corresponding grouping policy; chain will be
   *  \b NULL if no chain was created.
   */
  virtual pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain)=0;
};

/** Represents a parser for a particular type of argument. */
class group_policy_parser
{
public:
  virtual ~group_policy_parser()
  {
  }

  /** Parse the given string range into a grouping policy. */
  virtual group_policy_parse_node *parse(string::const_iterator &begin,
					 const string::const_iterator &end)=0;
};



template<class F>
class policy_terminal_node0 : public group_policy_parse_node
{
public:
  bool terminal() override
  {
    return true;
  }

  pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain) override
  {
    if(chain != NULL)
      throw GroupParseException("Internal error: non-NULL chain passed to terminal policy");

    return new F;
  }
};

// stupid; apparently you can't use string literals as template args.
typedef policy_terminal_node0<pkg_grouppolicy_end_factory> group_policy_end_node;

/** A grouping policy node that pairs two others. */
class group_policy_pair_node : public group_policy_parse_node
{
  class group_policy_parse_node *left, *right;
public:
  group_policy_pair_node(group_policy_parse_node *_left,
			 group_policy_parse_node *_right)
    :left(_left), right(_right)
  {
  }

  bool terminal() override
  {
    return right->terminal();
  }

  ~group_policy_pair_node()
  {
    delete left;
    delete right;
  }

  pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain) override
  {
    return left->instantiate(right->instantiate(chain));
  }
};

/** Convenience classes to generate grouping policy nodes that
 *  generate a particular type of factory.
 */
template<class F>
class policy_node0 : public group_policy_parse_node
{
public:
  pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain) override
  {
    return new F(chain);
  }
};

template<class F, typename A1>
class policy_node1 : public group_policy_parse_node
{
  A1 arg1;
public:
  policy_node1(const A1 &_arg1) : arg1(_arg1)
  {
  }

  pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain) override
  {
    return new F(arg1, chain);
  }
};

template<class F, typename A1, typename A2>
class policy_node2 : public group_policy_parse_node
{
  A1 arg1;
  A2 arg2;
public:
  policy_node2(const A1 &_arg1, const A2 &_arg2)
    : arg1(_arg1), arg2(_arg2)
  {
  }

  pkg_grouppolicy_factory *instantiate(pkg_grouppolicy_factory *chain) override
  {
    return new F(arg1, arg2, chain);
  }
};




// Generic parsers:

/** Parse a grouping policy with no arguments. */
template<class PN>
class null_policy_parser : public group_policy_parser
{
public:
  group_policy_parse_node *parse(string::const_iterator &,
				 const string::const_iterator &)
  {
    return new PN;
  }
};

/** Parse a grouping policy based on a list.  Looks for policy names,
 *  then starts parsing their parameters as necessary according to a
 *  table of parsers.  Names must be alphanumeric; the occurance
 *  of a nonalphanumeric at the top level will terminate the parse.
 */
class list_policy_parser : public group_policy_parser
{
public:
  typedef map<string, group_policy_parser *> parsemap;

private:
  const parsemap &parsers;
public:
  list_policy_parser(const parsemap &_parsers)
    :parsers(_parsers)
  {
  }

  group_policy_parse_node *parse(string::const_iterator &begin,
				 const string::const_iterator &end)
    override
  {
    string name;
    auto_ptr<group_policy_parse_node> rval(NULL);

    while(begin != end)
      {
	string last = name;
	name.clear();

	// This might be a bit overly eager to eat commas, but it
	// shouldn't do any harm.
	while(begin != end && (isspace(*begin) || *begin == ','))
	  ++begin;

	if(begin != end && !isalnum(*begin))
	  throw GroupParseException(_("Expected policy identifier, got '%c'"), *begin);

	while(begin != end && isalnum(*begin))
	  {
	    name += *begin;
	    ++begin;
	  }

	while(begin != end && isspace(*begin))
	  ++begin;

	if(begin != end && *begin != ',' && *begin != '(')
	  throw GroupParseException(_("Expected ',' or '(', got '%c'"), *begin);

	if(!name.empty())
	  {
	    parsemap::const_iterator found = parsers.find(name);

	    if(found == parsers.end())
	      throw GroupParseException(_("Unknown grouping policy \"%s\""),
					name.c_str());
	    else
	      {
		auto_ptr<group_policy_parse_node> curr(found->second->parse(begin, end));

		if(rval.get() != NULL && rval.get()->terminal())
		  throw GroupParseException(_("Terminal policy \"%s\" should be the last policy in the list"), last.c_str());


		if(rval.get() == NULL)
		  rval = curr;
		else
		  rval = auto_ptr<group_policy_parse_node>(new group_policy_pair_node(rval.release(), curr.release()));
	      }
	  }
      }

    if(rval.get() == NULL)
      rval = auto_ptr<group_policy_parse_node>(new group_policy_end_node);
    else if(!rval->terminal())
      rval = auto_ptr<group_policy_parse_node>(new group_policy_pair_node(rval.release(), new group_policy_end_node));

    eassert(rval->terminal());
    return rval.release();
  }
};

/** Parse a grouping policy based on a list of bare strings. */
class string_policy_parser : public group_policy_parser
{
public:
  virtual group_policy_parse_node *create_node(const vector<string> &args)=0;

  group_policy_parse_node *parse(string::const_iterator &begin,
				 const string::const_iterator &end)
    override
  {
    vector<string> args;

    while(begin != end && isspace(*begin))
      ++begin;

    if(begin != end && *begin == '(')
      {
	++begin;

	while(begin != end && *begin != ')')
	  {
	    string::const_iterator oldbegin=begin;

	    while(begin != end && *begin != ')' && *begin != ',')
	      ++begin;

	    string val(oldbegin, begin);
	    stripws(val);
	    args.push_back(val);

	    if(begin != end && *begin == ',')
	      {
		++begin;
		if(begin != end && *begin == ')')
		  args.push_back(string());
	      }
	  }
      }

    if(begin != end)
      ++begin;

    return create_node(args);
  }
};

class section_policy_parser : public string_policy_parser
{
public:
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    int split_mode=pkg_grouppolicy_section_factory::split_none;
    bool passthrough=false;

    if(args.size() >= 1)
      {
	if(!strcasecmp(args[0].c_str(), "none"))
	  split_mode=pkg_grouppolicy_section_factory::split_none;
	else if(!strcasecmp(args[0].c_str(), "topdir"))
	  split_mode=pkg_grouppolicy_section_factory::split_topdir;
	else if(!strcasecmp(args[0].c_str(), "subdir"))
	  split_mode=pkg_grouppolicy_section_factory::split_subdir;
	else
	  throw GroupParseException(_("Bad section name '%s' (use 'none', 'topdir', or 'subdir')"), args[0].c_str());
      }

    if(args.size() >= 2)
      {
	if(!strcasecmp(args[1].c_str(), "passthrough"))
	  passthrough=true;
	else if(!strcasecmp(args[1].c_str(), "nopassthrough"))
	  passthrough=false;
	else
	  throw GroupParseException(_("Bad passthrough setting '%s' (use 'passthrough' or 'nopassthrough')"),
				    args[1].c_str());
      }

    if(args.size()>2)
      throw GroupParseException(_("Too many arguments to by-section grouping policy"));

    return new policy_node2<pkg_grouppolicy_section_factory,
      int, bool>(split_mode, passthrough);
  }
};

class status_policy_parser : public string_policy_parser
{
public:
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("By-status grouping policies take no arguments"));

    return new policy_node0<pkg_grouppolicy_status_factory>;
  }
};

class filter_policy_parser : public group_policy_parser
{
public:
  group_policy_parse_node *parse(string::const_iterator &begin,
				 const string::const_iterator &end)
    override
  {
    // Backwards compatibility cruft:
    static const string missing = "missing";
    bool is_missing = true;

    if(begin != end && *begin != '(')
      throw GroupParseException(_("Expected '(' after 'filter'"));

    ++begin;

    string::const_iterator begin2 = begin;
    while(isspace(*begin2))
      ++begin2;

    for(string::const_iterator begin3 = missing.begin();
	is_missing && begin2 != end && begin3 != missing.end(); ++begin2, ++begin3)
      if(*begin2 != *begin3)
	is_missing = false;

    while(begin2 != end && isspace(*begin2))
      ++begin2;

    if(begin2 == end || *begin2 != ')')
      is_missing = false;

    if(is_missing)
      {
	pkg_matcher *m = parse_pattern("~T");
	begin = begin2;
	++begin;

	return new policy_node1<pkg_grouppolicy_filter_factory, pkg_matcher *>(m);
      }
    else
      {
	vector<const char *> terminators;
	terminators.push_back(",");
	terminators.push_back(")");

	auto_ptr<pkg_matcher> m(parse_pattern(begin, end, terminators,
					      false, true, false));

	if(m.get() == NULL)
	  throw GroupParseException(_("Unable to parse pattern at '%s'"),
				    string(begin, end).c_str());
	else if(begin != end && *begin != ')')
	  {
	    eassert(*begin == ',');
	    throw GroupParseException(_("Exactly one filter must be provided as an argument to a filter policy"));
	  }
	else
	  {
	    if(begin != end)
	      ++begin;

	    return new policy_node1<pkg_grouppolicy_filter_factory, pkg_matcher *>(m.release());
	  }
      }
  }
};

class mode_policy_parser : public string_policy_parser
{
public:
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("By-mode grouping policies take no arguments"));

    return new policy_node0<pkg_grouppolicy_mode_factory>;
  }
};

class firstchar_policy_parser : public string_policy_parser
{
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("First-character grouping policies take no arguments"));


    return new policy_node0<pkg_grouppolicy_firstchar_factory>;
  }
};

class ver_policy_parser : public string_policy_parser
{
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("Version-generating grouping policies take no arguments"));

    return new policy_terminal_node0<pkg_grouppolicy_ver_factory>;
  }
};

class dep_policy_parser: public string_policy_parser
{
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("Dep-generating grouping policies take no arguments"));

    return new policy_terminal_node0<pkg_grouppolicy_dep_factory>;
  }
};

class priority_policy_parser : public string_policy_parser
{
  group_policy_parse_node *create_node(const vector<string> &args) override
  {
    if(args.size()!=0)
      throw GroupParseException(_("By-priority grouping policies take no arguments"));

    return new policy_node0<pkg_grouppolicy_priority_factory>;
  }
};

class pattern_policy_parser : public group_policy_parser
{
  group_policy_parse_node *parse(string::const_iterator &begin,
				 const string::const_iterator &end)
    override
  {
    while(begin != end && isspace(*begin))
      ++begin;

    if(begin == end || *begin != '(')
      throw GroupParseException(_("Expected '(' after 'pattern'"));

    ++begin;

    while(begin != end && isspace(*begin))
      ++begin;

    if(begin == end || *begin == ')')
      throw GroupParseException(_("Missing arguments to 'pattern'"));

    vector<pkg_grouppolicy_matchers_factory::match_pair> subgroups;

    vector<const char *> terminators;
    terminators.push_back(",");
    terminators.push_back("=>");

    try
      {
	while(begin != end && *begin != ')')
	  {
	    string format = "\\1";

	    const string::const_iterator begin0 = begin;

	    auto_ptr<pkg_matcher> matcher(parse_pattern(begin, end,
							terminators,
							false, true, false));

	    if(matcher.get() == NULL)
	      throw GroupParseException(_("Unable to parse pattern after \"%s\""),
					string(begin0, end).c_str());

	    if(begin != end && *begin == '=')
	      {
		++begin;

		eassert(begin != end && *begin == '>');

		++begin;

		format.clear();

		while(begin != end && *begin != ',' && *begin != ')')
		  {
		    format += *begin;
		    ++begin;
		  }

		stripws(format);

		if(format.empty())
		  throw GroupParseException(_("Unexpectedly empty tree title after \"%s\""),
					    string(begin0, end).c_str());
	      }

	    subgroups.push_back(pkg_grouppolicy_matchers_factory::match_pair(matcher.release(), transcode(format)));

	    if(begin != end && *begin == ',')
	      ++begin;
	  }

	if(begin == end)
	  throw GroupParseException(_("Unmatched '(' in pattern grouping policy"));
	else
	  {
	    eassert(*begin == ')');
	    ++begin;
	  }

	return new policy_node1<pkg_grouppolicy_matchers_factory, vector<pkg_grouppolicy_matchers_factory::match_pair> >(subgroups);
      }
    catch(...)
      {
	for(vector<pkg_grouppolicy_matchers_factory::match_pair>::const_iterator i = subgroups.begin();
	    i != subgroups.end(); ++i)
	  delete i->matcher;

	throw;
      }
  }
};

static map<string, group_policy_parser *> parse_types;

static void init_parse_types()
{
  static bool initted_parse_types=false;

  if(!initted_parse_types)
    {
      parse_types["section"]=new section_policy_parser;
      parse_types["status"]=new status_policy_parser;
      parse_types["action"]=new mode_policy_parser;
      parse_types["filter"]=new filter_policy_parser;
      parse_types["firstchar"]=new firstchar_policy_parser;

      parse_types["versions"]=new ver_policy_parser;
      parse_types["deps"]=new dep_policy_parser;
      parse_types["priority"]=new priority_policy_parser;

      parse_types["pattern"]=new pattern_policy_parser;

      initted_parse_types=true;
    }
}

// Parses a chain of grouping policies from the given string and returns them.
pkg_grouppolicy_factory *parse_grouppolicy(string s)
{
  init_parse_types();

  string::const_iterator begin = s.begin();

  try
    {
      auto_ptr<group_policy_parse_node> node(list_policy_parser(parse_types).parse(begin, s.end()));

      eassert(begin == s.end());

      pkg_grouppolicy_factory *rval = node->instantiate(NULL);

      return rval;
    }
  catch(GroupParseException e)
    {
      _error->Error("%s", e.get_msg().c_str());

      return NULL;
    }
}
