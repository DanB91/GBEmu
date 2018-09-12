//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license

//unity
#ifdef CO_DEBUG
#define CO_IMPL
#endif
#include "gbemu.h"
#include "debugger.cpp"
#include "serialize.cpp"

#define MAX_LY 153
#define TOTAL_SCANLINE_DURATION 456
#define HBLANK_DURATION 204
#define VBLANK_DURATION 456
#define SCAN_OAM_DURATION 80
#define SCAN_VRAM_AND_OAM_DURATION 172

#ifdef CO_PROFILE
static ProfileState *profileState;
#endif


static bool
shouldBreakOnPC(u16 PC, GameBoyDebug *gbDebug, Breakpoint **hitBreakpoint) {
    if (!gbDebug->isEnabled || gbDebug->numBreakpoints <= 0)  {
        return false;
    }
    foriarr (gbDebug->breakpoints) {
        Breakpoint *bp = &gbDebug->breakpoints[i];
        if (!bp->isUsed || bp->isDisabled || bp->type != BreakpointType::Regular) {
            continue;
        }
        
        switch (bp->op) {
            case BreakpointOP::Equal: {
                if (bp->address == PC) {
                    *hitBreakpoint = bp;
                    return true;
                }
                
            } break;
            case BreakpointOP::LessThan: {
                if (PC < bp->address) {
                    *hitBreakpoint = bp;
                    return true;
                }
                
            } break;
            case BreakpointOP::GreaterThan: {
                if (PC > bp->address) {
                    *hitBreakpoint = bp;
                    return true;
                }
                
            } break;
        }
    }
    
    return false;
}


static void setToneFrequencyForSquareWave(u16 toneFrequency, u16 *destToneFrequency, u16 *destTonePeriod) {
    *destToneFrequency = toneFrequency;
    *destTonePeriod = (2048 - toneFrequency) * 4;
}

static void setToneFrequencyForWave(u16 toneFrequency, MMU::Wave *waveChannel) {
    waveChannel->toneFrequency = toneFrequency;
    waveChannel->tonePeriod = (2048 - toneFrequency) * 2;
}

static inline
u8 byteForColorPallette(PaletteColor *pallette) {
    u8 ret = 0;
    fori (PALETTE_LEN) {
        ret |= (int)pallette[i] << (i * 2);
    }
    
    return ret;
}


inline void 
updateColorPaletteFromU8(PaletteColor *palette, u8 val) {
    fori (PALETTE_LEN) {
        palette[i] = (PaletteColor)((val >> (2 * i)) & 3);
    }
}


u8 readByte(u16 address, MMU *mmu) {
    LCD *lcd = &mmu->lcd;
    switch (address) {
        case 0 ... 0xFF: {
            //        if (mmu->inBios) {
            //            return BIOS[address];
            //        }
            //        else {
            //            return mmu->romData[address];
            //        }
            return mmu->romData[address];
        }break;
        
        case 0x100 ... 0x3FFF: return mmu->romData[address];
        case 0x4000 ... 0x7FFF: {
            switch (mmu->mbcType) {
                case MBCType::MBC0: 
                return mmu->romData[address];
                case MBCType::MBC1: 
                case MBCType::MBC3: 
                case MBCType::MBC5:
                return mmu->romData[address + (0x4000 * (mmu->currentROMBank - 1))];
            }
        } break;
        case 0x8000 ... 0x9FFF: {
            //vram can only be properly accessed when not being drawn from
            //         return (lcd->mode != LCDMode::ScanVRAMAndOAM) ? lcd->videoRAM[address - 0x8000] : 0xFF;
            return lcd->videoRAM[address - 0x8000];
        } break;
        case 0xA000 ... 0xBFFF: {
            if (mmu->hasRAM && mmu->isCartRAMEnabled) {
                switch (mmu->mbcType) {
                    case MBCType::MBC0: 
                    return mmu->cartRAM[address - 0xA000];
                    case MBCType::MBC1: 
                    case MBCType::MBC5:
                    return mmu->cartRAM[(address - 0xA000) +  (0x2000 * mmu->currentRAMBank)];
                    case MBCType::MBC3: {
                        if (mmu->currentRAMBank <= 3) {
                            return mmu->cartRAM[(address - 0xA000) +  (0x2000 * mmu->currentRAMBank)];
                        }
                        else if (mmu->currentRAMBank >= 0x8 &&
                                 mmu->currentRAMBank <= 0xC) {
                            switch (mmu->currentRAMBank) {
                                case 0x8: return mmu->rtc.latchedSeconds;
                                case 0x9: return mmu->rtc.latchedMinutes;
                                case 0xA: return mmu->rtc.latchedHours;
                                case 0xB: return mmu->rtc.latchedDays;
                                case 0xC: return mmu->rtc.latchedMisc;
                                default: return 0xFF;
                            }
                        }
                        else {
                            return 0xFF;
                        }
                    } break;
                }
            }
            else {
                return 0xFF;
            }
        } break;
        case 0xC000 ... 0xDFFF: return mmu->workingRAM[address - 0xC000];
        case 0xE000 ... 0xFDFF: return mmu->workingRAM[address - 0xE000];
        case 0xFE00 ... 0xFE9F: {
            //            switch (lcd->mode) {
            //            case LCDMode::ScanVRAMAndOAM:
            //            case LCDMode::ScanOAM:
            //                return 0xFF;
            
            //            default: return lcd->oam[address - 0xFE00];
            //            }
            return lcd->oam[address - 0xFE00];
        } break;
        case 0xFF00: {
            u8 joyPadReg = 0xCF;
            
            switch (mmu->joyPad.selectedButtonGroup) {
                case JPButtonGroup::DPad: {
                    joyPadReg = 0xE0;
                    
                    joyPadReg |= (int)mmu->joyPad.down << 3;
                    joyPadReg |= (int)mmu->joyPad.up << 2;
                    joyPadReg |= (int)mmu->joyPad.left << 1;
                    joyPadReg |= (int)mmu->joyPad.right << 0;
                    
                } break;
                
                case JPButtonGroup::FaceButtons: {
                    joyPadReg = 0xD0;
                    
                    joyPadReg |= (int)mmu->joyPad.start << 3;
                    joyPadReg |= (int)mmu->joyPad.select << 2;
                    joyPadReg |= (int)mmu->joyPad.b << 1;
                    joyPadReg |= (int)mmu->joyPad.a << 0;
                    
                } break;
                
                case JPButtonGroup::Nothing: break;
                
            }
            
            return joyPadReg;
        } break;
        
        case 0xFF04: return mmu->divider;
        case 0xFF05: return mmu->timer;
        case 0xFF06: return mmu->timerModulo;
        case 0xFF07: {
            u8 ret = 0;
            if (mmu->isTimerEnabled) {
                setBit(2, &ret);
            }
            
            switch (mmu->timerIncrementRate) {
                case TimerIncrementRate::TIR_0: break;
                case TimerIncrementRate::TIR_1: ret |= 1; break;
                case TimerIncrementRate::TIR_2: ret |= 2; break;
                case TimerIncrementRate::TIR_3: ret |= 3; break;
            }
            
            return ret;
        } break;
        
        
        //TODO
        case 0xFF0F: return mmu->requestedInterrupts;
        
        case 0xFF10: return mmu->NR10 | 0x80;
        case 0xFF11: return mmu->NR11 | 0x3F;
        case 0xFF12: return mmu->NR12;
        case 0xFF13: return 0xFF;
        case 0xFF14: return mmu->NR14 | 0xBF;
        case 0xFF15: return 0xFF;
        case 0xFF16: return mmu->NR21 | 0x3F;
        case 0xFF17: return mmu->NR22;
        case 0xFF18: return 0xFF;
        case 0xFF19: return mmu->NR24 | 0xBF;
        case 0xFF1A: return mmu->NR30 | 0x7F;
        case 0xFF1B: return 0xFF;
        case 0xFF1C: return mmu->NR32 | 0x9F;
        case 0xFF1D: return 0xFF;
        case 0xFF1E: return mmu->NR34 | 0xBF;
        case 0xFF1F: return 0xFF;
        case 0xFF20: return 0xFF;
        case 0xFF21: return mmu->NR42;
        case 0xFF22: return mmu->NR43;
        case 0xFF23: return mmu->NR44 | 0xBF;
        case 0xFF24: return mmu->NR50;
        
        case 0xFF25: return mmu->NR51;
        case 0xFF26: {
            u8 lengthEnableStatus = 0;
            lengthEnableStatus |= (mmu->squareWave1Channel.isEnabled) ? 1 : 0;
            lengthEnableStatus |= (mmu->squareWave2Channel.isEnabled) ? 2 : 0;
            lengthEnableStatus |= (mmu->waveChannel.isEnabled) ? 4 : 0;
            lengthEnableStatus |= (mmu->noiseChannel.isEnabled) ? 8 : 0;
            return mmu->NR52 | lengthEnableStatus | 0x70;
        } break; 
        case 0xFF27 ... 0xFF2F: return 0xFF;
        case 0xFF30 ... 0xFF3F:  {
            return mmu->waveChannel.waveTable[address & 0xF];
        } break;
        case 0xFF40: { //LCD Control
            profileStart("Read LCD Status", profileState);
            //Bit 7 - LCD Enabled
            u8 control = (lcd->isEnabled) ? (1 << 7) : 0;
            //Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
            control |= lcd->windowTileMap << 6;
            //Bit 5 - Window Display Enable
            control |= lcd->isWindowEnabled << 5;
            //Bit 4 - Background Tile Set Select
            control |= lcd->backgroundTileSet << 4;
            //Bit 3 - Background Tile Data Select
            control |= lcd->backgroundTileMap << 3;
            //Bit 2 - Sprite size
            switch (lcd->spriteHeight) {
                case SpriteHeight::Short: break; //bit unset in 8x8 mode
                case SpriteHeight::Tall: control |= (1 << 2); break; //bit set in 8x16 mode
            };
            //Bit 1 - OAM Enable
            if (lcd->isOAMEnabled) control |= (1 << 1);
            
            //Bit 0 - Background enabled
            if (lcd->isBackgroundEnabled) control |= 1;
            profileEnd(profileState);
            
            return control;
        } break;
        
        case 0xFF41: return lcd->stat; //LCD Status
        
        case 0xFF42: return lcd->scy;
        case 0xFF43: return lcd->scx;
        case 0xFF44: return lcd->ly;
        case 0xFF45: return lcd->lyc;
        case 0xFF46: return (mmu->currentDMAAddress >> 8) & 0xFF;
        case 0xFF47: return byteForColorPallette(lcd->backgroundPalette);
        case 0xFF48: return byteForColorPallette(lcd->spritePalette0);
        case 0xFF49: return byteForColorPallette(lcd->spritePalette1);
        case 0xFF4A: return lcd->wy;
        case 0xFF4B: return lcd->wx;
        case 0xFF50: return 1;//(mmu->inBios) ? 1 : 0;
        case 0xFF80 ... 0xFFFE: return mmu->zeroPageRAM[address - 0xFF80];
        case 0xFFFF: return mmu->enabledInterrupts;
        
        default: return 0;
    }
}

static inline void changeRAMBank(MMU *mmu, u8 newBank) {
    mmu->currentRAMBank = newBank & mmu->maxCartRAMBank; 
}
static inline void changeROMBank(MMU *mmu, u16 newBank) {
    mmu->currentROMBank = newBank & mmu->maxROMBank; 
}

