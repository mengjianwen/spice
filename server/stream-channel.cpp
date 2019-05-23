/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
   Copyright (C) 2017 Red Hat, Inc.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, see <http://www.gnu.org/licenses/>.
*/
#include <config.h>

#include <common/generated_server_marshallers.h>
#include <common/recorder.h>
#include <spice/stream-device.h>

#include "red-channel-client.h"
#include "stream-channel.h"
#include "reds.h"
#include "common-graphics-channel.h"
#include "display-limits.h"
#include "video-stream.h" // TODO remove, put common stuff

/* we need to inherit from CommonGraphicsChannelClient
 * to get buffer handling */
class StreamChannelClient final: public CommonGraphicsChannelClient
{
protected:
    ~StreamChannelClient();
public:
    using CommonGraphicsChannelClient::CommonGraphicsChannelClient;

    /* current video stream id, <0 if not initialized or
     * we are not sending a stream */
    int stream_id = -1;
private:
    /* Array with SPICE_VIDEO_CODEC_TYPE_ENUM_END elements, with the client
     * preference order (index) as value */
    GArray *client_preferred_video_codecs;
    bool handle_preferred_video_codec_type(SpiceMsgcDisplayPreferredVideoCodecType *msg);
    virtual void on_disconnect() override;
    virtual bool handle_message(uint16_t type, uint32_t size, void *msg) override;
};

struct StreamChannel final: public RedChannel
{
    /* current video stream id, <0 if not initialized or
     * we are not sending a stream */
    int stream_id;
    /* size of the current video stream */
    unsigned width, height;

    StreamQueueStat queue_stat;

    /* callback to notify when a stream should be started or stopped */
    stream_channel_start_proc start_cb;
    void *start_opaque;

    /* callback to notify when queue statistics changes */
    stream_channel_queue_stat_proc queue_cb;
    void *queue_opaque;
};

struct StreamChannelClass {
    RedChannelClass parent_class;
};

G_DEFINE_TYPE(StreamChannel, stream_channel, RED_TYPE_CHANNEL)

enum {
    RED_PIPE_ITEM_TYPE_SURFACE_CREATE = RED_PIPE_ITEM_TYPE_COMMON_LAST,
    RED_PIPE_ITEM_TYPE_SURFACE_DESTROY,
    RED_PIPE_ITEM_TYPE_FILL_SURFACE,
    RED_PIPE_ITEM_TYPE_STREAM_CREATE,
    RED_PIPE_ITEM_TYPE_STREAM_DATA,
    RED_PIPE_ITEM_TYPE_STREAM_DESTROY,
    RED_PIPE_ITEM_TYPE_STREAM_ACTIVATE_REPORT,
    RED_PIPE_ITEM_TYPE_MONITORS_CONFIG,
};

typedef struct StreamCreateItem {
    RedPipeItem base;
    SpiceMsgDisplayStreamCreate stream_create;
} StreamCreateItem;

typedef struct StreamDataItem {
    RedPipeItem base;
    StreamChannel *channel;
    // NOTE: this must be the last field in the structure
    SpiceMsgDisplayStreamData data;
} StreamDataItem;

#define PRIMARY_SURFACE_ID 0

RECORDER(stream_channel_data, 32, "Stream channel data packet");

StreamChannelClient::~StreamChannelClient()
{
    g_clear_pointer(&client_preferred_video_codecs, g_array_unref);
}

static void
request_new_stream(StreamChannel *channel, StreamMsgStartStop *start)
{
    if (channel->start_cb) {
        channel->start_cb(channel->start_opaque, start, channel);
    }
}

void
StreamChannelClient::on_disconnect()
{
    StreamChannel *channel = STREAM_CHANNEL(get_channel());

    // if there are still some client connected keep streaming
    // TODO, maybe would be worth sending new codecs if they are better
    if (channel->is_connected()) {
        return;
    }

    channel->stream_id = -1;
    channel->width = 0;
    channel->height = 0;

    // send stream stop to device
    StreamMsgStartStop stop = { 0, };
    request_new_stream(channel, &stop);
}

static StreamChannelClient*
stream_channel_client_new(StreamChannel *channel, RedClient *client, RedStream *stream,
                          int mig_target, RedChannelCapabilities *caps)
{
    auto rcc = new StreamChannelClient(RED_CHANNEL(channel), client, stream, caps);
    if (!rcc->init()) {
        rcc->unref();
        rcc = nullptr;
    }
    return rcc;
}

