#include "dusk/game_clock.h"
#include "dusk/interp/frame_interpolation.h"
#include "dusk/interp/vdt.h"

#include "d/d_com_inf_game.h"
#include "d/d_meter_HIO.h"
#include "d/d_meter_map.h"
#include "d/d_meter2_info.h"
#include "d/d_pane_class.h"

namespace {

struct FadeRow {
    bool used;
    bool show;
    bool follow;
    bool got_free;
    bool got_follow;
    f32 timer;
    f32 duration;
    f32 full;
};

struct FollowA {
    bool live;
    bool dim;
    bool dim_txt;
};

struct FollowB {
    bool live;
    bool dim;
    bool dim_txt;
    bool light_on;
    bool item_visible;
};

struct FollowXY {
    bool live;
    bool dim;
    bool fishing;
    bool item_visible;
};

struct MidonaChase {
    bool used;
    bool live;
    bool hold;
    f32 z_target;
    f32 xy_target;
};

struct GaugeFill {
    bool used;
    s32 prev;
    s32 now;
    s32 max;
};

enum FadeRowId {
    kFadeRowLife,
    kFadeRowKantera,
    kFadeRowOxygen,
    kFadeRowLightDrop,
    kFadeRowRupee,
    kFadeRowKey,
    kFadeRowButtonParent,
    kFadeRowButtonA,
    kFadeRowButtonB,
    kFadeRowCross,
    kFadeRowCrossItem,
    kFadeRowCrossMap,
    kFadeRowMap,
    kFadeRowCount
};

FadeRow s_life;
FadeRow s_kantera;
FadeRow s_oxygen;
FadeRow s_light_drop;
FadeRow s_rupee;
FadeRow s_key;
FadeRow s_button_parent;
FadeRow s_button_a;
FadeRow s_button_b;
FadeRow s_cross;
FadeRow s_cross_item;
FadeRow s_cross_map;
FadeRow s_map;
FollowA s_follow_a;
FollowB s_follow_b;
FollowXY s_follow_xy[2];
MidonaChase s_midona;
GaugeFill s_kantera_fill;
GaugeFill s_oxygen_fill;
bool s_light_drop_get;
bool s_fade_registered;

f32 fade_row_factor(FadeRow const& r) {
    if (r.duration <= 0.0f) {
        return r.show ? 1.0f : 0.0f;
    }
    f32 factor = r.timer / r.duration;
    if (factor < 0.0f) {
        return 0.0f;
    }
    if (factor > 1.0f) {
        return 1.0f;
    }
    return factor;
}

void publish_fade_row(FadeRow& c, bool show, f32 duration, f32 full) {
    c.used = true;
    c.duration = duration > 0.0f ? duration : 1.0f;
    if (c.timer > c.duration) {
        c.timer = c.duration;
    }
    c.full = full;
    c.show = show;
    c.got_free = true;
}

void publish_follow_row(FadeRow& c) {
    c.used = true;
    c.got_follow = true;
}

f32 button_parent_rate(dMeter2Draw_c* d) {
    if (s_button_parent.used) {
        return s_button_parent.full * fade_row_factor(s_button_parent);
    }
    if (d->mpButtonParent != NULL) {
        return d->mpButtonParent->getAlphaRate();
    }
    return 0.0f;
}

void reset_follow_intent(FadeRow& r) {
    r.got_free = false;
    r.got_follow = false;
}

void resolve_follow_row(FadeRow& r, FadeRow const& parent) {
    if (r.got_follow) {
        r.follow = true;
    } else if (r.follow && r.got_free) {
        r.follow = false;
        r.timer = fade_row_factor(parent) * r.duration;
    }
}

void step_fade_row(FadeRow& c) {
    if (!c.used || c.follow) {
        return;
    }

    const f32 step = dusk::game_clock::original_frames();
    if (c.show) {
        if (c.timer < c.duration) {
            c.timer += step;
            if (c.timer > c.duration) {
                c.timer = c.duration;
            }
        }
    } else if (c.timer > 0.0f) {
        c.timer -= step;
        if (c.timer < 0.0f) {
            c.timer = 0.0f;
        }
    }
}

void advance_fade_rows();

void ensure_fade_registered() {
    if (s_fade_registered) {
        return;
    }
    dusk::vdt::register_advance(&advance_fade_rows);
    s_fade_registered = true;
}

f32 const kFadeDuration = 5.0f;

f32 fade_full(dMeter2Draw_c* d, FadeRowId id) {
    f32 const parent = g_drawHIO.mParentAlpha;
    f32 const buttons = parent * g_drawHIO.mMainHUDButtonsAlpha;
    f32 const rupee_key = parent * g_drawHIO.mRupeeKeyAlpha;
    switch (id) {
    case kFadeRowLife:
    case kFadeRowKantera:
    case kFadeRowOxygen:
    case kFadeRowCross:
        return parent;
    case kFadeRowLightDrop:
        return parent * d->field_0x6fc;
    case kFadeRowRupee:
        return g_drawHIO.mRupeeAlpha * rupee_key;
    case kFadeRowKey:
        return g_drawHIO.mKeyAlpha * rupee_key;
    case kFadeRowButtonParent:
        return buttons;
    case kFadeRowButtonA:
        return g_drawHIO.mButtonAAlpha * buttons;
    case kFadeRowButtonB:
        return g_drawHIO.mButtonBAlpha * buttons;
    case kFadeRowCrossItem:
        return parent * g_drawHIO.mButtonCrossITEMAlpha;
    case kFadeRowCrossMap:
        return parent * g_drawHIO.mButtonCrossMAPAlpha;
    case kFadeRowMap:
        return 1.0f;
    default:
        return 0.0f;
    }
}

void apply_parent_rate(CPaneMgrAlpha* pane, FadeRow const& row) {
    if (!row.used || pane == NULL || pane->getPanePtr() == NULL) {
        return;
    }
    const f32 rate = row.full * fade_row_factor(row);
    if (pane->getAlphaRate() != rate) {
        pane->setAlphaRate(rate);
    }
}

void apply_meter_rate(dMeter2Draw_c* d, int index, FadeRow const& row) {
    if (!row.used) {
        return;
    }
    d->mMeterAlphaRate[index] = row.full * fade_row_factor(row);
}

void apply_cross_text(CPaneMgr* text, CPaneMgr** juji, FadeRow const& row) {
    if (!row.used) {
        return;
    }
    const f32 factor = fade_row_factor(row);
    if (text != NULL) {
        text->setAlphaRate(row.full * factor);
    }
    const f32 juji_rate = g_drawHIO.mParentAlpha * factor;
    for (int i = 0; i < 5; i++) {
        if (juji[i] != NULL) {
            juji[i]->setAlphaRate(juji_rate);
        }
    }
}

void apply_light_drop(dMeter2Draw_c* d) {
    if (!s_light_drop.used || d->mpLightDropParent == NULL) {
        return;
    }

    const f32 parent_rate = s_light_drop.full * fade_row_factor(s_light_drop);
    if (d->mpLightDropParent->getAlphaRate() != parent_rate) {
        d->mpLightDropParent->setAlphaRate(parent_rate);
    }

    f32 si_parent_rate[2];
    for (int i = 0; i < 2; i++) {
        si_parent_rate[i] = g_drawHIO.mLightDrop.mVesselAlpha[i + 2] * parent_rate;
        if (d->mpSIParent[i] != NULL) {
            d->mpSIParent[i]->setAlphaRate(si_parent_rate[i]);
        }
    }

    if (s_light_drop_get && s_light_drop.show) {
        if (g_drawHIO.mLightDrop.mDropGetScaleAnimFrameNum > 1) {
            d->field_0x6f8 = 1.0f + (((g_drawHIO.mLightDrop.mDropGetScale - 1.0f) * ((f32)g_drawHIO.mLightDrop.mDropGetScaleAnimFrameNum - s_light_drop.timer)) / ((f32)g_drawHIO.mLightDrop.mDropGetScaleAnimFrameNum - 1.0f));
        } else {
            d->field_0x6f8 = 1.0f;
        }
    }

    const int drop_num = dComIfGs_getLightDropNum(dComIfGp_getStartStageDarkArea());
    const f32 si1 = si_parent_rate[1];
    for (int i = 0; i < 16; i++) {
        const f32 rate = i < drop_num ? si1 : si1 * 0.3f;
        if (d->mpSIParts[i][1] != NULL) {
            d->mpSIParts[i][1]->setAlphaRate(rate);
        }
        if (d->mpSIParts[i][2] != NULL) {
            d->mpSIParts[i][2]->setAlphaRate(rate);
        }
    }
}

void apply_rupee(dMeter2Draw_c* d) {
    if (!s_rupee.used) {
        return;
    }

    const f32 factor = fade_row_factor(s_rupee);
    const f32 parent_rate[3] = {
        s_rupee.full * factor,
        g_drawHIO.mRupeeFrameAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mRupeeKeyAlpha) * factor,
        g_drawHIO.mRupeeFrameAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mRupeeKeyAlpha) * factor,
    };

    for (int i = 0; i < 3; i++) {
        if (d->mpRupeeParent[i] != NULL && d->mpRupeeParent[i]->getAlphaRate() != parent_rate[i]) {
            d->mpRupeeParent[i]->setAlphaRate(parent_rate[i]);
        }
    }
}