void writeByte(u8 byte, u16 address, MMU *mmu, GameBoyDebug *gbDebug) {
    LCD *lcd = &mmu->lcd;
    //        if (address >= 0xFF10 && address <= 0xFF26) {
    //            CO_LOG("Addr: %X, Old Val %X, New Val %X", address, readByte(address, mmu), byte);
    //        }
    //        if (address >= 0xFF10 && address <= 0xFF14) {
    //            CO_LOG("Addr: %X, New Val %X", address, byte);
    //        }
    
    if (gbDebug->numBreakpoints > 0 && gbDebug->isEnabled) {
        fori ((i64)BreakpointExpectedValueType::OnePastLast) {
            Breakpoint *bp = hardwareBreakpointForAddress(address, (BreakpointExpectedValueType)i, gbDebug);
            if (bp && !bp->isDisabled) {
                auto breakpointHit = [&byte, &address, &mmu, &bp, &gbDebug]() {
                    gbDebug->hitBreakpoint = bp;
                    bp->valueBefore = readByte(address, mmu);
                    bp->valueAfter = byte;
                };
                switch (bp->expectedValueType) {
                    case BreakpointExpectedValueType::Custom: {
                        if (bp->expectedValue == byte) {
                            breakpointHit();
                        }
                    } break;
                    case BreakpointExpectedValueType::Any: {
                        breakpointHit();
                    } break;    
                    case BreakpointExpectedValueType::BitClear: {
                        if ((bp->expectedValue & byte) == 0) {
                            breakpointHit();
                        }
                    } break;
                    case BreakpointExpectedValueType::BitSet: {
                        if ((bp->expectedValue & byte) == bp->expectedValue) {
                            breakpointHit();
                        }
                    } break;
                    case BreakpointExpectedValueType::OnePastLast:
                    case BreakpointExpectedValueType::None:
                    //do nothing
                    break;
                }
            }
            
        }
    }
    
    //        if ((address == 0xFF13 || address == 0xFF14) && mmu->squareWave1.toneFrequency == 0x6EB){
    //            Breakpoint *bp = &gbDebug->breakpoints[0];
    //            gbDebug->hitBreakpoint =bp; 
    //            bp->valueBefore = 0xEB;
    //            bp->valueAfter = byte;
    
    //        }
    
    
    switch (address) {
        case 0 ... 0x1FFF: {
            if (mmu->hasRAM) {
                if (byte == 0xA) {
                    mmu->isCartRAMEnabled = true;
                }
                else {
                    mmu->isCartRAMEnabled = false;
                }
            }
        } break;
        
        case 0x2000 ... 0x3FFF: {
            switch (mmu->mbcType){
                case MBCType::MBC0:
                break;
                case MBCType::MBC1: {
                    u16 bank = (mmu->currentROMBank & 0x60) | (byte & 0x1F);
                    switch (bank) {
                        case 0x20:
                        case 0x40:
                        case 0x60:
                        bank += 1;
                    }
                    changeROMBank(mmu, bank);
                } break;
                case MBCType::MBC3:  {
                    u16 bank = byte & 0x7F; 
                    if (bank == 0) bank = 1;
                    changeROMBank(mmu, bank);
                } break; 
                case MBCType::MBC5: {
                    if (address < 0x3000) {
                        changeROMBank(mmu, (mmu->currentROMBank & 0x100) | byte);
                    }
                    else {
                        changeROMBank(mmu, (mmu->currentROMBank & 0xFF) | (u16)((byte & 1) << 9));
                    }
                } break;
            }
        } break;
        
        case 0x4000 ... 0x5FFF: {
            switch (mmu->mbcType){
                case MBCType::MBC0:
                break;
                case MBCType::MBC1: {
                    if (!mmu->hasRAM || mmu->bankingMode == BankingMode::Mode0) {
                        changeROMBank(mmu, (mmu->currentROMBank & 0x1F) | (byte & 0x60));
                    }
                    else if (mmu->bankingMode == BankingMode::Mode1) {
                        changeRAMBank(mmu, byte & 0x3);
                    }
                } break;
                case MBCType::MBC3:  
                case MBCType::MBC5: {
                    //TODO: support rumble.  Rumble is bit 3
                    if (mmu->hasRTC && byte >= 0x8) {
                        mmu->currentRAMBank = byte & 0xF;
                    }
                    else {
                        changeRAMBank(mmu, byte & 0x3);
                    }
                } break;
            }
        } break;
        case 0x6000 ... 0x7FFF: {
            if (mmu->hasRAM && mmu->mbcType == MBCType::MBC1) {
                mmu->bankingMode = (BankingMode)(byte & 1);
            }
            else if (mmu->hasRTC && mmu->mbcType == MBCType::MBC3){
                if (byte == 1 && mmu->rtc.latchState == 0) {
                    mmu->rtc.latchState = 1;
                    mmu->rtc.latchedSeconds = mmu->rtc.seconds;
                    mmu->rtc.latchedMinutes = mmu->rtc.minutes;
                    mmu->rtc.latchedHours = mmu->rtc.hours;
                    mmu->rtc.latchedDays = mmu->rtc.days;
                    
                    //Upper 1 bit of Day Counter, Carry Bit, Halt Flag
                    mmu->rtc.latchedMisc = 0;
                    mmu->rtc.latchedMisc |= mmu->rtc.daysHigh;
                    mmu->rtc.latchedMisc |= mmu->rtc.isStopped ? 0x40 : 0;
                    mmu->rtc.latchedMisc |= (mmu->rtc.didOverflow) ? 0x80 : 0;
                    
                    //persist to file
                    {
                        RTCFileState *rtcFS = mmu->cartRAMPlatformState.rtcFileMap;  
                        rtcFS->latchedSeconds = mmu->rtc.seconds;
                        rtcFS->latchedMinutes = mmu->rtc.minutes;
                        rtcFS->latchedHours = mmu->rtc.hours;
                        rtcFS->latchedDays = mmu->rtc.days;
                        rtcFS->latchedDaysHigh = mmu->rtc.daysHigh;
                    }
                }
                else {
                    mmu->rtc.latchState = byte;
                }
            }
        } break;
        case 0x8000 ... 0x9FFF:
        //vram can only be properly accessed when not being drawn from
        //TODO: Proper emulation
        /*if (lcd->mode != LCDMode::ScanVRAMAndOAM)*/ {
            lcd->videoRAM[address - 0x8000] = byte;
            if (gbDebug->isEnabled) {
                gbDebug->tiles[(address-0x8000)/16].needsUpdate = true;
            }
            
        } break;
        case 0xA000 ... 0xBFFF: {
            //TODO move code around for rtc
            if (mmu->hasRAM && mmu->isCartRAMEnabled) {
                switch (mmu->mbcType) {
                    case MBCType::MBC0: 
                    mmu->cartRAM[address - 0xA000] = byte; 
                    break;
                    case MBCType::MBC1: 
                    case MBCType::MBC5: 
                    mmu->cartRAM[(address - 0xA000) + (0x2000 * mmu->currentRAMBank)] = byte; 
                    break;
                    case MBCType::MBC3: {
                        if (mmu->currentRAMBank <= 3) {
                            mmu->cartRAM[(address - 0xA000) + (0x2000 * mmu->currentRAMBank)] = byte; 
                            if (mmu->hasBattery) {
                                mmu->cartRAMPlatformState.cartRAMFileMap[(address - 0xA000) + (0x2000 * mmu->currentRAMBank)] = byte; 
                            }
                        }
                        else if (mmu->hasRTC) {
                            switch (mmu->currentRAMBank) {
                                case 0x8: {
                                    //seconds
                                    mmu->rtc.seconds = byte;
                                    mmu->cartRAMPlatformState.rtcFileMap->seconds = byte;
                                } break;
                                case 0x9: {
                                    //minutes
                                    mmu->rtc.minutes = byte;
                                    mmu->cartRAMPlatformState.rtcFileMap->minutes = byte;
                                } break;
                                case 0xA: {
                                    //hours
                                    mmu->rtc.hours = byte;
                                    mmu->cartRAMPlatformState.rtcFileMap->hours = byte;
                                } break;
                                case 0xB: {
                                    //days
                                    mmu->rtc.days = byte;
                                    mmu->cartRAMPlatformState.rtcFileMap->days = byte;
                                } break;
                                case 0xC: {
                                    //misc
                                    mmu->rtc.daysHigh = byte & 0x1;
                                    mmu->cartRAMPlatformState.rtcFileMap->daysHigh = byte & 0x1;
                                    mmu->rtc.isStopped = isBitSet(6, byte);
                                    mmu->rtc.didOverflow = isBitSet(7, byte);
                                } break;
                            }
                        }
                        
                    } break; 
                }
                
                if (mmu->hasBattery) {
                    switch (mmu->mbcType) {
                        case MBCType::MBC0: 
                        mmu->cartRAMPlatformState.cartRAMFileMap[address - 0xA000] = byte; 
                        break;
                        case MBCType::MBC1: 
                        case MBCType::MBC5:
                        mmu->cartRAMPlatformState.cartRAMFileMap[(address - 0xA000) + (0x2000 * mmu->currentRAMBank)] = byte; 
                        break;
                        default: break;
                    }
                }
            }
        } break;
        case 0xC000 ... 0xDFFF: mmu->workingRAM[address - 0xC000] = byte; break;
        case 0xE000 ... 0xFDFF: mmu->workingRAM[address - 0xE000] = byte; break;
        
        case 0xFE00 ... 0xFE9F: {
            lcd->oam[address - 0xFE00] = byte;
            //TODO: Shouldn't be able to write to OAM memory during these modes.
            //      Enabling commented out code breaks sprites.  Figure out why...
            //            if (lcd->mode != ScanVRAMAndOAM && lcd->mode != ScanOAM) {
            //                lcd->oam[address - 0xFE00] = byte;
            //            }
        } break;
        
        case 0xFF00: {
            switch (byte & 0x30) {
                case 0x10: {
                    mmu->joyPad.selectedButtonGroup = JPButtonGroup::FaceButtons;
                } break;
                
                case 0x20: {
                    mmu->joyPad.selectedButtonGroup = JPButtonGroup::DPad;
                } break;
                
                case 0x00:
                case 0x30: {
                    mmu->joyPad.selectedButtonGroup = JPButtonGroup::Nothing;
                } break;
            }
        } break;
        case 0xFF04: {
            mmu->divider = 0;
            //TODO: not too sure about this...
            mmu->cyclesSinceDividerIncrement = 0;
        } break;
        case 0xFF05: {
            mmu->timer = byte;
            //TODO: not too sure about this...
            //mmu->cyclesSinceTimerIncrement = 0;
        } break;
        case 0xFF06: mmu->timerModulo = byte; break;
        case 0xFF07:  {
            if (!mmu->isTimerEnabled && isBitSet(2, byte)) {
                mmu->timer = mmu->timerModulo;
                mmu->cyclesSinceTimerIncrement = 0;
            }
            
            mmu->isTimerEnabled = isBitSet(2, byte);
            
            switch (byte & 3) {
                case 0: mmu->timerIncrementRate = TimerIncrementRate::TIR_0; break;
                case 1: mmu->timerIncrementRate = TimerIncrementRate::TIR_1; break;
                case 2: mmu->timerIncrementRate = TimerIncrementRate::TIR_2; break;
                case 3: mmu->timerIncrementRate = TimerIncrementRate::TIR_3; break;
            }
            
            
        } break;
        //TODO
        case 0xFF0F: mmu->requestedInterrupts = byte; break;
        case 0xFF10:  { //NR10 FF10 -PPP NSSS Sweep period, negate, shift
            if (mmu->isSoundEnabled) {
                mmu->NR10 = byte;
                mmu->squareWave1Channel.sweepShift = byte & 0x7;
                mmu->squareWave1Channel.isSweepNegated = isBitSet(3, byte);
                mmu->squareWave1Channel.sweepPeriod = ((byte >> 4) & 0x7);
            }
            
        } break;
        case 0xFF11: {//NR11 FF11 DDLL LLLL Duty, Length load (64-L)
            if (mmu->isSoundEnabled) {
                mmu->NR11 = byte;
                switch ((byte >> 6) & 3) {
                    case 0: mmu->squareWave1Channel.waveForm = WaveForm::WF_12Pct; break;
                    case 1: mmu->squareWave1Channel.waveForm = WaveForm::WF_25Pct; break;
                    case 2: mmu->squareWave1Channel.waveForm = WaveForm::WF_50Pct; break;
                    case 3: mmu->squareWave1Channel.waveForm = WaveForm::WF_75Pct; break;
                }
            }
            //sound enabled does not affect length counter
            mmu->squareWave1Channel.lengthCounter = 64 - (byte & 0x3F);
        } break;
        case 0xFF12: { //NR12 FF12 VVVV APPP Starting volume, Envelope add mode, envelope period
            if (mmu->isSoundEnabled) {
                mmu->NR12 = byte;
                MMU::SquareWave1 *sq1 = &mmu->squareWave1Channel;
                sq1->volumeEnvelopPeriod = byte & 0x7;
                sq1->shouldIncreaseVolume = isBitSet(3, byte);
                sq1->startingVolume = (byte >> 4) & 0xF;
                if (sq1->startingVolume == 0 && !sq1->shouldIncreaseVolume) {
                    sq1->isEnabled = false;
                    sq1->isDACEnabled = false;
                }
                else {
                    sq1->isDACEnabled = true;
                }
            }
            
        } break;
        case 0xFF13: { //NR13 FF13 FFFF FFFF Frequency LSB
            if (mmu->isSoundEnabled) {
                mmu->squareWave1Channel.toneFrequency &= 0x700;
                setToneFrequencyForSquareWave(mmu->squareWave1Channel.toneFrequency | byte, &mmu->squareWave1Channel.toneFrequency, &mmu->squareWave1Channel.tonePeriod);
            }
        } break;  
        case 0xFF14: {//NR14 FF14 TL-- -FFF Trigger, Length enable, Frequency MSB
            MMU::SquareWave1 *sq1 = &mmu->squareWave1Channel;
            
            //sound enabled does not affect length counter
            sq1->isLengthCounterEnabled = isBitSet(6, byte);
            if (mmu->isSoundEnabled) {
                mmu->NR14 = byte;
                sq1->toneFrequency &= 0xFF;
                setToneFrequencyForSquareWave(sq1->toneFrequency | (u16)((byte & 0x7) << 8), &mmu->squareWave1Channel.toneFrequency, &mmu->squareWave1Channel.tonePeriod);
                
                if (isBitSet(7, byte)) {
                    //1) Channel is enabled (see length counter).
                    //2) If length counter is zero, it is set to 64 (256 for wave channel).
                    //3) Frequency timer is reloaded with period.
                    //4) Volume envelope timer is reloaded with period.
                    //5) Channel volume is reloaded from NRx2.
                    //6) Noise channel's LFSR bits are all set to 1.
                    //7) Wave channel's position is set to 0 but sample buffer is NOT refilled.
                    //8) Square 1's sweep does several things (see frequency sweep).
                    sq1->isEnabled = sq1->isDACEnabled;
                    
                    if (sq1->lengthCounter == 0) {
                        sq1->lengthCounter = 64;
                        mmu->ticksSinceLastLengthCounter = 0;
                    }
                    
                    sq1->frequencyClock = 0;
                    sq1->volumeEnvelopeClock = 0;
                    sq1->currentVolume = sq1->startingVolume;
                    
                    //During a trigger event, several things occur:
                    //1) Square 1's frequency is copied to the shadow register.
                    //2) The sweep timer is reloaded.
                    //3) The internal enabled flag is set if either the sweep period or shift are non-zero, cleared otherwise.
                    //4) If the sweep shift is non-zero, frequency calculation and the overflow check are performed immediately.
                    sq1->sweepShadowReg = sq1->toneFrequency;
                    sq1->sweepClock = 0;
                    sq1->isSweepEnabled = (sq1->sweepPeriod != 0 || sq1->sweepShift != 0) ? true : false;
                    
                    if (sq1->isSweepEnabled && sq1->sweepShift != 0) {
                        sq1->sweepShadowReg >>= sq1->sweepShift;
                        
                        sq1->sweepShadowReg = (sq1->isSweepNegated) ?
                            sq1->toneFrequency - sq1->sweepShadowReg :
                        sq1->toneFrequency + sq1->sweepShadowReg;
                        
                        if (sq1->toneFrequency <= 2047) {
                            setToneFrequencyForSquareWave(sq1->sweepShadowReg, &sq1->toneFrequency, &sq1->tonePeriod);
                        }
                        else {
                            sq1->isEnabled = false;
                        }
                    }
                }
            }
        } break;
        
        //FF15 is unused
        case 0xFF16: {//NR21 FF16 DDLL LLLL Duty, Length load (64-L)
            if (mmu->isSoundEnabled) {
                mmu->NR21 = byte;
                switch ((byte >> 6) & 3) {
                    case 0: mmu->squareWave2Channel.waveForm = WaveForm::WF_12Pct; break;
                    case 1: mmu->squareWave2Channel.waveForm = WaveForm::WF_25Pct; break;
                    case 2: mmu->squareWave2Channel.waveForm = WaveForm::WF_50Pct; break;
                    case 3: mmu->squareWave2Channel.waveForm = WaveForm::WF_75Pct; break;
                }
            }
            
            //sound enabled does not affect length counter
            mmu->squareWave2Channel.lengthCounter = 64 - (byte & 0x3F);
        } break;
        
        case 0xFF17: { //NR22 FF17 VVVV APPP Starting volume, Envelope add mode, envelope period
            if (mmu->isSoundEnabled) {
                mmu->NR22 = byte;
                MMU::SquareWave2 *sq2 = &mmu->squareWave2Channel;
                sq2->volumeEnvelopPeriod = byte & 0x7;
                sq2->shouldIncreaseVolume = isBitSet(3, byte);
                sq2->startingVolume = (byte >> 4) & 0xF;
                if (sq2->startingVolume == 0 && !sq2->shouldIncreaseVolume) {
                    sq2->isEnabled = false;
                    sq2->isDACEnabled = false;
                }
                else {
                    sq2->isDACEnabled = true;
                }
            }
            
        } break;
        
        case 0xFF18: { //NR23 FF18 FFFF FFFF Frequency LSB
            if (mmu->isSoundEnabled) {
                mmu->squareWave2Channel.toneFrequency &= 0x700;
                setToneFrequencyForSquareWave(mmu->squareWave2Channel.toneFrequency | byte, &mmu->squareWave2Channel.toneFrequency, &mmu->squareWave2Channel.tonePeriod);
            }
        } break;  
        
        case 0xFF19: {//NR24 FF19 TL-- -FFF Trigger, Length enable, Frequency MSB
            MMU::SquareWave2 *sq2 = &mmu->squareWave2Channel;
            
            //sound enabled does not affect length counter
            sq2->isLengthCounterEnabled = isBitSet(6, byte);
            if (mmu->isSoundEnabled) {
                mmu->NR24 = byte;
                mmu->squareWave2Channel.toneFrequency &= 0xFF;
                setToneFrequencyForSquareWave(sq2->toneFrequency | (u16)((byte & 0x7) << 8), &sq2->toneFrequency, &sq2->tonePeriod);
                
                if (isBitSet(7, byte)) {
                    // trigger event
                    //1) Channel is enabled (see length counter).
                    //2) If length counter is zero, it is set to 64 (256 for wave channel).
                    //3) Frequency timer is reloaded with period.
                    //4) Volume envelope timer is reloaded with period.
                    //5) Channel volume is reloaded from NRx2.
                    //6) Noise channel's LFSR bits are all set to 1.
                    //7) Wave channel's position is set to 0 but sample buffer is NOT refilled.
                    sq2->isEnabled = sq2->isDACEnabled;
                    
                    if (sq2->lengthCounter == 0) {
                        sq2->lengthCounter = 64;
                        mmu->ticksSinceLastLengthCounter = 0;
                    }
                    
                    sq2->frequencyClock = 0;
                    sq2->volumeEnvelopeClock = 0;
                    sq2->currentVolume = sq2->startingVolume;
                }
            }
        } break;
        
        case 0xFF1A: { //NR30 FF1A E--- ---- DAC power
            if (mmu->isSoundEnabled) {
                mmu->NR30 = byte;
                
                if (isBitSet(7, byte)) {
                    mmu->waveChannel.isDACEnabled = true; 
                    //TODO: do we enable the channel?
                }
                else {
                    mmu->waveChannel.isDACEnabled = false; 
                    mmu->waveChannel.isEnabled = false; 
                }
            }
            
        } break;
        
        case 0xFF1B: { //NR31 FF1B LLLL LLLL Length load (256-L)
            //sound enabled does not affect length counter
            mmu->waveChannel.lengthCounter = 256 - byte;
            
        } break;
        case 0xFF1C: { //NR32 FF1C -VV- ---- Volume code (00=0%, 01=100%, 10=50%, 11=25%)
            if (mmu->isSoundEnabled) {
                mmu->NR32 = byte;
                auto wave = &mmu->waveChannel;
                switch ((byte >> 5) & 3) {
                    case 0: wave->volumeShift = WaveVolumeShift::WV_0; break;
                    case 1: wave->volumeShift = WaveVolumeShift::WV_100; break;
                    case 2: wave->volumeShift = WaveVolumeShift::WV_50; break;
                    case 3: wave->volumeShift = WaveVolumeShift::WV_25; break;
                }
            }
        } break;
        case 0xFF1D: { //NR33 FF1D FFFF FFFF Frequency LSB
            if (mmu->isSoundEnabled) {
                mmu->waveChannel.toneFrequency &= 0x700;
                setToneFrequencyForWave(mmu->waveChannel.toneFrequency | byte, &mmu->waveChannel);
            }
        } break;        
        case 0xFF1E: {//NR34 FF1E TL-- -FFF Trigger, Length enable, Frequency MSB
            auto wave = &mmu->waveChannel;
            wave->isLengthCounterEnabled = isBitSet(6, byte);
            if (mmu->isSoundEnabled) {
                mmu->NR34 = byte;
                mmu->waveChannel.toneFrequency &= 0xFF;
                setToneFrequencyForWave(wave->toneFrequency | (u16)((byte & 0x7) << 8), wave);
                
                if (isBitSet(7, byte)) {
                    // trigger event
                    //1) Channel is enabled (see length counter).
                    //2) If length counter is zero, it is set to 64 (256 for wave channel).
                    //3) Frequency timer is reloaded with period.
                    //4) Volume envelope timer is reloaded with period.
                    //5) Channel volume is reloaded from NRx2.
                    //6) Noise channel's LFSR bits are all set to 1.
                    //7) Wave channel's position is set to 0 but sample buffer is NOT refilled.
                    wave->isEnabled = wave->isDACEnabled;
                    
                    if (wave->lengthCounter == 0) {
                        wave->lengthCounter = 256;
                        mmu->ticksSinceLastLengthCounter = 0;
                    }
                    
                    wave->frequencyClock = 0;
                    wave->currentSampleIndex = 0;
                }
            }
            
            
        } break;
        
        
        
        //FF1F is unused
        case 0xFF20: {//NR41 FF20 --LL LLLL Length load (64-L)
            //sound enabled does not affect length counter
            mmu->noiseChannel.lengthCounter = 64 - (byte & 0x3F);
        } break;
        
        case 0xFF21: { //NR42 FF21 VVVV APPP Starting volume, Envelope add mode, envelope period
            if (mmu->isSoundEnabled) {
                mmu->NR42 = byte;
                MMU::Noise *noise = &mmu->noiseChannel;
                noise->volumeEnvelopPeriod = byte & 0x7;
                noise->shouldIncreaseVolume = isBitSet(3, byte);
                noise->startingVolume = (byte >> 4) & 0xF;
                if (noise->startingVolume == 0 && !noise->shouldIncreaseVolume) {
                    noise->isEnabled = false;
                    noise->isDACEnabled = false;
                }
                else {
                    noise->isDACEnabled = true;
                }
            }
            
        } break;
        
        case 0xFF22: { //NR43 FF22 SSSS WDDD Clock shift, Width mode of LFSR, Divisor code
            if (mmu->isSoundEnabled) {
                mmu->NR43 = byte;
                MMU::Noise *noise = &mmu->noiseChannel;
                noise->divisorCode = byte & 0x7;
                noise->tonePeriod = (((noise->divisorCode == 0) ? 8 : noise->divisorCode * 16) << (byte >> 4));
                
                noise->is7BitMode = isBitSet(3, byte);
                
            }
        } break;  
        
        case 0xFF23: {//NR44 FF23 TL-- ---- Trigger, Length enable
            MMU::Noise *noise = &mmu->noiseChannel;
            
            //sound enabled does not affect length counter
            noise->isLengthCounterEnabled = isBitSet(6, byte);
            if (mmu->isSoundEnabled) {
                mmu->NR44 = byte;
                
                if (isBitSet(7, byte)) {
                    // trigger event
                    //1) Channel is enabled (see length counter).
                    //2) If length counter is zero, it is set to 64 (256 for wave channel).
                    //3) Frequency timer is reloaded with period.
                    //4) Volume envelope timer is reloaded with period.
                    //5) Channel volume is reloaded from NRx2.
                    //6) Noise channel's LFSR bits are all set to 1.
                    //7) Wave channel's position is set to 0 but sample buffer is NOT refilled.
                    noise->isEnabled = noise->isDACEnabled;
                    
                    if (noise->lengthCounter == 0) {
                        noise->lengthCounter = 64;
                        mmu->ticksSinceLastLengthCounter = 0;
                    }
                    
                    noise->shiftValue = 0xFFFF;
                    
                    noise->frequencyClock = 0;
                    noise->volumeEnvelopeClock = 0;
                    noise->currentVolume = noise->startingVolume;
                }
            }
        } break;
        
        case 0xFF24:  {
            //TODO: VIN and Volume control
            if (mmu->isSoundEnabled) {
                mmu->NR50 = byte;
                mmu->masterLeftVolume = ((byte >> 4) & 0x7) + 1;
                mmu->masterRightVolume = (byte & 0x7) + 1;
            }
        } break;
        
        
        case 0xFF25: {//NR51
            if (mmu->isSoundEnabled) {
                mmu->NR51 = byte;
                u8 channelState = 0;
                channelState |= (u8)(isBitSet(0, byte) ? ChannelEnabledState::Right : ChannelEnabledState::None);
                channelState |= (u8)(isBitSet(4, byte) ? ChannelEnabledState::Left : ChannelEnabledState::None);
                mmu->squareWave1Channel.channelEnabledState = (ChannelEnabledState)channelState;
                
                channelState = 0;
                channelState |= (u8)(isBitSet(1, byte) ? ChannelEnabledState::Right : ChannelEnabledState::None);
                channelState |= (u8)(isBitSet(5, byte) ? ChannelEnabledState::Left : ChannelEnabledState::None);
                mmu->squareWave2Channel.channelEnabledState = (ChannelEnabledState)channelState;
                
                channelState = 0;
                channelState |= (u8)(isBitSet(2, byte) ? ChannelEnabledState::Right : ChannelEnabledState::None);
                channelState |= (u8)(isBitSet(6, byte) ? ChannelEnabledState::Left : ChannelEnabledState::None);
                mmu->waveChannel.channelEnabledState = (ChannelEnabledState)channelState;
                
                channelState = 0;
                channelState |= (u8)(isBitSet(3, byte) ? ChannelEnabledState::Right : ChannelEnabledState::None);
                channelState |= (u8)(isBitSet(7, byte) ? ChannelEnabledState::Left : ChannelEnabledState::None);
                mmu->noiseChannel.channelEnabledState = (ChannelEnabledState)channelState;
            }
            
        } break;
        
        case 0xFF26: {//NR52
            mmu->NR52 = (byte & 0x80) | 0x70;
            if (isBitSet(7, byte)) {
                mmu->isSoundEnabled = true;
            }
            else {
                forirange (0xFF10, 0xFF26) {
                    writeByte(0, (u16)i, mmu, gbDebug);
                }
                mmu->isSoundEnabled = false;
            }
        } break;
        
        case 0xFF30 ... 0xFF3F:  {
            mmu->waveChannel.waveTable[address & 0xF] = byte;
        } break;
        
        case 0xFF40: { //LCD Control
            lcd->isEnabled = isBitSet(7, byte);
            if (lcd->isEnabled) {
                lcd->stat = (lcd->stat & 0xF8) | (u8)lcd->mode;
            }
            else {
                lcd->stat &= 0xF8 ;  //reset to HBlank if disabled. TODO: Not sure if this correct
                lcd->modeClock = 0;
                lcd->mode = LCDMode::HBlank;
                lcd->ly = 0;
            }
            
            //Bit 6 - Window Tile Map Display Select (0=9800-9BFF, 1=9C00-9FFF)
            lcd->windowTileMap = isBitSet(6, byte) ? 1 : 0;
            //Bit 5 - Window Display Enable
            lcd->isWindowEnabled = isBitSet(5, byte);
            //Bit 4 - Background Tile Set Select
            lcd->backgroundTileSet = isBitSet(4, byte) ? 1 : 0;
            //Bit 3 - Background Tile Map Select
            lcd->backgroundTileMap = isBitSet(3, byte) ? 1 : 0;
            //Bit 2 - Sprite size
            lcd->spriteHeight = isBitSet(2, byte) ? SpriteHeight::Tall : SpriteHeight::Short;
            //Bit 1 - OAM enabled
            lcd->isOAMEnabled = isBitSet(1, byte);
            //Bit 0 - Background enabled
            lcd->isBackgroundEnabled = isBitSet(0, byte);
            
        } break;
        case 0xFF41: lcd->stat = (lcd->stat & 7) | byte; break; //STAT interrupt (last 3 bytes read-only)
        case 0xFF42: lcd->scy = byte; break;
        case 0xFF43: lcd->scx = byte; break;
        case 0xFF44: lcd->ly = 0; break; //reset ly
        case 0xFF45: lcd->lyc = byte; break;
        case 0xFF46: {
            if (byte < 0xF1) {
                mmu->currentDMAAddress = ((u16)(byte << 8)) & 0xFF00;
                mmu->isDMAOccurring = true;
            }
        } break;
        case 0xFF47: updateColorPaletteFromU8(lcd->backgroundPalette, byte); break;
        case 0xFF48: updateColorPaletteFromU8(lcd->spritePalette0, byte); break;
        case 0xFF49: updateColorPaletteFromU8(lcd->spritePalette1, byte); break;
        case 0xFF4A: lcd->wy = byte; break;
        case 0xFF4B: lcd->wx = byte; break;
        //TODO: Implement writing to LCD status
        case 0xFF50: break;//mmu->inBios = (byte != 0) ? false : true; break;
        case 0xFF80 ... 0xFFFE: mmu->zeroPageRAM[address - 0xFF80] = byte; break;
        case 0xFFFF: mmu->enabledInterrupts = byte; break;
        
        
    }
}

