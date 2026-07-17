; vertex shader (ToyGPU asm): clip = MVP × vertex
; U0-U15 = MVP 矩陣（row-major），I0-I2 = 頂點 xyz，w=1 → O0-O3 = clip xyzw
LDIN  R13, I0      ; vx
LDIN  R14, I1      ; vy
LDIN  R15, I2      ; vz
LDI   R12, 1.0     ; w
; --- row 0 → O0 ---
LDUNI R1, U0
FMUL  R0, R1, R13  ; U0*vx
LDUNI R1, U1
FMA   R0, R1, R14  ; += U1*vy
LDUNI R1, U2
FMA   R0, R1, R15  ; += U2*vz
LDUNI R1, U3
FMA   R0, R1, R12  ; += U3*w
STOUT O0, R0
; --- row 1 → O1 ---
LDUNI R1, U4
FMUL  R0, R1, R13  ; U4*vx
LDUNI R1, U5
FMA   R0, R1, R14  ; += U5*vy
LDUNI R1, U6
FMA   R0, R1, R15  ; += U6*vz
LDUNI R1, U7
FMA   R0, R1, R12  ; += U7*w
STOUT O1, R0
; --- row 2 → O2 ---
LDUNI R1, U8
FMUL  R0, R1, R13  ; U8*vx
LDUNI R1, U9
FMA   R0, R1, R14  ; += U9*vy
LDUNI R1, U10
FMA   R0, R1, R15  ; += U10*vz
LDUNI R1, U11
FMA   R0, R1, R12  ; += U11*w
STOUT O2, R0
; --- row 3 → O3 ---
LDUNI R1, U12
FMUL  R0, R1, R13  ; U12*vx
LDUNI R1, U13
FMA   R0, R1, R14  ; += U13*vy
LDUNI R1, U14
FMA   R0, R1, R15  ; += U14*vz
LDUNI R1, U15
FMA   R0, R1, R12  ; += U15*w
STOUT O3, R0
RET