void apply_button_parent(dMeter2Draw_c* d) {
    if (!s_button_parent.used || d->mpButtonParent == NULL) {
        return;
    }

    apply_parent_rate(d->mpButtonParent, s_button_parent);
    const f32 parent_rate = s_button_parent.full * fade_row_factor(s_button_parent);
    if (d->mpUzu != NULL) {
        d->mpUzu->setAlphaRate(d->mButtonBaseAlpha * parent_rate);
    }
}

void apply_button_a(dMeter2Draw_c* d) {
    if (!s_button_a.used || d->mpButtonA == NULL) {
        return;
    }

    if (s_button_a.follow) {
        if (!s_follow_a.live) {
            return;
        }

        const f32 parent_rate = button_parent_rate(d);
        u8 alpha = (u8)(g_drawHIO.mButtonAAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha) * (f32)d->mpButtonA->getInitAlpha());
        if (s_follow_a.dim) {
            alpha = g_drawHIO.mButtonXYBaseDimAlpha;
        }
        d->mpButtonA->setAlpha((u8)((f32)alpha * parent_rate));

        for (int i = 0; i < 5; i++) {
            if (d->mpAText[i] == NULL) {
                continue;
            }
            u8 text = d->mpAText[i]->getInitAlpha();
            if (s_follow_a.dim_txt) {
                text = (u8)((f32)text * ((f32)g_drawHIO.field_0x42c / 255.0f));
            }
            d->mpAText[i]->setAlpha((u8)((f32)text * parent_rate));
        }
        return;
    }

    const f32 factor = fade_row_factor(s_button_a);
    d->mpButtonA->setAlphaRate(s_button_a.full * factor);
    const f32 text_rate = (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha) * factor;
    for (int i = 0; i < 5; i++) {
        if (d->mpAText[i] != NULL) {
            d->mpAText[i]->setAlphaRate(text_rate);
        }
    }
}