u16 readWord(u16 address, MMU *mmu) {
    CO_ASSERT((address + 1) > address);
    u16 ret = word(readByte(address + 1, mmu),readByte(address, mmu));
    return ret;
}

void writeWord(u16 word, u16 address, MMU *mmu, GameBoyDebug *gbDebug) {
    CO_ASSERT((address + 1) > address);
    writeByte(lb(word), address, mmu, gbDebug);
    writeByte(hb(word), address + 1, mmu, gbDebug);
}


/*** CPU Stuff ***/

#define SET_FLAG(flag) (cpu->F |= (int)Flag::flag)
#define CLEAR_FLAG(flag) (cpu->F &= ~(int)Flag::flag)
#define IS_FLAG_SET(flag) ((cpu->F & (int)Flag::flag) != 0)
#define IS_FLAG_CLEAR(flag) ((cpu->F & (int)Flag::flag) == 0)

#define COND_FLAG(flag, cond) if (cond) SET_FLAG(flag); else CLEAR_FLAG(flag)

#define LOAD_IMM16(hReg, lReg) do {\
    cpu->hReg = readByte(cpu->PC + 2, mmu);\
    cpu->lReg = readByte(cpu->PC + 1, mmu);\
    cpu->PC += 3;\
    cpu->instructionCycles = 12;} while (0)\

#define INC8(reg) do {\
    cpu->reg++;\
    COND_FLAG(Z, cpu->reg == 0);\
    CLEAR_FLAG(N);\
    COND_FLAG(H, (cpu->reg & 0xF) == 0);\
    cpu->PC += 1; cpu->instructionCycles = 4;} while (0)

#define DEC8(reg) do {\
    cpu->reg--;\
    COND_FLAG(Z, cpu->reg == 0);\
    SET_FLAG(N);\
    COND_FLAG(H, (cpu->reg & 0xF) == 0xF);\
    cpu->PC += 1; cpu->instructionCycles = 4;} while (0)

#define INC16(h, l) do {\
    u16 newVal = word(cpu->h, cpu->l) + 1;\
    cpu->h = hb(newVal);\
    cpu->l = lb(newVal);\
    cpu->PC += 1; cpu->instructionCycles = 8;} while(0)

#define DEC16(h, l)  do {\
    u16 newVal = word(cpu->h, cpu->l) - 1;\
    cpu->h = hb(newVal);\
    cpu->l = lb(newVal);\
    cpu->PC += 1; cpu->instructionCycles = 8;} while(0)

#define ROTATE_LEFT(toRotate, shouldClearZ) do {\
    CLEAR_FLAG(N);\
    CLEAR_FLAG(H);\
    COND_FLAG(C, (toRotate & 0x80) != 0);\
    toRotate = (u8)(toRotate << 1) | (u8)(toRotate >> 7);\
    if (shouldClearZ)\
    CLEAR_FLAG(Z);\
    else\
    COND_FLAG(Z, toRotate == 0);} while (0)

#define ROTATE_RIGHT(toRotate, shouldClearZ) do {\
    CLEAR_FLAG(N);\
    CLEAR_FLAG(H);\
    COND_FLAG(C, (toRotate & 0x1) != 0);\
    toRotate = (u8)(toRotate >> 1) | (u8)(toRotate << 7);\
    if (shouldClearZ)\
    CLEAR_FLAG(Z);\
    else\
    COND_FLAG(Z, toRotate == 0);} while (0)

#define ADD_TO_HL(hReg, lReg) do {\
    CLEAR_FLAG(N);\
    u32 src = word(cpu->hReg, cpu->lReg);\
    u32 HL = word(cpu->H, cpu->L);\
    u32 result = src + HL;\
    if ((result & 0x10000) != 0)\
    SET_FLAG(C);\
    else\
    CLEAR_FLAG(C);\
    if (isBitSet(12, (u16)(HL ^ src ^ result)))\
    SET_FLAG(H);\
    else\
    CLEAR_FLAG(H);\
    cpu->H = hb((u16)result);\
    cpu->L = lb((u16)result);\
    cpu->PC += 1;\
    cpu->instructionCycles = 8;} while (0)


#define ROTATE_LEFT_CARRY(toRotate, shouldClearZ) do {\
    CLEAR_FLAG(N);\
    CLEAR_FLAG(H);\
    bool isCarrySetBeforeRotate = IS_FLAG_SET(C);\
    COND_FLAG(C, (toRotate & 0x80) != 0);\
    toRotate = isCarrySetBeforeRotate ? (u8)((toRotate << 1) | 1)\
    : (u8)(toRotate << 1);\
    if (shouldClearZ)\
    CLEAR_FLAG(Z);\
    else\
    COND_FLAG(Z, toRotate == 0);} while (0)

#define ROTATE_RIGHT_CARRY(toRotate, shouldClearZ) do {\
    CLEAR_FLAG(N);\
    CLEAR_FLAG(H);\
    bool isCarrySetBeforeRotate = IS_FLAG_SET(C);\
    COND_FLAG(C, (toRotate & 0x1) != 0);\
    toRotate = isCarrySetBeforeRotate ? (u8)((toRotate >> 1) | 0x80)\
    : (u8)(toRotate >> 1);\
    if (shouldClearZ)\
    CLEAR_FLAG(Z);\
    else\
    COND_FLAG(Z, toRotate == 0);} while (0)

#define JUMP_REL(condition) do {\
    if (condition) {\
        i8 offset = (i8)readByte(cpu->PC + 1, mmu);\
        cpu->PC = (u16)((i16)cpu->PC + ((i16)offset + 2));\
        cpu->instructionCycles = 12;\
    }\
    else {\
        cpu->PC += 2;\
        cpu->instructionCycles = 8;}} while (0)

