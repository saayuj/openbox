#include "openbox/actions.h"
#include "openbox/client.h"
#include "openbox/screen.h"
#include "openbox/frame.h"
#include <stdlib.h> /* for atoi */

enum {
    CURRENT_MONITOR = -1,
    ALL_MONITORS = -2,
    NEXT_MONITOR = -3,
    PREV_MONITOR = -4
};

typedef struct {
    gboolean xcenter;
    gboolean ycenter;
    gboolean xopposite;
    gboolean yopposite;
    gint x;
    gint y;
    gint w;
    gint h;
    gint monitor;
} Options;

static gpointer setup_func(xmlNodePtr node);
static gboolean run_func(ObActionsData *data, gpointer options);

void action_moveresizeto_startup(void)
{
    actions_register("MoveResizeTo", setup_func, g_free, run_func, NULL, NULL);
}

static void parse_coord(xmlNodePtr n, gint *pos,
                        gboolean *opposite, gboolean *center)
{
    gchar *s = obt_parse_node_string(n);
    if (g_ascii_strcasecmp(s, "current") != 0) {
        if (!g_ascii_strcasecmp(s, "center"))
            *center = TRUE;
        else {
            if (s[0] == '-')
                *opposite = TRUE;
            if (s[0] == '-' || s[0] == '+')
                *pos = atoi(s+1);
            else
                *pos = atoi(s);
        }
    }
    g_free(s);
}

static gpointer setup_func(xmlNodePtr node)
{
    xmlNodePtr n;
    Options *o;

    o = g_new0(Options, 1);
    o->x = G_MININT;
    o->y = G_MININT;
    o->w = G_MININT;
    o->h = G_MININT;
    o->monitor = CURRENT_MONITOR;

    if ((n = obt_parse_find_node(node, "x")))
        parse_coord(n, &o->x, &o->xopposite, &o->xcenter);

    if ((n = obt_parse_find_node(node, "y")))
        parse_coord(n, &o->y, &o->yopposite, &o->ycenter);

    if ((n = obt_parse_find_node(node, "width"))) {
        gchar *s = obt_parse_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0)
            o->w = obt_parse_node_int(n);
        g_free(s);
    }
    if ((n = obt_parse_find_node(node, "height"))) {
        gchar *s = obt_parse_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0)
            o->h = obt_parse_node_int(n);
        g_free(s);
    }

    if ((n = obt_parse_find_node(node, "monitor"))) {
        gchar *s = obt_parse_node_string(n);
        if (g_ascii_strcasecmp(s, "current") != 0) {
            if (!g_ascii_strcasecmp(s, "all"))
                o->monitor = ALL_MONITORS;
            else if(!g_ascii_strcasecmp(s, "next"))
                o->monitor = NEXT_MONITOR;
            else if(!g_ascii_strcasecmp(s, "prev"))
                o->monitor = PREV_MONITOR;
            else
                o->monitor = obt_parse_node_int(n) - 1;
        }
        g_free(s);
    }

    return o;
}

/* Always return FALSE because its not interactive */
static gboolean run_func(ObActionsData *data, gpointer options)
{
    Options *o = options;

    if (data->client) {
        Rect *area, *carea;
        ObClient *c;
        gint mon, cmon;
        gint x, y, lw, lh, w, h;

        c = data->client;
        mon = o->monitor;
        cmon = client_monitor(c);
        if (mon == CURRENT_MONITOR) mon = cmon;
        else if (mon == ALL_MONITORS) mon = SCREEN_AREA_ALL_MONITORS;
        else if (mon == NEXT_MONITOR) mon = (cmon + 1 > screen_num_monitors - 1) ? 0 : (cmon + 1);
        else if (mon == PREV_MONITOR) mon = (cmon == 0) ? (screen_num_monitors - 1) : (cmon - 1);

        area = screen_area(c->desktop, mon, NULL);
        carea = screen_area(c->desktop, cmon, NULL);

        w = o->w;
        if (w == G_MININT) w = c->area.width;

        h = o->h;
        if (h == G_MININT) h = c->area.height;

        /* it might not be able to resize how they requested, so find out what
           it will actually be resized to */
        x = c->area.x;
        y = c->area.y;
        client_try_configure(c, &x, &y, &w, &h, &lw, &lh, TRUE);

        /* get the frame's size */
        w += c->frame->size.left + c->frame->size.right;
        h += c->frame->size.top + c->frame->size.bottom;

        x = o->x;
        if (o->xcenter) x = (area->width - w) / 2;
        else if (x == G_MININT) x = c->frame->area.x - carea->x;
        else if (o->xopposite) x = area->width - w - x;
        x += area->x;

        y = o->y;
        if (o->ycenter) y = (area->height - h) / 2;
        else if (y == G_MININT) y = c->frame->area.y - carea->y;
        else if (o->yopposite) y = area->height - h - y;
        y += area->y;

        /* get the client's size back */
        w -= c->frame->size.left + c->frame->size.right;
        h -= c->frame->size.top + c->frame->size.bottom;

        frame_frame_gravity(c->frame, &x, &y); /* get the client coords */
        client_try_configure(c, &x, &y, &w, &h, &lw, &lh, TRUE);
        /* force it on screen if its moving to another monitor */
        client_find_onscreen(c, &x, &y, w, h, mon != cmon);

        actions_client_move(data, TRUE);
        client_configure(c, x, y, w, h, TRUE, TRUE, FALSE);
        actions_client_move(data, FALSE);

        g_free(area);
        g_free(carea);
    }

    return FALSE;
}
