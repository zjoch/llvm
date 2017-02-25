; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc < %s -mtriple=i386-unknown-linux-gnu -mattr=avx512dq | FileCheck %s

define void @f_f___un_3C_unf_3E_un_3C_unf_3E_() {
; CHECK-LABEL: f_f___un_3C_unf_3E_un_3C_unf_3E_:
; CHECK:       # BB#0:
; CHECK-NEXT:    vmovapd 0, %zmm0
; CHECK-NEXT:    vmovapd 64, %zmm1
; CHECK-NEXT:    vmovapd {{.*#+}} zmm2 = [0,16,0,16,0,16,0,16,0,16,0,16,0,16,0,16]
; CHECK-NEXT:    vorpd %zmm2, %zmm0, %zmm0 {%k1}
; CHECK-NEXT:    vorpd %zmm2, %zmm1, %zmm1 {%k1}
; CHECK-NEXT:    vmovapd %zmm1, 64
; CHECK-NEXT:    vmovapd %zmm0, 0
; CHECK-NEXT:    retl
  %a_load22 = load <16 x i64>, <16 x i64>* null, align 1
  %bitop = or <16 x i64> %a_load22, <i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736, i64 68719476736>
  %v.i = load <16 x i64>, <16 x i64>* null
  %v1.i41 = select <16 x i1> undef, <16 x i64> %bitop, <16 x i64> %v.i
  store <16 x i64> %v1.i41, <16 x i64>* null
  ret void
}