#define ADD8(src, shouldAddCarry) do {\
    u16 sum = (u16)cpu->A + src;\
    if (shouldAddCarry && IS_FLAG_SET(C)) {\
        sum++;\
    }\
    CLEAR_FLAG(N);\
    COND_FLAG(Z, (sum & 0xFF) == 0);\
    COND_FLAG(C, (sum & 0x100) != 0);\
    COND_FLAG(H, ((cpu->A ^ src ^ (sum & 0xFF)) & 0x10) != 0);\
    cpu->A = (u8)sum;} while (0)

#define SUB8(src, shouldSubCarry, isCPInstr) do {\
    u16 diff = (u16)cpu->A - (u16)src;\
    if (shouldSubCarry && IS_FLAG_SET(C)) {\
        diff--;\
    }\
    SET_FLAG(N);\
    COND_FLAG(Z, (diff & 0xFF) == 0);\
    COND_FLAG(C, diff > 0xFF);\
    COND_FLAG(H, isBitSet(4, (u8)(cpu->A ^ src ^ diff)));\
    if (!isCPInstr) cpu->A = (u8)diff;} while (0)

#define RET_FROM_PROC(condition) do {\
    if (condition) {\
        cpu->PC = popOffStack(&cpu->SP, mmu);\
        cpu->instructionCycles = 20;\
    }\
    else {\
        cpu->PC++;\
        cpu->instructionCycles = 8;}} while (0)

#define CALL_PROC(condition) do {\
    if (condition) {\
        pushOnToStack(cpu->PC + 3, &cpu->SP, mmu, gbDebug);\
        cpu->PC = readWord(cpu->PC + 1, mmu);\
        cpu->instructionCycles = 24;\
    }\
    else {\
        cpu->PC += 3;\
        cpu->instructionCycles = 12; }} while (0)

#define PUSH_REGS(hReg, lReg) do {\
    pushOnToStack(word(cpu->hReg, cpu->lReg), &cpu->SP, mmu, gbDebug);\
    cpu->PC++;\
    cpu->instructionCycles = 16;} while (0)

#define POP_REGS(hReg, lReg) do {\
    u16 val = popOffStack(&cpu->SP, mmu);\
    cpu->hReg = hb(val);\
    cpu->lReg = lb(val);\
    cpu->PC++;\
    cpu->instructionCycles = 12;} while (0)

#define JUMP_ABS(condition) do {\
    if (condition) {\
        cpu->PC = readWord(cpu->PC + 1, mmu);\
        cpu->instructionCycles = 16;\
    }\
    else {\
        cpu->PC += 3;\
        cpu->instructionCycles = 12;}} while (0)

#define RESTART(restartAddress) do {\
    pushOnToStack(cpu->PC + 1, &cpu->SP, mmu, gbDebug);\
    cpu->PC = restartAddress;\
    cpu->instructionCycles = 16;} while (0)



static u16 addSPAndOperand(CPU *cpu, MMU *mmu) {
    i32 addend = (i32)((i8)readByte(cpu->PC + 1, mmu));
    i32 sum = (i32)cpu->SP + addend;
    i32 bitsCarried = addend ^ (i32)cpu->SP ^ (sum & 0xFFFF);
    
    CLEAR_FLAG(Z);
    CLEAR_FLAG(N);
    
    //NOTE: Documentation is poor on this, but
    //      previously the H and C flags were only set on positive nubmers. However
    //      according to the 03-ops ROM, these bits are set on both negative and
    //      positive nubmers.
    COND_FLAG(H, (bitsCarried & 0x10) != 0);
    COND_FLAG(C, (bitsCarried & 0x100) != 0);
    
    return (u16)sum;
}

static void pushOnToStack(u16 value, u16 *SP, MMU* mmu, GameBoyDebug *gbDebug) {
    *SP -= 2;
    writeWord(value, *SP, mmu, gbDebug);
}

static u16 popOffStack(u16 *SP, MMU *mmu) {
    u16 ret = readWord(*SP, mmu);
    *SP += 2;
    
    return ret;
}

//addresses of interrupt service routines in order of priority
const u16 interruptRoutineAddresses[] = {0x40, 0x48, 0x50, 0x58, 0x60};