void apply_button_b(dMeter2Draw_c* d) {
    if (!s_button_b.used || d->mpButtonB == NULL) {
        return;
    }

    if (s_button_b.follow) {
        if (!s_follow_b.live) {
            return;
        }

        const f32 parent_rate = button_parent_rate(d);
        const f32 full = g_drawHIO.mButtonBAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha);
        const f32 base = s_follow_b.item_visible ? d->mItemBBaseAlpha[0] : d->mItemBBaseAlpha[1];

        u8 item = d->mpItemB != NULL ? d->mpItemB->getInitAlpha() : 0;
        u8 light = d->mpLightB != NULL ? (u8)(base * (full * (f32)d->mpLightB->getInitAlpha())) : 0;
        u8 button = (u8)(full * (f32)d->mpButtonB->getInitAlpha());

        if (s_follow_b.dim) {
            item = g_drawHIO.mButtonXYItemDimAlpha;
            light = g_drawHIO.mButtonXYItemDimAlpha;
            button = g_drawHIO.mButtonXYBaseDimAlpha;
        }
        if (!s_follow_b.light_on) {
            light = 0;
        }

        if (d->mpItemB != NULL) {
            d->mpItemB->setAlpha((u8)((f32)item * parent_rate));
        }
        if (d->mpLightB != NULL) {
            d->mpLightB->setAlpha((u8)((f32)light * parent_rate));
        }
        d->mpButtonB->setAlpha((u8)((f32)button * parent_rate));

        for (int i = 0; i < 5; i++) {
            if (d->mpBText[i] == NULL) {
                continue;
            }
            u8 text = d->mpBText[i]->getInitAlpha();
            if (s_follow_b.dim_txt) {
                text = (u8)((f32)text * ((f32)g_drawHIO.field_0x42c / 255.0f));
            }
            d->mpBText[i]->setAlpha((u8)((f32)text * parent_rate));
        }
        return;
    }

    const f32 factor = fade_row_factor(s_button_b);
    const f32 button_rate = s_button_b.full * factor;
    const f32 text_rate = (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha) * factor;
    d->mpButtonB->setAlphaRate(button_rate);
    if (d->mpLightB != NULL) {
        d->mpLightB->setAlphaRate(button_rate);
    }
    if (d->mpItemB != NULL) {
        d->mpItemB->setAlphaRate(text_rate);
    }
    for (int i = 0; i < 5; i++) {
        if (d->mpBText[i] != NULL) {
            d->mpBText[i]->setAlphaRate(text_rate);
        }
    }
}