static void
fill_base(SpiceMarshaller *m, const StreamChannel *channel)
{
    SpiceMsgDisplayBase base;

    base.surface_id = PRIMARY_SURFACE_ID;
    base.box = (SpiceRect) { 0, 0, channel->width, channel->height };
    base.clip = (SpiceClip) { SPICE_CLIP_TYPE_NONE, NULL };

    spice_marshall_DisplayBase(m, &base);
}

static void
marshall_monitors_config(RedChannelClient *rcc, StreamChannel *channel, SpiceMarshaller *m)
{
    struct {
        SpiceMsgDisplayMonitorsConfig config;
        SpiceHead head;
    } msg = {
        { 1, 1, },
        {
            // monitor ID. These IDs are allocated per channel starting from 0
            0,
            PRIMARY_SURFACE_ID,
            channel->width, channel->height,
            0, 0,
            0 // flags
        }
    };

    rcc->init_send_data(SPICE_MSG_DISPLAY_MONITORS_CONFIG);
    spice_marshall_msg_display_monitors_config(m, &msg.config);
}

XXX_CAST(RedChannelClient, StreamChannelClient, STREAM_CHANNEL_CLIENT)

static void
stream_channel_send_item(RedChannelClient *rcc, RedPipeItem *pipe_item)
{
    SpiceMarshaller *m = rcc->get_marshaller();
    StreamChannelClient *client = STREAM_CHANNEL_CLIENT(rcc);
    StreamChannel *channel = STREAM_CHANNEL(rcc->get_channel());

    switch (pipe_item->type) {
    case RED_PIPE_ITEM_TYPE_SURFACE_CREATE: {
        rcc->init_send_data(SPICE_MSG_DISPLAY_SURFACE_CREATE);
        SpiceMsgSurfaceCreate surface_create = {
            PRIMARY_SURFACE_ID,
            channel->width, channel->height,
            SPICE_SURFACE_FMT_32_xRGB, SPICE_SURFACE_FLAGS_PRIMARY
        };

        // give an hint to client that we are sending just streaming
        // see spice.proto for capability check here
        if (rcc->test_remote_cap(SPICE_DISPLAY_CAP_MULTI_CODEC)) {
            surface_create.flags |= SPICE_SURFACE_FLAGS_STREAMING_MODE;
        }

        spice_marshall_msg_display_surface_create(m, &surface_create);
        break;
    }
    case RED_PIPE_ITEM_TYPE_MONITORS_CONFIG:
        if (!rcc->test_remote_cap(SPICE_DISPLAY_CAP_MONITORS_CONFIG)) {
            return;
        }
        marshall_monitors_config(rcc, channel, m);
        break;
    case RED_PIPE_ITEM_TYPE_SURFACE_DESTROY: {
        rcc->init_send_data(SPICE_MSG_DISPLAY_SURFACE_DESTROY);
        SpiceMsgSurfaceDestroy surface_destroy = { PRIMARY_SURFACE_ID };
        spice_marshall_msg_display_surface_destroy(m, &surface_destroy);
        break;
    }
    case RED_PIPE_ITEM_TYPE_FILL_SURFACE: {
        rcc->init_send_data(SPICE_MSG_DISPLAY_DRAW_FILL);

        fill_base(m, channel);

        SpiceFill fill;
        fill.brush = (SpiceBrush) { SPICE_BRUSH_TYPE_SOLID, { .color = 0 } };
        fill.rop_descriptor = SPICE_ROPD_OP_PUT;
        fill.mask = (SpiceQMask) { 0, { 0, 0 }, NULL };
        SpiceMarshaller *brush_pat_out, *mask_bitmap_out;
        spice_marshall_Fill(m, &fill, &brush_pat_out, &mask_bitmap_out);
        break;
    }
    case RED_PIPE_ITEM_TYPE_STREAM_CREATE: {
        StreamCreateItem *item = SPICE_UPCAST(StreamCreateItem, pipe_item);
        client->stream_id = item->stream_create.id;
        rcc->init_send_data(SPICE_MSG_DISPLAY_STREAM_CREATE);
        spice_marshall_msg_display_stream_create(m, &item->stream_create);
        break;
    }
    case RED_PIPE_ITEM_TYPE_STREAM_ACTIVATE_REPORT: {
        if (client->stream_id < 0
            || !rcc->test_remote_cap(SPICE_DISPLAY_CAP_STREAM_REPORT)) {
            return;
        }
        SpiceMsgDisplayStreamActivateReport msg;
        msg.stream_id = client->stream_id;
        msg.unique_id = 1; // TODO useful ?
        msg.max_window_size = RED_STREAM_CLIENT_REPORT_WINDOW;
        msg.timeout_ms = RED_STREAM_CLIENT_REPORT_TIMEOUT;
        rcc->init_send_data(SPICE_MSG_DISPLAY_STREAM_ACTIVATE_REPORT);
        spice_marshall_msg_display_stream_activate_report(m, &msg);
        break;
    }
    case RED_PIPE_ITEM_TYPE_STREAM_DATA: {
        StreamDataItem *item = SPICE_UPCAST(StreamDataItem, pipe_item);
        rcc->init_send_data(SPICE_MSG_DISPLAY_STREAM_DATA);
        spice_marshall_msg_display_stream_data(m, &item->data);
        red_pipe_item_ref(pipe_item);
        spice_marshaller_add_by_ref_full(m, item->data.data, item->data.data_size,
                                         marshaller_unref_pipe_item, pipe_item);
        record(stream_channel_data, "Stream data packet size %u mm_time %u",
               item->data.data_size, item->data.base.multi_media_time);
        break;
    }
    case RED_PIPE_ITEM_TYPE_STREAM_DESTROY: {
        if (client->stream_id < 0) {
            return;
        }
        SpiceMsgDisplayStreamDestroy stream_destroy = { client->stream_id };
        rcc->init_send_data(SPICE_MSG_DISPLAY_STREAM_DESTROY);
        spice_marshall_msg_display_stream_destroy(m, &stream_destroy);
        client->stream_id = -1;
        break;
    }
    default:
        spice_error("invalid pipe item type");
    }

    rcc->begin_send_message();
}

