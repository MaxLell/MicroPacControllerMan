#include "ott_framebuffer.h"

#include "framebuffer.h"
#include "render.h"

/* ==========================================================================
 * ott_framebuffer - public
 * ========================================================================= */

framebuffer_t* ott_framebuffer_get(void)
{
    return render_get_framebuffer();
}
