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

/*
 * This pragma was set to do not show warnings of no return value for the
 * "implemented" functions for this class.
 * This file was created just to handle PPC64 architecture and initially
 * to support PPC64 with EvalJit=false.
 * This is a work in progress to port HHVM Jit to PPC64 architecture.
 * */

#pragma GCC diagnostic ignored "-Wreturn-type"

#include "hphp/runtime/vm/jit/back-end-ppc64.h"

#include "hphp/ppc64-asm/asm-ppc64.h"
#include "hphp/util/disasm.h"
#include "hphp/util/text-color.h"

#include "hphp/runtime/vm/func.h"
#include "hphp/runtime/vm/jit/abi-ppc64.h"
#include "hphp/runtime/vm/jit/block.h"
#include "hphp/runtime/vm/jit/check.h"
#include "hphp/runtime/vm/jit/code-gen-helpers-ppc64.h"
#include "hphp/runtime/vm/jit/code-gen-x64.h"
#include "hphp/runtime/vm/jit/func-prologues-ppc64.h"
#include "hphp/runtime/vm/jit/cfg.h"
#include "hphp/runtime/vm/jit/mc-generator.h"
#include "hphp/runtime/vm/jit/print.h"
#include "hphp/runtime/vm/jit/service-requests-inline.h"
#include "hphp/runtime/vm/jit/service-requests-ppc64.h"
#include "hphp/runtime/vm/jit/timer.h"
#include "hphp/runtime/vm/jit/unique-stubs-ppc64.h"
#include "hphp/runtime/vm/jit/vasm-print.h"
#include "hphp/runtime/vm/jit/vasm-llvm.h"
#include "hphp/runtime/vm/jit/relocation.h"

namespace HPHP { namespace jit {

namespace ppc64 {

extern "C" void enterTCHelper(Cell* vm_sp,
                              ActRec* vm_fp,
                              TCA start,
                              ActRec* firstAR,
                              void* targetCacheBase,
                              ActRec* stashedAR);

struct BackEnd final : public jit::BackEnd {
  BackEnd() {}
  ~BackEnd() {}

   Abi abi() override { not_implemented(); };

   size_t cacheLineSize() override { not_implemented(); };

   PhysReg rSp() override { not_implemented(); };

   PhysReg rVmSp() override { not_implemented(); };

   PhysReg rVmFp() override { not_implemented(); };

   PhysReg rVmTl() override { not_implemented(); };

//TODO PPC64 review this code, since it is duplicated
#if defined (__powerpc64__)
  #define CALLEE_SAVED_BARRIER()
#else
  #define CALLEE_SAVED_BARRIER()                                    \
      asm volatile("" : : : "rbx", "r12", "r13", "r14", "r15");
#endif

   void enterTCHelper(TCA start, ActRec* stashedAR) override {
      // We have to force C++ to spill anything that might be in a callee-saved
      // register (aside from rbp). enterTCHelper does not save them.
      CALLEE_SAVED_BARRIER();
      auto& regs = vmRegsUnsafe();
      jit::ppc64::enterTCHelper(regs.stack.top(), regs.fp, start,
                         vmFirstAR(), rds::tl_base, stashedAR);
      CALLEE_SAVED_BARRIER();
   };
   UniqueStubs emitUniqueStubs() override {
     return ppc64::emitUniqueStubs();
   };
   TCA emitServiceReqWork(
    CodeBlock& cb,
    TCA start,
    SRFlags flags,
    folly::Optional<FPInvOffset> spOff,
    ServiceRequest req,
    const ServiceReqArgVec& argv) override { not_implemented(); };
   size_t reusableStubSize() const override { not_implemented(); };

   void emitInterpReq(CodeBlock& code,
                             SrcKey sk,
                             FPInvOffset spOff) override {
     not_implemented();
   };

   bool funcPrologueHasGuard(TCA prologue, const Func* func) override {
     not_implemented();
   };

   TCA funcPrologueToGuard(TCA prologue, const Func* func) override {
     not_implemented();
   };

   SrcKey emitFuncPrologue(TransID transID, Func* func, int argc,
                                  TCA& start) override { not_implemented(); };

   TCA emitCallArrayPrologue(Func* func, DVFuncletsVec& dvs) override {
     not_implemented();
   };

   void funcPrologueSmashGuard(TCA prologue, const Func* func) override {
     not_implemented();
   };

   void emitIncStat(CodeBlock& cb, intptr_t disp, int n) override {
     not_implemented();
   };

   void prepareForTestAndSmash(CodeBlock& cb, int testBytes,
                                      TestAndSmashFlags flags) override {
     not_implemented();
   };

   void smashJmp(TCA jmpAddr, TCA newDest) override { not_implemented(); };

   void smashCall(TCA callAddr, TCA newDest) override { not_implemented(); };

   void smashJcc(TCA jccAddr, TCA newDest) override { not_implemented(); };

   void emitSmashableJump(CodeBlock& cb, TCA dest, ConditionCode cc) override {
     not_implemented();
   };

   void emitSmashableCall(CodeBlock& cb, TCA dest) override {
     not_implemented();
   };

   TCA smashableCallFromReturn(TCA returnAddr) override {
     not_implemented();
   };

   TCA jmpTarget(TCA jmp) override { not_implemented(); };

   TCA jccTarget(TCA jmp) override { not_implemented(); };

   ConditionCode jccCondCode(TCA jmp) override { not_implemented(); };

   TCA callTarget(TCA call) override { not_implemented(); };

   void addDbgGuard(CodeBlock& codeMain, CodeBlock& codeCold,
                           SrcKey sk, size_t dbgOff) override {
     not_implemented();
   };


   void streamPhysReg(std::ostream& os, PhysReg reg) override {
     not_implemented();
   };

   void disasmRange(std::ostream& os, int indent, bool dumpIR,
                           TCA begin, TCA end) override { not_implemented(); };


   void genCodeImpl(IRUnit& unit, CodeKind, AsmInfo*) override {
     not_implemented();
   };


private:
   void do_moveToAlign(CodeBlock&, MoveToAlignFlags) override {
     not_implemented();
   };

   bool do_isSmashable(Address, int, int) override { not_implemented(); };

   void do_prepareForSmash(CodeBlock&, int, int) override {
     not_implemented();
   };


};


std::unique_ptr<jit::BackEnd> newBackEnd() {
  return folly::make_unique<BackEnd>();
}

//////////////////////////////////////////////////////////////////////

bool isSmashable(Address frontier, int nBytes, int offset /* = 0 */) {
  return false;
}

void prepareForSmashImpl(CodeBlock& cb, int nBytes, int offset) {
  not_implemented();
}

void smashJmp(TCA jmpAddr, TCA newDest) { not_implemented(); }

void smashCall(TCA callAddr, TCA newDest) { not_implemented(); }

//////////////////////////////////////////////////////////////////////

//void BackEnd::genCodeImpl(IRUnit& unit, AsmInfo* asmInfo) {
//  not_implemented();
//}

}}}

#pragma GCC diagnostic pop
