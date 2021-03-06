// load_grouppolicy.h
//
//  Copyright 2001 Daniel Burrows
//
//  Routines to parse grouping policies.
//
// Grouping-policy configurations are written as follows:
//
// POLICY1,POLICY2(arg1,arg2,...,..,argN),POLICY3,...,POLICYN
//
// Each POLICY name specifies a way to sort at a particular level.  Obviously,
// the policy names may not contain commas.  Similarly, arg1..argN are arguments
// to the policies.  Different policies, just to be confusing, may implement
// different handling of their arguments.
//
// If this ever gets totally out of control, it may be worthwhile to write a
// routine allowing other modules to register parsers for an individual policy.
// Right now I don't think this is necessary.

#ifndef LOAD_GROUPPOLICY_H
#define LOAD_GROUPPOLICY_H

#include <string>

class pkg_grouppolicy_factory;

pkg_grouppolicy_factory *parse_grouppolicy(std::string s);
// Parses the given string as a grouping-policy description.

#endif
