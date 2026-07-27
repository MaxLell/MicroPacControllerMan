/*
 * msg.h
 *
 * The vocabulary every module shares: the topic IDs, the payload types, and the
 * fixed-size envelope they travel in. Transcribed from
 * [03 §3.3](../../../Docu/PrePlanning/03-Architecture.md) — that table is the
 * authority, this header is its code form.
 *
 * A message is a topic plus a small payload, **copied by value**, so no module ever
 * holds a pointer into another's memory and nothing needs the heap (NFR-103). The one
 * sanctioned exception is the render frame, which carries a handle to a read-only
 * snapshot rather than 2 kB of pixels (R-007).
 *
 * Header-only: there is no msg.c, because a vocabulary has no behaviour.
 */

#ifndef MSG_H
#define MSG_H

#include <stdbool.h>
#include <stdint.h>

#include "framebuffer.h"

/* ==========================================================================
 * msg - topics
 * ========================================================================= */

/*! \brief One value per topic, named as in [03 §3.3]. `MSG_NONE` and `MSG_LAST`
 *         bracket the range so a topic can be validated, and `MSG_LAST` sizes the
 *         broker's subscription table. */
typedef enum
{
    MSG_NONE = 0,
    MSG_INPUT_DIRECTION,                        /*!< Input  -> Game                    */
    MSG_INPUT_BUTTON,                           /*!< Input  -> System                  */
    MSG_SYSTEM_SHOW_LOADING,                    /*!< System -> Render                  */
    MSG_SYSTEM_SHOW_MENU,                       /*!< System -> Render                  */
    MSG_SYSTEM_START_GAME,                      /*!< System -> Game                    */
    MSG_RENDER_FRAME,                           /*!< Game   -> Render                  */
    MSG_GAME_SCORE_UPDATED,                     /*!< Game   -> NVM                     */
    MSG_GAME_OVER,                              /*!< Game   -> System, NVM             */
    MSG_HIGHSCORE_LOADED,                       /*!< NVM    -> System                  */
    MSG_LAST
} msg_id_e;

/* ==========================================================================
 * msg - payloads
 * ========================================================================= */

/*! \brief Directions Pacman can be sent in. `NONE` means "no direction yet". */
typedef enum
{
    DIRECTION_NONE = 0,
    DIRECTION_NORTH,
    DIRECTION_SOUTH,
    DIRECTION_EAST,
    DIRECTION_WEST
} direction_e;

/*! \brief Payload of \ref MSG_INPUT_DIRECTION (FR-004). */
typedef struct
{
    direction_e direction;
} msg_input_direction_t;

/*! \brief Payload of \ref MSG_INPUT_BUTTON (FR-003). */
typedef struct
{
    bool is_pressed;
} msg_input_button_t;

/*! \brief Payload of \ref MSG_SYSTEM_SHOW_MENU and \ref MSG_HIGHSCORE_LOADED
 *         (FR-002, FR-009). */
typedef struct
{
    uint32_t high_score;
} msg_high_score_t;

/*! \brief Payload of \ref MSG_GAME_SCORE_UPDATED. */
typedef struct
{
    uint32_t score;
} msg_game_score_t;

/*! \brief Payload of \ref MSG_GAME_OVER (FR-007, FR-021). */
typedef struct
{
    uint32_t final_score;
    bool has_won;
} msg_game_over_t;

/*! \brief Payload of \ref MSG_RENDER_FRAME (FR-005, R-007).
 *
 * The sanctioned exception to copy-by-value: this carries a *handle* to one of the
 * publisher's statically-allocated, double-buffered snapshots, plus the version that
 * identifies which frame it is. The publisher must not write the buffer it has just
 * handed out — that is what the second buffer is for.
 */
typedef struct
{
    uint32_t version;
    const framebuffer_t* frame;
} msg_render_frame_t;

/* ==========================================================================
 * msg - envelope
 * ========================================================================= */

/*! \brief Size of the payload area. Must hold the largest payload above; the static
 *         assertions below fail the build if a new payload outgrows it. */
#define MSG_PAYLOAD_MAX_SIZE (16U)

typedef struct
{
    msg_id_e id;                                /*!< Topic, a member of \ref msg_id_e  */
    uint16_t payload_size;                      /*!< Valid bytes in `payload`          */
    uint8_t payload[MSG_PAYLOAD_MAX_SIZE];      /*!< Opaque to the broker              */
} msg_t;

_Static_assert(sizeof(msg_input_direction_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_input_button_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_high_score_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_game_score_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_game_over_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(msg_render_frame_t) <= MSG_PAYLOAD_MAX_SIZE, "payload too large");

#endif /* MSG_H */
