/*
 * message.h
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
 * Header-only: there is no message.c, because a vocabulary has no behaviour.
 */

#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "framebuffer.h"

/* ==========================================================================
 * message - topics
 * ========================================================================= */

/*! \brief One value per topic. `NONE` and `LAST` bracket the range so a topic can be
 *         validated, and `LAST` sizes the broker's subscription table. */
typedef enum
{
    MESSAGE_ID_NONE = 0,
    MESSAGE_ID_INPUT_DIRECTION,                 /*!< Input  -> Game                   */
    MESSAGE_ID_INPUT_BUTTON,                    /*!< Input  -> System                  */
    MESSAGE_ID_SYSTEM_SHOW_LOADING,             /*!< System -> Render                  */
    MESSAGE_ID_SYSTEM_SHOW_MENU,                /*!< System -> Render                  */
    MESSAGE_ID_SYSTEM_START_GAME,               /*!< System -> Game                    */
    MESSAGE_ID_RENDER_FRAME,                    /*!< Game   -> Render                  */
    MESSAGE_ID_GAME_SCORE_UPDATED,              /*!< Game   -> NVM                     */
    MESSAGE_ID_GAME_OVER,                       /*!< Game   -> System, NVM             */
    MESSAGE_ID_HIGHSCORE_LOADED,                /*!< NVM    -> System                  */
    MESSAGE_ID_LAST
} message_id_e;

/* ==========================================================================
 * message - payloads
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

/*! \brief Payload of \ref MESSAGE_ID_INPUT_DIRECTION (FR-004). */
typedef struct
{
    direction_e direction;
} message_input_direction_t;

/*! \brief Payload of \ref MESSAGE_ID_INPUT_BUTTON (FR-003). */
typedef struct
{
    bool is_pressed;
} message_input_button_t;

/*! \brief Payload of \ref MESSAGE_ID_SYSTEM_SHOW_MENU and
 *         \ref MESSAGE_ID_HIGHSCORE_LOADED (FR-002, FR-009). */
typedef struct
{
    uint32_t high_score;
} message_high_score_t;

/*! \brief Payload of \ref MESSAGE_ID_GAME_SCORE_UPDATED. */
typedef struct
{
    uint32_t score;
} message_game_score_t;

/*! \brief Payload of \ref MESSAGE_ID_GAME_OVER (FR-007, FR-021). */
typedef struct
{
    uint32_t final_score;
    bool has_won;
} message_game_over_t;

/*! \brief Payload of \ref MESSAGE_ID_RENDER_FRAME (FR-005, R-007).
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
} message_render_frame_t;

/* ==========================================================================
 * message - envelope
 * ========================================================================= */

/*! \brief Size of the payload area. Must hold the largest payload above; the static
 *         assertions below fail the build if a new payload outgrows it. */
#define MESSAGE_PAYLOAD_MAX_SIZE (16U)

typedef struct
{
    message_id_e id;                            /*!< Topic, a member of \ref message_id_e */
    uint16_t payload_size;                      /*!< Valid bytes in `payload`             */
    uint8_t payload[MESSAGE_PAYLOAD_MAX_SIZE];  /*!< Opaque to the broker                 */
} message_t;

_Static_assert(sizeof(message_input_direction_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(message_input_button_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(message_high_score_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(message_game_score_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(message_game_over_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");
_Static_assert(sizeof(message_render_frame_t) <= MESSAGE_PAYLOAD_MAX_SIZE, "payload too large");

#endif /* MESSAGE_H */
