#include <vm.h>
#include <nitro/fx.h>

extern void (*data_02059d8c)(void*);
extern void* data_0205003c;
extern void* data_02050040;

void VM_readCommand(VM_Metadata *metadata, VM_Manager *manager, VM_Command *command) {
    u16 attributes;
    u16 numArguments;
    VmOpcode opcode;


    opcode = manager->pc[0];
    command->opcode = opcode;
    manager->pc += 1;

    attributes = metadata->attributes[command->opcode];
    if ((attributes & 0x20) != 0) {
        command->result = *manager->pc;
        manager->pc += 1;
    }

    numArguments = attributes & 0x1F;
    if (numArguments == 0) {
        return;
    }

    if ((attributes & 0x40) != 0) {
        u16 flags;
        s32 *arguments;

        command->flags = *manager->pc;
        flags = command->flags;
        arguments = &command->arguments[0];

        manager->pc += 1;

        while (numArguments != 0) {
            u32 value;

            if ((flags & 1) == 0) {
                value = (s16)*manager->pc;
            }
            else {
                value = VM_readVariable(*manager->pc, metadata, manager);
            }

            *arguments = value;

            flags >>= 1;
            manager->pc += 1;

            arguments += 1;
            numArguments -= 1;
        }
    }

    else {
        s32 *arguments = &command->arguments[0];
        while (numArguments != 0) {
            *arguments = *(s16*)manager->pc;
            arguments += 1;
            manager->pc += 1;
            numArguments -= 1;
        }
    }
}