bool StreamChannelClient::handle_message(uint16_t type, uint32_t size, void *msg)
{
    switch (type) {
    case SPICE_MSGC_DISPLAY_INIT:
    case SPICE_MSGC_DISPLAY_PREFERRED_COMPRESSION:
        return true;
    case SPICE_MSGC_DISPLAY_STREAM_REPORT:
        /* TODO these will help tune the streaming reducing/increasing quality */
        return true;
    case SPICE_MSGC_DISPLAY_GL_DRAW_DONE:
        /* client should not send this message */
        return false;
    case SPICE_MSGC_DISPLAY_PREFERRED_VIDEO_CODEC_TYPE:
        return handle_preferred_video_codec_type(
            (SpiceMsgcDisplayPreferredVideoCodecType *)msg);
    default:
        return CommonGraphicsChannelClient::handle_message(type, size, msg);
    }
}


StreamChannel*
stream_channel_new(RedsState *server, uint32_t id)
{
    return (StreamChannel*) g_object_new(TYPE_STREAM_CHANNEL,
                        "spice-server", server,
                        "core-interface", reds_get_core_interface(server),
                        "channel-type", SPICE_CHANNEL_DISPLAY,
                        // TODO this id should be after all qxl devices
                        "id", id,
                        "handle-acks", TRUE, // TODO sure ??
                        NULL);
}

#define MAX_SUPPORTED_CODECS SPICE_VIDEO_CODEC_TYPE_ENUM_END

// find common codecs supported by all clients
static uint8_t
stream_channel_get_supported_codecs(StreamChannel *channel, uint8_t *out_codecs)
{
    RedChannelClient *rcc;
    int codec;

    static const uint16_t codec2cap[] = {
        0, // invalid
        SPICE_DISPLAY_CAP_CODEC_MJPEG,
        SPICE_DISPLAY_CAP_CODEC_VP8,
        SPICE_DISPLAY_CAP_CODEC_H264,
        SPICE_DISPLAY_CAP_CODEC_VP9,
        SPICE_DISPLAY_CAP_CODEC_H265,
    };

    bool supported[SPICE_N_ELEMENTS(codec2cap)];

    for (codec = 0; codec < SPICE_N_ELEMENTS(codec2cap); ++codec) {
        supported[codec] = true;
    }

    FOREACH_CLIENT(channel, rcc) {
        for (codec = 1; codec < SPICE_N_ELEMENTS(codec2cap); ++codec) {
            // if do not support codec delete from list
            if (!rcc->test_remote_cap(codec2cap[codec])) {
                supported[codec] = false;
            }
        }
    }

    // surely mjpeg is supported
    supported[SPICE_VIDEO_CODEC_TYPE_MJPEG] = true;

    int num = 0;
    for (codec = 1; codec < SPICE_N_ELEMENTS(codec2cap); ++codec) {
        if (supported[codec]) {
            out_codecs[num++] = codec;
        }
    }

    return num;
}

