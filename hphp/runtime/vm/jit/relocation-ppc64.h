/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2016 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/
#ifndef incl_HPHP_CODE_RELOCATION_PPC64_H_
#define incl_HPHP_CODE_RELOCATION_PPC64_H_

#include "hphp/runtime/vm/jit/relocation.h"

#include "hphp/util/asm-x64.h"

namespace HPHP { namespace jit { namespace ppc64 {

void adjustForRelocation(RelocationInfo&);
void adjustForRelocation(RelocationInfo& rel, TCA srcStart, TCA srcEnd);
void adjustCodeForRelocation(RelocationInfo& rel, CGMeta& fixups);
void adjustMetaDataForRelocation(RelocationInfo&, AsmInfo*, CGMeta&);
void findFixups(TCA start, TCA end, CGMeta& fixups);
size_t relocate(RelocationInfo&, CodeBlock&, TCA, TCA, CGMeta&, TCA*);

}}}

#endif