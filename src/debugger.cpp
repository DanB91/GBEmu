//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

#include "gbemu.h"
#include "debugger.h"
#define IS_FLAG_SET(flag) ((cpu->F & (int)Flag::flag) != 0)
#define IS_FLAG_CLEAR(flag) ((cpu->F & (int)Flag::flag) == 0)
#define IS_DOWN(button) (input->newState.button)

static i32 disassembleInstructionAtAddress(u16 startAddress, MMU *mmu, char *outDisassembledInstruction, size_t maxLen) {
#define OP(nBPI, str, ...) snprintf(outDisassembledInstruction, maxLen, str, ##__VA_ARGS__);\
    numBytesPerInstruction = nBPI
#define NEXTB() readByte(startAddress + 1, mmu)
#define NEXTW() readWord(startAddress + 1, mmu)

        u8 instructionToExecute = readByte(startAddress, mmu);
        i32 numBytesPerInstruction = 0;
        switch (instructionToExecute) {
        case 0x0: OP(1, "NOP"); break;

        case 0x1:
        case 0x11:
        case 0x21:
        case 0x31: {
            const char *registers[] = {"BC", "DE", "HL", "SP"};
            OP(3, "LD %s, $%X", registers[instructionToExecute >> 4], NEXTW());
        } break;

        case 0x2:
        case 0x12:
        case 0x22:
        case 0x32: {
            const char *registers[] = {"BC", "DE", "HL+", "HL-"};
            OP(1, "LD (%s), A", registers[instructionToExecute >> 4]);
        } break;

        case 0x03:
        case 0x13:
        case 0x23:
        case 0x33: {
            const char *registers[] = {"BC", "DE", "HL", "SP"};
            OP(1, "INC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x04:
        case 0x14:
        case 0x24:
        case 0x34: {
            const char *registers[] = {"B", "D", "H", "(HL)"};
            OP(1, "INC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x05:
        case 0x15:
        case 0x25:
        case 0x35: {
            const char *registers[] = {"B", "D", "H", "(HL)"};
            OP(1, "DEC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x06:
        case 0x16:
        case 0x26:
        case 0x36: {
            const char *registers[] = {"B", "D", "H", "(HL)"};
            OP(2, "LD %s, $%X", registers[instructionToExecute >> 4], NEXTB());
        } break;

        case 0x07: OP(1, "RLCA");  break;
        case 0x08: OP(3, "LD ($%X), SP", NEXTW()); break;

        case 0x09:
        case 0x19:
        case 0x29:
        case 0x39: {
            const char *registers[] = {"BC", "DE", "HL", "SP"};
            OP(1, "ADD HL, %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x0A:
        case 0x1A:
        case 0x2A:
        case 0x3A: {
            const char *registers[] = {"BC", "DE", "HL+", "HL-"};
            OP(1, "LD A, (%s)", registers[instructionToExecute >> 4]);
        } break;

        case 0x0B:
        case 0x1B:
        case 0x2B:
        case 0x3B: {
            const char *registers[] = {"BC", "DE", "HL", "SP"};
            OP(1, "DEC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x0C:
        case 0x1C:
        case 0x2C:
        case 0x3C: {
            const char *registers[] = {"C", "E", "L", "A"};
            OP(1, "INC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x0D:
        case 0x1D:
        case 0x2D:
        case 0x3D: {
            const char *registers[] = {"C", "E", "L", "A"};
            OP(1, "DEC %s", registers[instructionToExecute >> 4]);
        } break;

        case 0x0E:
        case 0x1E:
        case 0x2E:
        case 0x3E: {
            const char *registers[] = {"C", "E", "L", "A"};
            OP(2, "LD %s, $%X", registers[instructionToExecute >> 4], NEXTB());
        } break;

        case 0x0F: OP(1, "RRCA"); break;
        case 0x10: OP(2, "STOP 0"); break;
        case 0x17: OP(1, "RLA");  break;
        case 0x18: OP(2, "JR %d", (i8)NEXTB()); break;
        case 0x1F: OP(1, "RRA"); break;

        case 0x20:
        case 0x30: {
            const char *conditions[] = {"NZ", "NC"};
            OP(2, "JR %s, %d", conditions[(instructionToExecute >> 4) - 2], (i8)NEXTB());
        } break;

        case 0x27: OP(1, "DAA"); break;

        case 0x28:
        case 0x38: {
            const char *conditions[] = {"Z", "C"};
            OP(2, "JR %s, %d", conditions[(instructionToExecute >> 4) - 2], (i8)NEXTB());
        } break;

        case 0x2F: OP(1, "CPL"); break;

        case 0x37: OP(1, "SCF"); break;
        case 0x3F: OP(1, "CCF"); break;

        case 0x40 ... 0x7F: {
            if (instructionToExecute == 0x76) {
                OP(1, "HALT");
                break;
            }
            const char *registers[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
            u8 dstRegIndex = ((instructionToExecute >> 4) - 4) * 2;
            u8 srcRegIndex = instructionToExecute & 0xF;
            if (srcRegIndex > 0x7) {
                dstRegIndex++;
            }
            OP(1, "LD %s, %s", registers[dstRegIndex], registers[srcRegIndex & 0x7]);

        } break;

        case 0x80 ... 0xBF: {
            const char *ops[] = {"ADD", "ADC", "SUB", "SBC", "AND", "XOR", "OR", "CP"};
            const char *registers[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
            u8 opIndex = ((instructionToExecute >> 4) - 8) * 2;
            u8 srcRegIndex = instructionToExecute & 0xF;
            if (srcRegIndex > 0x7) {
                opIndex++;
            }
            OP(1, "%s %s", ops[opIndex], registers[srcRegIndex & 0x7]);
        } break;

        case 0xC0:
        case 0xD0: {
            const char *conditions[] = {"NZ", "NC"};
            OP(1, "RET %s", conditions[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xC1:
        case 0xD1:
        case 0xE1:
        case 0xF1: {
            const char *registers[] = {"BC", "DE", "HL", "AF"};
            OP(1, "POP %s", registers[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xC2:
        case 0xD2: {
            const char *conditions[] = {"NZ", "NC"};
            OP(3, "JP %s, $%X", conditions[(instructionToExecute >> 4) - 0xC], NEXTW());
        } break;

        case 0xC3: OP(3, "JP $%X", NEXTW());  break;

        case 0xC4:
        case 0xD4: {
            const char *conditions[] = {"NZ", "NC"};
            OP(3, "CALL %s, $%X", conditions[(instructionToExecute >> 4) - 0xC], NEXTW());
        } break;

        case 0xC5:
        case 0xD5:
        case 0xE5:
        case 0xF5: {
            const char *registers[] = {"BC", "DE", "HL", "AF"};
            OP(1, "PUSH %s", registers[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xC6:
        case 0xD6:
        case 0xE6:
        case 0xF6: {
            const char *ops[] = {"ADD", "SUB", "AND", "OR"};
            OP(2, "%s $%X", ops[(instructionToExecute >> 4) - 0xC], NEXTB());
        } break;

        case 0xC7:
        case 0xD7:
        case 0xE7:
        case 0xF7: {
            const char *addrs[] = {"$0", "$10", "$20", "$30"};
            OP(1, "RST %s", addrs[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xC8:
        case 0xD8: {
            const char *conditions[] = {"Z", "C"};
            OP(1, "RET %s", conditions[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xC9: OP(1, "RET"); break;

        case 0xCA:
        case 0xDA: {
            const char *conditions[] = {"Z", "C"};
            OP(3, "JP %s, $%X", conditions[(instructionToExecute >> 4) - 0xC], NEXTW());
        } break;

        case 0xCB: {
            const char *registers[] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};
            u8 cbInstruction = NEXTB();
            switch (cbInstruction) {
            case 0x0 ... 0x3F: {
                const char *ops[] = {"RLC", "RRC", "RL", "RR", "SLA", "SRA", "SWAP", "SRL"};
                u8 opIndex = (cbInstruction >> 4) * 2;
                u8 srcRegIndex = cbInstruction & 0xF;
                if (srcRegIndex > 0x7) {
                    opIndex++;
                }
                OP(2, "%s %s", ops[opIndex], registers[srcRegIndex & 0x7]);

            } break;

            case 0x40 ... 0xFF: {
                const char *ops[] = {"BIT", "RES", "SET"};
                u8 opIndex = (cbInstruction >> 6) - 1;
                u8 srcRegIndex = cbInstruction & 0xF;
                u8 bitNum = ((cbInstruction >> 4) - (4 * (opIndex + 1))) * 2;

                if (srcRegIndex > 0x7) {
                    bitNum++;
                }
                OP(2, "%s %X, %s", ops[opIndex], bitNum, registers[srcRegIndex & 0x7]);
            } break;

            }
        } break;

        case 0xCC:
        case 0xDC: {
            const char *conditions[] = {"Z", "C"};
            OP(3, "CALL %s, $%X", conditions[(instructionToExecute >> 4) - 0xC], NEXTW());
        } break;

        case 0xCD: OP(3, "CALL $%X", NEXTW()); break;

        case 0xCE:
        case 0xDE:
        case 0xEE:
        case 0xFE: {
            const char *ops[] = {"ADC", "SBC", "XOR", "CP"};
            OP(2, "%s $%X", ops[(instructionToExecute >> 4) - 0xC], NEXTB());
        } break;

        case 0xCF:
        case 0xDF:
        case 0xEF:
        case 0xFF: {
            const char *addrs[] = {"$8", "$18", "$28", "$38"};
            OP(1, "RST %s", addrs[(instructionToExecute >> 4) - 0xC]);
        } break;

        case 0xD9: OP(1, "RETI"); break;

        case 0xE0: OP(2, "LD ($FF00+$%X), A", NEXTB()); break;
        case 0xE2: OP(1, "LD ($FF00+C), A");break;
        case 0xE8: OP(2, "ADD SP, %d", (i8)NEXTB()); break;
        case 0xE9: OP(1, "JP (HL)"); break;
        case 0xEA: OP(3, "LD ($%X), A", NEXTW()); break;

        case 0xF0: OP(2, "LD A, ($FF00+$%X)", NEXTB()); break;
        case 0xF2: OP(1, "LD A, ($FF00+C)");break;
        case 0xF3: OP(1, "DI");break;
        case 0xF8: OP(2, "LD HL, SP+%d", (i8)NEXTB()); break;
        case 0xF9: OP(1, "LD SP, HL"); break;
        case 0xFA: OP(3, "LD A, ($%X)", NEXTW()); break;
        case 0xFB: OP(1, "EI");break;
        default: OP(1, "Illegal"); break;

        }

#undef NEXTB
#undef NEXTW
#undef OP

        CO_ASSERT(numBytesPerInstruction > 0);
        return numBytesPerInstruction;

    }

    static void refreshDisassemblerIfOpen(GameBoyDebug *gbDebug, MMU *mmu) {
        if (!gbDebug->isDisassemblerOpen) {
            return;
        }
        i64 i = 0;
        for (i32 currentAddress = 0; i < MAX_INSTRUCTIONS;) {
            gbDebug->disassembledInstructionAddresses[i] = (u16)currentAddress;
            currentAddress += disassembleInstructionAtAddress((u16)currentAddress, mmu, 
                                                              gbDebug->disassembledInstructions[currentAddress], MAX_INSTRUCTION_LEN);  
            if (currentAddress > 0xFFFF) {
                break;
            }
            i++;
        }
        gbDebug->numDisassembledInstructions = i;
    }
Breakpoint *hardwareBreakpointForAddress(u16 address, BreakpointExpectedValueType expectedValueType, GameBoyDebug *gbDebug) {
    foriarr (gbDebug->breakpoints) {
        Breakpoint *bp = &gbDebug->breakpoints[i];

        if (bp->isUsed && bp->op == BreakpointOP::Equal) {
            switch (bp->type) {
            case BreakpointType::Hardware: {
                if (bp->address == address && bp->expectedValueType == expectedValueType)
                    return bp;
            }  break;
            default:
                continue;
            }
        }
    }

    return nullptr;
}

static Breakpoint
*regularBreakpointForAddress(u16 address, GameBoyDebug *gbDebug) {
    foriarr (gbDebug->breakpoints) {
        Breakpoint *bp = &gbDebug->breakpoints[i];

        if (bp->isUsed && bp->op == BreakpointOP::Equal) {
            switch (bp->type) {
            case BreakpointType::Regular: {
                if (bp->address == address)
                    return bp;
            }  break;
            default:
                continue;
            }
        }
    }

    return nullptr;
}

static void 
deleteBreakpoint(Breakpoint *bp, GameBoyDebug *gbDebug) {
    bp->isUsed = false;
    gbDebug->numBreakpoints--;
    
    //TODO: Compact
}

//TODO
//static void 
//deleteHardwareBreakpoint(u16 address, GameboyDebug *gbDebug) {
//deleteBreakpoint(address, gbDebug, true);
//}

static void 
addBreakpoint(GameBoyDebug *gbDebug, u16 address, const char *reg, BreakpointType type, BreakpointExpectedValueType expectedValueType,
             BreakpointOP op, u16 expectedVaue) {
    //TODO: add return value if more than 16 breakpoints
    foriarr (gbDebug->breakpoints) {
        if (!gbDebug->breakpoints[i].isUsed) {
            auto *bp = &gbDebug->breakpoints[i];
            *bp = {};

            switch (type) {
            case BreakpointType::Hardware:
            case BreakpointType::Regular: {
                bp->address = (u16)address;
            } break;

            case BreakpointType::Register: {
                copyMemory(reg, bp->reg, 3);
            } break;

            }

            bp->isUsed = true;
            bp->type = type;
            bp->expectedValueType = expectedValueType;
            bp->op = op;
            bp->expectedValue = expectedVaue;
            gbDebug->numBreakpoints++;
            break;
        }
    }
}


static void
addRegularBreakpoint(u16 address, GameBoyDebug *gbDebug, BreakpointOP op) {
    addBreakpoint(gbDebug, address, nullptr, BreakpointType::Regular, BreakpointExpectedValueType::None, op, 0);
}

static void 
addAnyValueHardwareBreakpoint(u16 address, GameBoyDebug *gbDebug) {
    addBreakpoint(gbDebug, address, nullptr,  BreakpointType::Hardware, BreakpointExpectedValueType::Any, BreakpointOP::Equal, true);
}

static void 
addCustomValueHardwareBreakpoint(u16 address, GameBoyDebug *gbDebug, u8 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, address, nullptr,  BreakpointType::Hardware, BreakpointExpectedValueType::Custom, op, expectedValue);
}
static void 
addBitClearHardwareBreakpoint(u16 address, GameBoyDebug *gbDebug, u8 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, address, nullptr,  BreakpointType::Hardware, BreakpointExpectedValueType::BitClear, op, expectedValue);
}

static void 
addBitSetHardwareBreakpoint(u16 address, GameBoyDebug *gbDebug, u8 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, address, nullptr,  BreakpointType::Hardware, BreakpointExpectedValueType::BitSet, op, expectedValue);
}

static void 
addCustomValueRegisterBreakpoint(const char *reg, GameBoyDebug *gbDebug, u16 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, 0, reg,  BreakpointType::Register, BreakpointExpectedValueType::Custom, op, expectedValue);
}

static void 
addBitClearRegisterBreakpoint(const char *reg, GameBoyDebug *gbDebug, u16 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, 0, reg,  BreakpointType::Register, BreakpointExpectedValueType::BitClear, op, expectedValue);
}

static void 
addBitSetRegisterBreakpoint(const char *reg, GameBoyDebug *gbDebug, u16 expectedValue, BreakpointOP op) {
    addBreakpoint(gbDebug, 0, reg,  BreakpointType::Register, BreakpointExpectedValueType::BitSet, op, expectedValue);
}
static void 
addAnyValueRegisterBreakpoint(const char *reg, GameBoyDebug *gbDebug) {
    addBreakpoint(gbDebug, 0, reg,  BreakpointType::Register, BreakpointExpectedValueType::Any, BreakpointOP::Equal, 0);
}

static void drawBreakpointDescription(Breakpoint *bp) {

    switch (bp->type) {
    case BreakpointType::Regular: {
        switch (bp->op) {
        case BreakpointOP::Equal: ImGui::Text("Break when PC == %X", bp->address); break;
        case BreakpointOP::GreaterThan: ImGui::Text("Break when PC > %X", bp->address); break;
        case BreakpointOP::LessThan: ImGui::Text("Break when PC < %X", bp->address); break;
        }
    } break;
    case BreakpointType::Hardware: {
        switch (bp->expectedValueType) {
        case BreakpointExpectedValueType::Any: {
            ImGui::Text("Break when writing to %X", bp->address);
        } break;
        case BreakpointExpectedValueType::Custom: {
            ImGui::Text("Break when writing %X to %X", bp->expectedValue, bp->address);
        } break;
        case BreakpointExpectedValueType::BitClear: {
            ImGui::Text("Break when (%X & [%X]) == 0", bp->expectedValue, bp->address);
        } break;
        case BreakpointExpectedValueType::BitSet: {
            ImGui::Text("Break when (%X & [%X]) == %X", bp->expectedValue, bp->address, bp->expectedValue);
        } break;
        case BreakpointExpectedValueType::OnePastLast:
        case BreakpointExpectedValueType::None:
            break;
        }
    } break;
    case BreakpointType::Register: {
        switch (bp->expectedValueType) {
        case BreakpointExpectedValueType::Any: {
            ImGui::Text("Break when writing to %s", bp->reg);
        } break;
        case BreakpointExpectedValueType::Custom: {
            ImGui::Text("Break when writing %X to %s", bp->expectedValue, bp->reg);
        } break;
        case BreakpointExpectedValueType::BitClear: {
            ImGui::Text("Break when (%X & %s) == 0", bp->expectedValue, bp->reg);
        } break;
        case BreakpointExpectedValueType::BitSet: {
            ImGui::Text("Break when (%X & %s) == %X", bp->expectedValue, bp->reg, bp->expectedValue);
        } break;
        case BreakpointExpectedValueType::OnePastLast:
        case BreakpointExpectedValueType::None:
            break;
        }

    } break;
    }

}

static void drawHitBreakpointDescription(Breakpoint *bp, GameBoyDebug *gbDebug) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
    drawBreakpointDescription(bp);
    if (bp->type == BreakpointType::Hardware || bp->type == BreakpointType::Register) {
        ImGui::SameLine();    
        ImGui::Text("-- Old Value: %X, New Value: %X",
                    gbDebug->hitBreakpoint->valueBefore,
                    gbDebug->hitBreakpoint->valueAfter);

    }
    ImGui::PopStyleColor();
}

static void drawBreakpointWindow(GameBoyDebug *gbDebug, TimeUS dt) {

    if (ImGui::Begin("Breakpoints", &gbDebug->isBreakpointsViewOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        //list break points
        if (gbDebug->numBreakpoints > 0) {

            ImGui::BeginChild("##breakpoint_scrolling",
                              ImVec2(0, ImGui::GetFrameHeightWithSpacing() * gbDebug->numBreakpoints - 1));

            ImGuiListClipper clipper((i32)gbDebug->numBreakpoints);
            i32 line = 0;
            while (clipper.Step()) {
                foriarr (gbDebug->breakpoints) {
                    Breakpoint *bp = gbDebug->breakpoints + i;
                    if (bp->isUsed) {
                        if (bp == gbDebug->hitBreakpoint) {
                            drawHitBreakpointDescription(bp, gbDebug);
                        }
                        else {
                            drawBreakpointDescription(bp);
                        }
                        ImGui::SameLine();
                        {
                            i64 labelCharLen = 12;
                            char *label = PUSHM(labelCharLen, char);
                            AutoMemory _am(label);

                            snprintf(label, (size_t)labelCharLen, "Delete##%zd", i);
                            if (ImGui::Button(label)) {
                                bp->isUsed = false;
                                gbDebug->numBreakpoints--;
                            }
                            ImGui::SameLine(); 
                            snprintf(label, (size_t)labelCharLen, (bp->isDisabled) ? "Enable##%zd":"Disable##%zd", i);
                            if (ImGui::Button(label)) {
                                bp->isDisabled = !bp->isDisabled;
                            }
                        }

                        line++;
                    }

                }

            }
            ImGui::EndChild();
            if (ImGui::Button("Clear all")) {
                foriarr (gbDebug->breakpoints) {
                    gbDebug->breakpoints[i].isUsed = false;
                }
                gbDebug->numBreakpoints = 0;
            }
        }

        static const char *opItems[] = {
            "=", "<", ">"  
        };
        {
            char inputAddr[5] = {};
            ImGui::Text("Break if PC ");
            ImGui::SameLine();

            static int indexOfCurrentItem = 0;
            ImGui::Combo("##opselect", &indexOfCurrentItem, opItems, ARRAY_LEN(opItems)); 
            ImGui::SameLine();

            ImGui::Text("address: ");
            ImGui::SameLine();
            if (ImGui::InputText("##regbp", inputAddr, 5,
                                 ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsHexadecimal)) {
                u16 bpAddr;
                sscanf(inputAddr, "%hx", &bpAddr);

                addRegularBreakpoint(bpAddr, gbDebug, (BreakpointOP)indexOfCurrentItem);

            }
        }
        ImGui::Separator();
        ImGui::NewLine();
        static const char *hwBreakpointTypes[] = {"Any Value", "Custom Value", "Bits are set", "Bits are cleared"};
        constexpr int ANY_INDEX = 0;
        constexpr int CUSTOM_INDEX = 1;  
        constexpr int BIT_SET_INDEX = 2;
        constexpr int BIT_CLEAR_INDEX = 3;
        {
            static char inputAddr[5] = {}, inputExpectedValue[3] = {};
            static int indexOfHWBreakpointType = 0;

            static const char *errMessage = nullptr;
            static TimeUS timeShownTheErrorMessage = 0;

            ImGui::Text("Break if writing value to address:");
            ImGui::SameLine();

            ImGui::InputText("##hwbp", inputAddr, 5, ImGuiInputTextFlags_CharsHexadecimal); 
            ImGui::Text("When to break:");
            ImGui::SameLine();
            ImGui::Combo("##hwbp_type", &indexOfHWBreakpointType, hwBreakpointTypes, ARRAY_LEN(hwBreakpointTypes));
            if (indexOfHWBreakpointType > 0) {
                ImGui::Text("Value:");
                ImGui::SameLine();
                ImGui::InputText("##expectedvalue", inputExpectedValue, 3, ImGuiInputTextFlags_CharsHexadecimal);
            }

            ImGui::SameLine();
            if (ImGui::Button("Add##addbp")) {
                if (stringLength(inputAddr, sizeof(inputAddr)) > 0) {
                    int hwBPAddr, expectedValue;
                    sscanf(inputAddr, "%x", &hwBPAddr);
                    switch (indexOfHWBreakpointType) {
                    case ANY_INDEX: {
                        addAnyValueHardwareBreakpoint((u16)hwBPAddr, gbDebug);
                        zeroMemory(inputAddr, sizeof(inputAddr));
                        errMessage = nullptr;
                    } break;
                    case CUSTOM_INDEX: {
                        if (stringLength(inputExpectedValue, sizeof(inputExpectedValue)) > 0) {
                            sscanf(inputExpectedValue, "%x", &expectedValue);
                            addCustomValueHardwareBreakpoint((u16)hwBPAddr, gbDebug, (u8)expectedValue, BreakpointOP::Equal);
                            zeroMemory(inputAddr, sizeof(inputAddr));
                            zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                            errMessage = nullptr;
                        }
                        else {
                            errMessage = "Please enter in a value.";
                        }
                    } break;
                    case BIT_CLEAR_INDEX: {
                        if (stringLength(inputExpectedValue, sizeof(inputExpectedValue)) > 0) {
                            sscanf(inputExpectedValue, "%x", &expectedValue);
                            if (expectedValue != 0) {
                                addBitClearHardwareBreakpoint((u16)hwBPAddr, gbDebug, (u8)expectedValue, BreakpointOP::Equal);
                                zeroMemory(inputAddr, sizeof(inputAddr));
                                zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                                errMessage = nullptr;
                            }
                            else {
                                errMessage = "Value must not be 0.";
                            }
                        }
                        else {
                            errMessage = "Please enter in a value.";
                        }

                    } break;
                    case BIT_SET_INDEX: {
                        if (stringLength(inputExpectedValue, sizeof(inputExpectedValue)) > 0) {
                            sscanf(inputExpectedValue, "%x", &expectedValue);
                            if (expectedValue != 0) {
                                addBitSetHardwareBreakpoint((u16)hwBPAddr, gbDebug, (u8)expectedValue, BreakpointOP::Equal);
                                zeroMemory(inputAddr, sizeof(inputAddr));
                                zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                                errMessage = nullptr;
                            }
                            else {
                                errMessage = "Value must not be 0.";
                            }
                        }
                        else {
                            errMessage = "Please enter in a value.";
                        }

                    } break;
                    }

                }
                else {
                    errMessage = "Please enter in an address.";
                }

            }

            if (errMessage) {
                timeShownTheErrorMessage += dt;
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s", errMessage);
                //show error for 2 seconds
                if (timeShownTheErrorMessage > 2000000) {
                    errMessage = nullptr; 
                    timeShownTheErrorMessage = 0;
                }
            }

        }
        ImGui::Separator();
        ImGui::NewLine();
        {
            static int indexOfRegisterBreakpointType = 0;
            static char inputExpectedValue[5] = {};
            static const char *errMessage = nullptr;
            static TimeUS timeShownTheErrorMessage = 0;
            static int currentRegisterIndex = 0;
            static const char *registers[] = {
                "A", "B", "C", "D", "E", "F", "H", "L", 
                "AF", "BC", "DE", "HL", "SP", "PC"
            };

            ImGui::Text("Break if writing value to register:");
            ImGui::SameLine();

            ImGui::Combo("##registers", &currentRegisterIndex, registers, ARRAY_LEN(registers));


            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::Spacing();
            ImGui::SameLine();
            ImGui::Text("Any Value:");
            ImGui::SameLine();
            ImGui::Combo("##registerbp_type", &indexOfRegisterBreakpointType, hwBreakpointTypes, ARRAY_LEN(hwBreakpointTypes));
            if (indexOfRegisterBreakpointType > 0) {
                ImGui::Text("Value:");
                ImGui::SameLine();
                ImGui::InputText("##expectedvalue", inputExpectedValue, 3, ImGuiInputTextFlags_CharsHexadecimal);
            }

            ImGui::SameLine();
            if (ImGui::Button("Add##addregbp")) {
                const char *chosenReg = registers[currentRegisterIndex];
                int expectedValue;
                switch (indexOfRegisterBreakpointType) {
                case ANY_INDEX: {
                    addAnyValueRegisterBreakpoint(chosenReg, gbDebug);
                    errMessage = nullptr;
                } break; 
                case CUSTOM_INDEX: {
                    int valueLength = stringLength(inputExpectedValue, sizeof(inputExpectedValue));
                    if (valueLength > 0) {
                        sscanf(inputExpectedValue, "%x", &expectedValue);
                        addCustomValueRegisterBreakpoint(chosenReg, gbDebug, (u16)expectedValue, BreakpointOP::Equal);
                        zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                        errMessage = nullptr;
                    }
                    else {
                        errMessage = "Please enter in a value.";
                    }

                } break; 
                case BIT_CLEAR_INDEX: {
                    if (stringLength(inputExpectedValue, sizeof(inputExpectedValue)) > 0) {
                        sscanf(inputExpectedValue, "%x", &expectedValue);
                        if (expectedValue != 0) {
                            sscanf(inputExpectedValue, "%x", &expectedValue);
                            addBitClearRegisterBreakpoint(chosenReg, gbDebug, (u16)expectedValue, BreakpointOP::Equal);
                            zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                            errMessage = nullptr;
                        }
                        else {
                            errMessage = "Value must not be 0.";
                        }
                    }
                    else {
                        errMessage = "Please enter in a value.";
                    }

                } break;
                case BIT_SET_INDEX: {
                    if (stringLength(inputExpectedValue, sizeof(inputExpectedValue)) > 0) {
                        sscanf(inputExpectedValue, "%x", &expectedValue);
                        if (expectedValue != 0) {
                            sscanf(inputExpectedValue, "%x", &expectedValue);
                            addBitSetRegisterBreakpoint(chosenReg, gbDebug, (u16)expectedValue, BreakpointOP::Equal);
                            zeroMemory(inputExpectedValue, sizeof(inputExpectedValue));
                            errMessage = nullptr;
                        }
                        else {
                            errMessage = "Value must not be 0.";
                        }
                    }
                    else {
                        errMessage = "Please enter in a value.";
                    }

                } break;
                }
            }

            if (errMessage) {
                timeShownTheErrorMessage += dt;
                ImGui::TextColored(ImVec4(255, 0, 0, 255), "%s", errMessage);
                //show error for 2 seconds
                if (timeShownTheErrorMessage > 2000000) {
                    errMessage = nullptr; 
                    timeShownTheErrorMessage = 0;
                }
            }

        }



    }
    ImGui::End();
}

#ifdef CO_PROFILE
static void drawProfileSection(ProfileState *profileState, ProfileSectionState *pss, ProfileSectionState *parent, double now) {
    bool shouldDrawChildren = false;
    if (pss->numChildren > 0 ) {
        if (ImGui::TreeNode(pss->sectionName)) {
            shouldDrawChildren = true; 
        }
    }
    else {
        ImGui::Text("%s", pss->sectionName);
    }
    ImGui::NextColumn();
    char *maxRunStr = nullptr;
    buf_gen_memory_printf(maxRunStr, "%.2fus##%s", pss->maxRunTimeInSeconds * 1000000, pss->sectionName);
    ImGui::Selectable(maxRunStr, false, ImGuiSelectableFlags_SpanAllColumns);
    buf_gen_memory_free(maxRunStr);

    ImGui::NextColumn();
    ImGui::Text("%.2fus", (pss->totalRunTimeInSeconds / pss->numTimesCalled) * 1000000);
    ImGui::NextColumn();
    ImGui::Text("%.2fs", pss->totalRunTimeInSeconds);
    ImGui::NextColumn();
    ImGui::Text("%.2f%%",  (pss->totalRunTimeInSeconds / (now - profileState->programStartTime)) * 100);
    ImGui::NextColumn();
    if (parent) {
        ImGui::Text("%.2f%%",  (pss->totalRunTimeInSeconds / parent->totalRunTimeInSeconds) * 100);
    }
    else {
        ImGui::Text("%.2f%%",  (pss->totalRunTimeInSeconds / (now - profileState->programStartTime)) * 100);
    }
    ImGui::NextColumn();

    if (shouldDrawChildren) {
        fori (pss->numChildren) {
            drawProfileSection(profileState, pss->children[i], pss, now);
        }
        ImGui::TreePop();
    }
}
#endif
void drawDebugger(GameBoyDebug *gbDebug, MMU *mmu, CPU *cpu, ProgramState *programState, TimeUS dt) {

    SoundState *soundState = &programState->soundState;
#ifdef CO_PROFILE
    ProfileState *profileState = &programState->profileState;
#endif
    Input *input = &programState->input;

    /*************************
         * ImGui/Debug window Code
         *************************/

    if (cpu->isPaused && !gbDebug->wasCPUPaused) {
        gbDebug->wasCPUPaused = true;
        gbDebug->shouldRefreshDisassembler = true;
    }
    else if (!cpu->isPaused) {
        gbDebug->wasCPUPaused = false;
    }

    ImGui::SetCurrentContext((ImGuiContext*)programState->guiContext);


    /* ImGui draw code  */
    foriarr (gbDebug->tiles) {
        auto tile = &gbDebug->tiles[i];
        if (tile->needsUpdate) {
            for (i64 y = 0; y < TILE_HEIGHT; y++) {
                for (i64 x = 0; x < TILE_WIDTH; x++) {
                    u16 absoluteTileAddr = (u16)i * BYTES_PER_TILE;
                    u8 xMask = 0x80 >> (x & 7); 

                    absoluteTileAddr += (u16)(y & 7) * BYTES_PER_TILE_ROW;

                    u8 highBit = (mmu->lcd.videoRAM[absoluteTileAddr + 1] & xMask) ? 1 : 0;
                    u8 lowBit = (mmu->lcd.videoRAM[absoluteTileAddr] & xMask) ? 1 : 0;

                    auto color = (ColorID)((highBit * 2) + lowBit);
                    //auto color =  colorIDFromTileReference((u8)i, (u8)x, (u8)y, mmu->lcd.videoRAM);
                    tile->pixels[y*TILE_WIDTH + x] = color;
                }
            }
        }
    }
    if (!(gbDebug->isTypingInTextBox && input->newState.enterPressed)) {
        gbDebug->isTypingInTextBox = ImGui::GetIO().WantCaptureKeyboard;
    }

#undef IS_DOWN
#undef WAS_PRESSED

    if (cpu->didHitIllegalOpcode) {
        ImGui::OpenPopup("Illegal Opcode");
        if (ImGui::BeginPopupModal("Illegal Opcode")) {
            ImGui::Text("Illegal opcode hit");

            if (ImGui::Button("OK")) {
                cpu->didHitIllegalOpcode = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    if (ImGui::Begin("Debug data", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::SetWindowPos(ImVec2(0, 0), ImGuiCond_Once);

        ImGui::Text("Frame time %.2f", gbDebug->frameTimeMS);
        ImGui::Text("Number of states saved: %zd", gbDebug->numGBStates);
        ImGui::Text("Mouse X: %d, Y: %d", input->newState.mouseX / DEFAULT_SCREEN_SCALE, input->newState.mouseY/ DEFAULT_SCREEN_SCALE);
        if (ImGui::CollapsingHeader("CPU")) {
            ImGui::Text("A: %X B: %X C: %X D: %X", cpu->A, cpu->B, cpu->C, cpu->D);
            ImGui::Text("E: %X F: %X H: %X L: %X", cpu->E, cpu->F, cpu->H, cpu->L);
            ImGui::Text("PC: %X SP: %X", cpu->PC, cpu->SP);
            ImGui::Text("Z: %d N: %d H: %d C %d", IS_FLAG_SET(Z),
                        IS_FLAG_SET(N), IS_FLAG_SET(H), IS_FLAG_SET(C));
            ImGui::Text("Next instruction to execute: %X", readByte(cpu->PC, mmu));
            {
                char disassembledInstruction[MAX_INSTRUCTION_LEN];
                disassembleInstructionAtAddress(cpu->PC, mmu, disassembledInstruction, ARRAY_LEN(disassembledInstruction));
                ImGui::Text("Disassembled instruction:\n\t%s", disassembledInstruction);
            }

            ImGui::Text("All Interrupts Enabled: %s", cpu->enableInterrupts ? "true" : "false");
            ImGui::Text("Interrupts enabled: VBlank: %s, LCD: %s, Timer: %s", 
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::VBlankRequested, mmu->enabledInterrupts)),
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::LCDRequested, mmu->enabledInterrupts)),
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::TimerRequested, mmu->enabledInterrupts)));
            ImGui::Text("Interrupts to service: VBlank: %s, LCD: %s, Timer: %s", 
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::VBlankRequested,(u8)(mmu->enabledInterrupts & mmu->requestedInterrupts))),
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::LCDRequested, (u8)(mmu->enabledInterrupts & mmu->requestedInterrupts))),
                        BOOL_TO_STR(isBitSet((int)InterruptRequestedBit::TimerRequested, u8(mmu->enabledInterrupts & mmu->requestedInterrupts))));
            ImGui::Text("Is halted: %s", BOOL_TO_STR(cpu->isHalted));

            //ImGui::Text("In BIOS: %s", BOOL_TO_STR(mmu->inBios));
            ImGui::Text("Clock speed: %f cyles", cpu->cylesExecutedThisFrame / (dt/ 1000000.));
            ImGui::Text("Total Cycles: %" PRId64, cpu->totalCycles);
        }

        if (ImGui::CollapsingHeader("Timer/Divider")) {
            ImGui::Text("Divider: %d", mmu->divider);
            ImGui::Text("Timer: %d, Timer Enabled: %s", mmu->timer,
                        mmu->isTimerEnabled ? "true" : "false");
            ImGui::Text("Cycles since last incr %d, Timer increment rate: %d", mmu->cyclesSinceTimerIncrement, mmu->timerIncrementRate);
            ImGui::Text("Timer Modulo: %d", mmu->timerModulo);
            if (mmu->hasRTC) {
                ImGui::Text("RTC -- Days High: %d, Days: %d, Hours: %d, Minutes %d, Seconds: %d",
                            mmu->rtc.daysHigh, mmu->rtc.days, mmu->rtc.hours, 
                            mmu->rtc.minutes, mmu->rtc.seconds);
                ImGui::Text("Is RTC stopped: %s, Did RTC overflow: %s",BOOL_TO_STR(mmu->rtc.isStopped), BOOL_TO_STR(mmu->rtc.didOverflow)); 
                ImGui::Text("Latched -- Days: %d, Hours: %d, Minutes %d, Seconds: %d",
                            mmu->rtc.latchedDays, mmu->rtc.latchedHours, mmu->rtc.latchedMinutes,
                            mmu->rtc.latchedSeconds);
            }
        }

        if (ImGui::CollapsingHeader("LCD")) {
            const char *lcdModeStr;

            switch (mmu->lcd.mode) {
            case LCDMode::HBlank:
                lcdModeStr = "HBlank";
                break;
            case LCDMode::VBlank:
                lcdModeStr = "VBlank";
                break;
            case LCDMode::ScanOAM:
                lcdModeStr = "ScanOAM";
                break;
            case LCDMode::ScanVRAMAndOAM:
                lcdModeStr = "ScanVRAMAndOAM";
                break;

            }

            ImGui::Text("Enabled: %s", mmu->lcd.isEnabled ? "true" : "false");
            ImGui::Text("Mode: %s", lcdModeStr);
            ImGui::Text("Mode clock: %d", mmu->lcd.modeClock);
            ImGui::Text("scx: %u scy: %u", mmu->lcd.scx, mmu->lcd.scy);
            ImGui::Text("wx: %u wy: %u", mmu->lcd.wx, mmu->lcd.wy);
            ImGui::Text("ly: %u lyc: %u", mmu->lcd.ly, mmu->lcd.lyc);
            ImGui::Text("Bckg Tile Map: %d Backg Tile set: %d", mmu->lcd.backgroundTileMap, mmu->lcd.backgroundTileSet);
            ImGui::Text("Window Tile Map: %d", mmu->lcd.windowTileMap);
            ImGui::Text("Bckg palette color %d, %d, %d, %d",
                        mmu->lcd.backgroundPalette[0], mmu->lcd.backgroundPalette[1],
                    mmu->lcd.backgroundPalette[2], mmu->lcd.backgroundPalette[3]);
            ImGui::Text("OAM enabled: %s, Window Enabled: %s", mmu->lcd.isOAMEnabled ? "true" : "false", mmu->lcd.isWindowEnabled ? "true": "false");
            ImGui::Text("OAM palette 0 color %d, %d, %d, %d",
                        mmu->lcd.spritePalette0[0], mmu->lcd.spritePalette0[1],
                    mmu->lcd.spritePalette0[2], mmu->lcd.spritePalette0[3]);
            ImGui::Text("OAM palette 1 color %d, %d, %d, %d",
                        mmu->lcd.spritePalette1[0], mmu->lcd.spritePalette1[1],
                    mmu->lcd.spritePalette1[2], mmu->lcd.spritePalette1[3]);
            ImGui::Text("Sprite height %s", (mmu->lcd.spriteHeight == SpriteHeight::Tall) ? "Tall" : (mmu->lcd.spriteHeight == SpriteHeight::Short) ? "Short": "Error");
            ImGui::Text("Is DMA Occurring: %s", BOOL_TO_STR(mmu->isDMAOccurring));
            ImGui::Text("STAT interrupts enabled: LYC: %s, OAM:%s , VBlank: %s, HBlank %s",
                        BOOL_TO_STR(isBitSet((int)LCDCBit::LY_LYCCoincidenceInterrupt,mmu->lcd.stat)),
                        BOOL_TO_STR(isBitSet((int)LCDCBit::OAMInterrupt,mmu->lcd.stat)),
                        BOOL_TO_STR(isBitSet((int)LCDCBit::VBlankInterrupt,mmu->lcd.stat)),
                        BOOL_TO_STR(isBitSet((int)LCDCBit::HBlankInterrupt,mmu->lcd.stat)));
            if (mmu->isDMAOccurring) {
                ImGui::Text("DMA src: %X, DMA dest: %X", mmu->currentDMAAddress, 0xFE00 + (mmu->currentDMAAddress & 0xFF));
            }

        }

        if (ImGui::CollapsingHeader("Bank Data")) {
            ImGui::Text("MBC: %s, Banking Mode: %X",
                        (mmu->mbcType == MBCType::MBC0) ? "MBC0" :
                                                          (mmu->mbcType == MBCType::MBC1) ? "MBC1" :
                                                                                            (mmu->mbcType == MBCType::MBC3) ? "MBC3" : 
                                                                                                                              (mmu->mbcType == MBCType::MBC5) ? "MBC5": "?",
                        mmu->bankingMode);
            ImGui::Text("ROM Bank: %X, RAM Bank: %X",
                        mmu->currentROMBank, mmu->currentRAMBank);
            ImGui::Text("Has RAM: %s, Has Battery: %s, Has RTC: %s",
                        BOOL_TO_STR(mmu->hasRAM), BOOL_TO_STR(mmu->hasBattery), BOOL_TO_STR(mmu->hasRTC));
            ImGui::Text("RAM Enabled: %s, RAM Size: %zd", BOOL_TO_STR(mmu->isCartRAMEnabled), (isize)mmu->cartRAMSize);
            ImGui::Text("ROM Size %zd", (isize)mmu->romSize);

        }

        if (ImGui::CollapsingHeader("Windows", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button("Memory Viewer")) {
                if (gbDebug->isMemoryViewOpen) {
                    ImGui::SetWindowFocus("Memory Viewer");
                }
                else {
                    gbDebug->isMemoryViewOpen = true;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Disassembler")) {
                gbDebug->shouldRefreshDisassembler = true;
                if (gbDebug->isDisassemblerOpen) {
                    ImGui::SetWindowFocus("Disassembler");
                }
                else {
                    gbDebug->isDisassemblerOpen = true;
                }
            }


            ImGui::SameLine();

            if (ImGui::Button("Breakpoints")) {
                if (gbDebug->isBreakpointsViewOpen) {
                    ImGui::SetWindowFocus("Breakpoints");
                }
                else {
                    gbDebug->isBreakpointsViewOpen = true;
                }
            }

            if (ImGui::Button("OAM Debug")) {
                if (gbDebug->isOAMViewOpen) {
                    ImGui::SetWindowFocus("OAM Debug"); 
                }
                else {
                    gbDebug->isOAMViewOpen = true;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Background Tiles")) {
                if (gbDebug->isBackgroundTileMapOpen) {
                    ImGui::SetWindowFocus("Background Tiles");
                }
                else {
                    gbDebug->isBackgroundTileMapOpen = true;
                }
            }

            ImGui::SameLine();

            if (ImGui::Button("Sound Debug")) {
                if (gbDebug->isSoundViewOpen) {
                    ImGui::SetWindowFocus("Sound Debug"); 
                }
                else {
                    gbDebug->isSoundViewOpen = true;
                }
            }
#ifdef CO_PROFILE 
            if (ImGui::Button("Profiler")) {
                if (gbDebug->isProfilerOpen) {
                    ImGui::SetWindowFocus("Profiler"); 
                }
                else {
                    gbDebug->isProfilerOpen = true;
                }
            }
#endif
        }

        if (ImGui::CollapsingHeader("Debug Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (gbDebug->isRecordDebugStateEnabled && cpu->isPaused) {
                ImGui::Text("Number of instructions recorded: %d", gbDebug->numDebugStates);
            }
            if (cpu->isPaused) {
                if (ImGui::Button("Step")) {
                    step(cpu, mmu, gbDebug, programState->soundState.volume);
                    gbDebug->shouldRefreshDisassembler = true;
                }
                if (gbDebug->numDebugStates > 0) {
                    ImGui::SameLine();
                    if (ImGui::Button("Back")) {

                        if (gbDebug->currentDebugState - 1 < 0) {
                            gbDebug->currentDebugState = ARRAY_LEN(gbDebug->prevGBDebugStates) - 1;
                        }
                        else {
                            gbDebug->currentDebugState--;
                        }
                        gbDebug->numDebugStates--;


                        auto prevDebugState = &gbDebug->prevGBDebugStates[gbDebug->currentDebugState];
                        *cpu = prevDebugState->cpu;

                        if (mmu->hasRAM) {
                            copyMemory(prevDebugState->mmu.cartRAM, mmu->cartRAM, mmu->cartRAMSize);
                        }
                        *mmu = prevDebugState->mmu;
                        setPausedState(true, programState, cpu);
                    }
                }
                ImGui::SameLine();

                if (ImGui::Button("Continue")) {
                    continueFromBreakPoint(gbDebug, mmu, cpu, programState);
                }

            }
            else {
                if (ImGui::Button("Pause")) {
                    setPausedState(true, programState, cpu);
                    gbDebug->shouldRefreshDisassembler = true;
                }

            }
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                reset(cpu, mmu, gbDebug, programState);
                gbDebug->shouldRefreshDisassembler = true;
            }
            //ImGui::SameLine();
            //ImGui::Checkbox("Skip Boot Screen", &gbDebug->shouldSkipBootScreen);

            ImGui::Checkbox("Debug Recording (Slow)", &gbDebug->isRecordDebugStateEnabled);

            if (!gbDebug->isRecordDebugStateEnabled && !cpu->isPaused) {
                gbDebug->numDebugStates = 0;

            }
        }

        if (ImGui::CollapsingHeader("Sound Control", ImGuiTreeNodeFlags_DefaultOpen)) {
            int tmpVolume = soundState->volume * 2;
            ImGui::SliderInt("Volume", &tmpVolume, 0, 100);
            soundState->volume = tmpVolume / 2;
            if (ImGui::Button(soundState->isMuted ? "Unmute" : "Mute")) {
                soundState->isMuted = !soundState->isMuted;
                programState->shouldUpdateTitleBar = true;
            }
        }

#ifdef CO_DEBUG
        if (ImGui::CollapsingHeader("Program Memory")) {
            auto mss = memorySnapShot();

            fori (mss->numStacks) {
                ImGui::Text("Amount used for stack %s: %.2f KB",
                            mss->existingStacks[i]->name, mss->usedSpacePerStack[i]/1000.);
            }

        }
#endif


    }
    ImGui::End();

#ifdef CO_PROFILE
    if (gbDebug->isProfilerOpen) {
        double now = nowInSeconds();
        if (ImGui::Begin("Profiler", &gbDebug->isProfilerOpen)) {
            char fileName[MAX_PATH_LEN];
            constexpr const char *columns[] = {"Name", "Slowest", "Avg", "Total", "Pct of Runtime"};
            constexpr int numColumns = ARRAY_LEN(columns);
            ImGui::Text("Total Runtime %.2fs:", now - profileState->programStartTime);
            ImGui::SameLine();
            if (ImGui::Button("Reset")) {
                profileInit(profileState); 
            }
            ImGui::SameLine();
            if (ImGui::Button("Save")) {
                char *buffer = nullptr;
                fori(numColumns - 1) {
                    buf_gen_memory_printf(buffer, "%s,", columns[i]);
                }
                buf_gen_memory_printf(buffer, "%s,Total Runtime", columns[numColumns - 1]);
                buf_gen_memory_printf(buffer, "\r\n");

                fori(profileState->numProfileSections) {
                    auto pss = profileSectionStateForName(profileState->sectionNames[i], profileState);
                    buf_gen_memory_printf(buffer, "%s,", pss->sectionName);
                    buf_gen_memory_printf(buffer, "%.2fus,", pss->maxRunTimeInSeconds * 1000000);
                    buf_gen_memory_printf(buffer, "%.2fus,", (pss->totalRunTimeInSeconds / pss->numTimesCalled) * 1000000);
                    buf_gen_memory_printf(buffer, "%.2fs,", pss->totalRunTimeInSeconds);
                    buf_gen_memory_printf(buffer, "%.2f%%,", (pss->totalRunTimeInSeconds / (now - profileState->programStartTime)) * 100);
                    buf_gen_memory_printf(buffer, "%.2f", now - profileState->programStartTime);
                    buf_gen_memory_printf(buffer, "\r\n");
                }
                i64 fileNameLen = timestampFileName("profile", "csv", fileName);
                writeDataToFile(buffer, (i64)buf_len(buffer), fileName);
                copyMemory(fileName, gbDebug->lastFileNameWritten, (i64)fileNameLen + 1);
                buf_gen_memory_free(buffer);
            }

            if (gbDebug->lastFileNameWritten[0]) {
                ImGui::SameLine();
                ImGui::Text("Last saved: ");
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
                ImGui::Text("%s", gbDebug->lastFileNameWritten);
                ImGui::PopStyleColor();
            }


            ImGui::Columns(6, "profiler");
            ImGui::Separator();
            fori (numColumns) {
                ImGui::Text("%s:", columns[i]); 
                ImGui::NextColumn();
            }
            ImGui::Text("Pct of Parent:"); 
            ImGui::NextColumn();
            ImGui::Separator();
            fori (profileState->numProfileSections) {
                auto pss = profileSectionStateForName(profileState->sectionNames[i], profileState);
                if (pss->isRoot) {
                    drawProfileSection(profileState, pss, nullptr, now);
                }
            }
            ImGui::Columns(1);
            ImGui::Separator();
        }
        ImGui::End();
    }
#endif

    ImGui::SetNextWindowSize(ImVec2(300,200), ImGuiCond_Once);
    if (gbDebug->isBreakpointsViewOpen) {
        drawBreakpointWindow(gbDebug, dt);
    }
    ImVec2 debugDataSize = ImGui::GetWindowSize();
    if (gbDebug->isBackgroundTileMapOpen) {
        if (ImGui::Begin("Background Tiles", &gbDebug->isBackgroundTileMapOpen, ImGuiWindowFlags_AlwaysAutoResize )) {
            if (ImGui::GetWindowPos().x == 0 && ImGui::GetWindowPos().y == 0) {
                ImGui::SetWindowPos(ImVec2(debugDataSize.x, 0), ImGuiCond_Once);
            }

            struct Tile {
                int tileIndex;
                int x, y;
                u16 backgroundTileRefAddr;
                u8 backgroundTileRef; 
            };
            Tile *tiles = nullptr;
            for (int y = mmu->lcd.scy / TILE_HEIGHT; y < 18 + (mmu->lcd.scy/TILE_HEIGHT); y++) {
                for (int x = mmu->lcd.scx / TILE_WIDTH; x <  20 + (mmu->lcd.scx/TILE_WIDTH); x++) {
                    //TODO: get tile map value (1 byte)
                    u16 backgroundTileRefAddr = 0x1800;
                    switch (mmu->lcd.backgroundTileMap) {
                    //it is 0x1800 instead of 0x9800 because this is relative to start of vram
                    case 0: backgroundTileRefAddr = 0x1800; break;
                    case 1: backgroundTileRefAddr = 0x1C00; break;
                    default: CO_ERR("Wrong tile map");
                    }
                    //which tile in the y dimension?
                    backgroundTileRefAddr += y * TILE_MAP_WIDTH;
                    //which tile in x dimension?
                    backgroundTileRefAddr += x;
                    u8 backgroundTile = mmu->lcd.videoRAM[backgroundTileRefAddr];

                    int tileIndex = 0;
                    switch (mmu->lcd.backgroundTileSet) {
                    case 0: 
                        tileIndex = 0x100; 
                        tileIndex += (i8)backgroundTile;
                        break;
                    case 1: 
                        tileIndex = 0; 
                        tileIndex += backgroundTile;
                        break;
                    default: CO_ERR("Wrong tile set");
                    }

                    Tile tile;
                    tile.backgroundTileRef = backgroundTile;
                    tile.x = x;
                    tile.y = y;
                    tile.backgroundTileRefAddr = backgroundTileRefAddr;
                    tile.tileIndex = tileIndex;

                    buf_gen_memory_push(tiles, tile);

                }
            }
            ImGuiListClipper clipper(18*20);
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    Tile *tile = tiles + i; 
                    ImGui::Text("Tile Ref addr: %X Tile: %X, X: %d, Y: %d", 
                                tile->backgroundTileRefAddr + 0x8000, tile->backgroundTileRef, 
                                tile->x, tile->y);
                    ImGui::SameLine();
                    ImGui::Image(gbDebug->tiles[tile->tileIndex].textureID, ImVec2(TILE_WIDTH * DEFAULT_SCREEN_SCALE, TILE_HEIGHT * DEFAULT_SCREEN_SCALE));
                }
            }
            buf_gen_memory_free(tiles);

        }
        ImGui::End();
    }

    if (gbDebug->isOAMViewOpen) {
        if (ImGui::Begin("OAM Debug", &gbDebug->isOAMViewOpen, ImGuiWindowFlags_AlwaysAutoResize )) {
            if (ImGui::GetWindowPos().x == 0 && ImGui::GetWindowPos().y == 0) {
                ImGui::SetWindowPos(ImVec2(debugDataSize.x, 0), ImGuiCond_Once);
            }
            u8 *oam = mmu->lcd.oam;
            u8 *oamRefs = nullptr;
            for (i64 i = 0; i < ARRAY_LEN(mmu->lcd.oam); i += 4) {
                if (oam[i] > 0 && oam[i] < SCREEN_HEIGHT + MAX_SPRITE_HEIGHT && oam[i+1] > 0 && oam[i+1] < SCREEN_WIDTH + MAX_SPRITE_WIDTH) {
                    buf_gen_memory_push(oamRefs, oam[i+2]);
                }
            }
            if (buf_len(oamRefs) > 0) {
                ImGuiListClipper clipper((int)buf_len(oamRefs), ImGui::GetTextLineHeightWithSpacing());
                fori ((i64)buf_len(oamRefs)) {
                    ImGui::Text("OAM Addr: %zX, X: %u, Y: %u, Tile Ref: %d, Flags: %X", (size_t)i + 0xFE00, oam[i+1], oam[i], oam[i+2], oam[i+3]);
                    ImGui::SameLine();
                    ImGui::Image(gbDebug->tiles[oamRefs[i]].textureID, ImVec2(TILE_WIDTH * DEFAULT_SCREEN_SCALE, TILE_HEIGHT * DEFAULT_SCREEN_SCALE));
                }
                clipper.End();
            }
            else {
                ImGui::Text("No sprites to display");
            }
        }
        ImGui::End();
    }
    if (gbDebug->isDisassemblerOpen)  {
        if (gbDebug->shouldRefreshDisassembler) {
            refreshDisassemblerIfOpen(gbDebug, mmu);
            gbDebug->shouldRefreshDisassembler = false;
        }
        if (ImGui::Begin("Disassembler", &gbDebug->isDisassemblerOpen)) {

            if (ImGui::GetWindowPos().x == 0 && ImGui::GetWindowPos().y == 0) {
                ImGui::SetWindowPos(ImVec2(debugDataSize.x, 0), ImGuiCond_Once);
            }
            ImGui::BeginChild("##disassembly_scrolling", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()*3.9f));
            i64 numLines = gbDebug->numDisassembledInstructions;
            ImGuiListClipper clipper((i32)numLines, ImGui::GetTextLineHeight());
            while (clipper.Step()) {
                for (i32 line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
                    u16 currAddr = gbDebug->disassembledInstructionAddresses[line];
                    const char *disassembledInstruction = gbDebug->disassembledInstructions[currAddr];
                    if (cpu->PC == currAddr) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
                        ImGui::Text("%X: %s", currAddr, disassembledInstruction);
                        ImGui::PopStyleColor();
                    }
                    else if (gbDebug->shouldHighlightSearch &&
                             gbDebug->lineNumberOfHighlightedSearch == line ) {
                        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 255, 0, 255));
                        ImGui::Text("%X: %s", currAddr, disassembledInstruction);
                        ImGui::PopStyleColor();

                    }
                    else {
                        ImGui::Text("%X: %s", currAddr, disassembledInstruction);
                    }

                    ImGui::SameLine();
                    Breakpoint *bp = regularBreakpointForAddress(currAddr, gbDebug);
                    if (bp) {
                        char buttonTitleTemplate[] = "Delete Breakpoint##disassembler";
                        i64 buttonTitleLen = ARRAY_LEN(buttonTitleTemplate) + 5;
                        char *buttonTitle = PUSHM(buttonTitleLen, char);
                        AutoMemory _am(buttonTitle);

                        snprintf(buttonTitle, (size_t)buttonTitleLen, "%s%x", buttonTitleTemplate, currAddr);

                        if (ImGui::Button(buttonTitle)) {
                            deleteBreakpoint(bp, gbDebug);
                        }
                    }
                    else {
                        char buttonTitleTemplate[] = "Breakpoint##disassembler";
                        i64 buttonTitleLen = ARRAY_LEN(buttonTitleTemplate) + 5;
                        char *buttonTitle = PUSHM(buttonTitleLen, char);
                        AutoMemory _am(buttonTitle);

                        snprintf(buttonTitle, (size_t)buttonTitleLen, "%s%x", buttonTitleTemplate, currAddr);
                        if (ImGui::Button(buttonTitle)) {
                            addRegularBreakpoint(currAddr, gbDebug, BreakpointOP::Equal);
                        }

                    }
                }
            }

            ImGui::EndChild();

            ImGui::Separator();

            if (ImGui::Button("Go to PC##disassembler")) {
                refreshDisassemblerIfOpen(gbDebug, mmu);    
                ImGui::BeginChild("##disassembly_scrolling");
                i64 index = 0;
                fori (gbDebug->numDisassembledInstructions) {
                    if (gbDebug->disassembledInstructionAddresses[i] >= cpu->PC) {
                        index = i;
                        break;
                    }
                }
                
                ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y +
                                         (index * ImGui::GetTextLineHeight()));
              
                ImGui::EndChild();
            }
            ImGui::SameLine();
            if (ImGui::Button("Refresh##refresh_disassembler")) {
                refreshDisassemblerIfOpen(gbDebug, mmu);
            }
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Go to: ");
            ImGui::SameLine();
            char addrInput[5] = {};
            if (ImGui::InputText("##disassembler_goto", addrInput, ARRAY_LEN(addrInput),
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {

                u16 address;
                if (sscanf(addrInput, "%hX", &address) == 1) {
                    ImGui::BeginChild("##disassembly_scrolling");
                    i64 index = 0;
                    fori (gbDebug->numDisassembledInstructions) {
                        if (gbDebug->disassembledInstructionAddresses[i] >= address) {
                            index = i;
                            break;
                        }
                    }
                    ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y +
                                             index * ImGui::GetTextLineHeight());
                    ImGui::EndChild();
                }
            }
            ImGui::Text("Search: ");
            ImGui::SameLine();
            bool goToNext = 
                    ImGui::InputText("##disassembler_search", gbDebug->disassemblerSearchString, ARRAY_LEN(gbDebug->disassemblerSearchString),
                                     ImGuiInputTextFlags_EnterReturnsTrue);                 
            ImGui::SameLine();
            goToNext |= ImGui::Button("Next##disassembler");
            ImGui::SameLine();   
            if (ImGui::Button("Prev##disassembler")) {
                gbDebug->didSearchFail = true;
                for (i64 i = gbDebug->lineNumberOfHighlightedSearch - 1; 
                     i >= 0; 
                     i--) {
                    if (isSubstringCaseInsensitive(gbDebug->disassemblerSearchString, 
                                    gbDebug->disassembledInstructions[gbDebug->disassembledInstructionAddresses[i]],
                                    MAX_INSTRUCTION_LEN)) {

                        gbDebug->shouldHighlightSearch = true;
                        gbDebug->lineNumberOfHighlightedSearch = i;
                        ImGui::BeginChild("##disassembly_scrolling");
                        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y +
                                                 i * ImGui::GetTextLineHeight());
                        ImGui::EndChild();
                        gbDebug->didSearchFail = false;
                        break;
                    }
                }
            }
            if (goToNext) {
                gbDebug->didSearchFail = true;
                for (i64 i = gbDebug->lineNumberOfHighlightedSearch + 1; 
                     i < gbDebug->numDisassembledInstructions; 
                     i++) {
                    if (isSubstringCaseInsensitive(gbDebug->disassemblerSearchString, 
                                    gbDebug->disassembledInstructions[gbDebug->disassembledInstructionAddresses[i]],
                                    MAX_INSTRUCTION_LEN)) {

                        gbDebug->shouldHighlightSearch = true;
                        gbDebug->lineNumberOfHighlightedSearch = i;
                        ImGui::BeginChild("##disassembly_scrolling");
                        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y +
                                                 i * ImGui::GetTextLineHeight());
                        ImGui::EndChild();
                        gbDebug->didSearchFail = false;
                        break;
                    }
                }

            }

            if (gbDebug->didSearchFail) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
                ImGui::Text("Text not found!");
                ImGui::PopStyleColor();
            }


        }
        ImGui::End();
    }
    ImGui::SetNextWindowSize(ImVec2(300,300), ImGuiCond_Once);
    if (gbDebug->isSoundViewOpen) {
        if (ImGui::Begin("Sound Debug", &gbDebug->isSoundViewOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Is muted: %s", soundState->isMuted ? "true" : "false");

            ImGui::Text("Samples backed up %zd", (isize)mmu->soundFramesBuffer.numItemsQueued);
            ImGui::Text("Cycles since last frame seq tick: %d", mmu->cyclesSinceLastFrameSequencer);
            ImGui::Text("Master Left Volume: %d  Master Right Volume %d", mmu->masterLeftVolume, mmu->masterRightVolume); 

#define MAKE_ENABLE_BUTTON(name) do {\
    ImGui::SameLine();\
    if (gbDebug->shouldDisable##name) {\
    if (ImGui::Button("Enable##"#name)) {\
    gbDebug->shouldDisable##name = false; \
        }\
        }\
    else {\
    if (ImGui::Button("Disable##"#name)) {\
    gbDebug->shouldDisable##name = true;\
        }\
        }}while (0)\

            if (ImGui::CollapsingHeader("Square Wave 1", ImGuiTreeNodeFlags_DefaultOpen)) {
                MMU::SquareWave1 *sq1 = &mmu->squareWave1Channel;
                ImGui::Text("Enabled: %s, DAC Enabled %s", BOOL_TO_STR(sq1->isEnabled), BOOL_TO_STR(sq1->isDACEnabled));
                MAKE_ENABLE_BUTTON(SQ1); 
                ImGui::Text("Channels enabled: Left: %s, Right: %s",
                            BOOL_TO_STR(((int)sq1->channelEnabledState & (int)ChannelEnabledState::Left) != 0),
                            BOOL_TO_STR(((int)sq1->channelEnabledState & (int)ChannelEnabledState::Right) != 0));
                //Registers
                ImGui::Text("FF10 -- Sweep Period: %X, Negate: %s, Shift %X",
                            sq1->sweepPeriod, BOOL_TO_STR(sq1->isSweepNegated), sq1->sweepShift);
                ImGui::Text("FF11 -- Duty: %X, Length Load: %d", sq1->waveForm, sq1->lengthCounter);
                ImGui::Text("FF12 -- Volume: %X, Should Increase Volume: %s, Envelope Period %X",
                            sq1->currentVolume, BOOL_TO_STR(sq1->shouldIncreaseVolume), sq1->volumeEnvelopPeriod);
                ImGui::Text("FF13/FF14 -- Frequency %X, Is Length Enabled %s",
                            sq1->toneFrequency, BOOL_TO_STR(sq1->isLengthCounterEnabled));
            }

            if (ImGui::CollapsingHeader("Square Wave 2", ImGuiTreeNodeFlags_DefaultOpen)) {
                MMU::SquareWave2 *sq2 = &mmu->squareWave2Channel;
                ImGui::Text("Enabled: %s, DAC Enabled %s", BOOL_TO_STR(sq2->isEnabled), BOOL_TO_STR(sq2->isDACEnabled));
                MAKE_ENABLE_BUTTON(SQ2); 
                ImGui::Text("Channels enabled: Left: %s, Right: %s",
                            BOOL_TO_STR(((int)sq2->channelEnabledState & (int)ChannelEnabledState::Left) != 0),
                            BOOL_TO_STR(((int)sq2->channelEnabledState & (int)ChannelEnabledState::Right) != 0));

                ImGui::Text("FF16 -- Duty: %X, Length Load: %X", sq2->waveForm, sq2->lengthCounter);
                ImGui::Text("FF17 -- Volume: %X, Should Increase Volume: %s, Envelope Period %X",
                            sq2->currentVolume, BOOL_TO_STR(sq2->shouldIncreaseVolume), sq2->volumeEnvelopPeriod);
                ImGui::Text("FF18/FF19 -- Frequency %X, Is Length Enabled %s",
                            sq2->toneFrequency, BOOL_TO_STR(sq2->isLengthCounterEnabled));

            }

            if (ImGui::CollapsingHeader("Wave", ImGuiTreeNodeFlags_DefaultOpen)) {
                MMU::Wave *wave = &mmu->waveChannel;
                ImGui::Text("Enabled: %s, DAC Enabled %s", BOOL_TO_STR(wave->isEnabled), BOOL_TO_STR(wave->isDACEnabled));
                MAKE_ENABLE_BUTTON(Wave); 
                ImGui::Text("Sample buffer %X, Position in Sample buffer %d", wave->currentSample, wave->currentSampleIndex);
                ImGui::Text("Channels enabled: Left: %s, Right: %s",
                            BOOL_TO_STR(((int)wave->channelEnabledState & (int)ChannelEnabledState::Left) != 0),
                            BOOL_TO_STR(((int)wave->channelEnabledState & (int)ChannelEnabledState::Right) != 0));
                ImGui::Text("FF1B --  Length Load: %X", wave->lengthCounter);

                {
                    int vol;
                    switch (wave->volumeShift) {
                    case WaveVolumeShift::WV_0: vol = 0; break;
                    case WaveVolumeShift::WV_25: vol = 25; break;
                    case WaveVolumeShift::WV_50: vol = 50; break;
                    case WaveVolumeShift::WV_100: vol = 100; break;
                    }

                    ImGui::Text("FF1C -- Volume: %d", vol) ;
                }
                ImGui::Text("FF1D/FF1E -- Frequency %X, Is Length Enabled %s",
                            wave->toneFrequency, BOOL_TO_STR(wave->isLengthCounterEnabled));
                ImGui::Text("Wave Table (FF30-FF3F): %02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X", 
                            wave->waveTable[0], wave->waveTable[1],
                        wave->waveTable[2], wave->waveTable[3],
                        wave->waveTable[4], wave->waveTable[5],
                        wave->waveTable[6], wave->waveTable[7],
                        wave->waveTable[8], wave->waveTable[9],
                        wave->waveTable[10], wave->waveTable[11],
                        wave->waveTable[12], wave->waveTable[13],
                        wave->waveTable[14], wave->waveTable[15]);

            }

            if (ImGui::CollapsingHeader("Noise", ImGuiTreeNodeFlags_DefaultOpen)) {
                MMU::Noise *noise = &mmu->noiseChannel;
                ImGui::Text("Enabled: %s, DAC Enabled %s", BOOL_TO_STR(noise->isEnabled), BOOL_TO_STR(noise->isDACEnabled));
                MAKE_ENABLE_BUTTON(Noise); 
                ImGui::Text("Channels enabled: Left: %s, Right: %s",
                            BOOL_TO_STR(((int)noise->channelEnabledState & (int)ChannelEnabledState::Left) != 0),
                            BOOL_TO_STR(((int)noise->channelEnabledState & (int)ChannelEnabledState::Right) != 0));

                ImGui::Text("FF20 -- Length Load: %X", noise->lengthCounter);
                ImGui::Text("FF21 -- Volume: %X, Should Increase Volume: %s, Envelope Period %X",
                            noise->currentVolume, BOOL_TO_STR(noise->shouldIncreaseVolume), noise->volumeEnvelopPeriod);
                ImGui::Text("FF22 -- Shift value: %X, 7-Bit mode: %s, Tone Period: %d", 
                            noise->shiftValue, BOOL_TO_STR(noise->is7BitMode), noise->tonePeriod);
                ImGui::Text("FF23 -- Is Length Enabled %s", BOOL_TO_STR(noise->isLengthCounterEnabled));

            }
#undef MAKE_ENABLE_BUTTON

        }
        ImGui::End();
    }

    ImGui::SetNextWindowSize(ImVec2(300,300), ImGuiCond_Once);
    if (gbDebug->isMemoryViewOpen) {
        if (ImGui::Begin("Memory Viewer", &gbDebug->isMemoryViewOpen)) {

            ImGui::SetWindowPos(ImVec2(debugDataSize.x, 0), ImGuiCond_Once);
            ImGui::BeginChild("##mmu_scrolling", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));

            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0,0));
            i32 addrDigitsCount = 0;
            i32 rows = 8;
            i32 memSize = 0x10000;
            i32 baseDisplayAddr = 0;

            for (i32 n = baseDisplayAddr + memSize - 1; n > 0; n >>= 4) {
                addrDigitsCount++;
            }

            float glyphWidth = ImGui::CalcTextSize("F").x;
            float cellWidth = glyphWidth * 3; // "FF " we include trailing space in the width to easily catch clicks everywhere
            int lineTotalCount = (int)((memSize + rows-1) / rows);

            ImGuiListClipper clipper(lineTotalCount);

            bool shouldDrawSeparator = true;
            while (clipper.Step()) {
                for (i32 line = clipper.DisplayStart; line < clipper.DisplayEnd; line++) {
                    u16 addr = (u16)(line * rows);

                    ImGui::Text("%0*X:", addrDigitsCount, baseDisplayAddr + addr);
                    ImGui::SameLine();

                    float lineStartX = ImGui::GetCursorPosX();

                    for (i32 n = 0; n < rows && addr < memSize; n++, addr++) {
                        char label[10];
                        snprintf(label, 9, "%02X##%04X", readByte(addr, mmu), addr);
                        ImGui::SameLine(lineStartX + cellWidth * n);


                        if (regularBreakpointForAddress(addr, gbDebug)) {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(1, 0, 0, 255));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 1, 255));
                        }
                        else {
                            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 1, 255));
                            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 0, 0, 255));

                        }
                        if (addr == cpu->PC) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(255, 0, 0, 255));
                        }


                        if (ImGui::Button(label)) {
                            auto bp = regularBreakpointForAddress(addr, gbDebug);
                            if (bp) {
                                deleteBreakpoint(bp, gbDebug);
                            }
                            else {
                                addRegularBreakpoint(addr, gbDebug, BreakpointOP::Equal);
                            }
                        }

                        ImGui::PopStyleColor((addr == cpu->PC) ? 3 : 2);

                    }

                    ImGui::SameLine(lineStartX + (cellWidth * rows) + (glyphWidth * 2));

                    if (shouldDrawSeparator) {
                        ImVec2 screenPos = ImGui::GetCursorScreenPos();
                        ImGui::GetWindowDrawList()->AddLine(
                                    ImVec2(screenPos.x - glyphWidth, screenPos.y - 9999),
                                    ImVec2(screenPos.x - glyphWidth, screenPos.y + 9999),
                                    ImColor(ImGui::GetStyle().Colors[ImGuiCol_Border]));
                        shouldDrawSeparator = false;
                    }

                    addr = (u16)(line * rows);
                    for (i32 n = 0; n < rows && addr < memSize; n++, addr++) {
                        if (n > 0) ImGui::SameLine();
                        u8 c = readByte(addr, mmu);
                        ImGui::Text("%c", (c >= 32 && c < 128) ? c : '.');
                    }
                }
            }
            ImGui::PopStyleVar(2);

            ImGui::EndChild();

            ImGui::Separator();

            ImGui::AlignTextToFramePadding();
            ImGui::SameLine();
            ImGui::Text("Range %0*X..%0*X", addrDigitsCount, (int)baseDisplayAddr, addrDigitsCount, (int)baseDisplayAddr+memSize-1);
            ImGui::SameLine();
            char addrInput[32] = {};
            if (ImGui::InputText("##addr", addrInput, 32,
                                 ImGuiInputTextFlags_CharsHexadecimal | ImGuiInputTextFlags_EnterReturnsTrue)) {
                int gotoAddr;

                if (sscanf(addrInput, "%X", &gotoAddr) == 1) {
                    gotoAddr -= baseDisplayAddr;
                    if (gotoAddr >= 0 && gotoAddr < memSize) {
                        ImGui::BeginChild("##mmu_scrolling");
                        ImGui::SetScrollFromPosY(ImGui::GetCursorStartPos().y +
                                                 (gotoAddr / rows) * ImGui::GetTextLineHeight());
                        ImGui::EndChild();
                    }
                }
            }

        }
        ImGui::End();
    }

    ImGui::Render();

    //NOTE: used to test the pop that happens when switching between applications
    //        fori (soundState->buffer.len) {
    //            CO_ASSERT(soundState->buffer.data[i].value == 0);
    //        }
}
