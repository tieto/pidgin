#define PERL_constant_NOTFOUND	1
#define PERL_constant_NOTDEF	2
#define PERL_constant_ISIV	3
#define PERL_constant_ISNO	4
#define PERL_constant_ISNV	5
#define PERL_constant_ISPV	6
#define PERL_constant_ISPVN	7
#define PERL_constant_ISSV	8
#define PERL_constant_ISUNDEF	9
#define PERL_constant_ISUV	10
#define PERL_constant_ISYES	11

#ifndef NVTYPE
typedef double NV; /* 5.6 and later define NVTYPE, and typedef NV to it.  */
#endif
#ifndef aTHX_
#define aTHX_ /* 5.6 or later define this for threading support.  */
#endif
#ifndef pTHX_
#define pTHX_ /* 5.6 or later define this for threading support.  */
#endif

static int
constant (pTHX_ const char *name, STRLEN len, IV *iv_return) {
  /* Initially switch on the length of the name.  */
  /* When generated this function returned values for the list of names given
     in this section of perl code.  Rather than manually editing these functions
     to add or remove constants, which would result in this comment and section
     of code becoming inaccurate, we recommend that you edit this section of
     code, and use it to regenerate a new set of constant functions which you
     then use to replace the originals.

     Regenerate these constant functions by feeding this entire source file to
     perl -x

#!/usr/bin/perl -w
use ExtUtils::Constant qw (constant_types C_constant XS_constant);

my $types = {map {($_, 1)} qw(IV)};
my @names = (qw(),
            {name=>"PURPLE_DEBUG_ALL", type=>"IV", macro=>"1"},
            {name=>"PURPLE_DEBUG_ERROR", type=>"IV", macro=>"1"},
            {name=>"PURPLE_DEBUG_FATAL", type=>"IV", macro=>"1"},
            {name=>"PURPLE_DEBUG_INFO", type=>"IV", macro=>"1"},
            {name=>"PURPLE_DEBUG_MISC", type=>"IV", macro=>"1"},
            {name=>"PURPLE_DEBUG_WARNING", type=>"IV", macro=>"1"});

print constant_types(); # macro defs
foreach (C_constant ("Purple::DebugLevel", 'constant', 'IV', $types, undef, 3, @names) ) {
    print $_, "\n"; # C constant subs
}
print "#### XS Section:\n";
print XS_constant ("Purple::DebugLevel", $types);
__END__
   */

  switch (len) {
  case 14:
    if (memEQ(name, "PURPLE_DEBUG_ALL", 14)) {
      *iv_return = PURPLE_DEBUG_ALL;
      return PERL_constant_ISIV;
    }
    break;
  case 15:
    /* Names all of length 15.  */
    /* PURPLE_DEBUG_INFO PURPLE_DEBUG_MISC */
    /* Offset 11 gives the best switch position.  */
    switch (name[11]) {
    case 'I':
      if (memEQ(name, "PURPLE_DEBUG_INFO", 15)) {
      /*                          ^          */
        *iv_return = PURPLE_DEBUG_INFO;
        return PERL_constant_ISIV;
      }
      break;
    case 'M':
      if (memEQ(name, "PURPLE_DEBUG_MISC", 15)) {
      /*                          ^          */
        *iv_return = PURPLE_DEBUG_MISC;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 16:
    /* Names all of length 16.  */
    /* PURPLE_DEBUG_ERROR PURPLE_DEBUG_FATAL */
    /* Offset 11 gives the best switch position.  */
    switch (name[11]) {
    case 'E':
      if (memEQ(name, "PURPLE_DEBUG_ERROR", 16)) {
      /*                          ^           */
        *iv_return = PURPLE_DEBUG_ERROR;
        return PERL_constant_ISIV;
      }
      break;
    case 'F':
      if (memEQ(name, "PURPLE_DEBUG_FATAL", 16)) {
      /*                          ^           */
        *iv_return = PURPLE_DEBUG_FATAL;
        return PERL_constant_ISIV;
      }
      break;
    }
    break;
  case 18:
    if (memEQ(name, "PURPLE_DEBUG_WARNING", 18)) {
      *iv_return = PURPLE_DEBUG_WARNING;
      return PERL_constant_ISIV;
    }
    break;
  }
  return PERL_constant_NOTFOUND;
}

