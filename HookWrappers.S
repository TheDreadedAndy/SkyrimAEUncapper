;
; @file HookWrappers.S
; @author Andrew Spaulding (Kasplat)
; @brief Assembly hook entry points for each hook.
; @bug I am convinced that these hooks are tearing calling convention a new one.
;
; ASSEMBLY IS ASSEMBLY.
; ASSEMBLY BELONGS IN ASSEMBLY FILES.
;

.CODE

EXTERN ClampSkillEffect:PROC
EXTERN GetSkillCap_Hook:PROC
EXTERN ModifyPerkPool_Hook:PROC
EXTERN ImproveLevelExpBySkillLevel_Hook:PROC
EXTERN ImprovePlayerSkillPoints_ReturnTrampoline:PTR
EXTERN ImproveAttributeWhenLevelUp_ReturnTrampoline:PTR

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

ModifyPerkPool_Wrapper PROC PUBLIC
    sub rsp, 20h
    mov rcx, rdi ; AE changed rbx to rdi.
    call ModifyPerkPool_Hook
    add rsp, 20h
    ret
ModifyPerkPool_Wrapper ENDP

ImproveLevelExpBySkillLevel_Wrapper PROC PUBLIC
    push rax
    push rdx
    movss xmm0, xmm1 ; xmm1 level exp
    mov rdx, rsi
    sub rsp, 20h
    call ImproveLevelExpBySkillLevel_Hook
    add rsp, 20h
    pop rdx
    pop rax
    addss xmm0, dword ptr [rax] ; replaced code 1, but xmm0 instead of xmm1.
    movss dword ptr [rax], xmm0 ; replaced code 2
    ret
ImproveLevelExpBySkillLevel_Wrapper ENDP

; This function allows us to call the games original ImprovePlayerSkillPoints
; function by reimplementing the code our hook replaces and then jumping to
; the original game code that follows our hook.
ImprovePlayerSkillPoints_Original PROC PUBLIC
    mov rax, rsp ; 48 8b c4
    push rdi     ; 57
    push r12     ; 41 54
    jmp ImprovePlayerSkillPoints_ReturnTrampoline
ImprovePlayerSkillPoints_Original ENDP

; This function allows us to call the games OG ImproveAttributeWhenLevelUp
; function by reimplementing the code we replaced and then jumping to
; the address immediately after our hook.
ImproveAttributeWhenLevelUp_Original PROC PUBLIC
    push rdi
    sub rsp, 30h
    jmp ImproveAttributeWhenLevelUp_ReturnTrampoline
ImproveAttributeWhenLevelUp_Original ENDP

END
