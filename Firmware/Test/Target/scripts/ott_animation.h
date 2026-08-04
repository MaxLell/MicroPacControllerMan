/*
 * ott_animation.h
 *
 * On-target test: does moving Pacman actually *look* smooth?
 *
 * `display_test` answers how many frames the panel can take (290 per second, for a
 * realistic dirty area). That is not the same question. Smoothness is a property of
 * how far a sprite jumps between frames, and the only instrument that can judge it
 * is a person looking at the panel.
 *
 * So this drives five actors across the screen at a *constant* speed and varies only
 * the frame rate — the step per frame shrinks as the rate rises. The firmware reports
 * the rate it actually held and what fraction of the frame budget it spent; the
 * operator says where the motion stops being smooth.
 *
 * This is the test the **60 FPS** frame rate was chosen against — see
 * [M2 Board Bring-Up §3.2](../../../../Docu/Design/M2-Board-Bring-Up.md). It now serves as
 * the regression check that the rate is still held, and reports the unpaced ceiling so the
 * headroom the game inherits is a number rather than a hope.
 */

#ifndef OTT_ANIMATION_H
#define OTT_ANIMATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==========================================================================
 * ott_animation - public API
 * ========================================================================= */

/*! \brief Run the animation smoothness test.
 *
 * Plays a ladder of frame rates automatically, then hands the rate to the joystick
 * so the operator can dial it up and down while watching. Passes when every pass
 * held its requested rate and the operator confirms with B1.
 *
 * \param[in]       in_parameter: unused
 * \param[out]      out_reason: filled with the failure reason when it fails
 * \param[in]       in_reason_size: size of `out_reason`
 * \return          `true` when the test passed
 */
bool ott_animation_run(const uint8_t* in_parameter, char* out_reason, size_t in_reason_size);

#endif /* OTT_ANIMATION_H */
