/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2015 Facebook, Inc. (http://www.facebook.com)     |
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

#include "hphp/runtime/vm/jit/unique-stubs-ppc64.h"

#include "hphp/runtime/base/header-kind.h"
#include "hphp/runtime/base/rds-header.h"
#include "hphp/runtime/base/runtime-option.h"
#include "hphp/runtime/base/stats.h"
#include "hphp/runtime/vm/event-hook.h"

#include "hphp/runtime/vm/jit/types.h"
#include "hphp/runtime/vm/jit/abi-ppc64.h"
#include "hphp/runtime/vm/jit/align-ppc64.h"
#include "hphp/runtime/vm/jit/code-gen-cf.h"
#include "hphp/runtime/vm/jit/code-gen-helpers.h"
#include "hphp/runtime/vm/jit/code-gen-tls.h"
#include "hphp/runtime/vm/jit/fixup.h"
#include "hphp/runtime/vm/jit/mc-generator.h"
#include "hphp/runtime/vm/jit/phys-reg.h"
#include "hphp/runtime/vm/jit/service-requests.h"
#include "hphp/runtime/vm/jit/translator-inline.h"
#include "hphp/runtime/vm/jit/unique-stubs.h"
#include "hphp/runtime/vm/jit/unwind-ppc64.h"
#include "hphp/runtime/vm/jit/vasm-gen.h"
#include "hphp/runtime/vm/jit/vasm-instr.h"

#include "hphp/ppc64-asm/asm-ppc64.h"
#include "hphp/util/data-block.h"

namespace HPHP { namespace jit {

///////////////////////////////////////////////////////////////////////////////

TRACE_SET_MOD(ustubs);

extern "C" void enterTCHelper(Cell* vm_sp,
                              ActRec* vm_fp,
                              TCA start,
                              ActRec* firstAR,
                              void* targetCacheBase,
                              ActRec* stashedAR);

///////////////////////////////////////////////////////////////////////////////

namespace ppc64 {

///////////////////////////////////////////////////////////////////////////////

static void alignJmpTarget(CodeBlock& cb) {
  align(cb, Alignment::JmpTarget, AlignContext::Dead);
}

///////////////////////////////////////////////////////////////////////////////

TCA emitFunctionEnterHelper(CodeBlock& cb, UniqueStubs& us) {
    alignJmpTarget(cb);

    ppc64_asm::Assembler a { cb };

    auto const start = a.frontier();

    a.mflr(rfuncln());
    a.std(rfuncln(), rsp()[16]);
    a.stdu(rsp(), rsp()[-80]);

    bool (*hook)(const ActRec*, int) = &EventHook::onFunctionCall;
    unsigned char *hook_ptr = (unsigned char *)hook;

    a.branchAuto((hook_ptr), ppc64_asm::BranchConditions::Always,
        ppc64_asm::LinkReg::Save);

    auto const HelperReturn = a.frontier();
    us.functionEnterHelperReturn = HelperReturn;
    a.cmpdi(rret(),0);
    ppc64_asm::Label l;
    a.branchAuto(l,ppc64_asm::BranchConditions::Equal,
        ppc64_asm::LinkReg::DoNotTouch);
        a.ld(rsp(),rsp()[0]);
        a.addi(rsp(),rsp(),80);

        a.ld(rfuncln(), rsp()[16]);
        ppc64_asm::BranchParams bp(ppc64_asm::BranchConditions::Always);
        a.mtctr(rfuncln());
        a.bcctr(bp.bo(), bp.bi(), 0);

    l.asm_label(a);
    a.ld(rsp(),rsp()[0]);
    a.addi(rsp(),rsp(),80);

    return start;
}

///////////////////////////////////////////////////////////////////////////////

/*
 * Helper for the freeLocalsHelpers which does the actual work of decrementing
 * a value's refcount or releasing it.
 *
 * This helper is reached via call from the various freeLocalHelpers.  It
 * expects `tv' to be the address of a TypedValue with refcounted type `type'
 * (though it may be static, and we will do nothing in that case).
 *
 * The `saved' register should be a callee-saved GP register that the helper
 * can use to preserve `tv' across native calls.
 */
static TCA emitDecRefHelper(CodeBlock& cb, PhysReg tv, PhysReg type,
                            RegSet live) {
  return vwrap(cb, [&] (Vout& v) {
    // We use the first argument register for the TV data because we may pass
    // it to the release routine.  It's not live when we enter the helper.
    auto const data = rarg(0);
    v << load{tv[TVOFF(m_data)], data};

    auto const sf = v.makeReg();
    v << cmplim{1, data[FAST_REFCOUNT_OFFSET], sf};

    ifThen(v, CC_NL, sf, [&] (Vout& v) {
      // The refcount is positive, so the value is refcounted.  We need to
      // either decref or release.
      ifThen(v, CC_NE, sf, [&] (Vout& v) {
        // The refcount is greater than 1; decref it.
        v << declm{data[FAST_REFCOUNT_OFFSET], v.makeReg()};
        v << ret{};
      });

      // Note that the stack is aligned since we called to this helper from an
      // stack-unaligned stub.
      PhysRegSaver prs{v, live};

      // The refcount is exactly 1; release the value.
      v << callm{lookupDestructor(v, type)};

      // Between where rsp is now and the saved RIP of the call into the
      // freeLocalsHelpers stub, we have all the live regs we pushed, plus the
      // saved RIP of the call from the stub to this helper.
      v << syncpoint{makeIndirectFixup(prs.dwordsPushed() + 1)};
      // fallthru
    });

    // Either we did a decref, or the value was static.
    v << ret{};
  });
}

TCA emitFreeLocalsHelpers(CodeBlock& cb, UniqueStubs& us) {
  // The address of the first local is passed in the second argument register.
  // We use the third and fourth as scratch registers.
  auto const local = rarg(1);
  auto const last = rarg(2);
  auto const type = rarg(3);

  // This stub is very hot; keep it cache-aligned.
  align(cb, Alignment::CacheLine, AlignContext::Dead);
  auto const release = emitDecRefHelper(cb, local, type, local | last);

  auto const decref_local = [&] (Vout& v) {
    auto const sf = v.makeReg();

    // We can't use emitLoadTVType() here because it does a byte load, and we
    // need to sign-extend since we use `type' as a 32-bit array index to the
    // destructor table.
    v << loadzbl{local[TVOFF(m_type)], type};
    emitCmpTVType(v, sf, KindOfRefCountThreshold, type);

    ifThen(v, CC_G, sf, [&] (Vout& v) {
      v << call{release, arg_regs(3)};
    });
  };

  auto const next_local = [&] (Vout& v) {
    v << addqi{static_cast<int>(sizeof(TypedValue)),
               local, local, v.makeReg()};
  };

  alignJmpTarget(cb);

  us.freeManyLocalsHelper = vwrap(cb, [&] (Vout& v) {
    // We always unroll the final `kNumFreeLocalsHelpers' decrefs, so only loop
    // until we hit that point.
    v << lea{rvmfp()[localOffset(kNumFreeLocalsHelpers - 1)], last};

    doWhile(v, CC_NZ, {},
      [&] (const VregList& in, const VregList& out) {
        auto const sf = v.makeReg();

        decref_local(v);
        next_local(v);
        v << cmpq{local, last, sf};
        return sf;
      }
    );
  });

  for (auto i = kNumFreeLocalsHelpers - 1; i >= 0; --i) {
    us.freeLocalsHelpers[i] = vwrap(cb, [&] (Vout& v) {
      decref_local(v);
      if (i != 0) next_local(v);
    });
  }

  // All the stub entrypoints share the same ret.
  vwrap(cb, [] (Vout& v) { v << ret{}; });

  // This stub is hot, so make sure to keep it small.
#if defined(PPC64_PERFORMANCE)
  always_assert(Stats::enabled() ||
                (cb.frontier() - release <= 4 * cache_line_size()));
#endif

  return release;
}

///////////////////////////////////////////////////////////////////////////////

extern "C" void enterTCExit();

TCA emitCallToExit(CodeBlock& cb) {
  ppc64_asm::Assembler a { cb };
  auto const start = a.frontier();

  // Simply go to enterTCExit, no worries about the stack because it's balanced
  a.branchAuto(TCA(enterTCExit));
  return start;
}

TCA emitEndCatchHelper(CodeBlock& cb, UniqueStubs& us) {
  auto const udrspo = rvmtl()[unwinderDebuggerReturnSPOff()];

  auto const debuggerReturn = vwrap(cb, [&] (Vout& v) {
    v << load{udrspo, rvmsp()};
    v << storeqi{0, udrspo};
  });
  svcreq::emit_persistent(cb, folly::none, REQ_POST_DEBUGGER_RET);

  auto const resumeCPPUnwind = vwrap(cb, [] (Vout& v) {
    static_assert(sizeof(tl_regState) == 1,
                  "The following store must match the size of tl_regState.");
    auto const regstate = emitTLSAddr(v, tls_datum(tl_regState));
    v << storebi{static_cast<int32_t>(VMRegState::CLEAN), regstate};

    v << load{rvmtl()[unwinderExnOff()], rarg(0)};
    v << call{TCA(_Unwind_Resume), arg_regs(1)};
  });
  us.endCatchHelperPast = cb.frontier();
  vwrap(cb, [] (Vout& v) { v << ud2{}; });

  alignJmpTarget(cb);

  return vwrap(cb, [&] (Vout& v) {
    auto const done1 = v.makeBlock();
    auto const sf1 = v.makeReg();

    v << cmpqim{0, udrspo, sf1};
    v << jcci{CC_NE, sf1, done1, debuggerReturn};
    v = done1;

    // Normal end catch situation: call back to tc_unwind_resume, which returns
    // the catch trace (or null) in $r3, and the new vmfp in $r4.
    v << copy{rvmfp(), rarg(0)};
    v << call{TCA(tc_unwind_resume)};
    v << copy{ppc64_asm::reg::r4, rvmfp()};

    auto const done2 = v.makeBlock();
    auto const sf2 = v.makeReg();

    v << testq{ppc64_asm::reg::r3, ppc64_asm::reg::r3, sf2};
    v << jcci{CC_Z, sf2, done2, resumeCPPUnwind};
    v = done2;

    // We need to do a syncForLLVMCatch(), but vmfp is already in $r4.
    v << jmpr{ppc64_asm::reg::r3};
  });
}

///////////////////////////////////////////////////////////////////////////////

void enterTCImpl(TCA start, ActRec* stashedAR) {
  // We have to force C++ to spill anything that might be in a callee-saved
  // register (aside from %rbp), since enterTCHelper does not save them.
  CALLEE_SAVED_BARRIER();
  auto& regs = vmRegsUnsafe();
  jit::enterTCHelper(regs.stack.top(), regs.fp, start,
                     vmFirstAR(), rds::tl_base, stashedAR);
  CALLEE_SAVED_BARRIER();
}

///////////////////////////////////////////////////////////////////////////////

}}}
