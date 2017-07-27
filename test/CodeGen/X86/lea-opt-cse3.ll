; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=x86_64-unknown | FileCheck %s -check-prefix=X64
; RUN: llc < %s -mtriple=i686-unknown   | FileCheck %s -check-prefix=X86

define i32 @foo(i32 %a, i32 %b) local_unnamed_addr #0 {
; X64-LABEL: foo:
; X64:       # BB#0: # %entry
; X64-NEXT:    # kill: %ESI<def> %ESI<kill> %RSI<def>
; X64-NEXT:    # kill: %EDI<def> %EDI<kill> %RDI<def>
; X64-NEXT:    leal 4(%rdi,%rsi,2), %ecx
; X64-NEXT:    leal 4(%rdi,%rsi,4), %eax
; X64-NEXT:    imull %ecx, %eax
; X64-NEXT:    retq
;
; X86-LABEL: foo:
; X86:       # BB#0: # %entry
; X86-NEXT:    movl {{[0-9]+}}(%esp), %eax
; X86-NEXT:    movl {{[0-9]+}}(%esp), %ecx
; X86-NEXT:    leal 4(%ecx,%eax,2), %edx
; X86-NEXT:    leal 4(%ecx,%eax,4), %eax
; X86-NEXT:    imull %edx, %eax
; X86-NEXT:    retl
entry:
  %mul = shl i32 %b, 1
  %add = add i32 %a, 4
  %add1 = add i32 %add, %mul
  %mul2 = shl i32 %b, 2
  %add4 = add i32 %add, %mul2
  %mul5 = mul nsw i32 %add1, %add4
  ret i32 %mul5
}