bool
StreamChannelClient::handle_preferred_video_codec_type(SpiceMsgcDisplayPreferredVideoCodecType *msg)
{
    if (msg->num_of_codecs == 0) {
        return true;
    }

    g_clear_pointer(&client_preferred_video_codecs, g_array_unref);
    client_preferred_video_codecs = video_stream_parse_preferred_codecs(msg);

    return true;
}

static void
stream_channel_connect(RedChannel *red_channel, RedClient *red_client, RedStream *stream,
                       int migration, RedChannelCapabilities *caps)
{
    StreamChannel *channel = STREAM_CHANNEL(red_channel);
    StreamChannelClient *client;
    struct {
        StreamMsgStartStop base;
        uint8_t codecs_buffer[MAX_SUPPORTED_CODECS];
    } start_msg;
    StreamMsgStartStop *const start = &start_msg.base;

    spice_return_if_fail(stream != NULL);

    client = stream_channel_client_new(channel, red_client, stream, migration, caps);
    if (client == NULL) {
        return;
    }

    // request new stream
    start->num_codecs = stream_channel_get_supported_codecs(channel, start->codecs);
    // send in any case, even if list is not changed
    // notify device about changes
    request_new_stream(channel, start);


    // see guest_set_client_capabilities
    RedChannelClient *rcc = client;
    rcc->push_set_ack();

    // TODO what should happen on migration, dcc return if on migration wait ??
    rcc->ack_zero_messages_window();

    // "emulate" dcc_start
    rcc->pipe_add_empty_msg(SPICE_MSG_DISPLAY_INVAL_ALL_PALETTES);

    // only if "surface"
    if (channel->width == 0 || channel->height == 0) {
        return;
    }

    // pass proper data
    rcc->pipe_add_type(RED_PIPE_ITEM_TYPE_SURFACE_CREATE);
    rcc->pipe_add_type(RED_PIPE_ITEM_TYPE_MONITORS_CONFIG);
    // surface data
    rcc->pipe_add_type(RED_PIPE_ITEM_TYPE_FILL_SURFACE);
    // TODO monitor configs ??
    rcc->pipe_add_empty_msg(SPICE_MSG_DISPLAY_MARK);
}

static void
stream_channel_constructed(GObject *object)
{
    RedChannel *red_channel = RED_CHANNEL(object);
    RedsState *reds = red_channel->get_server();

    G_OBJECT_CLASS(stream_channel_parent_class)->constructed(object);

    red_channel->set_cap(SPICE_DISPLAY_CAP_MONITORS_CONFIG);
    red_channel->set_cap(SPICE_DISPLAY_CAP_STREAM_REPORT);
    red_channel->set_cap(SPICE_DISPLAY_CAP_PREF_VIDEO_CODEC_TYPE);

    reds_register_channel(reds, red_channel);
}

static void
stream_channel_class_init(StreamChannelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    RedChannelClass *channel_class = RED_CHANNEL_CLASS(klass);

    object_class->constructed = stream_channel_constructed;

    channel_class->parser = spice_get_client_channel_parser(SPICE_CHANNEL_DISPLAY, NULL);

    channel_class->send_item = stream_channel_send_item;
    channel_class->connect = stream_channel_connect;
}

static void
stream_channel_init(StreamChannel *channel)
{
    channel->stream_id = -1;
    channel->width = 0;
    channel->height = 0;
}

