;
; @file HookWrappers.S
; @author Andrew Spaulding (Kasplat)
; @author Vadfromnu
; @author Kassent
; @brief Assembly hook entry points for each hook.
; @bug No known bugs.
;
; ASSEMBLY IS ASSEMBLY.
; ASSEMBLY BELONGS IN ASSEMBLY FILES.
;

.CODE

EXTERN GetSkillCap_Hook:PROC
EXTERN PlayerAVOGetCurrent_ReturnTrampoline:PTR
EXTERN DisplayTrueSkillLevel_ReturnTrampoline:PTR

EXTERN ImprovePlayerSkillPoints_ReturnTrampoline:PTR
EXTERN ModifyPerkPool_Hook:PROC
EXTERN ModifyPerkPool_ReturnTrampoline:PTR
EXTERN ImproveLevelExpBySkillLevel_Hook:PROC

EXTERN LegendaryResetSkillLevel_Hook:PROC
EXTERN CheckConditionForLegendarySkill_Hook:PROC
EXTERN CheckConditionForLegendarySkill_ReturnTrampoline:PTR
EXTERN HideLegendaryButton_Hook:PROC
EXTERN HideLegendaryButton_ReturnTrampoline:PTR

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

; This function gets injected in the middle of another, so we must use the call
; injection macros to protect the register state.
SkillCapPatch_Wrapper PROC PUBLIC
    BEGIN_INJECTED_CALL
    mov ecx, esi ; pass SkillID in ecx to hook.
    sub rsp, 20h ; Required by calling convention to alloc this.
    call GetSkillCap_Hook
    add rsp, 20h
    movss xmm10, xmm0 ; Replace maximum with fn result.
    END_INJECTED_CALL
    ret
SkillCapPatch_Wrapper ENDP

; This function allows us to call the OG PlayerAVOGetCurrent function by
; running the overwritten instructions and then jumping to the address after
; our hook.
PlayerAVOGetCurrent_Original PROC PUBLIC
    mov r11, rsp
    push rbp
    push rsi
    push rdi
    jmp PlayerAVOGetCurrent_ReturnTrampoline
PlayerAVOGetCurrent_Original ENDP

; Forces the code which displays skill values in the skills menu to show the
; true skill level instead of the damaged value by calling the OG function.
DisplayTrueSkillLevel_Hook PROC PUBLIC
    call PlayerAVOGetCurrent_Original ; Replacing a call, no need to save.
    cvttss2si ecx, xmm0
    jmp DisplayTrueSkillLevel_ReturnTrampoline
DisplayTrueSkillLevel_Hook ENDP

; This function allows us to call the games original ImprovePlayerSkillPoints
; function by reimplementing the code our hook replaces and then jumping to
; the original game code that follows our hook.
ImprovePlayerSkillPoints_Original PROC PUBLIC
    mov rax, rsp ; 48 8b c4
    push rdi     ; 57
    push r12     ; 41 54
    jmp ImprovePlayerSkillPoints_ReturnTrampoline
ImprovePlayerSkillPoints_Original ENDP

; Injected after the current number of perk points is read. Returns to the
; instruction where the new perk count is written back. We do this to avoid
; non-portable (across skyrim versions) accesses to the player class.
ModifyPerkPool_Wrapper PROC PUBLIC
    BEGIN_INJECTED_CALL
    mov rdx, rdi ; Get modification count.
    sub rsp, 20h
    call ModifyPerkPool_Hook
    add rsp, 20h
    END_INJECTED_CALL
    mov cl, al ; We'll return to an instruction that'll store this in the player
    jmp ModifyPerkPool_ReturnTrampoline
ModifyPerkPool_Wrapper ENDP

; Passes the EXP gain to our function for further modification.
; Note that the code here is rather different than the OG implementation
; because the original ImproveLevelExpBySkillLevel() function got inlined.
ImproveLevelExpBySkillLevel_Wrapper PROC PUBLIC
    sub rsp, 10h
    movdqu [rsp], xmm6 ; Save xmm6, we will wipe it later.

    SAVEALL ; Save all caller saved regs.

    movss xmm0, xmm1 ; xmm1 contains level exp
    mov rdx, rsi ; rsi contains skill_id.
    sub rsp, 20h
    call ImproveLevelExpBySkillLevel_Hook
    add rsp, 20h
    movss xmm6, xmm0 ; Save result in xmm6.

    RESTOREALL ; Restore all caller saved regs.

    addss xmm6, dword ptr [rax] ; This is the code we overwrote, except we
    movss dword ptr [rax], xmm6 ; now use xmm6 instead of xmm1.

    movdqu xmm6, [rsp] ; Restore xmm6.
    add rsp, 10h

    ret
ImproveLevelExpBySkillLevel_Wrapper ENDP

; Modifies the rest level of legendarying a skill depending on the user
; settings and the base level in xmm0.
LegendaryResetSkillLevel_Wrapper PROC PUBLIC
    SAVEALL
    sub rsp, 20h
    call LegendaryResetSkillLevel_Hook
    add rsp, 20h
    RESTOREALL
    ret
LegendaryResetSkillLevel_Wrapper ENDP

; Overwrites the call to the original legendary skill condition check
; with a call to our own condition check.
; Since we are overwriting another fn call, we dont need to reg save.
CheckConditionForLegendarySkill_Wrapper PROC PUBLIC
    call CheckConditionForLegendarySkill_Hook
    cmp al, 1
    jmp CheckConditionForLegendarySkill_ReturnTrampoline
CheckConditionForLegendarySkill_Wrapper ENDP

; Allows the legendary button to be hidden based on an INI setting.
; We are overwriting another function call here, so we dont need to save
; the register state.
HideLegendaryButton_Wrapper PROC PUBLIC
    call HideLegendaryButton_Hook
    cmp al, 1
    jmp HideLegendaryButton_ReturnTrampoline
HideLegendaryButton_Wrapper ENDP

END
