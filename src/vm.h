#include <nitro.h>

typedef enum VmOpcode {
    TERMINATE,
    RETURN,
    BRANCH,
    WAIT,
    JUMP_COND,
    UNUSED_05,
    UNUSED_06,
    UNUSED_07,
    UNUSED_08,
    UNKNOWN_09,
    PUSH,
    POP,
    STACK_JUMP_COND,
    MOV_OFFSET_IDX,
    MOV_OFFSET,
    MOV_INT,
    ADD_INT,
    SUB_INT,
    MUL_INT,
    DIV_INT,
    MOD_INT,
    LSL,
    ASR,
    AND,
    ORR,
    EOR,
    NOT,
    NEG,
    SQRT_INT,
    INVSQRT_INT,
    INVERT_INT,
    COS_INT,
    SIN_INT,
    ATAN_INT,
    ATAN2_INT,
    RANDOM,
    MOV_FIXED,
    TRUNC_FIXED,
    FLOOR_FIXED,
    ADD_FIXED,
    SUB_FIXED,
    MUL_FIXED,
    DIV_FIXED,
    MOD_FIXED,
    SQRT_FIXED,
    INVSQRT_FIXED,
    INVERT_FIXED,
    COS_FIXED,
    SIN_FIXED,
    ATAN_FIXED,
    ATAN2_FIXED,
    PADDING = 0x7FFF,
} VmOpcode;

typedef struct VM_Command {
    VmOpcode opcode;
    u16 result;
    u16 flags;
    u16 unknown1;
    s32 arguments[16];
    s32 unknown2;
} VM_Command;

typedef struct VM_Manager {
    u16 *pc;
    s32 unknown[25];
    s32 stack[16];
    u16 stackSize;
    u16 waitFrames;
} VM_Manager;

struct VM_Metadata;

typedef s32 (*VM_ExecuteCommandDialect)(struct VM_Metadata*, VM_Manager*, VM_Command*);

typedef struct VM_Metadata {
    char unknown1[8];
    VM_ExecuteCommandDialect executeDialect;
    u32 *attributes;
} VM_Metadata;