void apply_button_xy(dMeter2Draw_c* d, int no) {
    if (no < 0 || no > 1 || !s_follow_xy[no].live) {
        return;
    }

    FollowXY const& s = s_follow_xy[no];
    const f32 parent_rate = button_parent_rate(d);
    const int base_index = s.item_visible ? 0 : 1;
    f32 full;
    f32 base;
    if (no == 0) {
        full = g_drawHIO.mButtonXAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha);
        base = g_drawHIO.mButtonXItemBaseAlpha[base_index];
    } else {
        full = g_drawHIO.mButtonYAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha);
        base = g_drawHIO.mButtonYItemBaseAlpha[base_index];
    }

    u8 item = d->mpItemXY[no] != NULL ? d->mpItemXY[no]->getInitAlpha() : 0;
    u8 light = d->mpLightXY[no] != NULL ? (u8)(base * (full * (f32)d->mpLightXY[no]->getInitAlpha())) : 0;
    u8 button = d->mpButtonXY[no] != NULL ? (u8)(full * (f32)d->mpButtonXY[no]->getInitAlpha()) : 0;

    if (s.dim) {
        item = s.fishing ? 0 : g_drawHIO.mButtonXYItemDimAlpha;
        light = g_drawHIO.mButtonXYItemDimAlpha;
        button = g_drawHIO.mButtonXYBaseDimAlpha;
    }

    if (d->mpItemXY[no] != NULL) {
        d->mpItemXY[no]->setAlpha((u8)((f32)item * parent_rate));
    }
    if (d->mpLightXY[no] != NULL) {
        d->mpLightXY[no]->setAlpha((u8)((f32)light * parent_rate));
    }
    if (d->mpButtonXY[no] != NULL) {
        d->mpButtonXY[no]->setAlpha((u8)((f32)button * parent_rate));
    }

    for (int i = 0; i < 5; i++) {
        if (d->mpXYText[i][no] == NULL) {
            continue;
        }
        u8 text = d->mpXYText[i][no]->getInitAlpha();
        if (s.dim) {
            text = (u8)((f32)text * ((f32)g_drawHIO.field_0x42c / 255.0f));
        }
        d->mpXYText[i][no]->setAlpha((u8)((f32)text * parent_rate));
    }
}

void apply_gauge_fill(dMeter2Draw_c* draw, u8 index, f32 value, s32 max, f32 pos_x, f32 pos_y, f32 scale, s32 cap) {
    f32 var_f7 = cap != 0 ? value / (f32)cap : 0.0f;
    f32 var_f4 = cap != 0 ? (f32)max / (f32)cap : 0.0f;
    f32 frameWidth = draw->mpMagicFrameR->getInitPosX() - draw->mpMagicFrameL->getInitPosX();

    draw->field_0x584[index] = var_f7 * draw->mpMagicMeter->getInitSizeX();
    draw->field_0x590[index] = draw->mpMagicMeter->getInitSizeY();
    draw->field_0x59c[index] = (frameWidth * var_f4) + draw->mpMagicFrameL->getInitPosX();
    draw->field_0x5a8[index] = draw->mpMagicFrameL->getInitPosY();
    draw->field_0x5b4[index] = var_f4 * draw->mpMagicBase->getInitSizeX();
    draw->field_0x5c0[index] = draw->mpMagicBase->getInitSizeY();
    draw->field_0x5cc[index] = draw->field_0x5d8[index] = scale;
    draw->field_0x5e4[index] = pos_x;
    draw->field_0x5f0[index] = pos_y;
}

