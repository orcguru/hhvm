/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2014 Facebook, Inc. (http://www.facebook.com)     |
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

#include "hphp/runtime/vm/jit/back-end.h"

#include "hphp/runtime/base/arch.h"

#if defined(__powerpc64__)
#include "hphp/runtime/vm/jit/back-end-ppc64.h"
#else
#include "hphp/runtime/vm/jit/back-end-x64.h"
#include "hphp/runtime/vm/jit/back-end-arm.h"
#endif

namespace HPHP { namespace jit {

std::unique_ptr<BackEnd> newBackEnd() {

  switch (arch()) {
  case Arch::X64:
    return x64::newBackEnd();
  case Arch::ARM:
    return arm::newBackEnd();
  case Arch::PPC64:
    {
    #if defined(__powerpc64__)
    return ppc64::newBackEnd();
    #endif
    }
  }
  not_reached();
}

BackEnd::~BackEnd() {
}

}}