s32 VM_executeCommand(VM_Metadata *metadata, VM_Manager *manager, VM_Command *command) {
    VmOpcode opcode;
    s32 canJump;
    s32 *piVar5;
    u16 jumpType;
    s32 value;
    s32 offset;
    fx32 fxArg1;
    fx32 fxArg2;
    fx32 fxComp;
    u64 lVar1;

    opcode = command->opcode;
    switch (opcode) {
        case TERMINATE: {
            manager->pc = 0;
            return 1;
        }

        case RETURN: {
            if (manager->stackSize == 0) {
                manager->pc = 0;
                return 2;
            }

            manager->stackSize--;
            offset = manager->stackSize;
            manager->pc = (u16*)manager->stack[offset];
            return 0;
        }

        case BRANCH: {
            if (command->arguments[0] == 1) {
                manager->stack[manager->stackSize] = (s32)manager->pc;
                manager->stackSize = manager->stackSize + 1;
            }
            manager->pc += command->arguments[1];
            return 0;
        }

        case WAIT: {
            manager->waitFrames = (u16)command->arguments[0];
            return 3;
        }

        case JUMP_COND: {
            canJump = VM_checkJumpCondition(command->arguments[0], command->arguments[1], command->arguments[2]);
            if (command->arguments[3] == canJump) {
                manager->pc += command->arguments[4];
            }
            return 0;
        }

        case UNUSED_05: return 0;
        case UNUSED_06: return 0;
        case UNUSED_07: return 0;
        case UNUSED_08: return 0;
        case UNKNOWN_09: {
            s16 temp;
            u32 temp2;

            data_02059d8c("0b");

            value = command->flags == 0;
            value *= 2;

            value = 3 - value;
            temp = value;
            for (temp = value; temp >= 0; temp--) {
                for (
                    temp2 = (command->arguments[0] >> (temp * 8)) & 0xff;
                    temp2 != 0;
                    temp2 >>= 1) {
                    // a print would be nice here...
                }
            }
            data_02059d8c("\n");
            return 0;
        }

        case PUSH: {
            manager->stack[manager->stackSize] = command->arguments[0];
            manager->stackSize = manager->stackSize + 1;
            return 0;
        }

        case POP: {
            manager->stackSize -= 1;
            VM_writeVariable(command->result, manager->stack[manager->stackSize], metadata, manager);
            return 0;
        }

        case STACK_JUMP_COND: {
            piVar5 = manager->stack + manager->stackSize - 1;
            jumpType = 7 & command->arguments[0];

            if (jumpType == 3) *piVar5 += 1;
            else if (jumpType == 4) *piVar5 -= 1;

            canJump = VM_checkJumpCondition(command->arguments[1], command->arguments[2], *piVar5);
            if (canJump) {
                manager->pc += command->arguments[3];
                if ((command->arguments[0] & 0x18) == 0x10) {
                    manager->stackSize -= 1;
                    return 0;
                }
            }
            else if ((command->arguments[0] & 0x18) == 0x18) {
                manager->stackSize -= 1;
                return 0;
            }

            if (jumpType == 1) *piVar5 += 1;
            else if (jumpType == 2) *piVar5 -= 1;

            return 0;
        }

        case MOV_OFFSET_IDX: {
            value = (s32)(manager->pc + command->arguments[0] + command->arguments[1] * 2 + 2);
            value = *(s32*)(value);
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case MOV_OFFSET: {
            value = *(s32*)(manager->pc + command->arguments[0]);
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case MOV_INT: {
            value = command->arguments[0];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case ADD_INT: {
            value = command->arguments[0] + command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case SUB_INT: {
            value = command->arguments[0] - command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case MUL_INT: {
            value = command->arguments[0] * command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case DIV_INT: {
            value = command->arguments[0] / command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case MOD_INT: {
            value = command->arguments[0] % command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case LSL: {
            value = command->arguments[0] << command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case ASR: {
            value = command->arguments[0] >> command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case AND: {
            value = command->arguments[0] & command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case ORR: {
            value = command->arguments[0] | command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case EOR: {
            value = command->arguments[0] ^ command->arguments[1];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case NOT: {
            value = (command->arguments[0] == 0);
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case NEG: {
            value = ~command->arguments[0];
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case SQRT_INT: {
            value = FX_sqrt(command->arguments[0] << FX32B_INT);
            VM_writeVariable(
                command->result,
                FX32_TO_S32(value),
                metadata,
                manager);
            return 0;
        }

        case INVSQRT_INT: {
            value = FX_invSqrt(command->arguments[0] << FX32B_INT);
            VM_writeVariable(
                command->result,
                FX32_TO_S32(value),
                metadata,
                manager);
            return 0;
        }

        case INVERT_INT: {
            value = FX_inv(command->arguments[0] << FX32B_INT);
            VM_writeVariable(
                command->result,
                FX32_TO_S32(value),
                metadata,
                manager);
            return 0;
        }

        case COS_INT: {
            offset = (command->arguments[0] << 16) / 360;
            value = FX_sinIdx(offset & 0xffff);
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case SIN_INT: {
            offset = (command->arguments[0] << 16) / 360;
            value = FX_cosIdx(offset & 0xffff);
            VM_writeVariable(command->result, value, metadata, manager);
            return 0;
        }

        case ATAN_INT: {
            value = FX_atanIdx(command->arguments[0]);
            VM_writeVariable(
                command->result,
                (s32)(value * 360 + ((u32)(value * 360 >> 0xf) >> 0x10)) >> 0x10,
                metadata,
                manager);
            return 0;
        }

        case ATAN2_INT: {
            value = FX_atan2Idx(command->arguments[0], command->arguments[1]);
            VM_writeVariable(
                command->result,
                (s32)(value * 360 + ((u32)(value * 360 >> 0xf) >> 0x10)) >> 0x10,
                metadata,
                manager);
            return 0;
        }

        case RANDOM: {
            value = VM_random(command->arguments[0]);
            VM_writeVariable(command->result, value ,metadata, manager);
            return 0;
        }

        // 1-argument fixed point commands
        case MOV_FIXED:
        case TRUNC_FIXED:
        case FLOOR_FIXED:
        case SQRT_FIXED:
        case INVSQRT_FIXED:
        case INVERT_FIXED:
        case COS_FIXED:
        case SIN_FIXED:
        case ATAN_FIXED: {
            fxArg1 = command->arguments[0];
            if ((command->flags & 1) == 0) {
                fxArg1 = (fxArg1 & 0xffff) + command->arguments[1] * 0x10000;
            }

            switch(opcode) {
                case MOV_FIXED: {
                    value = fxArg1;
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case TRUNC_FIXED: {
                    value = fxArg1 >> FX32B_INT;
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case FLOOR_FIXED: {
                    value = fxArg1 & 0xfffff000;
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case SQRT_FIXED: {
                    value = FX_sqrt(fxArg1);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case INVSQRT_FIXED: {
                    value = FX_invSqrt(fxArg1);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case INVERT_FIXED: {
                    value = FX_inv(fxArg1);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case COS_FIXED: {
                    offset = (fxArg1 << 4) / 360;
                    value = FX_sinIdx(offset & 0xffff);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case SIN_FIXED: {
                    offset = (fxArg1 << 4) / 360;
                    value = FX_cosIdx(offset & 0xffff);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case ATAN_FIXED: {
                    value = FX_atanIdx(fxArg1);
                    VM_writeVariable(
                        command->result,
                        (s32)(value * 360 + ((u32)(value * 360 >> 3) >> 0x1c)) >> 4,
                        metadata,
                        manager);
                    return 0;
                }
                default: {
                    goto baseopc_default;
                }
            }
        }

        // 2-argument fixed point commands
        case ADD_FIXED:
        case SUB_FIXED:
        case MUL_FIXED:
        case DIV_FIXED:
        case MOD_FIXED:
        case ATAN2_FIXED: {
            fxArg1 = command->arguments[0];
            if ((command->flags & 1) == 0) {
                fxArg1 = (fxArg1 & 0xFFFF) + command->arguments[1] * 0x10000;
            }

            fxArg2 = command->arguments[2];
            if ((command->flags & 4) == 0) {
                fxArg2 = (fxArg2 & 0xFFFF) + command->arguments[3] * 0x10000;
            }

            switch (opcode) {
                case ADD_FIXED: {
                    VM_writeVariable(command->result, fxArg1 + fxArg2, metadata, manager);
                    return 0;
                }

                case SUB_FIXED: {
                    VM_writeVariable(command->result, fxArg1 - fxArg2, metadata, manager);
                    return 0;
                }

                case MUL_FIXED:
                    lVar1 = (s64)fxArg1 * (s64)fxArg2 + 0x800;
                    VM_writeVariable(
                        command->result,
                        (u32)lVar1 >> FX32B_INT | (int)((u64)lVar1 >> 0x20) * 0x100000,
                        metadata,
                        manager);
                    return 0;

                case DIV_FIXED: {
                    value = FX_div(fxArg1, fxArg2);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case MOD_FIXED: {
                    value = FX_modS32(fxArg1, fxArg2);
                    VM_writeVariable(command->result, value, metadata, manager);
                    return 0;
                }

                case ATAN2_FIXED: {
                    value = FX_atan2Idx(fxArg1, fxArg2);
                    VM_writeVariable(
                        command->result,
                        (s32)(value * 360 + ((u32)(value * 360 >> 3) >> 0x1c)) >> 4,
                        metadata,
                        manager);
                    return 0;
                }
            }
        }

        default: {
baseopc_default:;
            metadata->executeDialect(metadata, manager, command);
        }
    }
}