void
stream_channel_change_format(StreamChannel *channel, const StreamMsgFormat *fmt)
{
    // send destroy old stream
    channel->pipes_add_type(RED_PIPE_ITEM_TYPE_STREAM_DESTROY);

    // send new create surface if required
    if (channel->width != fmt->width || channel->height != fmt->height) {
        if (channel->width != 0 && channel->height != 0) {
            channel->pipes_add_type(RED_PIPE_ITEM_TYPE_SURFACE_DESTROY);
        }
        channel->width = fmt->width;
        channel->height = fmt->height;
        channel->pipes_add_type(RED_PIPE_ITEM_TYPE_SURFACE_CREATE);
        channel->pipes_add_type(RED_PIPE_ITEM_TYPE_MONITORS_CONFIG);
        // TODO monitors config ??
        channel->pipes_add_empty_msg(SPICE_MSG_DISPLAY_MARK);
    }

    // allocate a new stream id
    channel->stream_id = (channel->stream_id + 1) % NUM_STREAMS;

    // send create stream
    StreamCreateItem *item = g_new0(StreamCreateItem, 1);
    red_pipe_item_init(&item->base, RED_PIPE_ITEM_TYPE_STREAM_CREATE);
    item->stream_create.id = channel->stream_id;
    item->stream_create.flags = SPICE_STREAM_FLAGS_TOP_DOWN;
    item->stream_create.codec_type = fmt->codec;
    item->stream_create.stream_width = fmt->width;
    item->stream_create.stream_height = fmt->height;
    item->stream_create.src_width = fmt->width;
    item->stream_create.src_height = fmt->height;
    item->stream_create.dest = (SpiceRect) { 0, 0, fmt->width, fmt->height };
    item->stream_create.clip = (SpiceClip) { SPICE_CLIP_TYPE_NONE, NULL };
    channel->pipes_add(&item->base);

    // activate stream report if possible
    channel->pipes_add_type(RED_PIPE_ITEM_TYPE_STREAM_ACTIVATE_REPORT);
}

static inline void
stream_channel_update_queue_stat(StreamChannel *channel,
                                 int32_t num_diff, int32_t size_diff)
{
    channel->queue_stat.num_items += num_diff;
    channel->queue_stat.size += size_diff;
    if (channel->queue_cb) {
        channel->queue_cb(channel->queue_opaque, &channel->queue_stat, channel);
    }
}

static void
data_item_free(RedPipeItem *base)
{
    StreamDataItem *pipe_item = SPICE_UPCAST(StreamDataItem, base);

    stream_channel_update_queue_stat(pipe_item->channel, -1, -pipe_item->data.data_size);

    g_free(pipe_item);
}

void
stream_channel_send_data(StreamChannel *channel, const void *data, size_t size, uint32_t mm_time)
{
    if (channel->stream_id < 0) {
        // this condition can happen if the guest didn't handle
        // the format stop that we send so think the stream is still
        // started
        return;
    }

    StreamDataItem *item = (StreamDataItem*) g_malloc(sizeof(*item) + size);
    red_pipe_item_init_full(&item->base, RED_PIPE_ITEM_TYPE_STREAM_DATA,
                            data_item_free);
    item->data.base.id = channel->stream_id;
    item->data.base.multi_media_time = mm_time;
    item->data.data_size = size;
    item->channel = channel;
    stream_channel_update_queue_stat(channel, 1, size);
    // TODO try to optimize avoiding the copy
    memcpy(item->data.data, data, size);
    channel->pipes_add(&item->base);
}

void
stream_channel_register_start_cb(StreamChannel *channel,
                                 stream_channel_start_proc cb, void *opaque)
{
    channel->start_cb = cb;
    channel->start_opaque = opaque;
}

void
stream_channel_register_queue_stat_cb(StreamChannel *channel,
                                      stream_channel_queue_stat_proc cb, void *opaque)
{
    channel->queue_cb = cb;
    channel->queue_opaque = opaque;
}

void
stream_channel_reset(StreamChannel *channel)
{
    struct {
        StreamMsgStartStop base;
        uint8_t codecs_buffer[MAX_SUPPORTED_CODECS];
    } start_msg;
    StreamMsgStartStop *const start = &start_msg.base;

    // send destroy old stream
    channel->pipes_add_type(RED_PIPE_ITEM_TYPE_STREAM_DESTROY);

    // destroy display surface
    if (channel->width != 0 && channel->height != 0) {
        channel->pipes_add_type(RED_PIPE_ITEM_TYPE_SURFACE_DESTROY);
    }

    channel->stream_id = -1;
    channel->width = 0;
    channel->height = 0;

    if (!channel->is_connected()) {
        return;
    }

    // try to request a new stream, this should start a new stream
    // if the guest is connected to the device and a client is already connected
    start->num_codecs = stream_channel_get_supported_codecs(channel, start->codecs);
    // send in any case, even if list is not changed
    // notify device about changes
    request_new_stream(channel, start);
}
