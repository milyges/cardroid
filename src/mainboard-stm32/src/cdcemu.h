#ifndef __CDCEMU_H
#define __CDCEMU_H

enum ChangerEmulatorState {
    cdcStateBooting1 = 1,
    cdcStateBooting2,
    cdcStateStandby,
    cdcStatePlaying,
    cdcStatePause
};

enum ChangerEmulatorCDState {
    cdStateNoCD = 0x01,
    cdStatePaused = 0x03,
    cdStateLoadingTrack = 0x04,
    cdStatePlaying = 0x05,
    cdStateCueing = 0x07,
    cdStateRewinding = 0x09,
    cdStateSearchingTrack = 0x0A
};

enum ChangerEmulatorTrayState {
    trayStateNoTray = 0x02,
    trayStateCDReady = 0x03,
    trayStateLoadingCD = 0x04,
    trayStateUnloadingCD = 0x05
};

enum ChangerEmulatorTrackState {
    trackChangeEntering = 0x10,
    trackChangeEntered = 0x14,
    trackChangeLeavingCD = 0x40,
    trackChangeLeaving = 0x80
};

void cdcemu_tick(void);
void cdcemu_init(void);
void cdcemu_loop(void);
void cdcemu_radiopower_off(void);

#endif /* __CDCEMU_H */
