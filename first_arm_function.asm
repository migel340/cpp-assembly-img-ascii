
.globl _add
_add:
    add x0, x0, x1
    ret

.globl _rgbToHsvBatch
_rgbToHsvBatch:
    // Save non-volatile registers
    stp x29, x30, [sp, #-16]!
    mov x29, sp
    // Arguments: x0=src(float*), x1=dst(float*), x2=count
    cbz x2, 200f

    // vector loop count (4 pixels per iteration)
    lsr x3, x2, #2        // x3 = count / 4
    and x4, x2, #3        // x4 = count % 4

    // Prepare constants: eps = 1/1024, one = 1.0, two=2.0, four=4.0, sixty=60.0
    // compute scalar eps in s0
    mov w8, #1
    scvtf s0, w8
    mov w8, #1024
    scvtf s1, w8
    fdiv s0, s0, s1       // s0 = 1/1024
    // build vector eps v20 from integer dup + scvtf
    mov w8, #1
    dup v19.4s, w8        // int vector with 1
    mov w8, #1024
    dup v18.4s, w8        // int vector with 1024
    scvtf v19.4s, v19.4s  // v19 = 1.0
    scvtf v18.4s, v18.4s  // v18 = 1024.0
    fdiv v20.4s, v19.4s, v18.4s  // v20 = 1/1024

    // build other vector constants via dup+scvtf
    mov w8, #1
    dup v21.4s, w8
    scvtf v21.4s, v21.4s   // v21 = 1.0
    mov w8, #2
    dup v22.4s, w8
    scvtf v22.4s, v22.4s   // v22 = 2.0
    mov w8, #4
    dup v23.4s, w8
    scvtf v23.4s, v23.4s   // v23 = 4.0
    mov w8, #60
    dup v24.4s, w8
    scvtf v24.4s, v24.4s   // v24 = 60.0

10:
    cbz x3, 20f
    // load 4 pixels (interleaved RGB floats)
    ld3 {v0.4s, v1.4s, v2.4s}, [x0], #48

    // vmax = max(R,G,B)
    fmax v3.4s, v0.4s, v1.4s
    fmax v3.4s, v3.4s, v2.4s
    // vmin = min(R,G,B)
    fmin v4.4s, v0.4s, v1.4s
    fmin v4.4s, v4.4s, v2.4s
    // delta = vmax - vmin
    fsub v5.4s, v3.4s, v4.4s

    // S = delta / (vmax + eps)
    fadd v6.4s, v3.4s, v20.4s
    fdiv v6.4s, v5.4s, v6.4s

    // inv_delta = 1.0 / (delta + eps)
    fadd v8.4s, v5.4s, v20.4s
    fdiv v8.4s, v21.4s, v8.4s

    // t_r = (g - b) * inv_delta
    fsub v9.4s, v1.4s, v2.4s
    fmul v9.4s, v9.4s, v8.4s

    // t_g = (b - r) * inv_delta + 2.0
    fsub v10.4s, v2.4s, v0.4s
    fmul v10.4s, v10.4s, v8.4s
    fadd v10.4s, v10.4s, v22.4s

    // t_b = (r - g) * inv_delta + 4.0
    fsub v11.4s, v0.4s, v1.4s
    fmul v11.4s, v11.4s, v8.4s
    fadd v11.4s, v11.4s, v23.4s

    // multiply by 60.0 to get degrees
    fmul v12.4s, v9.4s, v24.4s   // Hr
    fmul v13.4s, v10.4s, v24.4s  // Hg
    fmul v14.4s, v11.4s, v24.4s  // Hb

    // masks: which channel equals vmax
    fcmeq v15.4s, v0.4s, v3.4s   // mask_r
    fcmeq v16.4s, v1.4s, v3.4s   // mask_g

    // select between Hg and Hb based on mask_g
    and v17.16b, v16.16b, v13.16b
    mvn v18.16b, v16.16b
    and v19.16b, v18.16b, v14.16b
    orr v20.16b, v17.16b, v19.16b   // v20 = Hg or Hb

    // final H = mask_r ? Hr : v20
    and v21.16b, v15.16b, v12.16b
    mvn v22.16b, v15.16b
    and v23.16b, v22.16b, v20.16b
    orr v24.16b, v21.16b, v23.16b   // v24 = H

    // zero H where delta <= eps
    fcmle v25.4s, v5.4s, v20.4s
    mvn v26.16b, v25.16b
    and v24.16b, v24.16b, v26.16b

    // move H,S,V into consecutive regs v0,v1,v2 for st3
    mov v0.16b, v24.16b
    mov v1.16b, v6.16b
    mov v2.16b, v3.16b
    st3 {v0.4s, v1.4s, v2.4s}, [x1], #48

    subs x3, x3, #1
    b.ne 10b

20:
    // scalar tail for remaining pixels
    cbz x4, 200f
    mov x5, x2
    and x5, x5, #3
    // compute start index = count - x5
    sub x6, x2, x5
    mov x7, #0
21:
    cmp x7, x5
    bge 200f
    // load pixel at index (x6 + x7)
    add x8, x0, x6, LSL #2
    ldr s0, [x8]
    ldr s1, [x8, #4]
    ldr s2, [x8, #8]
    fmax s3, s0, s1
    fmax s3, s3, s2
    fmin s4, s0, s1
    fmin s4, s4, s2
    fsub s5, s3, s4
    fadd s6, s3, s0
    fdiv s6, s5, s6
    fmov s7, #0.0
    add x9, x1, x6, LSL #2
    str s7, [x9]
    str s6, [x9, #4]
    str s3, [x9, #8]
    add x7, x7, #1
    add x6, x6, #1

    b 21b

200:
    mov sp, x29
    ldp x29, x30, [sp], #16
    ret

.globl _sobelGradients
.p2align 2
_sobelGradients:
    // x0=imageData, x1=width, x2=height, x3=stride, x4=startY, x5=endY, x6=outGx, x7=outGy
    
    // Prologue
    stp x29, x30, [sp, #-64]!
    mov x29, sp
    // Save callee-saved registers
    stp x19, x20, [sp, #16]
    stp x21, x22, [sp, #32]
    stp x23, x24, [sp, #48] 

    // Move arguments to safe registers
    mov x19, x1     // width
    mov x20, x2     // height
    mov x22, x6     // outGx
    mov x23, x7     // outGy
    
    // Temporary setup
    mov x9, x0      // imageData
    mov x10, x3     // stride
    mov x11, x4     // startY
    mov x12, x5     // endY

    // Load global luma buffer pointer
    adrp x8, _g_lumaBuffer@PAGE
    ldr x21, [x8, _g_lumaBuffer@PAGEOFF]
    cbz x21, .sobel_epilogue 

    // ==========================================
    // 1. FILL LUMA BUFFER (Grayscale conversion)
    // ==========================================
    
    mul x13, x19, x20  // total pixels
    
    mov x0, x9      // src ptr
    mov x1, x21     // dst ptr
    mov x2, #0      // loop counter

    // --- POPRAWKA 1: Ładowanie 255000 (0x3E3F8) ---
    // 255000 nie mieści się w zwykłym mov (limit 16 bitów).
    // Ładujemy dolne 16 bitów (0xE3F8 = 58360) i górne 16 bitów (0x3).
    mov w8, #58360
    movk w8, #3, lsl #16
    
    dup v3.4s, w8
    scvtf v3.4s, v3.4s // v3 = 255000.0

    // R coeff (v0)
    mov w8, #299
    dup v0.4s, w8
    scvtf v0.4s, v0.4s
    fdiv v0.4s, v0.4s, v3.4s 
    
    // G coeff (v1)
    mov w8, #587
    dup v1.4s, w8
    scvtf v1.4s, v1.4s
    fdiv v1.4s, v1.4s, v3.4s 
    
    // B coeff (v2)
    mov w8, #114
    dup v2.4s, w8
    scvtf v2.4s, v2.4s
    fdiv v2.4s, v2.4s, v3.4s 

.luma_loop_entry:
    cmp x2, x13
    bge .luma_done
    
    sub x8, x13, x2
    cmp x8, #8
    blt .luma_scalar_tail

    // Vector loop (8 pixels)
    ld3 {v4.8b, v5.8b, v6.8b}, [x0], #24
    
    // R channel
    uxtl v4.8h, v4.8b
    uxtl2 v24.4s, v4.8h    
    uxtl v4.4s, v4.4h      
    scvtf v4.4s, v4.4s
    scvtf v24.4s, v24.4s
    
    // G channel
    uxtl v5.8h, v5.8b
    uxtl2 v25.4s, v5.8h
    uxtl v5.4s, v5.4h
    scvtf v5.4s, v5.4s
    scvtf v25.4s, v25.4s
    
    // B channel
    uxtl v6.8h, v6.8b
    uxtl2 v26.4s, v6.8h
    uxtl v6.4s, v6.4h
    scvtf v6.4s, v6.4s
    scvtf v26.4s, v26.4s
    
    // Calc
    fmul v16.4s, v4.4s, v0.4s
    fmla v16.4s, v5.4s, v1.4s
    fmla v16.4s, v6.4s, v2.4s
    
    fmul v17.4s, v24.4s, v0.4s
    fmla v17.4s, v25.4s, v1.4s
    fmla v17.4s, v26.4s, v2.4s
    
    st1 {v16.4s, v17.4s}, [x1], #32
    add x2, x2, #8
    b .luma_loop_entry

.luma_scalar_tail:
    ldrb w3, [x0], #1 
    ldrb w4, [x0], #1 
    ldrb w5, [x0], #1 
    
    scvtf s4, w3
    scvtf s5, w4
    scvtf s6, w5
    
    // --- POPRAWKA 2: Użyj 'mov' zamiast 'fmov' ---
    mov s7, v0.s[0]
    mov s8, v1.s[0]
    // s24 (float) vs x24 (int) - to różne rejestry, jest ok
    mov s24, v2.s[0]
    
    fmul s4, s4, s7
    fmadd s4, s5, s8, s4
    fmadd s4, s6, s24, s4
    
    str s4, [x1], #4
    add x2, x2, #1
    cmp x2, x13
    blt .luma_scalar_tail

.luma_done:

    // ==========================================
    // 2. SOBEL OPERATOR
    // ==========================================
    
    // Clamp startY/endY
    cmp x11, #1
    csel x11, x11, x11, ge
    mov x8, #1
    csel x11, x8, x11, lt
    
    sub x8, x20, #1
    cmp x12, x8
    csel x12, x12, x12, le
    csel x12, x8, x12, gt
    
    lsl x24, x19, #2  // x24 (int) = row_bytes

.sobel_row_loop:
    cmp x11, x12
    bge .sobel_epilogue

    mul x8, x11, x19
    lsl x8, x8, #2    
    
    add x0, x22, x8   
    add x1, x23, x8   
    
    add x2, x21, x8   // cur
    sub x3, x2, x24   // prev
    add x4, x2, x24   // next
    
    add x0, x0, #4
    add x1, x1, #4
    
    mov x5, #1
    sub x6, x19, #5
    
.sobel_vec_check:
    cmp x5, x6
    bgt .sobel_scalar_col

    sub x7, x5, #1
    lsl x7, x7, #2
    
    add x13, x3, x7 
    add x14, x2, x7 
    add x15, x4, x7 
    
    // Load Top
    ld1 {v0.4s}, [x13]
    add x8, x13, #4
    ld1 {v1.4s}, [x8]
    add x8, x13, #8
    ld1 {v2.4s}, [x8]
    
    // Load Mid
    ld1 {v3.4s}, [x14]
    add x8, x14, #4
    ld1 {v4.4s}, [x8]
    add x8, x14, #8
    ld1 {v5.4s}, [x8]
    
    // Load Bot
    ld1 {v6.4s}, [x15]
    add x8, x15, #4
    ld1 {v7.4s}, [x8]
    add x8, x15, #8
    ld1 {v8.4s}, [x8]
    
    // Gx
    fadd v16.4s, v2.4s, v8.4s   
    fadd v17.4s, v5.4s, v5.4s   
    fadd v16.4s, v16.4s, v17.4s 
    
    fadd v18.4s, v0.4s, v6.4s   
    fadd v19.4s, v3.4s, v3.4s   
    fadd v18.4s, v18.4s, v19.4s 
    
    fsub v20.4s, v16.4s, v18.4s 
    
    // Gy
    fadd v16.4s, v6.4s, v8.4s   
    fadd v17.4s, v7.4s, v7.4s   
    fadd v16.4s, v16.4s, v17.4s 
    
    fadd v18.4s, v0.4s, v2.4s   
    fadd v19.4s, v1.4s, v1.4s   
    fadd v18.4s, v18.4s, v19.4s 
    
    fsub v21.4s, v16.4s, v18.4s 
    
    st1 {v20.4s}, [x0], #16
    st1 {v21.4s}, [x1], #16
    
    add x5, x5, #4
    b .sobel_vec_check

.sobel_scalar_col:
    sub x6, x19, #1
    
.sobel_scalar_loop:
    cmp x5, x6
    bge .sobel_next_row
    
    lsl x8, x5, #2
    add x14, x21, x8  
    
    sub x9, x14, x24  
    ldr s0, [x9, #-4] 
    ldr s1, [x9]      
    ldr s2, [x9, #4]  
    
    ldr s3, [x14, #-4]
    ldr s5, [x14, #4] 
    
    add x9, x14, x24  
    ldr s6, [x9, #-4] 
    ldr s7, [x9]      
    ldr s8, [x9, #4]  
    
    // Gx
    fadd s16, s2, s8
    fadd s17, s5, s5
    fadd s16, s16, s17
    
    fadd s18, s0, s6
    fadd s19, s3, s3
    fadd s18, s18, s19
    fsub s20, s16, s18
    
    // Gy
    fadd s16, s6, s8
    fadd s17, s7, s7
    fadd s16, s16, s17
    
    fadd s18, s0, s2
    fadd s19, s1, s1
    fadd s18, s18, s19
    fsub s21, s16, s18
    
    str s20, [x0], #4
    str s21, [x1], #4
    
    add x5, x5, #1
    b .sobel_scalar_loop

.sobel_next_row:
    add x11, x11, #1
    b .sobel_row_loop

.sobel_epilogue:
    ldp x23, x24, [sp, #48]
    ldp x21, x22, [sp, #32]
    ldp x19, x20, [sp, #16]
    mov sp, x29
    ldp x29, x30, [sp], #64
    ret