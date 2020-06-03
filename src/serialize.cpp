//Copyright (C) 2018 Daniel Bokser.  See LICENSE.txt for license
//Derived from code from https://yave.handmade.network/blogs/p/2723-how_media_molecule_does_serialization

#include <errno.h>
#include "gbemu.h"
enum class SaveStateVersion : i32 {
    Initial = 1,
    CycleAccurateUpdate = 2,
    
    //Don't delete this
    CurrentPlusOne
};
struct SerializingState {
    bool isWriting;
    SaveStateVersion version;
    FILE *f;
};
#define ADD(data, v) if (state->version >= (v)) { \
        auto res = serialize(&(data), state); \
        if (res != FileSystemResultCode::OK) {\
            CO_ERR("Failed to write " #data);\
            return res;\
        }\
    }
#define REM(_fieldAdded, _fieldRemoved, _type, _fieldName, _defaultValue) \
    _type _fieldName = (_defaultValue); \
    if (state->version >= (_fieldAdded) && state->version < (_fieldRemoved)){\
        auto res = serialize(&(_fieldName), state); \
        if (res != FileSystemResultCode::OK) {\
            CO_ERR("Failed to write " #_fieldName);\
            return res;\
        }\
    }
#define ADD_IGNORE_READ(data, v) if (state->version >= (v)) { \
        auto res = serialize(&(data), state, true); \
        if (res != FileSystemResultCode::OK) {\
            CO_ERR("Failed to write " #data);\
            return res;\
        }\
    }
#define ADD_N(data, n, v) if (state->version >= (v)) { \
        auto res = serialize(data, n, state); \
        if (res != FileSystemResultCode::OK) {\
            CO_ERR("Failed to write " #data);\
            return res;\
        }\
    }

//this is for fields that shouldn't change when deserializing
#define ADD_CHECK_SAME(data, v) do {\
    auto tmp = data;\
    ADD(data, v);\
    if (tmp != data) {\
        CO_ERR(#data " is not the same!");\
        return FileSystemResultCode::Unknown; } } while (0)

#define ADD_ARR(data, v) ADD_N(data, ARRAY_LEN(data), v)
    FileSystemResultCode checkResult(usize numItems, SerializingState *state) {
       if (numItems == 1) {
           return FileSystemResultCode::OK;
       }
       else if (ferror(state->f)) {
           switch (errno) {
           case ENOSPC:
               return FileSystemResultCode::OutOfSpace;
           case EACCES:
               return FileSystemResultCode::PermissionDenied; 
           case EIO:
               return FileSystemResultCode::IOError;
           default:
               return FileSystemResultCode::Unknown;
           }
       }
       else {
           return FileSystemResultCode::Unknown; 
       }
        
    }
    FileSystemResultCode checkWriteResult(usize numItems, SerializingState *state) {
       if (numItems == 1) {
           return FileSystemResultCode::OK;
       }
       else if (ferror(state->f)) {
           switch (errno) {
           case ENOSPC:
               return FileSystemResultCode::OutOfSpace;
           case EACCES:
               return FileSystemResultCode::PermissionDenied; 
           case EIO:
               return FileSystemResultCode::IOError;
           default:
               return FileSystemResultCode::Unknown;
           }
       }
       else {
           return FileSystemResultCode::Unknown; 
       }
        
    }
    template <typename T>
    FileSystemResultCode serialize(T *data, SerializingState *state, bool ignore = false) {
       usize numItems;
       if (state->isWriting) {
          numItems = fwrite(data, sizeof(*data), 1, state->f); 
       }
       else {
           if (ignore) {
               T throwAway;
               numItems = fread(&throwAway, sizeof(throwAway), 1, state->f);
           }
           else {
               numItems = fread(data, sizeof(*data), 1, state->f);
           }
       }
       return checkResult(numItems, state);
    }
    
    template <typename T> 
    FileSystemResultCode serialize(T *data, i64 len, SerializingState *state) {
       usize numItems;
       if (state->isWriting) {
          numItems = fwrite(data, (usize)len * sizeof(*data), 1, state->f); 
       }
       else {
           numItems = fread(data, (usize)len * sizeof(*data), 1, state->f);
       }
       CO_ASSERT(numItems == 1);
       return checkResult(numItems, state);
    }
    
    FileSystemResultCode serialize(SoundBuffer *data, SerializingState *state) {
        ADD_CHECK_SAME(data->len, SaveStateVersion::Initial);
        ADD(data->numItemsQueued, SaveStateVersion::Initial);
        ADD(data->readIndex, SaveStateVersion::Initial);
        ADD(data->writeIndex, SaveStateVersion::Initial);
        ADD_N(data->data, data->len, SaveStateVersion::Initial);
        
        return FileSystemResultCode::OK;
    }
   
    
    FileSystemResultCode serialize(RTC *data, SerializingState *state) {
        ADD_IGNORE_READ(data->latchState, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->isStopped, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->didOverflow, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->seconds, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->minutes, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->hours, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->days, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->daysHigh, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->wallClockTime, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->latchedSeconds, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->latchedMinutes, SaveStateVersion::Initial); 
        ADD_IGNORE_READ(data->latchedHours, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->latchedDays, SaveStateVersion::Initial);
        ADD_IGNORE_READ(data->latchedMisc, SaveStateVersion::Initial);
        
                
        return FileSystemResultCode::OK;
    }
    FileSystemResultCode serialize(JoyPad *data, SerializingState *state) {
        
        ADD(data->start, SaveStateVersion::Initial);
        ADD(data->select, SaveStateVersion::Initial);
        ADD(data->b, SaveStateVersion::Initial);
        ADD(data->a, SaveStateVersion::Initial);

        ADD(data->down, SaveStateVersion::Initial);
        ADD(data->up, SaveStateVersion::Initial);
        ADD(data->left, SaveStateVersion::Initial);
        ADD(data->right, SaveStateVersion::Initial);

        ADD(data->selectedButtonGroup, SaveStateVersion::Initial);
        return FileSystemResultCode::OK; 
    }
    FileSystemResultCode serialize(LCD *data, SerializingState *state) {
        ADD_ARR(data->backgroundPalette, SaveStateVersion::Initial);
        ADD_ARR(data->spritePalette0, SaveStateVersion::Initial);
        ADD_ARR(data->spritePalette1, SaveStateVersion::Initial);
        ADD_ARR(data->videoRAM, SaveStateVersion::Initial);
        ADD_ARR(data->oam, SaveStateVersion::Initial);
        ADD(data->mode, SaveStateVersion::Initial);
        ADD(data->modeClock, SaveStateVersion::Initial);
        ADD(data->spriteHeight, SaveStateVersion::Initial);
        
        ADD(data->isBackgroundEnabled, SaveStateVersion::Initial);
        ADD(data->isWindowEnabled, SaveStateVersion::Initial);
        ADD(data->isEnabled, SaveStateVersion::Initial);
        ADD(data->isOAMEnabled, SaveStateVersion::Initial);
        
        
        ADD(data->scx, SaveStateVersion::Initial); 
        ADD(data->scy, SaveStateVersion::Initial); 
        ADD(data->wx, SaveStateVersion::Initial);
        ADD(data->wy, SaveStateVersion::Initial); 
        ADD(data->ly, SaveStateVersion::Initial);
        ADD(data->stat, SaveStateVersion::Initial);
        ADD(data->lyc, SaveStateVersion::Initial); 
        ADD(data->backgroundTileMap, SaveStateVersion::Initial); 
        ADD(data->backgroundTileSet, SaveStateVersion::Initial);  
        ADD(data->windowTileMap, SaveStateVersion::Initial);
        
        ADD(data->numScreensToSkip, SaveStateVersion::Initial);
        
        if (state->version >= SaveStateVersion::Initial) {  
            bool isSwapped;
            if (state->isWriting) {
                isSwapped = data->screen == data->screenStorage;
                auto res = serialize(&isSwapped, state);
                if (res != FileSystemResultCode::OK) {
                    CO_ERR("Failed to write isSwapped");
                    return res;
                }
            }
            else {
                auto res = serialize(&isSwapped, state);
                if (res != FileSystemResultCode::OK) {
                    CO_ERR("Failed to read isSwapped");
                    return res;
                }
                if (isSwapped) {
                    data->screen = data->screenStorage;
                    data->backBuffer = data->backBufferStorage;
                }
                else {
                    data->screen = data->backBufferStorage;
                    data->backBuffer = data->screenStorage;
                }
            }
        }
        
        ADD_ARR(data->screenStorage, SaveStateVersion::Initial);
        ADD_ARR(data->backBufferStorage, SaveStateVersion::Initial);
        
        return FileSystemResultCode::OK;
    }
    
    FileSystemResultCode serialize(MMU::SquareWave1 *data, SerializingState *state) {
        ADD(data->sweepPeriod, SaveStateVersion::Initial);
        ADD(data->sweepShift, SaveStateVersion::Initial);
        ADD(data->isSweepNegated, SaveStateVersion::Initial);

        ADD(data->waveForm, SaveStateVersion::Initial);
        ADD(data->positionInWaveForm, SaveStateVersion::Initial);
        ADD(data->lengthCounter, SaveStateVersion::Initial);
        ADD(data->isLengthCounterEnabled, SaveStateVersion::Initial);

        ADD(data->startingVolume, SaveStateVersion::Initial);
        ADD(data->currentVolume, SaveStateVersion::Initial);
        ADD(data->shouldIncreaseVolume, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopPeriod, SaveStateVersion::Initial);
        
        ADD(data->toneFrequency, SaveStateVersion::Initial);
        ADD(data->tonePeriod, SaveStateVersion::Initial);
        ADD(data->isEnabled, SaveStateVersion::Initial);
        ADD(data->isDACEnabled, SaveStateVersion::Initial);

        ADD(data->channelEnabledState, SaveStateVersion::Initial);

        ADD(data->frequencyClock, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopeClock, SaveStateVersion::Initial);
        ADD(data->sweepClock, SaveStateVersion::Initial);
        ADD(data->sweepShadowReg, SaveStateVersion::Initial);
        ADD(data->isSweepEnabled, SaveStateVersion::Initial);
        
        return FileSystemResultCode::OK;
    }
    
    
    FileSystemResultCode serialize(MMU::SquareWave2 *data, SerializingState *state) {

        ADD(data->waveForm, SaveStateVersion::Initial);
        ADD(data->positionInWaveForm, SaveStateVersion::Initial);
        ADD(data->lengthCounter, SaveStateVersion::Initial);
        ADD(data->isLengthCounterEnabled, SaveStateVersion::Initial);

        ADD(data->startingVolume, SaveStateVersion::Initial);
        ADD(data->currentVolume, SaveStateVersion::Initial);
        ADD(data->shouldIncreaseVolume, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopPeriod, SaveStateVersion::Initial);
        
        ADD(data->toneFrequency, SaveStateVersion::Initial);
        ADD(data->tonePeriod, SaveStateVersion::Initial);
        ADD(data->isEnabled, SaveStateVersion::Initial);
        ADD(data->isDACEnabled, SaveStateVersion::Initial);

        ADD(data->channelEnabledState, SaveStateVersion::Initial);

        ADD(data->frequencyClock, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopeClock, SaveStateVersion::Initial);
        
        return FileSystemResultCode::OK;
    }
    
    FileSystemResultCode serialize(MMU::Wave *data, SerializingState *state) {
        ADD(data->lengthCounter, SaveStateVersion::Initial);
        ADD(data->isLengthCounterEnabled, SaveStateVersion::Initial);
        
        ADD(data->toneFrequency, SaveStateVersion::Initial);
        ADD(data->tonePeriod, SaveStateVersion::Initial);
        
        ADD(data->isEnabled, SaveStateVersion::Initial);
        ADD(data->isDACEnabled, SaveStateVersion::Initial);
        
        ADD(data->currentSample, SaveStateVersion::Initial);
        ADD(data->currentSampleIndex, SaveStateVersion::Initial);
        
        ADD(data->volumeShift, SaveStateVersion::Initial);
        
        ADD(data->frequencyClock, SaveStateVersion::Initial);
        
        ADD_ARR(data->waveTable, SaveStateVersion::Initial);
        
        ADD(data->channelEnabledState, SaveStateVersion::Initial);
        
        return FileSystemResultCode::OK;
    }
    
    FileSystemResultCode serialize(MMU::Noise *data, SerializingState *state) {
        ADD(data->lengthCounter, SaveStateVersion::Initial);
        ADD(data->isLengthCounterEnabled, SaveStateVersion::Initial);
        
        ADD(data->startingVolume, SaveStateVersion::Initial);
        ADD(data->currentVolume, SaveStateVersion::Initial);
        ADD(data->shouldIncreaseVolume, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopPeriod, SaveStateVersion::Initial);
        ADD(data->volumeEnvelopeClock, SaveStateVersion::Initial);
        
        ADD(data->isEnabled, SaveStateVersion::Initial);
        ADD(data->isDACEnabled, SaveStateVersion::Initial);
        
        ADD(data->divisorCode, SaveStateVersion::Initial);
        ADD(data->tonePeriod, SaveStateVersion::Initial);
        ADD(data->shiftValue, SaveStateVersion::Initial);
        ADD(data->is7BitMode, SaveStateVersion::Initial);
        ADD(data->frequencyClock, SaveStateVersion::Initial);
        
        ADD(data->channelEnabledState, SaveStateVersion::Initial);
        return FileSystemResultCode::OK;
    }
    FileSystemResultCode serialize(MMU *data, SerializingState *state) {
       ADD(data->soundFramesBuffer, SaveStateVersion::Initial); 
       ADD_ARR(data->workingRAM, SaveStateVersion::Initial);
       ADD_ARR(data->zeroPageRAM, SaveStateVersion::Initial);
       ADD(data->lcd, SaveStateVersion::Initial); 
       ADD(data->joyPad, SaveStateVersion::Initial); 
       
       ADD_CHECK_SAME(data->romSize, SaveStateVersion::Initial);
       ADD_CHECK_SAME(data->romNameLen, SaveStateVersion::Initial);
       ADD_CHECK_SAME(data->mbcType, SaveStateVersion::Initial);
       
       ADD_CHECK_SAME(data->hasRAM, SaveStateVersion::Initial);
       ADD_CHECK_SAME(data->hasBattery, SaveStateVersion::Initial);
       if (data->hasRAM) {
           ADD_N(data->cartRAM, data->cartRAMSize, SaveStateVersion::Initial);
           ADD_CHECK_SAME(data->cartRAMSize, SaveStateVersion::Initial);
       }
       
       //skip cart ram platform state
       
       ADD(data->currentROMBank, SaveStateVersion::Initial);
       ADD(data->currentRAMBank, SaveStateVersion::Initial);
       ADD(data->isCartRAMEnabled, SaveStateVersion::Initial);
       

       ADD(data->requestedInterrupts, SaveStateVersion::Initial);
       ADD(data->enabledInterrupts, SaveStateVersion::Initial);
       ADD(data->bankingMode, SaveStateVersion::Initial);
       ADD(data->hasRTC, SaveStateVersion::Initial);
       if (data->hasRTC) {
           ADD(data->rtc, SaveStateVersion::Initial);
           if (!state->isWriting) {
               syncRTCTime(&data->rtc, data->cartRAMPlatformState.rtcFileMap);
           }
       }


       ADD(data->isDMAOccurring, SaveStateVersion::Initial);
       ADD(data->currentDMAAddress, SaveStateVersion::Initial);
       ADD(data->cyclesSinceLastDMACopy, SaveStateVersion::Initial);

       //divider
       ADD(data->cyclesSinceDividerIncrement, SaveStateVersion::Initial);
       ADD(data->divider, SaveStateVersion::Initial);

       //timer
       ADD(data->cyclesSinceTimerIncrement, SaveStateVersion::Initial);
       ADD(data->timerIncrementRate, SaveStateVersion::Initial);
       ADD(data->timer, SaveStateVersion::Initial);
       ADD(data->timerModulo, SaveStateVersion::Initial);
       ADD(data->isTimerEnabled, SaveStateVersion::Initial);

       //Sound
       ADD(data->NR10, SaveStateVersion::Initial);
       ADD(data->NR11, SaveStateVersion::Initial);
       ADD(data->NR12, SaveStateVersion::Initial);
       ADD(data->NR13, SaveStateVersion::Initial);
       ADD(data->NR14, SaveStateVersion::Initial);
       ADD(data->NR21, SaveStateVersion::Initial);
       ADD(data->NR22, SaveStateVersion::Initial);
       ADD(data->NR23, SaveStateVersion::Initial);
       ADD(data->NR24, SaveStateVersion::Initial);
       ADD(data->NR30, SaveStateVersion::Initial);
       ADD(data->NR31, SaveStateVersion::Initial);
       ADD(data->NR32, SaveStateVersion::Initial);
       ADD(data->NR33, SaveStateVersion::Initial);
       ADD(data->NR34, SaveStateVersion::Initial);
       ADD(data->NR41, SaveStateVersion::Initial);
       ADD(data->NR42, SaveStateVersion::Initial);
       ADD(data->NR43, SaveStateVersion::Initial);
       ADD(data->NR44, SaveStateVersion::Initial);
       ADD(data->NR50, SaveStateVersion::Initial);
       ADD(data->NR51, SaveStateVersion::Initial);
       ADD(data->NR52, SaveStateVersion::Initial);
       ADD(data->isSoundEnabled, SaveStateVersion::Initial);
       
       ADD(data->squareWave1Channel, SaveStateVersion::Initial);
       ADD(data->squareWave2Channel, SaveStateVersion::Initial);
       ADD(data->waveChannel, SaveStateVersion::Initial);
       ADD(data->noiseChannel, SaveStateVersion::Initial);
       
       REM(SaveStateVersion::Initial, SaveStateVersion::CycleAccurateUpdate,
           i32, ticksSinceLastLengthCounter, 0);
       if (ticksSinceLastLengthCounter > 0) {
           //TODO:
       }
       REM(SaveStateVersion::Initial, SaveStateVersion::CycleAccurateUpdate,
           i32, ticksSinceLastEnvelop, 0);
       if (ticksSinceLastEnvelop > 0) {
           //TODO:
       }
       REM(SaveStateVersion::Initial, SaveStateVersion::CycleAccurateUpdate,
           i32, ticksSinceLastSweep, 0);
       if (ticksSinceLastSweep > 0) {
          //TODO: 
       }
       //TODO: figure out step 
           
//       ADD(data->ticksSinceLastLengthCounter, SaveStateVersion::Initial);
//       ADD(data->ticksSinceLastEnvelop, SaveStateVersion::Initial);
//       ADD(data->ticksSinceLastSweep, SaveStateVersion::Initial);
       
       ADD(data->cyclesSinceLastSoundSample, SaveStateVersion::Initial);
       //TODO fails here when loading old save from megaman
       ADD(data->cyclesSinceLastFrameSequencer, SaveStateVersion::Initial);
       ADD(data->masterLeftVolume, SaveStateVersion::Initial);
       ADD(data->masterRightVolume, SaveStateVersion::Initial);
//       ADD(data->soundFrameStep, SaveStateVersion::CycleAccurateUpdate);
       
       return FileSystemResultCode::OK;
    }
    
    FileSystemResultCode serialize(CPU *data, SerializingState *state) {
        ADD(data->PC, SaveStateVersion::Initial);
        ADD(data->SP, SaveStateVersion::Initial);
        ADD(data->A, SaveStateVersion::Initial);
        ADD(data->B, SaveStateVersion::Initial);
        ADD(data->C, SaveStateVersion::Initial);
        ADD(data->D, SaveStateVersion::Initial);
        ADD(data->E, SaveStateVersion::Initial);
        ADD(data->F, SaveStateVersion::Initial);
        ADD(data->H, SaveStateVersion::Initial);
        ADD(data->L, SaveStateVersion::Initial);
        ADD(data->totalCycles, SaveStateVersion::Initial);
        REM(SaveStateVersion::Initial, SaveStateVersion::CycleAccurateUpdate, 
            i32, instructionCycles, 0);
        ADD(data->leftOverCyclesFromPreviousFrame, SaveStateVersion::Initial);
        ADD(data->enableInterrupts, SaveStateVersion::Initial);
        ADD(data->isHalted, SaveStateVersion::Initial);
        ADD(data->isPaused, SaveStateVersion::Initial);
        ADD(data->didHitIllegalOpcode, SaveStateVersion::Initial);
        
        ADD(data->executingInstruction, SaveStateVersion::CycleAccurateUpdate);
        ADD(data->isPreparingToInterrupt, SaveStateVersion::CycleAccurateUpdate);
        ADD(data->cyclesSinceLastInstruction, SaveStateVersion::CycleAccurateUpdate);
        ADD(data->cyclesInstructionWillTake, SaveStateVersion::CycleAccurateUpdate);
        
       return FileSystemResultCode::OK;
    }

    static FileSystemResultCode saveSaveStateToFile(CPU *cpu, MMU *mmu, ProgramState *programState, int saveSlot) {
        CO_ASSERT(saveSlot >= 0 && saveSlot <= 9);
        char *romName = programState->loadedROMName;
        FileSystemResultCode ret = FileSystemResultCode::Unknown;
        char backupPath[MAX_PATH_LEN];
        bool didBackup = false;
        
        timestampFileName(romName, "gbes.backup", backupPath);
        
        char *saveStateFileName = nullptr; 
        buf_gen_memory_printf(saveStateFileName, "%s_%d.gbes", romName, saveSlot);
        
        auto copyFileRes = copyFile(saveStateFileName, backupPath, &programState->fileMemory);
        switch (copyFileRes) {
        case FileSystemResultCode::OK:
            didBackup = true;
            break;
        case FileSystemResultCode::NotFound:
            break;
        default:
            return copyFileRes;
        }

        FILE *f = fopen(saveStateFileName, "wb");
        if (!f) {
            CO_ERR("Could not save game state. Could not open file.");
            return FileSystemResultCode::NotFound;
        }
        buf_gen_memory_free(saveStateFileName);
        SerializingState ss;
        ss.f = f;
        ss.isWriting = true;
        ss.version = (SaveStateVersion)((i32)SaveStateVersion::CurrentPlusOne - 1);
        ret = serialize(&ss.version, &ss);
        if (ret != FileSystemResultCode::OK) {
            CO_ERR("Could not save the version to save state!");
            goto exit;
        }
        
        ret = serialize(romName, mmu->romNameLen, &ss);
        if (ret != FileSystemResultCode::OK) {
            CO_ERR("Could not save game state. Could not save rom name.");
            goto exit;
        }
        ret = serialize(cpu, &ss);
        if (ret != FileSystemResultCode::OK) {
            CO_ERR("Could not save game state. Could not write CPU.");
            goto exit; 
        }

        ret = serialize(mmu, &ss);
        if (ret != FileSystemResultCode::OK) {
            CO_ERR("Could not save game state. Could not write MMU.");
            goto exit; 
        }

        
        ret = FileSystemResultCode::OK;
exit:
        fclose(f);
        
        if (ret == FileSystemResultCode::OK && didBackup) {
            remove(backupPath);
        }
        else if (didBackup) {
           auto copyRes = copyFile(backupPath, saveStateFileName, &programState->fileMemory); 
           if (copyRes == FileSystemResultCode::OK) {
               remove(backupPath);
           }
           else {
               //TODO: need to communicate the backup location
           }
        }

        return ret;

    }

    enum class RestoreSaveResult {
        Success,
        NothingInSlot,
        Error
    };

    static RestoreSaveResult restoreSaveStateFromFile(CPU *cpu, MMU *mmu, const char *romName, int saveSlot) {
        CO_ASSERT(saveSlot >= 0 && saveSlot <= 9);
        char *tmpROMNamePtr = mmu->romName;
        CartRAMPlatformState tmpCartRAMPlatformState = mmu->cartRAMPlatformState;
        u8 *tmpROM = mmu->romData;
        char tmpROMName[MAX_ROM_NAME_LEN + 1] = {};
        
        CPU backupCPU = *cpu;
        MMU backupMMU = *mmu;
        
        char *saveStateFileName = nullptr; 
        buf_gen_memory_printf(saveStateFileName, "%s_%d.gbes", romName, saveSlot);
        
        FILE *f = fopen(saveStateFileName, "rb");
        if (!f) {
            return RestoreSaveResult::NothingInSlot;
        }
        buf_gen_memory_free(saveStateFileName);

        SerializingState ss;
        ss.f = f;
        ss.isWriting = false;
        auto res = serialize(&ss.version, &ss);
        if (res != FileSystemResultCode::OK) {
            CO_ERR("Could not read version number of save state");
            return RestoreSaveResult::Error;
        }
        res = serialize(tmpROMName, mmu->romNameLen, &ss);
        if (res != FileSystemResultCode::OK) {
            CO_ERR("Could not restore game state save. Could not read rom name.");
            fclose(f);
            return RestoreSaveResult::Error;
        }

        if (!areStringsEqual(romName, tmpROMName, mmu->romNameLen)) {
            CO_ERR("Save state is not for this rom! Actual %s, Expected %s", tmpROMName, romName);
            fclose(f);
            return RestoreSaveResult::Error;
        }

        res = serialize(cpu, &ss);
        if (res != FileSystemResultCode::OK) {
            CO_ERR("Could not load game state. Could not load CPU.");
            goto error; 
        }

        res = serialize(mmu, &ss);
        if (res != FileSystemResultCode::OK) {
            CO_ERR("Could not load game state. Could not load MMU.");
            goto error; 
        }

        if (mmu->hasRAM && mmu->hasBattery) {
            mmu->cartRAMPlatformState = tmpCartRAMPlatformState;
            usize expectedRAMLen = (usize)mmu->cartRAMSize;
            if (mmu->hasRTC) expectedRAMLen += sizeof(RTCFileState);
            if (expectedRAMLen != mmu->cartRAMPlatformState.ramLen) {
                goto error;
            }
            copyMemory(mmu->cartRAM, mmu->cartRAMPlatformState.cartRAMFileMap, mmu->cartRAMSize);
        }

        mmu->romName = tmpROMNamePtr;
        mmu->romData = tmpROM;

        fclose(f);
        return RestoreSaveResult::Success;
error:
        *cpu = backupCPU;
        *mmu = backupMMU;
        fclose(f);
        return RestoreSaveResult::Error;
    }
