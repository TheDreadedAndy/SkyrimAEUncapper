;
; @file HookWrappers.S
; @author Andrew Spaulding (Kasplat)
; @brief Assembly hook entry points for each hook.
; @bug I am convinced that these hooks are tearing calling convention a new
;      one.
;
; ASSEMBLY IS ASSEMBLY.
; ASSEMBLY BELONGS IN ASSEMBLY FILES.
;

.CODE

EXTERN ClampSkillEffect:PROC
EXTERN GetSkillCap_Hook:PROC

SkillEffectiveCapPatch_Wrapper PROC PUBLIC
    mov rcx, rbx ; Pass ID from RBX as first arg. Xmm0 and xmm1 has cur val.
    call ClampSkillEffect
    add rsp, 20h ; These next lines are just a copy of the og ret code.
    pop rbx
    ret
SkillEffectiveCapPatch_Wrapper ENDP

SkillCapPatch_Wrapper PROC PUBLIC
    movd xmm10, ecx ; Save ecx in xmm10 just in case. xmm6-xmm15 nonvolatile.
    mov ecx, esi    ; pass SkillID in ecx to hook.
    sub rsp, 30h
    movss dword ptr [rsp+28h], xmm0 ; Save current skill level
    call GetSkillCap_Hook
    movd ecx, xmm10 ; Restore ecx.
    movss xmm10, xmm0 ; Replace maximum with fn result.
    movss xmm0, dword ptr [rsp+28h] ; Restore current skill level.
    add rsp, 30h
    ret
SkillCapPatch_Wrapper ENDP

END