static void stepCPU(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug) {
    cpu->instructionCycles = 4;
    
    bool isHandlingInterrupts = false;
    u8 interruptsToHandle = mmu->enabledInterrupts & mmu->requestedInterrupts;
    if (interruptsToHandle > 0) {
        if (cpu->isHalted) {
            cpu->isHalted = false;
        }
        if (cpu->enableInterrupts) {
            //TODO: change names to be more distinct
            isHandlingInterrupts = true;
            foriarr (interruptRoutineAddresses) {
                if (isBitSet((int)i, interruptsToHandle)) {
                    pushOnToStack(cpu->PC, &cpu->SP, mmu, gbDebug);
                    cpu->PC = interruptRoutineAddresses[i];
                    
                    clearBit((int)i, &mmu->requestedInterrupts);
                    cpu->enableInterrupts = false;
                    Breakpoint *bp;
                    if (shouldBreakOnPC(cpu->PC, gbDebug, &bp)) {
                        gbDebug->hitBreakpoint = bp;
                        return;
                    }
                    break;
                }
            }
        }
    }
    
    if (!cpu->isHalted) {
        u8 instructionToExecute = readByte(cpu->PC, mmu);
        
        switch (instructionToExecute) {
            case 0x0: { //NOP
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x1: { //LD BC, NN
                LOAD_IMM16(B, C);
            } break;
            
            case 0x2: { //LD (BC), A
                writeByte(cpu->A, word(cpu->B, cpu->C), mmu, gbDebug);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x3: { //INC BC
                INC16(B,C);
            } break;
            
            case 0x4: { //INC B
                INC8(B);
            } break;
            
            case 0x5: { //DEC B
                DEC8(B);
            } break;
            
            case 0x6: { //LD B, d8
                cpu->B = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x7: { //RLCA
                ROTATE_LEFT(cpu->A, true);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x8:  { //LD (a16), SP
                u16 addr = readWord(cpu->PC + 1, mmu);
                writeWord(cpu->SP, addr, mmu, gbDebug);
                cpu->PC += 3;
                cpu->instructionCycles = 20;
            } break;
            
            case 0x9: { //ADD HL, BC
                ADD_TO_HL(B,C);
            } break;
            
            case 0xA: { //LD A, (BC)
                cpu->A = readByte(word(cpu->B, cpu->C), mmu);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0xB: { //DEC BC
                DEC16(B,C);
            } break;
            
            case 0xC: { //INC C
                INC8(C);
            } break;
            
            case 0xD: { //DEC C
                DEC8(C);
            } break;
            
            case 0xE: { //LD C, d8
                cpu->C = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0xF: { //RRCA
                ROTATE_RIGHT(cpu->A, true);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
                
            } break;
            
            case 0x10: { //STOP 0
                //TODO: to be implemented
                CO_ASSERT(readByte(cpu->PC + 1, mmu) == 0); //next byte should 0
                cpu->PC += 2;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x11: { //LD DE, d16
                LOAD_IMM16(D, E);
            } break;
            
            case 0x12: { //LD (DE), A
                writeByte(cpu->A, word(cpu->D, cpu->E), mmu, gbDebug);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x13: { //INC DE
                INC16(D, E);
            } break;
            
            case 0x14: { //INC D
                INC8(D);
            } break;
            
            case 0x15: { //DEC D
                DEC8(D);
            } break;
            
            case 0x16: { //LD D, d8
                cpu->D = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x17: { //RLA
                ROTATE_LEFT_CARRY(cpu->A, true);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x18: { //JR s8
                JUMP_REL(true);
            } break;
            
            case 0x19: { //ADD HL, DE
                ADD_TO_HL(D, E);
            } break;
            
            case 0x1A: { //LD A, (DE)
                cpu->A = readByte(word(cpu->D, cpu->E), mmu);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x1B: { //DEC DE
                DEC16(D, E);
            } break;
            
            case 0x1C: { //INC E
                INC8(E);
            } break;
            
            case 0x1D: { //DEC E
                DEC8(E);
            } break;
            
            case 0x1E: { //LD E, d8
                cpu->E = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x1F: { //RRA
                ROTATE_RIGHT_CARRY(cpu->A, true);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x20: { //JR NZ, s8
                JUMP_REL(IS_FLAG_CLEAR(Z));
            } break;
            
            case 0x21: { //LD HL, d16
                LOAD_IMM16(H, L);
            } break;
            
            case 0x22: { //LD (HL+), A
                writeByte(cpu->A, word(cpu->H, cpu->L), mmu, gbDebug);
                INC16(H,L);
            } break;
            
            case 0x23: { //INC HL
                INC16(H,L);
            } break;
            
            case 0x24: { //INC H
                INC8(H);
            } break;
            
            case 0x25: { //DEC H
                DEC8(H);
            } break;
            
            case 0x26: { //LD H, d8
                cpu->H = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x27: { //DAA
                if (!IS_FLAG_SET(N)) {
                    if (IS_FLAG_SET(C) || cpu->A > 0x99) {
                        cpu->A += 0x60;
                        SET_FLAG(C);
                    }
                    
                    if (IS_FLAG_SET(H) || (cpu->A & 0xF) > 0x9) {
                        cpu->A += 0x6;
                    }
                }
                else {
                    if (IS_FLAG_SET(H)) {
                        cpu->A -= 0x6;
                    }
                    
                    if (IS_FLAG_SET(C)) {
                        cpu->A -= 0x60;
                    }
                }
                
                CLEAR_FLAG(H);
                COND_FLAG(Z, cpu->A == 0);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
                
            } break;
            
            case 0x28: { //JR Z, s8
                JUMP_REL(IS_FLAG_SET(Z));
            } break;
            
            case 0x29: { //ADD HL, HL
                ADD_TO_HL(H,L);
            } break;
            
            case 0x2A: { //LD A, (HL+)
                cpu->A = readByte(word(cpu->H, cpu->L), mmu);
                INC16(H, L);
            } break;
            
            case 0x2B: { //DEC HL
                DEC16(H,L);
            } break;
            
            case 0x2C: { //INC L
                INC8(L);
            } break;
            
            case 0x2D: { //DEC L
                DEC8(L);
            } break;
            
            case 0x2E: { //LD L, d8
                cpu->L = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x2F: { //CPL
                cpu->A = ~cpu->A;
                SET_FLAG(N);
                SET_FLAG(H);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x30: { //JR NC, s8
                JUMP_REL(IS_FLAG_CLEAR(C));
            } break;
            
            case 0x31: { //LD SP, d16
                cpu->SP = readWord(cpu->PC + 1, mmu);
                cpu->PC += 3;
                cpu->instructionCycles = 12;
            } break;
            
            case 0x32: { //LD (HL-), A
                writeByte(cpu->A, word(cpu->H, cpu->L), mmu, gbDebug);
                DEC16(H, L);
            } break;
            
            case 0x33: { //INC SP
                cpu->SP++;
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x34: { //INC (HL)
                u8 val = readByte(word(cpu->H, cpu->L), mmu) + 1;
                COND_FLAG(Z, val == 0);
                COND_FLAG(H, (val & 0xF) == 0);
                CLEAR_FLAG(N);
                writeByte(val, word(cpu->H, cpu->L), mmu, gbDebug);
                cpu->PC += 1;
                cpu->instructionCycles = 12;
            } break;
            
            case 0x35: { //DEC (HL)
                u8 val = readByte(word(cpu->H, cpu->L), mmu) - 1;
                COND_FLAG(Z, val == 0);
                COND_FLAG(H, (val & 0xF) == 0xF);
                SET_FLAG(N);
                writeByte(val, word(cpu->H, cpu->L), mmu, gbDebug);
                cpu->PC += 1;
                cpu->instructionCycles = 12;
            } break;
            
            case 0x36: { //LD (HL), d8
                u8 val = readByte(cpu->PC + 1, mmu);
                writeByte(val, word(cpu->H, cpu->L), mmu, gbDebug);
                cpu->PC += 2;
                cpu->instructionCycles = 12;
            } break;
            
            case 0x37: { //SCF
                CLEAR_FLAG(H);
                CLEAR_FLAG(N);
                SET_FLAG(C);
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x38: { //JR C, r8
                JUMP_REL(IS_FLAG_SET(C));
            } break;
            
            case 0x39: { //ADD HL, SP
                u32 src = cpu->SP;
                CLEAR_FLAG(N);
                u32 HL = word(cpu->H, cpu->L);
                u32 result = src + HL;
                if ((result & 0x10000) != 0) {
                    SET_FLAG(C);
                }
                else {
                    CLEAR_FLAG(C);
                }
                
                if (((HL ^ src ^ (result & 0xFFFF)) & 0x1000) != 0) {
                    SET_FLAG(H);
                }
                else {
                    CLEAR_FLAG(H);
                }
                cpu->H = hb((u16)result);
                cpu->L = lb((u16)result);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x3A: { //LD A, (HL-)
                cpu->A = readByte(word(cpu->H, cpu->L), mmu);
                DEC16(H, L);
            } break;
            
            case 0x3B: { //DEC SP
                cpu->SP--;
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x3C: { //INC A
                INC8(A);
            } break;
            
            case 0x3D: { //DEC A
                DEC8(A);
            } break;
            
            case 0x3E: { //LD A, d8
                cpu->A = readByte(cpu->PC + 1, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x3F: { //CCF
                CLEAR_FLAG(H);
                CLEAR_FLAG(N);
                COND_FLAG(C, IS_FLAG_CLEAR(C));
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            case 0x40 ... 0x6F:
            case 0x78 ... 0x7F: { //8 bit load instructions except for LD (HL), Reg
                u8 src, *dest;
                bool isMemoryOp = false;
                
                switch ((instructionToExecute & 0xF) % 8) {
                    case 0: src = cpu->B; break;
                    case 1: src = cpu->C; break;
                    case 2: src = cpu->D; break;
                    case 3: src = cpu->E; break;
                    case 4: src = cpu->H; break;
                    case 5: src = cpu->L; break;
                    case 6: 
                    isMemoryOp = true;
                    src = readByte(word(cpu->H, cpu->L), mmu); 
                    break;
                    case 7: src = cpu->A; break;
                    default: cpu->didHitIllegalOpcode = true; return;
                }
                
                switch (instructionToExecute) {
                    case 0x40 ... 0x47: dest = &cpu->B; break;
                    case 0x48 ... 0x4F: dest = &cpu->C; break;
                    case 0x50 ... 0x57: dest = &cpu->D; break;
                    case 0x58 ... 0x5F: dest = &cpu->E; break;
                    case 0x60 ... 0x67: dest = &cpu->H; break;
                    case 0x68 ... 0x6F: dest = &cpu->L; break;
                    case 0x78 ... 0x7F: dest = &cpu->A; break;
                }
                
                *dest = src;
                
                cpu->PC += 1;
                if (isMemoryOp) {
                    cpu->instructionCycles = 8; 
                }
                else {
                    cpu->instructionCycles = 4; 
                }
                
            } break;
            case 0x77:
            case 0x70 ... 0x75: { //LD (HL), N
                u8 src;
                switch ((instructionToExecute & 0xF) % 8) {
                    case 0: src = cpu->B; break;
                    case 1: src = cpu->C; break;
                    case 2: src = cpu->D; break;
                    case 3: src = cpu->E; break;
                    case 4: src = cpu->H; break;
                    case 5: src = cpu->L; break;
                    case 7: src = cpu->A; break;
                    default: cpu->didHitIllegalOpcode = true; return;
                }
                
                writeByte(src, word(cpu->H, cpu->L), mmu, gbDebug);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0x76: { //HALT
                //TODO: finish implementing
                cpu->isHalted = true;
                cpu->instructionCycles = 4;
                cpu->PC += 1;
            } break;
            
            case 0x80 ... 0xBF:
            case 0xC6:
            case 0xD6:
            case 0xE6:
            case 0xF6:
            case 0xCE:
            case 0xDE:
            case 0xEE:
            case 0xFE: {
                //ADD, ADC, SUB, SBC, AND, XOR, OR and CP instructions, where destination is register A
                u8 src;
                
                //get source byte
                switch (instructionToExecute) {
                    case 0x80 ... 0xBF: {
                        bool isMemoryOp = false;
                        
                        switch ((instructionToExecute & 0xF) % 8) {
                            case 0: src = cpu->B; break;
                            case 1: src = cpu->C; break;
                            case 2: src = cpu->D; break;
                            case 3: src = cpu->E; break;
                            case 4: src = cpu->H; break;
                            case 5: src = cpu->L; break;
                            case 6: 
                            src = readByte(word(cpu->H, cpu->L), mmu); 
                            isMemoryOp = true;
                            break;
                            case 7: src = cpu->A; break;
                            default: cpu->didHitIllegalOpcode = true; return;
                        }
                        
                        cpu->PC += 1;
                        if (isMemoryOp) {
                            cpu->instructionCycles = 8; 
                        }
                        else {
                            cpu->instructionCycles = 4; 
                        }
                        
                    } break;
                    
                    case 0xC6:
                    case 0xD6:
                    case 0xE6:
                    case 0xF6:
                    case 0xCE:
                    case 0xDE:
                    case 0xEE:
                    case 0xFE: {
                        src = readByte(cpu->PC + 1, mmu);
                        cpu->PC += 2;
                        cpu->instructionCycles = 8;
                    } break;
                    default: cpu->didHitIllegalOpcode = true; return;
                }
                
                switch (instructionToExecute) {
                    case 0x80 ... 0x87:
                    case 0xC6: { //ADD A, N
                        ADD8(src, false);
                    } break;
                    
                    case 0x88 ... 0x8F:
                    case 0xCE: { //ADC A, N
                        ADD8(src, true);
                    } break;
                    
                    case 0x90 ... 0x97:
                    case 0xD6: { //SUB N
                        SUB8(src, false, false);
                    } break;
                    
                    case 0x98 ... 0x9F:
                    case 0xDE: { //SBC N
                        SUB8(src, true, false);
                    } break;
                    
                    case 0xA0 ... 0xA7:
                    case 0xE6: { //AND N
                        cpu->A &= src;
                        SET_FLAG(H);
                        CLEAR_FLAG(N);
                        CLEAR_FLAG(C);
                        COND_FLAG(Z, cpu->A == 0);
                    } break;
                    
                    case 0xA8 ... 0xAF:
                    case 0xEE: { //XOR N
                        cpu->A ^= src;
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        CLEAR_FLAG(C);
                        COND_FLAG(Z, cpu->A == 0);
                    } break;
                    
                    case 0xB0 ... 0xB7:
                    case 0xF6: { //OR N
                        cpu->A |= src;
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        CLEAR_FLAG(C);
                        COND_FLAG(Z, cpu->A == 0);
                    } break;
                    
                    case 0xB8 ... 0xBF:
                    case 0xFE: { //CP N
                        SUB8(src, false, true);
                    } break;
                }
                
            } break;
            
            case 0xC0: { //RET NZ
                RET_FROM_PROC(IS_FLAG_CLEAR(Z));
            } break;
            
            case 0xC1: { //POP BC
                POP_REGS(B, C);
            } break;
            
            case 0xC2: { //JP NZ, a16
                JUMP_ABS(IS_FLAG_CLEAR(Z));
            } break;
            
            case 0xC3: { //JP a16
                JUMP_ABS(true);
            } break;
            
            case 0xC4: { //CALL NZ, a16
                CALL_PROC(IS_FLAG_CLEAR(Z));
            } break;
            
            case 0xC5: { //PUSH BC
                PUSH_REGS(B,C);
            } break;
            
            //C6 implemented above
            
            case 0xC7: { //RST 00H
                RESTART(0x0);
            } break;
            
            case 0xC8: { //RET Z
                RET_FROM_PROC(IS_FLAG_SET(Z));
            } break;
            
            case 0xC9: { //RET
                cpu->PC = popOffStack(&cpu->SP, mmu);
                cpu->instructionCycles = 16;
            } break;
            
            case 0xCA: { //JP Z, a16
                JUMP_ABS(IS_FLAG_SET(Z));
            } break;
            
            case 0xCB: { //CB prefixes
                //TODO: Email pastraiser.  There seemse to be a discrepency between
                //pastraiser and marc rawer manuals.  SRA should set Carry and RLCA should set Zero
                bool shouldSaveResult = true;
                bool isMemoryOp = false;
                
                u8 cbInstruction = readByte(cpu->PC + 1, mmu);
                u8 src;
                switch (cbInstruction % 8) {
                    case 0: src = cpu->B; break;
                    case 1: src = cpu->C; break;
                    case 2: src = cpu->D; break;
                    case 3: src = cpu->E; break;
                    case 4: src = cpu->H; break;
                    case 5: src = cpu->L; break;
                    case 6: src = readByte(word(cpu->H, cpu->L), mmu); isMemoryOp = true; break;
                    case 7: src = cpu->A; break;
                    default: cpu->didHitIllegalOpcode = true; return;
                }
                
                switch (cbInstruction) {
                    case 0x0 ... 0x7: ROTATE_LEFT(src, false); break; //RLC
                    case 0x8 ... 0xF: ROTATE_RIGHT(src, false); break; //RRC
                    case 0x10 ... 0x17: ROTATE_LEFT_CARRY(src, false); break; //RL
                    case 0x18 ... 0x1F: ROTATE_RIGHT_CARRY(src, false); break; //RR
                    case 0x20 ... 0x27: { //SLA
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        
                        //store high bit in carry
                        COND_FLAG(C, (src & 0x80) != 0);
                        src <<= 1;
                        COND_FLAG(Z, src == 0);
                    } break;
                    
                    case 0x28 ... 0x2F: { //SRA
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        
                        //store low bit in carry
                        COND_FLAG(C, (src & 1) != 0);
                        
                        //propagate sign bit
                        src = (u8)((i8)src >> 1);
                        COND_FLAG(Z, src == 0);
                    } break;
                    
                    case 0x30 ... 0x37: { //SWAP
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        CLEAR_FLAG(C);
                        
                        src = (u8)(src << 4) | (u8)(src >> 4);
                        COND_FLAG(Z, src == 0);
                    } break;
                    
                    case 0x38 ... 0x3F: { //SRL
                        CLEAR_FLAG(H);
                        CLEAR_FLAG(N);
                        
                        //store low bit in carry
                        COND_FLAG(C, (src & 1) != 0);
                        
                        src >>= 1;
                        
                        COND_FLAG(Z, src == 0);
                    } break;
                    
                    case 0x40 ... 0x7F: { //BIT
                        SET_FLAG(H);
                        CLEAR_FLAG(N);
                        
                        u8 bitToTest = (cbInstruction - 0x40) / 8;
                        COND_FLAG(Z, !isBitSet(bitToTest, src));
                        shouldSaveResult = false;
                    } break;
                    
                    case 0x80 ... 0xBF: { //RES
                        u8 maskToResetBit = (u8)~(1 << (cbInstruction - 0x80) / 8);
                        src &= maskToResetBit;
                    } break;
                    
                    case 0xC0 ... 0xFF: { //SET
                        u8 maskToSetBit = (u8)(1 << (cbInstruction - 0xC0) / 8);
                        src |= maskToSetBit;
                    } break;
                    
                }
                
                cpu->PC += 2;
                
                if (shouldSaveResult) {
                    switch (cbInstruction % 8) {
                        case 0: {
                            cpu->B = src;
                        } break;
                        case 1: {
                            cpu->C = src;
                        } break;
                        case 2: {
                            cpu->D = src;
                        } break;
                        case 3: {
                            cpu->E = src;
                        } break;
                        case 4: {
                            cpu->H = src;
                        } break;
                        
                        case 5: {
                            cpu->L = src;
                        } break;
                        
                        case 6: {
                            writeByte(src, word(cpu->H, cpu->L), mmu, gbDebug);
                        } break;
                        
                        case 7: {
                            cpu->A = src;
                        } break;
                    }
                    
                }
                if (isMemoryOp) {
                    cpu->instructionCycles = 16;
                }
                else {
                    cpu->instructionCycles = 8;
                }
            } break;
            
            case 0xCC: CALL_PROC(IS_FLAG_SET(Z));         break; //CALL Z, a16
            case 0xCD: CALL_PROC(true);                       break; //CALL a16
            //CE Implemented above
            case 0xCF: RESTART(0x8);                          break; //RST 08H
            case 0xD0: RET_FROM_PROC(IS_FLAG_CLEAR(C));   break; //RET NC
            case 0xD1: POP_REGS(D, E);                        break; //POP DE
            case 0xD2: JUMP_ABS(IS_FLAG_CLEAR(C));        break; //JP NC, a16
            //No D3
            case 0xD4: CALL_PROC(IS_FLAG_CLEAR(C));       break; //CALL NC, a16
            case 0xD5: PUSH_REGS(D, E);                       break; //PUSH DE
            //D6 implemented above
            case 0xD7: RESTART(0x10);                         break; //RST 10H
            case 0xD8: RET_FROM_PROC(IS_FLAG_SET(C));     break; //RET C
            
            case 0xD9: {                                             //RETI
                cpu->enableInterrupts = true;
                RET_FROM_PROC(true);
            } break;
            
            case 0xDA: JUMP_ABS(IS_FLAG_SET(C));          break; //JP C, a16
            //No DB
            case 0xDC: CALL_PROC(IS_FLAG_SET(C));         break; //CALL C
            //No DD
            //DE implemented above
            case 0xDF: RESTART(0x18);                         break; //RST 18H
            
            case 0xE0: {                                             //LDH (a8), A
                u16 addr = (u16)readByte(cpu->PC + 1, mmu) + 0xFF00;
                writeByte(cpu->A, addr, mmu, gbDebug);
                cpu->PC += 2;
                cpu->instructionCycles = 12;
            } break;
            
            case 0xE1: POP_REGS(H, L);                        break; //POP HL
            case 0xE2: {                                             //LD (C), A
                writeByte(cpu->A, (u16)cpu->C + 0xFF00, mmu, gbDebug);
                //TODO: this maybe PC += 2, according to pastrasier???
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            //No E3
            //No E4
            case 0xE5: PUSH_REGS(H, L);                       break; //PUSH HL
            //E6 implemented above
            case 0xE7: RESTART(0x20);                         break; //RST 20H
            
            case 0xE8: {                                             //ADD SP, r8
                cpu->SP = addSPAndOperand(cpu, mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 16;
            } break;
            
            case 0xE9: {                                             //JP (HL)
                cpu->PC = word(cpu->H, cpu->L);
                cpu->instructionCycles = 4;
            } break;
            
            case 0xEA: {                                             //LD (a16) A
                writeByte(cpu->A, readWord(cpu->PC + 1, mmu), mmu, gbDebug);
                cpu->PC += 3;
                cpu->instructionCycles = 16;
            } break;
            //No EB
            //No EC
            //No ED
            //EE implemented above
            
            case 0xEF: RESTART(0x28);                         break; //RST 28H
            case 0xF0: {                                             //LDH A, (a8)
                cpu->A = readByte(0xFF00 + (u16)readByte(cpu->PC + 1, mmu), mmu);
                cpu->PC += 2;
                cpu->instructionCycles = 12;
            } break;
            
            case 0xF1: POP_REGS(A, F); cpu->F &= 0xF0;        break; //POP AF
            
            case 0xF2: {                                             //LDH A, (C)
                cpu->A = readByte(0xFF00 + cpu->C, mmu);
                //TODO: this maybe PC += 2, according to pastrasier???
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0xF3: {                                              //DI
                cpu->enableInterrupts = false;
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            //No F4
            case 0xF5: PUSH_REGS(A, F);                        break; //PUSH AF
            //F6 implemented above
            case 0xF7: RESTART(0x30);                          break; //RST 30H
            
            case 0xF8: {                                              //LD HL, SP + r8
                u16 sum = addSPAndOperand(cpu, mmu);
                cpu->H = hb(sum);
                cpu->L = lb(sum);
                cpu->PC += 2;
                cpu->instructionCycles = 12;
            } break;
            
            case 0xF9: {                                              //LD SP, HL
                cpu->SP = word(cpu->H, cpu->L);
                cpu->PC += 1;
                cpu->instructionCycles = 8;
            } break;
            
            case 0xFA: {                                              //LD A, (a16)
                cpu->A = readByte(readWord(cpu->PC + 1, mmu), mmu);
                cpu->PC += 3;
                cpu->instructionCycles = 16;
            } break;
            
            case 0xFB: {                                              //EI
                cpu->enableInterrupts = true;
                cpu->PC += 1;
                cpu->instructionCycles = 4;
            } break;
            
            //No FC
            //No FD
            //FE implemented above
            
            case 0xFF: RESTART(0x38);                        break;  //RST 38H
            
            default: { //Illegal instruction
                cpu->didHitIllegalOpcode = true;
                
            } break;
        }
        
        CO_ASSERT(cpu->instructionCycles > 0);
    }
    
    cpu->F &= 0xF0;
    
    if (isHandlingInterrupts) {
        cpu->instructionCycles += 20;
    }
    
    cpu->totalCycles += cpu->instructionCycles;
    
    
    
}
static void recordDebugState(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug) {
    if (gbDebug->numDebugStates < ARRAY_LEN(gbDebug->prevGBDebugStates)) {
        gbDebug->numDebugStates++;
    }
    
    auto prevState = &gbDebug->prevGBDebugStates[gbDebug->currentDebugState];
    prevState->cpu = *cpu;
    
    if (mmu->hasRAM) {
        copyMemory(mmu->cartRAM, prevState->mmu.cartRAM, prevState->mmu.cartRAMSize);
    }
    prevState->mmu = *mmu;
    
    if (gbDebug->currentDebugState < ARRAY_LEN(gbDebug->prevGBDebugStates) - 1){
        gbDebug->currentDebugState++;
    }
    else {
        gbDebug->currentDebugState = 0;
    }
}

static bool rewindState(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug) {
    if (gbDebug->numGBStates == 0) {
        return false;
    }
    
    if (gbDebug->nextFreeGBStateIndex - 1 < 0) {
        gbDebug->nextFreeGBStateIndex = ARRAY_LEN(gbDebug->recordedGBStates) - 1;
    }
    else {
        gbDebug->nextFreeGBStateIndex--;
    }
    gbDebug->numGBStates--;
    
    auto prevState = &gbDebug->recordedGBStates[gbDebug->nextFreeGBStateIndex];
    *cpu = prevState->cpu;
    
    auto cartRAM = mmu->cartRAM;
    copyMemory(prevState->mmu.cartRAM, cartRAM, mmu->cartRAMSize);
    //Since there is only one copy of the MMAPed RAM file, the cartRAMFileMap address 
    //will be the same upon *mmu = prevState->mmu
    copyMemory(prevState->mmu.cartRAM, mmu->cartRAMPlatformState.cartRAMFileMap, mmu->cartRAMSize);
    
    auto soundFrameBufferData = prevState->mmu.soundFramesBuffer.data;
    copyMemory(soundFrameBufferData, mmu->soundFramesBuffer.data,
               mmu->soundFramesBuffer.len);
    
    
    *mmu = prevState->mmu;
    mmu->cartRAM = cartRAM;
    mmu->soundFramesBuffer.data = soundFrameBufferData;
    
    if (mmu->hasRTC) {
        syncRTCTime(&mmu->rtc, mmu->cartRAMPlatformState.rtcFileMap);
    }
    gbDebug->elapsedTimeSinceLastRecord = 0;
    
    return true;
}
void continueFromBreakPoint(GameBoyDebug *gbDebug, MMU *mmu, CPU *cpu, ProgramState *programState) {
    gbDebug->hitBreakpoint = nullptr;
    rewindState(cpu, mmu, gbDebug);
    setPausedState(false, programState, cpu);
    if (mmu->hasRTC) {
        syncRTCTime(&mmu->rtc, mmu->cartRAMPlatformState.rtcFileMap);
    }
}

static void recordState(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug) {
    profileStart("record state", profileState); 
    if (gbDebug->numGBStates < ARRAY_LEN(gbDebug->recordedGBStates)) {
        gbDebug->numGBStates++;
    }
    
    auto prevState = &gbDebug->recordedGBStates[gbDebug->nextFreeGBStateIndex];
    prevState->cpu = *cpu;
    
    auto cartRAM = prevState->mmu.cartRAM; 
    if (mmu->hasRAM) {
        copyMemory(mmu->cartRAM, cartRAM, mmu->cartRAMSize);
    }
    
    auto soundFrameBufferData = prevState->mmu.soundFramesBuffer.data;
    copyMemory(mmu->soundFramesBuffer.data, soundFrameBufferData, 
               mmu->soundFramesBuffer.len);
    
    prevState->mmu = *mmu;
    if (mmu->hasRAM) {
        prevState->mmu.cartRAM = cartRAM;
    }
    
    prevState->mmu.soundFramesBuffer.data = soundFrameBufferData;
    
    
    if (gbDebug->nextFreeGBStateIndex < ARRAY_LEN(gbDebug->recordedGBStates) - 1){
        gbDebug->nextFreeGBStateIndex++;
    }
    else {
        gbDebug->nextFreeGBStateIndex = 0;
    }
    profileEnd(profileState);
}

static void nextScanLine(LCD *lcd, u8 *outRequestedInterrupts) {
    lcd->ly = (lcd->ly == MAX_LY) ? 0 : (lcd->ly + 1);
    
    if (lcd->ly == lcd->lyc) {
        setBit((int)LCDCBit::LY_LYCCoincidenceOccurred, &lcd->stat);
        
        //request lcdc interrupt if enabled
        if (isBitSet((int)LCDCBit::LY_LYCCoincidenceInterrupt, lcd->stat)) {
            setBit((int)InterruptRequestedBit::LCDRequested, outRequestedInterrupts);
        }
    }
    else {
        clearBit((int)LCDCBit::LY_LYCCoincidenceOccurred, &lcd->stat);
    }
}

static void changeToNewLCDMode(LCDMode newMode, LCD *lcd, u8* outRequestedInterrupts) {
    lcd->mode = newMode;
    lcd->stat = (lcd->stat & 0xFC) | (u8)newMode;
    
    switch (newMode) {
        case LCDMode::ScanOAM:
        if (isBitSet((int)LCDCBit::OAMInterrupt, lcd->stat)) {
            setBit((int)InterruptRequestedBit::LCDRequested, outRequestedInterrupts);
        } break;
        case LCDMode::HBlank:
        if (isBitSet((int)LCDCBit::HBlankInterrupt, lcd->stat)) {
            setBit((int)InterruptRequestedBit::LCDRequested, outRequestedInterrupts);
        } break;
        case LCDMode::VBlank:
        if (isBitSet((int)LCDCBit::VBlankInterrupt, lcd->stat)) {
            setBit((int)InterruptRequestedBit::LCDRequested, outRequestedInterrupts);
        }
        setBit((int)InterruptRequestedBit::VBlankRequested, outRequestedInterrupts);
        
        break;
        default:
        break;
    }
}

inline static PaletteColor *spritePaletteForNumber(SpritePalette selectedPalette, LCD *lcd) {
    switch (selectedPalette) {
        case SpritePalette::Palette0: return lcd->spritePalette0;
        case SpritePalette::Palette1: return lcd->spritePalette1;
    }
}



static ColorID colorIDFromTileReference(u8 tileReference, u8 currXPositionInSprite,
                                        u8 currYPositionInSprite, u8 *videoRAM) {
    ColorID ret;
    u8 xMask = 0x80 >> currXPositionInSprite;
    i64 tileAddr = tileReference * BYTES_PER_TILE;
    tileAddr += currYPositionInSprite * BYTES_PER_TILE_ROW;
    
    u8 highBit = (videoRAM[tileAddr + 1] & xMask) ?  1 : 0;
    u8 lowBit = (videoRAM[tileAddr] & xMask) ?  1 : 0;
    
    ret = (ColorID)((highBit * 2) + lowBit);
    return ret;
}

static void drawScanLine(LCD *lcd) {
    
    /* Tile Map:
     *
     * Each "row" is 32 bytes long where each byte is a tile reference
     * Each byte represents a 8x8 pixel tile, so each row and column are 256 pixels long
     * Each byte represents a 16 byte tile where every 2 bytes represents an 8 pixel row
     *
     *------------------------------------------------------
     *|tile ref | tile ref | ...............................
     *|-----------------------------------------------------
     *|tile ref | tile ref | ...............................
     *|.
     *|.
     *|.
     */
    
    struct {
        PaletteColor *palette;
        ColorID color;
    } scanLineToDraw[SCREEN_WIDTH];
    /**************
     * Background
     **************/
    if (lcd->isBackgroundEnabled) {
        u8 yPositionInBackground = lcd->scy + lcd->ly;
        
        u16 backgroundTileRefAddr = 0x1800;
        switch (lcd->backgroundTileMap) {
            //it is 0x1800 instead of 0x9800 because this is relative to start of vram
            case 0: backgroundTileRefAddr = 0x1800; break;
            case 1: backgroundTileRefAddr = 0x1C00; break;
            default: CO_ERR("Wrong tile map");
        }
        //which tile in the y dimension?
        backgroundTileRefAddr += (yPositionInBackground / TILE_HEIGHT) * TILE_MAP_WIDTH;
        //which tile in x dimension?
        backgroundTileRefAddr += lcd->scx / TILE_WIDTH;
        u16 backgroundTileRefRowStart = backgroundTileRefAddr - (lcd->scx / TILE_WIDTH);
        u16 backgroundTileRefRowEnd = backgroundTileRefRowStart + TILE_MAP_WIDTH;
        u8 xPosInBackground = lcd->scx;
        
        fori (SCREEN_WIDTH) {
            u8 xMask = 0x80 >> (xPosInBackground & 7); 
            u8 currPixelXPositionOnScreen = (u8)i;
            
            u8 tileRef = lcd->videoRAM[backgroundTileRefAddr];
            ColorID colorIDOfBackgroundPixel = ColorID::Color0;
            
            u16 absoluteTileAddr = 0;
            switch (lcd->backgroundTileSet) {
                case 0: absoluteTileAddr = (u16)(0x1000 + (i16)(i8)tileRef * BYTES_PER_TILE); break;
                case 1: absoluteTileAddr = tileRef * BYTES_PER_TILE; break;
                default: INVALID_CODE_PATH(); //should not be here
            }
            
            absoluteTileAddr += (u16)(yPositionInBackground & 7) * BYTES_PER_TILE_ROW;
            
            u8 highBit = (lcd->videoRAM[absoluteTileAddr + 1] & xMask) ? 1 : 0;
            u8 lowBit = (lcd->videoRAM[absoluteTileAddr] & xMask) ? 1 : 0;
            
            colorIDOfBackgroundPixel = (ColorID)((highBit * 2) + lowBit);
            scanLineToDraw[currPixelXPositionOnScreen] = {lcd->backgroundPalette, colorIDOfBackgroundPixel};
            
            xPosInBackground++;
            xPosInBackground %= TILE_MAP_WIDTH * TILE_WIDTH;
            if ((xPosInBackground & 7) == 0) {
                backgroundTileRefAddr++;
                
                if (backgroundTileRefAddr >= backgroundTileRefRowEnd) {
                    backgroundTileRefAddr = backgroundTileRefRowStart;
                }
            }
            
        }
        
    }
    else {
        foriarr (scanLineToDraw) {
            scanLineToDraw[i] = {lcd->backgroundPalette, ColorID::Color0};
        }
    }
    /******************
     * Window
     ******************/
    if (lcd->isWindowEnabled && lcd->ly >= lcd->wy && lcd->wx < SCREEN_WIDTH + 7) {
        u8 yPositionInWindow = lcd->ly - lcd->wy;
        u8 xPosInWindow = 0;
        //which tile in the y dimension for window?
        u16 windowTileRefAddr = 0x1C00;
        switch (lcd->windowTileMap) {
            //it is 0x1800 instead of 0x9800 because this is relative to start of vram
            case 0: windowTileRefAddr = 0x1800; break;
            case 1: windowTileRefAddr = 0x1C00; break;
            default: CO_ASSERT_MSG(false, "Wrong tile ref");
        }
        windowTileRefAddr += (yPositionInWindow / TILE_HEIGHT) * TILE_MAP_WIDTH;
        forirange (lcd->wx - 7, SCREEN_WIDTH) {
            u8 xMask = 0x80 >> (xPosInWindow & 7); 
            u8 currPixelXPositionOnScreen = (u8)i;
            
            u16 absoluteTileAddr = 0;
            u8 tileRef = lcd->videoRAM[windowTileRefAddr];
            switch (lcd->backgroundTileSet) {
                case 0: absoluteTileAddr = (u16)(0x1000 + (i16)(i8)tileRef * BYTES_PER_TILE); break;
                case 1: absoluteTileAddr = tileRef * BYTES_PER_TILE; break;
                default: CO_ASSERT(false); //should not be here
            }
            absoluteTileAddr += ((u16)yPositionInWindow & 7) * BYTES_PER_TILE_ROW;
            
            u8 highBit = (lcd->videoRAM[absoluteTileAddr + 1] & xMask) ? 1 : 0;
            u8 lowBit = (lcd->videoRAM[absoluteTileAddr] & xMask) ? 1 : 0;
            
            auto colorIDOfBackgroundPixel = (ColorID)((highBit * 2) + lowBit);
            scanLineToDraw[currPixelXPositionOnScreen] = {lcd->backgroundPalette, colorIDOfBackgroundPixel};
            
            xPosInWindow++;
            if ((xPosInWindow & 7) == 0) {
                windowTileRefAddr++;
            }
        }
    }
    
    /**************
     * Sprites
     **************/
    
    if (lcd->isOAMEnabled) {
        
        Sprite spritesToDraw[MAX_SPRITES_PER_SCANLINE] = {};
        
        i64 numSpritesToDraw = 0, numSpritesOnScanLine = 0;
        
        Sprite *maxSprite = spritesToDraw;
        i64 indexLeftOff = -1;
        u8 minSpriteY = (lcd->spriteHeight == SpriteHeight::Short) ? SHORT_SPRITE_HEIGHT : 0;
        for (i64 i = 0; i < ARRAY_LEN(lcd->oam); i += 4) {
            if (numSpritesOnScanLine >= MAX_SPRITES_PER_SCANLINE) {
                indexLeftOff = i;
                break;
            }
            //TODO: need an array of oam where sprites are sorted by X
            
            //sprite location is lower right hand corner
            //so x and y coords are offset by 8 and 16 respectively
            
            u8 spriteY = lcd->oam[i];
            u8 spriteX = lcd->oam[i + 1];
            //x coordinates explicitly ignored since even though sprites outside of the
            //screen are not drawn, they do affect priority
            if (lcd->ly + minSpriteY < spriteY  && lcd->ly + MAX_SPRITE_HEIGHT >= spriteY) {
                
                numSpritesOnScanLine++;
                
                if (spriteX - MAX_SPRITE_WIDTH < SCREEN_WIDTH && spriteX >= 1) {
                    auto sprite = &spritesToDraw[numSpritesToDraw++];
                    forj (numSpritesToDraw) {
                        if (spritesToDraw[j].x == spriteX) {
                            sprite->isLowPriority = true;
                            break;
                        }
                    }
                    sprite->x = spriteX;
                    sprite->y = spriteY;
                    sprite->tileReference = lcd->oam[i + 2];
                    sprite->flags = lcd->oam[i + 3];
                    
                    if (maxSprite->x < spriteX) {
                        maxSprite = sprite;
                    }
                }
            }
        }
        if (indexLeftOff >= 0) {
            for (i64 i = indexLeftOff; i < ARRAY_LEN(lcd->oam); i += 4) {
                u8 spriteY = lcd->oam[i];
                u8 spriteX = lcd->oam[i + 1];
                
                if (lcd->ly + minSpriteY < spriteY && lcd->ly + MAX_SPRITE_HEIGHT >= spriteY &&
                    spriteX - MAX_SPRITE_WIDTH < SCREEN_WIDTH &&
                    spriteX >= 1) {
                    Sprite *sprite = nullptr;
                    
                    forj (numSpritesToDraw) {
                        if (spritesToDraw[j].isLowPriority) {
                            sprite = &spritesToDraw[j];
                            break;
                        }
                    }
                    
                    if (!sprite) {
                        if (spriteX >= maxSprite->x) {
                            continue;
                        }
                        sprite = maxSprite;
                    }
                    
                    sprite->isLowPriority = false;
                    forj (numSpritesToDraw) {
                        if (spritesToDraw[j].x == spriteX) {
                            sprite->isLowPriority = true;
                            break;
                        }
                    }
                    sprite->x = spriteX;
                    sprite->y = spriteY;
                    sprite->tileReference = lcd->oam[i + 2];
                    sprite->flags = lcd->oam[i + 3];
                    
                    maxSprite = spritesToDraw;
                    forjarr (spritesToDraw) {
                        if (maxSprite->x < spritesToDraw[j].x) {
                            maxSprite = &spritesToDraw [j];
                        }
                    }
                }
            }
        }
        ColorID colorIDOfSpritePixel = ColorID::Color0;
        
        fori (numSpritesToDraw) {
            auto sprite = &spritesToDraw[i];
            u8 upperBound = (sprite->x < SCREEN_WIDTH) ? sprite->x : SCREEN_WIDTH;
            forjrange (sprite->x - MAX_SPRITE_WIDTH, upperBound) {
                u8 currPixelXPositionOnScreen = (u8)j;
                u8 currYPositionOnScreen = lcd->ly;
                
                
                u8 currXPositionInSprite = (sprite->x < MAX_SPRITE_WIDTH) ?
                    currPixelXPositionOnScreen + (MAX_SPRITE_WIDTH - sprite->x) :
                currXPositionInSprite = currPixelXPositionOnScreen - (sprite->x - MAX_SPRITE_WIDTH); 
                u8 currYPositionInSprite = (sprite->y < MAX_SPRITE_HEIGHT) ?
                    currYPositionOnScreen - (sprite->y - MAX_SPRITE_HEIGHT) : 
                currYPositionOnScreen + (MAX_SPRITE_HEIGHT - sprite->y);
                
                
                if (sprite->isXFlipped) {
                    currXPositionInSprite = 7 - currXPositionInSprite;
                }
                
                switch (lcd->spriteHeight) {
                    case SpriteHeight::Short: {
                        if (currYPositionInSprite < SHORT_SPRITE_HEIGHT) {
                            
                            if (sprite->isYFlipped) {
                                currYPositionInSprite = 7 - currYPositionInSprite;
                            }
                            
                            colorIDOfSpritePixel = colorIDFromTileReference(sprite->tileReference,
                                                                            currXPositionInSprite, currYPositionInSprite, lcd->videoRAM);
                            
                        }
                        else {
                            colorIDOfSpritePixel = ColorID::Color0;
                        }
                        
                    } break;
                    
                    case SpriteHeight::Tall: {
                        u8 tileReferenceToGetColorFrom = sprite->tileReference;
                        if ((currYPositionInSprite < TILE_HEIGHT && !sprite->isYFlipped) ||
                            (currYPositionInSprite >= TILE_HEIGHT && sprite->isYFlipped)) {
                            tileReferenceToGetColorFrom &= 0xFE;
                        }
                        else {
                            tileReferenceToGetColorFrom |= 1;
                        }
                        
                        if (sprite->isYFlipped) {
                            currYPositionInSprite = 15 - currYPositionInSprite;
                        }
                        
                        colorIDOfSpritePixel = colorIDFromTileReference(tileReferenceToGetColorFrom,
                                                                        currXPositionInSprite, currYPositionInSprite % TILE_HEIGHT, lcd->videoRAM);
                        
                        
                    } break;
                }
                
                //NOTE: Color0 indicates transparent in this case.  
                if (colorIDOfSpritePixel == ColorID::Color0 ||
                    (sprite->isBelowBackground && scanLineToDraw[currPixelXPositionOnScreen].color != ColorID::Color0)) {
                    continue;
                }
                
                //TODO: have a pointer instead of accessing an array
                scanLineToDraw[currPixelXPositionOnScreen] = 
                {spritePaletteForNumber((SpritePalette)sprite->selectedSpritePalette, lcd), 
                    colorIDOfSpritePixel};
                
            }
            
        } //end sprite loop
    }
    
    foriarr (scanLineToDraw) {
        lcd->backBuffer[(lcd->ly * SCREEN_WIDTH) + i] = scanLineToDraw[i].palette[(int)scanLineToDraw[i].color];
    }
}



static void stepLCD(LCD *lcd, u8 *outRequestedInterrupts, i32 cyclesTakeOfLastInstruction) {
    profileStart("Step LCD", profileState);
    if (lcd->isEnabled) {
        lcd->modeClock += cyclesTakeOfLastInstruction;
        
        switch (lcd->mode) {
            case LCDMode::HBlank:
            if (lcd->modeClock >= HBLANK_DURATION) {
                lcd->modeClock -= HBLANK_DURATION;
                nextScanLine(lcd, outRequestedInterrupts);
                
                if (lcd->ly == SCREEN_HEIGHT) {
                    changeToNewLCDMode(LCDMode::VBlank, lcd, outRequestedInterrupts);
                    
                    if (lcd->numScreensToSkip <= 0) {
                        auto tmpScreen = lcd->screen;
                        lcd->screen = lcd->backBuffer;
                        lcd->backBuffer = tmpScreen;
                    }
                    else {
                        lcd->numScreensToSkip--;
                    }
                    
                }
                else {
                    changeToNewLCDMode(LCDMode::ScanOAM, lcd, outRequestedInterrupts);
                }
                
                
            }
            break;
            
            case LCDMode::VBlank: {
                if (lcd->modeClock >= VBLANK_DURATION) {
                    lcd->modeClock -= VBLANK_DURATION;
                    nextScanLine(lcd, outRequestedInterrupts);
                    if (lcd->ly == 0) {
                        changeToNewLCDMode(LCDMode::ScanOAM, lcd, outRequestedInterrupts);
                    }
                }
                
            } break;
            
            case LCDMode::ScanOAM: {
                //TODO: Draw OAM to internal screen buffer
                
                if (lcd->modeClock >= SCAN_OAM_DURATION) {
                    lcd->modeClock -= SCAN_OAM_DURATION;
                    changeToNewLCDMode(LCDMode::ScanVRAMAndOAM, lcd, outRequestedInterrupts);
                }
                
            } break;
            
            case LCDMode::ScanVRAMAndOAM: {
                if (lcd->modeClock >= SCAN_VRAM_AND_OAM_DURATION) {
                    lcd->modeClock -= SCAN_VRAM_AND_OAM_DURATION;
                    if (lcd->numScreensToSkip <= 0) {
                        drawScanLine(lcd);
                    }
                    changeToNewLCDMode(LCDMode::HBlank, lcd, outRequestedInterrupts);
                }
            } break;
            
        }
    }
    
    profileEnd(profileState);
    
}

void step(CPU *cpu, MMU* mmu, GameBoyDebug *gbDebug, int volume) {
    
    u8 tmpRequestedInterrupts = 0;
    
    if (gbDebug->numBreakpoints > 0 && gbDebug->isEnabled ) {
        
        
        if (gbDebug->isRecordDebugStateEnabled && gbDebug->numBreakpoints > 0) {
            recordDebugState(cpu, mmu, gbDebug);
        }           
        CPU tmpCPU = *cpu;
        stepCPU(cpu, mmu, gbDebug);
        
        //get changed registers 
        foriarr (gbDebug->breakpoints) {
            auto *bp = &gbDebug->breakpoints[i];
            if (!bp->isUsed || bp->isDisabled || bp->type != BreakpointType::Register) {
                continue;
            }
            
            u16 before = 0, after = 0;
            
            if (areStringsEqual(bp->reg, "A", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.A;
                after = cpu->A;
            }
            else if (areStringsEqual(bp->reg, "B", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.B;
                after = cpu->B;
            }
            else if (areStringsEqual(bp->reg, "C", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.C;
                after = cpu->C;
            }
            else if (areStringsEqual(bp->reg, "D", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.D;
                after = cpu->D;
            }
            else if (areStringsEqual(bp->reg, "E", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.E;
                after = cpu->E;
            }
            else if (areStringsEqual(bp->reg, "H", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.H;
                after = cpu->H;
            }
            else if (areStringsEqual(bp->reg, "L", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.L;
                after = cpu->L;
            }
            else if (areStringsEqual(bp->reg, "AF", ARRAY_LEN(bp->reg))) {
                before = word(tmpCPU.A, tmpCPU.F);
                after = word(cpu->A, cpu->F);
            }
            else if (areStringsEqual(bp->reg, "BC", ARRAY_LEN(bp->reg))) {
                before = word(tmpCPU.B, tmpCPU.C);
                after = word(cpu->B, cpu->C);
            }
            else if (areStringsEqual(bp->reg, "DE", ARRAY_LEN(bp->reg))) {
                before = word(tmpCPU.D, tmpCPU.E);
                after = word(cpu->D, cpu->E);
            }
            else if (areStringsEqual(bp->reg, "HL", ARRAY_LEN(bp->reg))) {
                before = word(tmpCPU.H, tmpCPU.L);
                after = word(cpu->H, cpu->L);
            }
            else if (areStringsEqual(bp->reg, "SP", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.SP;
                after = cpu->SP; 
            }
            else if (areStringsEqual(bp->reg, "PC", ARRAY_LEN(bp->reg))) {
                before = tmpCPU.PC;
                after = cpu->PC; 
            }
            
            if (((bp->op == BreakpointOP::Equal && after == bp->expectedValue) ||
                 (bp->op == BreakpointOP::LessThan && after < bp->expectedValue) ||
                 (bp->op == BreakpointOP::GreaterThan && after > bp->expectedValue)) &&
                before != after) {
                
                gbDebug->hitBreakpoint = bp;
                bp->valueBefore = before;
                bp->valueAfter = after;
                break;
            }
            
        }
        
    }
    else {
        stepCPU(cpu, mmu, gbDebug);
    }
    
    profileStart("Step sound", profileState);
    MMU::SquareWave1 *sq1 = &mmu->squareWave1Channel;
    MMU::SquareWave2 *sq2 = &mmu->squareWave2Channel;
    MMU::Wave *wave = &mmu->waveChannel;
    MMU::Noise *noise = &mmu->noiseChannel;
    
    mmu->cyclesSinceLastFrameSequencer += cpu->instructionCycles;
    int tmpCyclesSinceLastFrameSeq = mmu->cyclesSinceLastFrameSequencer;
    
    //sweep
    while (tmpCyclesSinceLastFrameSeq >= FRAME_SEQUENCER_PERIOD) {
        mmu->ticksSinceLastSweep++;
        while (mmu->ticksSinceLastSweep >= SWEEP_TIMER_PERIOD) {
            sq1->sweepClock++; 
            while (sq1->sweepClock >= sq1->sweepPeriod && sq1->sweepPeriod > 0) {
                if (sq1->isEnabled && sq1->isSweepEnabled && sq1->sweepShift != 0) {
                    sq1->sweepShadowReg >>= sq1->sweepShift;
                    sq1->sweepShadowReg = (sq1->isSweepNegated) ?
                        sq1->toneFrequency - sq1->sweepShadowReg :
                    sq1->toneFrequency + sq1->sweepShadowReg;
                    
                    if (sq1->sweepShadowReg <= 2047) {
                        setToneFrequencyForSquareWave(sq1->sweepShadowReg, &sq1->toneFrequency, &sq1->tonePeriod);
                        
                        u16 tmpSweep = sq1->sweepShadowReg >> sq1->sweepShift;
                        tmpSweep = (sq1->isSweepNegated) ?
                            sq1->toneFrequency - tmpSweep :
                        sq1->toneFrequency + tmpSweep;
                        if (tmpSweep > 2047) {
                            sq1->isEnabled = false;
                        }
                    }
                    else {
                        sq1->isEnabled = false;
                    }
                }
                
                sq1->sweepClock -= sq1->sweepPeriod;
            }
            mmu->ticksSinceLastSweep -= SWEEP_TIMER_PERIOD;
        }
        tmpCyclesSinceLastFrameSeq -= FRAME_SEQUENCER_PERIOD;
    }
    
    //duty
    sq1->frequencyClock += cpu->instructionCycles;
    while (sq1->tonePeriod > 0 && sq1->frequencyClock >= sq1->tonePeriod) {
        sq1->positionInWaveForm++;
        if (sq1->positionInWaveForm >= 8) {
            sq1->positionInWaveForm = 0;
        }
        
        sq1->frequencyClock -= sq1->tonePeriod;
    }
    sq2->frequencyClock += cpu->instructionCycles;
    while (sq2->tonePeriod > 0 && sq2->frequencyClock >= sq2->tonePeriod) {
        sq2->positionInWaveForm++;
        if (sq2->positionInWaveForm >= 8) {
            sq2->positionInWaveForm = 0;
        }
        
        sq2->frequencyClock -= sq2->tonePeriod;
    }
    wave->frequencyClock += cpu->instructionCycles;
    while (wave->tonePeriod > 0 && wave->frequencyClock >= wave->tonePeriod) {
        wave->currentSampleIndex++;
        if (wave->currentSampleIndex >= ARRAY_LEN(wave->waveTable) * 2) {
            wave->currentSampleIndex = 0;
        }
        wave->currentSample = wave->waveTable[wave->currentSampleIndex/2];
        wave->currentSample = ((wave->currentSampleIndex & 1) == 0) ? 
            (wave->currentSample >> 4) & 0xF :
        wave->currentSample & 0xF;
        
        
        wave->frequencyClock -= wave->tonePeriod;
    }
    noise->frequencyClock += cpu->instructionCycles;
    while (noise->tonePeriod > 0 && noise->frequencyClock >= noise->tonePeriod) {
        noise->shiftValue >>= 1;
        u8 bit = (noise->shiftValue & 1) ^ ((noise->shiftValue >> 1) & 1);
        if (noise->is7BitMode) {
            noise->shiftValue |= bit << 7;
        }
        else {
            noise->shiftValue |= bit << 15;
        }
        
        noise->frequencyClock -= noise->tonePeriod;
    }
    
    
    //frame sequencer
    while (mmu->cyclesSinceLastFrameSequencer >= FRAME_SEQUENCER_PERIOD) {
        //length counter
        mmu->ticksSinceLastLengthCounter++;
        while (mmu->ticksSinceLastLengthCounter >= LENGTH_TIMER_PERIOD) {
            if (sq1->lengthCounter > 0 && sq1->isLengthCounterEnabled) {
                sq1->lengthCounter--;
                
                if (sq1->lengthCounter <= 0) {
                    sq1->isEnabled = false;
                }
            }
            if (sq2->lengthCounter > 0 && sq2->isLengthCounterEnabled) {
                sq2->lengthCounter--;
                
                if (sq2->lengthCounter <= 0) {
                    sq2->isEnabled = false;
                }
            }
            if (wave->lengthCounter > 0 && wave->isLengthCounterEnabled) {
                wave->lengthCounter--;
                
                if (wave->lengthCounter <= 0) {
                    wave->isEnabled = false;
                }
            }
            if (noise->lengthCounter > 0 && noise->isLengthCounterEnabled) {
                noise->lengthCounter--;
                
                if (noise->lengthCounter <= 0) {
                    noise->isEnabled = false;
                }
            }
            mmu->ticksSinceLastLengthCounter -= LENGTH_TIMER_PERIOD;
        }
        
        //envelope
        mmu->ticksSinceLastEnvelop++;
        while (mmu->ticksSinceLastEnvelop >= VOLUME_ENVELOPE_TIMER_PERIOD ) {
            
            if (sq1->volumeEnvelopPeriod > 0) {
                
                sq1->volumeEnvelopeClock++;
                while (sq1->volumeEnvelopeClock >= sq1->volumeEnvelopPeriod)  {
                    if (sq1->shouldIncreaseVolume && sq1->currentVolume < 15) {
                        sq1->currentVolume++;
                    }
                    else if (sq1->currentVolume > 0) {
                        sq1->currentVolume--;
                    }
                    sq1->volumeEnvelopeClock -= sq1->volumeEnvelopPeriod;
                }
            }
            if (sq2->volumeEnvelopPeriod > 0) {
                
                sq2->volumeEnvelopeClock++;
                while (sq2->volumeEnvelopeClock >= sq2->volumeEnvelopPeriod)  {
                    if (sq2->shouldIncreaseVolume && sq2->currentVolume < 15) {
                        sq2->currentVolume++;
                    }
                    else if (sq2->currentVolume > 0) {
                        sq2->currentVolume--;
                    }
                    sq2->volumeEnvelopeClock -= sq2->volumeEnvelopPeriod;
                }
            }
            if (noise->volumeEnvelopPeriod > 0) {
                
                noise->volumeEnvelopeClock++;
                while (noise->volumeEnvelopeClock >= noise->volumeEnvelopPeriod)  {
                    if (noise->shouldIncreaseVolume && noise->currentVolume < 15) {
                        noise->currentVolume++;
                    }
                    else if (noise->currentVolume > 0) {
                        noise->currentVolume--;
                    }
                    noise->volumeEnvelopeClock -= noise->volumeEnvelopPeriod;
                }
            }
            
            mmu->ticksSinceLastEnvelop -= VOLUME_ENVELOPE_TIMER_PERIOD;
        }
        
        mmu->cyclesSinceLastFrameSequencer -= FRAME_SEQUENCER_PERIOD;
    }
    
    SoundFrame frame;
    
    
    if (mmu->isSoundEnabled) {
        i16 left = 0, right = 0;
        i16 voltage = 0;
        
        if (sq1->isDACEnabled && !gbDebug->shouldDisableSQ1) {
            if (sq1->isEnabled) {
                if (isBitSet(sq1->positionInWaveForm, (u8)sq1->waveForm)) {
                    voltage = sq1->currentVolume;
                }
                else {
                    voltage = -sq1->currentVolume;
                }
            }
            else {
                voltage = 0;
            }
            
            if (((int)sq1->channelEnabledState & (int)ChannelEnabledState::Left) != 0) {
                left += voltage;
            }
            
            if (((int)sq1->channelEnabledState & (int)ChannelEnabledState::Right) != 0) {
                right += voltage;
            }
        }
        if (sq2->isDACEnabled && !gbDebug->shouldDisableSQ2) {
            if (sq2->isEnabled) {
                if (isBitSet(sq2->positionInWaveForm, (u8)sq2->waveForm)) {
                    voltage = sq2->currentVolume;
                }
                else {
                    voltage = -sq2->currentVolume;
                }
            }
            else {
                voltage = 0;
            }
            
            if (((int)sq2->channelEnabledState & (int)ChannelEnabledState::Left) != 0) {
                left += voltage;
            }
            
            if (((int)sq2->channelEnabledState & (int)ChannelEnabledState::Right) != 0) {
                right += voltage;
            }
        }
        
        if (wave->isDACEnabled && !gbDebug->shouldDisableWave) {
            if (wave->isEnabled) {
                voltage = (i16)wave->currentSample - 8;
                switch (wave->volumeShift) {
					case WaveVolumeShift::WV_0: {
						voltage = 0;
					} break;
					case WaveVolumeShift::WV_25: {
						voltage /= 4;
					} break;
					case WaveVolumeShift::WV_50: {
						voltage /= 2;
					} break;
					case WaveVolumeShift::WV_100: {
						//do nothing 
					} break;
                }
            }
            else {
                voltage = 0;
            }
            
            
            if (((int)wave->channelEnabledState & (int)ChannelEnabledState::Left) != 0) {
                left += voltage;
            }
            
            if (((int)wave->channelEnabledState & (int)ChannelEnabledState::Right) != 0) {
                right += voltage;
            }
        }
        
        if (noise->isDACEnabled && !gbDebug->shouldDisableNoise) {
            if (noise->isEnabled) {
                if (!isBitSet(0, noise->shiftValue)) {
                    voltage = noise->currentVolume;
                }
                else {
                    voltage = -noise->currentVolume;
                }
            }
            else {
                voltage = 0;
            }
            
            if (((int)noise->channelEnabledState & (int)ChannelEnabledState::Left) != 0) {
                left += voltage;
            }
            
            if (((int)noise->channelEnabledState & (int)ChannelEnabledState::Right) != 0) {
                right += voltage;
            }
        }
        
        frame.leftChannel = (i16)((left * mmu->masterLeftVolume * volume));
        frame.rightChannel = (i16)((right * mmu->masterRightVolume * volume));
        
    }
    else {
        frame.leftChannel = 0;
        frame.rightChannel = 0;
    }
    
    //TODO: would like to move to platform layer, but not sure if i can
    mmu->cyclesSinceLastSoundSample += cpu->instructionCycles;
    while (mmu->cyclesSinceLastSoundSample >= CLOCK_SPEED_HZ/44100) {
        
        push(frame, &mmu->soundFramesBuffer);
        
        mmu->cyclesSinceLastSoundSample -= CLOCK_SPEED_HZ/44100;
    }
    profileEnd(profileState);
    stepLCD(&mmu->lcd, &tmpRequestedInterrupts, cpu->instructionCycles);
    
    //DMA
    if (mmu->isDMAOccurring) {
#define DMA_CYCLES_PER_BYTE 4
        mmu->cyclesSinceLastDMACopy += cpu->instructionCycles;
        
        while (mmu->cyclesSinceLastDMACopy >= DMA_CYCLES_PER_BYTE) {
            if ((mmu->currentDMAAddress & 0xFF) < 0xA0) {
                u16 destAddr = 0xFE00 + (mmu->currentDMAAddress & 0xFF);
                u8 srcByte = readByte(mmu->currentDMAAddress, mmu);
                
                writeByte(srcByte, destAddr, mmu, gbDebug);
                
                mmu->cyclesSinceLastDMACopy -= DMA_CYCLES_PER_BYTE;
                mmu->currentDMAAddress++;
            }
            else {
                mmu->isDMAOccurring = false;
                mmu->cyclesSinceLastDMACopy = 0;
                mmu->currentDMAAddress = 0;
            }
        }
#undef DMA_CYCLES_PER_BYTE 
    }
    
    //divider and timer
    {
#define DIVIDER_CYCLES_PER_INCREMENT 256
        mmu->cyclesSinceDividerIncrement += cpu->instructionCycles;
        
        while (mmu->cyclesSinceDividerIncrement >= DIVIDER_CYCLES_PER_INCREMENT) {
            mmu->divider++;
            mmu->cyclesSinceDividerIncrement -= DIVIDER_CYCLES_PER_INCREMENT;
        }
        
        if (mmu->isTimerEnabled) {
            mmu->cyclesSinceTimerIncrement += cpu->instructionCycles;
            while (mmu->cyclesSinceTimerIncrement >= (int)mmu->timerIncrementRate) {
                mmu->timer++;
                
                if (mmu->timer == 0) { //timer overFlowed
                    mmu->timer = mmu->timerModulo;
                    setBit((int)InterruptRequestedBit::TimerRequested, &mmu->requestedInterrupts);
                }
                
                mmu->cyclesSinceTimerIncrement -= (int)mmu->timerIncrementRate;
            }
        }
        
#undef DIVIDER_CYCLES_PER_INCREMENT
    }
    
    mmu->requestedInterrupts |= tmpRequestedInterrupts;
    
    if (cpu->didHitIllegalOpcode || gbDebug->hitBreakpoint) {
        return;
    }
    
    Breakpoint *bp;
    if (shouldBreakOnPC(cpu->PC, gbDebug, &bp)) {
        gbDebug->hitBreakpoint = bp;
        return;
    }
    
}

#ifdef CO_DEBUG
extern "C"
#endif
void reset(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState) {
    UNUSED(programState);
#ifdef CO_PROFILE
    profileState = &programState->profileState;
#endif
    bool wasPaused = cpu->isPaused;
    gbDebug->hitBreakpoint = nullptr;
    *cpu = {};
    cpu->isPaused = wasPaused;
    
    u8 *tmpROM = mmu->romData;
    u8 *tmpRAM = mmu->cartRAM;
    char *tmpROMName = mmu->romName;
    i64 tmpROMNameLen = mmu->romNameLen;
    MBCType tmpMBC = mmu->mbcType;
    bool tmpHasRAM = mmu->hasRAM;
    bool tmpHasBattery = mmu->hasBattery;
    bool tmpHasRTC = mmu->hasRTC;
    RTC tmpRTC = mmu->rtc;
    
    i64 tmpROMSize = mmu->romSize;
    PaletteColor *tmpScreen = mmu->lcd.screen;
    PaletteColor *tmpBackBuffer = mmu->lcd.backBuffer;
    SoundFrame *tmpSq1 = mmu->soundFramesBuffer.data;
    i64 tmpSq1Len = mmu->soundFramesBuffer.len;
    i64 cartRAMSize = mmu->cartRAMSize;
    
    CartRAMPlatformState tmpRAMPlatformState = mmu->cartRAMPlatformState;
    if (!tmpHasBattery) {
        zeroMemory(tmpRAM, mmu->cartRAMSize);
    }
    
    zeroMemory(tmpBackBuffer, SCREEN_WIDTH * SCREEN_HEIGHT);
    zeroMemory(tmpScreen, SCREEN_WIDTH * SCREEN_HEIGHT);
    zeroMemory(tmpSq1, mmu->soundFramesBuffer.len);
    
    *mmu = {};
    mmu->cartRAM = tmpRAM;
    mmu->romName = tmpROMName;
    mmu->romNameLen = tmpROMNameLen;
    mmu->mbcType = tmpMBC;
    mmu->hasRAM = tmpHasRAM;
    mmu->hasBattery = tmpHasBattery;
    mmu->cartRAMSize = cartRAMSize;
    mmu->maxCartRAMBank = calculateMaxBank(cartRAMSize);
    mmu->hasRTC = tmpHasRTC;
    mmu->rtc = tmpRTC;
    mmu->romData = tmpROM;
    mmu->romSize = tmpROMSize;
    mmu->maxROMBank = calculateMaxBank(tmpROMSize);
    mmu->currentROMBank = 1;
    mmu->lcd.screen = tmpScreen;
    mmu->lcd.backBuffer = tmpBackBuffer;
    
    if (tmpHasBattery) {
        mmu->cartRAMPlatformState = tmpRAMPlatformState;
    }
    
    mmu->joyPad.selectedButtonGroup = JPButtonGroup::Nothing;
    
    mmu->joyPad.a = JPButtonState::Up;
    mmu->joyPad.b = JPButtonState::Up;
    mmu->joyPad.start = JPButtonState::Up;
    mmu->joyPad.select = JPButtonState::Up;
    
    mmu->joyPad.up = JPButtonState::Up;
    mmu->joyPad.down = JPButtonState::Up;
    mmu->joyPad.left = JPButtonState::Up;
    mmu->joyPad.right = JPButtonState::Up;
    
    mmu->timerIncrementRate = TimerIncrementRate::TIR_0;
    
    mmu->soundFramesBuffer.data = tmpSq1;
    mmu->soundFramesBuffer.len = tmpSq1Len;
    
    mmu->noiseChannel.shiftValue = 1;
    
    mmu->lcd.stat = 0x82;
    mmu->lcd.mode = LCDMode::ScanOAM;
    
    gbDebug->nextFreeGBStateIndex = 0;
    
    //            while (mmu->inBios) {
    //                step(cpu, mmu, gbDebug, programState->soundState.volume);
    //            } 
    //initial state
    cpu->A = 1;
    cpu->B = 0;
    cpu->C = 13;
    cpu->D = 0;
    cpu->E = 0xD8;
    cpu->F = 0xB0;
    cpu->H = 1;
    cpu->L = 0x4D;
    cpu->PC = 0x100;
    cpu->SP = 0xFFFE;
    mmu->divider = 207;
    mmu->lcd.isEnabled = true;
    mmu->lcd.modeClock = 116;
    mmu->lcd.mode = LCDMode::VBlank;
    mmu->lcd.ly = 153;
    mmu->lcd.backgroundPalette[0] = PaletteColor::Black;
    mmu->lcd.backgroundPalette[1] = PaletteColor::White;
    mmu->lcd.backgroundPalette[2] = PaletteColor::White;
    mmu->lcd.backgroundPalette[3] = PaletteColor::White;
    mmu->lcd.backgroundTileSet = 1;
    mmu->lcd.spriteHeight = SpriteHeight::Short;
    clear(&mmu->soundFramesBuffer);
    
    recordState(cpu, mmu, gbDebug);
    
}


#ifdef CO_DEBUG
extern "C"
#endif
void runFrame(CPU *cpu, MMU *mmu, GameBoyDebug *gbDebug, ProgramState *programState, TimeUS dt) {
    Input *input = &programState->input;
    SoundState *soundState = &programState->soundState;
    NotificationState *notifications = &programState->notifications;
    
#ifdef CO_PROFILE
    profileState = &programState->profileState;
#endif
    
    
    
    /************************
     * Process input
     ************************/
    if (!gbDebug->isTypingInTextBox) {
#define WAS_PRESSED(button) (input->newState.button == true && input->oldState.button == false)
#define IS_DOWN(button) (input->newState.button)
        if (cpu->isPaused) {
            if (WAS_PRESSED(Step)) {
                step(cpu, mmu, gbDebug, programState->soundState.volume);
            }
        }
        
        if (WAS_PRESSED(Reset)) {
            reset(cpu, mmu, gbDebug, programState);
        }
        if (WAS_PRESSED(ShowHomePath)) {
            NOTIFY(notifications, "GBEmu Home Directory: %s", programState->homeDirectoryPath);
        }
        if (WAS_PRESSED(Mute)) {
            soundState->isMuted = !soundState->isMuted;
            programState->shouldUpdateTitleBar = true;
        }
        if (WAS_PRESSED(Pause)) {
            setPausedState(!cpu->isPaused, programState, cpu);
            if (!cpu->isPaused && mmu->hasRTC) {
                syncRTCTime(&mmu->rtc, mmu->cartRAMPlatformState.rtcFileMap);
            }
        }
        if (WAS_PRESSED(Continue)) {
            continueFromBreakPoint(gbDebug, mmu, cpu, programState); 
        }
        if (WAS_PRESSED(Rewind)) {
            if (!rewindState(cpu, mmu, gbDebug)) {
                NOTIFY(notifications, "Nothing left to rewind!");
            }
            else {
                NOTIFY(notifications, "Rewind");
            }
        }
        if (WAS_PRESSED(saveState)) {
            int slot = input->newState.slotToRestoreOrSave;
            switch (saveSaveStateToFile(cpu, mmu, programState, slot)) {
                case FileSystemResultCode::OK: {
                    NOTIFY(notifications, "Saved state to slot %d!", slot);  
                } break;
                case FileSystemResultCode::OutOfSpace: {
                    NOTIFY(notifications, "Could not save state to slot %d. Disk full!", slot);  
                } break;
                case FileSystemResultCode::PermissionDenied: {
                    NOTIFY(notifications, "Could not save state to slot %d. Permission denied!", slot);  
                } break;
                case FileSystemResultCode::IOError: {
                    NOTIFY(notifications, "Could not save state to slot %d. IO error!", slot);  
                } break;
                default: {
                    NOTIFY(notifications, "Could not save state to slot %d.", slot);  
                } break;
            }
        }
        if (WAS_PRESSED(restoreState)) {
            int slot = input->newState.slotToRestoreOrSave;
            auto result = restoreSaveStateFromFile(cpu, mmu, programState->loadedROMName, slot);
            switch (result) {
                case RestoreSaveResult::Success: {
                    NOTIFY(notifications, "Restored save state from slot %d!", slot);  
                } break;
                case RestoreSaveResult::NothingInSlot:  {
                    NOTIFY(notifications, "No save state in slot %d!", slot);  
                } break;
                case RestoreSaveResult::Error: {
                    NOTIFY(notifications, "Could not restore save state from slot %d!", slot);  
                } break;
            }
        }
        
        mmu->joyPad.a = IS_DOWN(A) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.b = IS_DOWN(B) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.start = IS_DOWN(Start) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.select = IS_DOWN(Select) ? JPButtonState::Down : JPButtonState::Up;
        
        mmu->joyPad.up = IS_DOWN(Up) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.down = IS_DOWN(Down) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.left = IS_DOWN(Left) ? JPButtonState::Down : JPButtonState::Up;
        mmu->joyPad.right = IS_DOWN(Right) ? JPButtonState::Down : JPButtonState::Up;
        
    }
    
    /************************
     * Step
     ************************/
    profileStart("Step loop", profileState);
    cpu->cylesExecutedThisFrame = 0;
    if (!cpu->isPaused){
        i32 cyclesToExecute = ((i32)((CLOCK_SPEED_HZ * dt) / 1000000) + cpu->leftOverCyclesFromPreviousFrame);
        
        //cycles are in multiples of 4
        cpu->leftOverCyclesFromPreviousFrame = cyclesToExecute - (cyclesToExecute & ~3);
        cyclesToExecute &= ~3;
        
        LCD *lcd = &mmu->lcd; 
        i32 cyclesLeftForThisScanLine;
        switch (lcd->mode) {
            case LCDMode::VBlank: cyclesLeftForThisScanLine = VBLANK_DURATION - lcd->modeClock; break;
            case LCDMode::HBlank: cyclesLeftForThisScanLine = (HBLANK_DURATION - lcd->modeClock) + SCAN_OAM_DURATION + SCAN_VRAM_AND_OAM_DURATION; break;
            case LCDMode::ScanOAM: cyclesLeftForThisScanLine = (SCAN_OAM_DURATION - lcd->modeClock) + SCAN_VRAM_AND_OAM_DURATION; break;
            case LCDMode::ScanVRAMAndOAM: cyclesLeftForThisScanLine = SCAN_VRAM_AND_OAM_DURATION - lcd->modeClock; break;
        }
        i32 cyclesLeftForThisFrame = cyclesLeftForThisScanLine + (MAX_LY - lcd->ly) * TOTAL_SCANLINE_DURATION; 
        lcd->numScreensToSkip = (cyclesToExecute - cyclesLeftForThisFrame) / (TOTAL_SCANLINE_DURATION * (MAX_LY+1));
        
        while (cyclesToExecute > 0) {
            step(cpu, mmu, gbDebug, programState->soundState.volume);
            cpu->cylesExecutedThisFrame += cpu->instructionCycles;
            cyclesToExecute -= cpu->instructionCycles;
            
            
            if (cpu->didHitIllegalOpcode || gbDebug->hitBreakpoint) {
                if (gbDebug->hitBreakpoint) {
                    recordState(cpu, mmu, gbDebug);
                }
                setPausedState(true, programState, cpu);
                if (cpu->didHitIllegalOpcode) {
                    NOTIFY(notifications, "Illegal opcode hit. Emulation paused.");
                }
                break;
            }
            
        }
        if (mmu->hasRTC) {
            profileStart("RTC tick", profileState);
            syncRTCTime(&mmu->rtc, mmu->cartRAMPlatformState.rtcFileMap);
            profileEnd(profileState);
        }
    }
    profileEnd(profileState);
    
    
    
    /**********
     * Record
     **********/
    if (!cpu->isPaused) {
        gbDebug->elapsedTimeSinceLastRecord += dt;
        if (gbDebug->elapsedTimeSinceLastRecord >= NUM_US_BETWEEN_RECORDS) {
            recordState(cpu, mmu, gbDebug);
            gbDebug->elapsedTimeSinceLastRecord = 0;
        }
    }        
    if (gbDebug->isEnabled) {
        drawDebugger(gbDebug, mmu, cpu, programState, dt);
    }
}
