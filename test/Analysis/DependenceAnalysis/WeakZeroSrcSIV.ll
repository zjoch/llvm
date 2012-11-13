; RUN: opt < %s -analyze -basicaa -da | FileCheck %s

; ModuleID = 'WeakZeroSrcSIV.bc'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-apple-macosx10.6.0"


;;  for (long unsigned i = 0; i < 30; i++) {
;;    A[10] = i;
;;    *B++ = A[2*i + 10];

define void @weakzerosrc0(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  br label %for.body

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - flow [p<=|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body:                                         ; preds = %entry, %for.body
  %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %B.addr.01 = phi i32* [ %B, %entry ], [ %incdec.ptr, %for.body ]
  %conv = trunc i64 %i.02 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = shl i64 %i.02, 1
  %add = add i64 %mul, 10
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %add
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.01, i64 1
  store i32 %0, i32* %B.addr.01, align 4
  %inc = add i64 %i.02, 1
  %exitcond = icmp ne i64 %inc, 30
  br i1 %exitcond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


;;  for (long unsigned i = 0; i < n; i++) {
;;    A[10] = i;
;;    *B++ = A[n*i + 10];

define void @weakzerosrc1(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  %cmp1 = icmp eq i64 %n, 0
  br i1 %cmp1, label %for.end, label %for.body.preheader

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - flow [p<=|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.body:                                         ; preds = %for.body.preheader, %for.body
  %i.03 = phi i64 [ %inc, %for.body ], [ 0, %for.body.preheader ]
  %B.addr.02 = phi i32* [ %incdec.ptr, %for.body ], [ %B, %for.body.preheader ]
  %conv = trunc i64 %i.03 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = mul i64 %i.03, %n
  %add = add i64 %mul, 10
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %add
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.02, i64 1
  store i32 %0, i32* %B.addr.02, align 4
  %inc = add i64 %i.03, 1
  %exitcond = icmp ne i64 %inc, %n
  br i1 %exitcond, label %for.body, label %for.end.loopexit

for.end.loopexit:                                 ; preds = %for.body
  br label %for.end

for.end:                                          ; preds = %for.end.loopexit, %entry
  ret void
}


;;  for (long unsigned i = 0; i < 5; i++) {
;;    A[10] = i;
;;    *B++ = A[2*i];

define void @weakzerosrc2(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  br label %for.body

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - none!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body:                                         ; preds = %entry, %for.body
  %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %B.addr.01 = phi i32* [ %B, %entry ], [ %incdec.ptr, %for.body ]
  %conv = trunc i64 %i.02 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = shl i64 %i.02, 1
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %mul
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.01, i64 1
  store i32 %0, i32* %B.addr.01, align 4
  %inc = add i64 %i.02, 1
  %exitcond = icmp ne i64 %inc, 5
  br i1 %exitcond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


;;  for (long unsigned i = 0; i < 6; i++) {
;;    A[10] = i;
;;    *B++ = A[2*i];

define void @weakzerosrc3(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  br label %for.body

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - flow [=>p|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body:                                         ; preds = %entry, %for.body
  %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %B.addr.01 = phi i32* [ %B, %entry ], [ %incdec.ptr, %for.body ]
  %conv = trunc i64 %i.02 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = shl i64 %i.02, 1
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %mul
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.01, i64 1
  store i32 %0, i32* %B.addr.01, align 4
  %inc = add i64 %i.02, 1
  %exitcond = icmp ne i64 %inc, 6
  br i1 %exitcond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


;;  for (long unsigned i = 0; i < 7; i++) {
;;    A[10] = i;
;;    *B++ = A[2*i];

define void @weakzerosrc4(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  br label %for.body

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - flow [*|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body:                                         ; preds = %entry, %for.body
  %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %B.addr.01 = phi i32* [ %B, %entry ], [ %incdec.ptr, %for.body ]
  %conv = trunc i64 %i.02 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = shl i64 %i.02, 1
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %mul
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.01, i64 1
  store i32 %0, i32* %B.addr.01, align 4
  %inc = add i64 %i.02, 1
  %exitcond = icmp ne i64 %inc, 7
  br i1 %exitcond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


;;  for (long unsigned i = 0; i < 7; i++) {
;;    A[-10] = i;
;;    *B++ = A[2*i];

define void @weakzerosrc5(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  br label %for.body

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - none!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body:                                         ; preds = %entry, %for.body
  %i.02 = phi i64 [ 0, %entry ], [ %inc, %for.body ]
  %B.addr.01 = phi i32* [ %B, %entry ], [ %incdec.ptr, %for.body ]
  %conv = trunc i64 %i.02 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 -10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = shl i64 %i.02, 1
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %mul
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.01, i64 1
  store i32 %0, i32* %B.addr.01, align 4
  %inc = add i64 %i.02, 1
  %exitcond = icmp ne i64 %inc, 7
  br i1 %exitcond, label %for.body, label %for.end

for.end:                                          ; preds = %for.body
  ret void
}


;;  for (long unsigned i = 0; i < n; i++) {
;;    A[10] = i;
;;    *B++ = A[3*i];

define void @weakzerosrc6(i32* %A, i32* %B, i64 %n) nounwind uwtable ssp {
entry:
  %cmp1 = icmp eq i64 %n, 0
  br i1 %cmp1, label %for.end, label %for.body.preheader

; CHECK: da analyze - consistent output [S|<]!
; CHECK: da analyze - none!
; CHECK: da analyze - confused!
; CHECK: da analyze - consistent input [0|<]!
; CHECK: da analyze - confused!
; CHECK: da analyze - confused!

for.body.preheader:                               ; preds = %entry
  br label %for.body

for.body:                                         ; preds = %for.body.preheader, %for.body
  %i.03 = phi i64 [ %inc, %for.body ], [ 0, %for.body.preheader ]
  %B.addr.02 = phi i32* [ %incdec.ptr, %for.body ], [ %B, %for.body.preheader ]
  %conv = trunc i64 %i.03 to i32
  %arrayidx = getelementptr inbounds i32* %A, i64 10
  store i32 %conv, i32* %arrayidx, align 4
  %mul = mul i64 %i.03, 3
  %arrayidx1 = getelementptr inbounds i32* %A, i64 %mul
  %0 = load i32* %arrayidx1, align 4
  %incdec.ptr = getelementptr inbounds i32* %B.addr.02, i64 1
  store i32 %0, i32* %B.addr.02, align 4
  %inc = add i64 %i.03, 1
  %exitcond = icmp ne i64 %inc, %n
  br i1 %exitcond, label %for.body, label %for.end.loopexit

for.end.loopexit:                                 ; preds = %for.body
  br label %for.end

for.end:                                          ; preds = %for.end.loopexit, %entry
  ret void
}