FadeRow* const kFadeRows[kFadeRowCount] = {
    &s_life,
    &s_kantera,
    &s_oxygen,
    &s_light_drop,
    &s_rupee,
    &s_key,
    &s_button_parent,
    &s_button_a,
    &s_button_b,
    &s_cross,
    &s_cross_item,
    &s_cross_map,
    &s_map,
};

void advance_fade_rows() {
    for (int i = 0; i < kFadeRowCount; i++) {
        step_fade_row(*kFadeRows[i]);
    }
}

void write_fade_row(dMeter2Draw_c* d, FadeRowId id, bool show) {
    if (id >= kFadeRowCount) {
        return;
    }
    ensure_fade_registered();
    publish_fade_row(*kFadeRows[id], show, kFadeDuration, fade_full(d, id));
    if (id == kFadeRowButtonParent && show) {
        if (fade_row_factor(s_button_parent) >= 1.0f) {
            if (dMeter2Info_getLightDropGetFlag(dComIfGp_getStartStageDarkArea()) == 1) {
                dMeter2Info_setLightDropGetFlag(dComIfGp_getStartStageDarkArea(), 2);
            }
        }
    }
}

void follow_button_a(bool live, bool dim, bool dim_txt) {
    ensure_fade_registered();
    s_follow_a.live = live;
    s_follow_a.dim = dim;
    s_follow_a.dim_txt = dim_txt;
    if (live) {
        publish_follow_row(s_button_a);
    }
}

void follow_button_b(bool live, bool dim, bool dim_txt, bool light_on, bool item_visible) {
    ensure_fade_registered();
    s_follow_b.live = live;
    s_follow_b.dim = dim;
    s_follow_b.dim_txt = dim_txt;
    s_follow_b.light_on = light_on;
    s_follow_b.item_visible = item_visible;
    if (live) {
        publish_follow_row(s_button_b);
    }
}

void follow_button_xy(int i_no, bool live, bool dim, bool fishing, bool item_visible) {
    if (i_no < 0 || i_no > 1) {
        return;
    }
    ensure_fade_registered();
    s_follow_xy[i_no].live = live;
    s_follow_xy[i_no].dim = dim;
    s_follow_xy[i_no].fishing = fishing;
    s_follow_xy[i_no].item_visible = item_visible;
}

void chase_midona(bool live, bool hold, f32 z_target, f32 xy_target) {
    s_midona.used = true;
    s_midona.live = live;
    s_midona.hold = hold;
    s_midona.z_target = z_target;
    s_midona.xy_target = xy_target;
}

void seed_gauge(GaugeFill& g, s32 now) {
    g.prev = now;
}

void publish_gauge(GaugeFill& g, s32 now, s32 max) {
    g.used = true;
    g.now = now;
    g.max = max;
}

f32 gauge_draw(GaugeFill const& g) {
    const f32 step = dusk::interp::get_interpolation_step();
    return (f32)g.prev + ((f32)g.now - (f32)g.prev) * step;
}
}  // namespace

f32 dMeter2Draw_c::getMeterGaugeAlphaRate(u8 i_no) {
    if (i_no == 1) {
        return fade_row_factor(s_kantera) * g_drawHIO.mParentAlpha;
    }
    if (i_no == 2) {
        return fade_row_factor(s_oxygen) * g_drawHIO.mParentAlpha;
    }
    return mMeterAlphaRate[i_no];
}

void dMeter2Draw_c::beginExecution() {
    mExecuting = true;
    reset_follow_intent(s_button_a);
    reset_follow_intent(s_button_b);
    seed_gauge(s_kantera_fill, dComIfGp_getItemNowOil());
    seed_gauge(s_oxygen_fill, dComIfGp_getNowOxygen());
}

void dMeter2Draw_c::endExecution() {
    resolve_follow_row(s_button_a, s_button_parent);
    resolve_follow_row(s_button_b, s_button_parent);
    mExecuting = false;
}

void dMeter2Draw_c::publishKantera(s32 now, s32 max) {
    publish_gauge(s_kantera_fill, now, max);
}

void dMeter2Draw_c::publishOxygen(s32 now, s32 max) {
    publish_gauge(s_oxygen_fill, now, max);
}

