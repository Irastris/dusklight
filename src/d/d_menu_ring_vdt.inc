void dMenu_Ring_c::presentAnims() {
    mPresenting = true;
    if (mOpenCloseDir > 0) {
        isOpen();
    } else if (mOpenCloseDir < 0) {
        isClose();
    }
    if (mStatus == STATUS_MOVE) {
        const u8 oldStatus = mStatus;
        stick_move_proc();
        if (mStatus != oldStatus) {
            (this->*stick_init[mStatus])();
        }
    }
    for (int i = 0; i < 4; i++) {
        if (field_0x674[i] != 0 && field_0x674[i] < 10) {
            field_0x674[i] += dusk::game_clock::original_frames();
        }
    }
    field_0x684 += dusk::game_clock::original_frames();
    if (field_0x684 >= g_ringHIO.mItemAlphaFlashDuration) {
        field_0x684 = 0;
    }
    mPresenting = false;
}
