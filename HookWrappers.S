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

; Saves all caller saved registers, except rax.
BEGIN_INJECTED_CALL MACRO
    push rcx
    push rdx
    push r8
    push r9
    push r10
    push r11
    sub rsp, 60h
    movdqu [rsp], xmm0
    movdqu [rsp + 10h], xmm1
    movdqu [rsp + 20h], xmm2
    movdqu [rsp + 30h], xmm3
    movdqu [rsp + 40h], xmm4
    movdqu [rsp + 50h], xmm5
ENDM

; Restores the caller saved registers, except rax.
END_INJECTED_CALL MACRO
    movdqu xmm0, [rsp]
    movdqu xmm1, [rsp + 10h]
    movdqu xmm2, [rsp + 20h]
    movdqu xmm3, [rsp + 30h]
    movdqu xmm4, [rsp + 40h]
    movdqu xmm5, [rsp + 50h]
    add rsp, 60h
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rcx
ENDM

; Saves all caller saved registers.
SAVEALL MACRO
    push rax
    BEGIN_INJECTED_CALL
ENDM

; Restores all caller saved registers.
RESTOREALL MACRO
    END_INJECTED_CALL
    pop rax
ENDM

; Our hook for this function tails into it, so no need to save regs.
SkillEffectiveCapPatch_Wrapper PROC PUBLIC
    mov rcx, rbx ; Pass ID from RBX as first arg. Xmm0 and xmm1 has cur val.
    call ClampSkillEffect
    add rsp, 20h ; These next lines are just a copy of the og ret code.
    pop rbx
    ret
SkillEffectiveCapPatch_Wrapper ENDP

; This function gets injected in the middle of another, so we must use the call
; injection macros to protect the register state.
SkillCapPatch_Wrapper PROC PUBLIC
    BEGIN_INJECTED_CALL
    mov ecx, esi    ; pass SkillID in ecx to hook.
    sub rsp, 30h
    movss dword ptr [rsp+28h], xmm0 ; Save current skill level
    call GetSkillCap_Hook
    movd ecx, xmm10 ; Restore ecx.
    movss xmm10, xmm0 ; Replace maximum with fn result.
    movss xmm0, dword ptr [rsp+28h] ; Restore current skill level.
    add rsp, 30h
    END_INJECTED_CALL
    ret
SkillCapPatch_Wrapper ENDP

; This function is tailed into by our hook, so we dont need to worry about
; register saving.
ModifyPerkPool_Wrapper PROC PUBLIC
    sub rsp, 20h
    mov rcx, rdi ; AE changed rbx to rdi.
    call ModifyPerkPool_Hook
    add rsp, 40h ; We reimplement the original return code as well here.
    pop rdi
    ret
ModifyPerkPool_Wrapper ENDP

; TODO: Not sure if just using our crap is safe here, check later.
ImproveLevelExpBySkillLevel_Wrapper PROC PUBLIC
    SAVEALL
    movss xmm0, xmm1 ; xmm1 level exp
    mov rdx, rsi
    sub rsp, 20h
    call ImproveLevelExpBySkillLevel_Hook
    add rsp, 20h
    RESTOREALL ; FIXME: Is it ok to wipe out XMM0 here? Intentional?
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