void dMeter2Draw_c::present() {
    apply_parent_rate(this->mpLifeParent, s_life);
    if (s_life.used) {
        this->setAlphaLifeChange(false);
    }

    apply_meter_rate(this, 1, s_kantera);
    if (s_kantera.used) {
        this->setAlphaKanteraChange(true);
    }
    if (s_kantera_fill.used) {
        s32 cap = dComIfGs_getMaxOil();
        apply_gauge_fill(this, 1, gauge_draw(s_kantera_fill), s_kantera_fill.max, g_drawHIO.mLanternMeterPosX, g_drawHIO.mLanternMeterPosY, g_drawHIO.mLanternMeterScale, cap != 0 ? cap : 0);
    }

    apply_meter_rate(this, 2, s_oxygen);
    if (s_oxygen.used) {
        this->setAlphaOxygenChange(false);
    }
    if (s_oxygen_fill.used) {
        s32 cap = dComIfGp_getMaxOxygen();
        apply_gauge_fill(this, 2, gauge_draw(s_oxygen_fill), s_oxygen_fill.max, g_drawHIO.mOxygenMeterPosX, g_drawHIO.mOxygenMeterPosY, g_drawHIO.mOxygenMeterScale, cap > 0 ? cap : 0);
    }

    apply_light_drop(this);
    if (s_light_drop.used) {
        this->setAlphaLightDropChange(false);
    }

    apply_rupee(this);
    if (s_rupee.used) {
        this->setAlphaRupeeChange(false);
    }

    apply_parent_rate(this->mpKeyParent, s_key);
    if (s_key.used) {
        this->setAlphaKeyChange(false);
    }

    apply_button_parent(this);
    apply_button_a(this);
    apply_button_b(this);
    apply_button_xy(this, 0);
    apply_button_xy(this, 1);
    if (s_button_parent.used) {
        this->setAlphaButtonChange(false);
    }

    apply_parent_rate(this->mpButtonCrossParent, s_cross);
    apply_cross_text(this->mpTextI, this->mpJujiI, s_cross_item);
    apply_cross_text(this->mpTextM, this->mpJujiM, s_cross_map);

    if (!s_midona.used || this->mpButtonMidona == NULL) {
        return;
    }

    const f32 parent_rate = button_parent_rate(this);
    if (s_midona.live) {
        if (s_midona.hold) {
            this->mButtonZAlpha = s_midona.z_target;
        } else {
            dusk::vdt::present_addCalc2(&this->mButtonZAlpha, s_midona.z_target, 0.4f, 0.5f, 0.1f);
        }
        const f32 full = g_drawHIO.mMidnaIconAlpha * (g_drawHIO.mParentAlpha * g_drawHIO.mMainHUDButtonsAlpha);
        this->mpButtonMidona->setAlpha((u8)(full * (255.0f * this->mButtonZAlpha * parent_rate)));
    } else {
        this->mButtonZAlpha = 0.0f;
    }
    dusk::vdt::present_addCalc2(&this->field_0x724, s_midona.xy_target, 0.4f, 0.5f, 0.1f);
    this->mpButtonXY[2]->setAlpha((u8)(255.0f * this->field_0x724 * parent_rate));
}

void dMeter2Draw_c::presentVessel(f32 i_posX, f32 i_posY, f32 i_scale) {
    mLightDropVesselScale = i_scale;
    const f32 lightDropScale = mLightDropVesselScale * field_0x6f8 * dGetUserHudScale();
    mpLightDropParent->scale(lightDropScale, lightDropScale);

    f32 lightDropPosX = i_posX;
    f32 lightDropPosY = i_posY;
    dAnchorHudScale(mpLightDropParent, HudCorner::TopRight, &lightDropPosX, &lightDropPosY);
    mpLightDropParent->paneTrans(lightDropPosX, lightDropPosY);
}

void dMeter2Draw_c::presentButtonA(f32 i_posX, f32 i_posY, f32 i_textPosX, f32 i_textPosY, f32 i_scale) {
    mpButtonA->scale(s_button_a_scale * i_scale, s_button_a_scale * i_scale);
    mpButtonA->paneTrans(i_posX, i_posY);
    mpTextA->scale(s_button_a_text_scale * i_scale, s_button_a_text_scale * i_scale);
    mpTextA->paneTrans(g_drawHIO.mButtonATextPosX + i_textPosX,
                       g_drawHIO.mButtonATextPosY + i_textPosY + (dusk::version::isLessThanWiiJpn() ? 6.0f : 0.0f));
}

void dMeter2Draw_c::presentButtonB(f32 i_posX, f32 i_posY, f32 i_textPosX, f32 i_textPosY, f32 i_scale) {
    const int kind = s_button_b_kind;
    field_0x730 = s_button_b_scale * i_scale;
    mpButtonB->scale(field_0x730 * field_0x734, field_0x730 * field_0x734);
    mpButtonB->paneTrans(g_drawHIO.mButtonBPosX + i_posX, g_drawHIO.mButtonBPosY + i_posY);

    field_0x728 = g_drawHIO.mButtonBItemScale[kind] * i_scale;
    mpItemB->scale(field_0x728 * field_0x734, field_0x728 * field_0x734);
    mpItemB->paneTrans(field_0x6dc + (g_drawHIO.mButtonBItemPosX[kind] + i_posX), field_0x6e0 + (g_drawHIO.mButtonBItemPosY[kind] + i_posY));

    field_0x72c = g_drawHIO.mItemBBaseScale[0] * i_scale;
    mpLightB->scale(field_0x72c * field_0x734, field_0x72c * field_0x734);
    mpLightB->paneTrans(g_drawHIO.mItemBBasePosX[0] + i_posX, g_drawHIO.mItemBBasePosY[0] + i_posY);

    mpTextB->scale(s_button_b_text_scale * i_scale, s_button_b_text_scale * i_scale);
    mpTextB->paneTrans(g_drawHIO.mButtonBFontPosX + i_textPosX,
                       g_drawHIO.mButtonBFontPosY + i_textPosY + (dusk::version::isLessThanWiiJpn() ? 4.0f : 0.0f));
}

void dMeter2Draw_c::presentButtonCross(f32 i_posX, f32 i_posY) {
    const f32 buttonCrossScale = g_drawHIO.mButtonCrossScale * dGetUserHudScale();
    mpButtonCrossParent->scale(buttonCrossScale, buttonCrossScale);
    f32 buttonCrossPosX = i_posX;
    f32 buttonCrossPosY = i_posY;
    dAnchorHudScale(mpButtonCrossParent, HudCorner::BottomLeft, &buttonCrossPosX, &buttonCrossPosY);
    mpButtonCrossParent->paneTrans(buttonCrossPosX, buttonCrossPosY);
}

void dMeter2Draw_c::presentMapAlpha(dMeterMap_c* map) {
    if (map == NULL) {
        return;
    }
    if (!s_map.used) {
        map->setMapAlpha(0);
        return;
    }
    map->setMapAlpha((u8)(fade_row_factor(s_map) * 255.0f));
}

void dMeter2Draw_c::showLightDrop(bool show, bool get_path) {
    if (show && get_path) {
        if (s_light_drop.timer <= 0.0f) {
            Z2GetAudioMgr()->seStart(Z2SE_SY_LIGHT_POT_EQUIP, NULL, 0, 0, 1.0f, 1.0f, -1.0f, -1.0f, 0);
        }
        if (fade_row_factor(s_light_drop) >= 1.0f) {
            dMeter2Info_setLightDropGetFlag(dComIfGp_getStartStageDarkArea(), 0xFF);
        }
    }
    ensure_fade_registered();
    const f32 duration = get_path ? (f32)g_drawHIO.mLightDrop.mDropGetScaleAnimFrameNum : kFadeDuration;
    publish_fade_row(s_light_drop, show, duration, fade_full(this, kFadeRowLightDrop));
    s_light_drop_get = get_path;
}

void dMeter2Draw_c::showCross(bool show) {
    write_fade_row(this, kFadeRowCross, show);
    write_fade_row(this, kFadeRowCrossItem, show && dMeter2Info_isItemOpenCheck());
    write_fade_row(this, kFadeRowCrossMap, show && dMeter2Info_isMapOpenCheck());
}

void dMeter2Draw_c::showMap(bool show) {
    write_fade_row(this, kFadeRowMap, show);
}

bool dMeter2Draw_c::lightDropRowUsed() const {
    return s_light_drop.used;
}

void dMeter2Draw_c::resetRows() {
    mExecuting = false;
    for (int i = 0; i < kFadeRowCount; i++) {
        *kFadeRows[i] = FadeRow{};
    }
    s_follow_a = FollowA{};
    s_follow_b = FollowB{};
    s_follow_xy[0] = FollowXY{};
    s_follow_xy[1] = FollowXY{};
    s_midona = MidonaChase{};
    s_kantera_fill = GaugeFill{};
    s_oxygen_fill = GaugeFill{};
    s_light_drop_get = false;
}
