/* spice-server spicevmc passthrough channel code

   Copyright (C) 2011 Red Hat, Inc.

   Red Hat Authors:
   Hans de Goede <hdegoede@redhat.com>

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h> // IPPROTO_TCP
#include <netinet/tcp.h> // TCP_NODELAY

#include <common/generated_server_marshallers.h>

#include "char-device.h"
#include "red-channel.h"
#include "red-channel-client.h"
#include "reds.h"
#include "migration-protocol.h"
#ifdef USE_LZ4
#include <lz4.h>
#endif

/* todo: add flow control. i.e.,
 * (a) limit the tokens available for the client
 * (b) limit the tokens available for the server
 */
/* 64K should be enough for all but the largest writes + 32 bytes hdr */
#define BUF_SIZE (64 * 1024 + 32)
#define COMPRESS_THRESHOLD 1000

typedef struct RedVmcChannel RedVmcChannel;
typedef struct RedVmcChannelClass RedVmcChannelClass;

typedef struct RedVmcPipeItem {
    RedPipeItem base;

    SpiceDataCompressionType type;
    uint32_t uncompressed_data_size;
    /* writes which don't fit this will get split, this is not a problem */
    uint8_t buf[BUF_SIZE];
    uint32_t buf_used;
} RedVmcPipeItem;

#define RED_TYPE_CHAR_DEVICE_SPICEVMC red_char_device_spicevmc_get_type()

#define RED_CHAR_DEVICE_SPICEVMC(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), RED_TYPE_CHAR_DEVICE_SPICEVMC, RedCharDeviceSpiceVmc))
#define RED_CHAR_DEVICE_SPICEVMC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), RED_TYPE_CHAR_DEVICE_SPICEVMC, RedCharDeviceSpiceVmcClass))
#define RED_IS_CHAR_DEVICE_SPICEVMC(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), RED_TYPE_CHAR_DEVICE_SPICEVMC))
#define RED_IS_CHAR_DEVICE_SPICEVMC_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), RED_TYPE_CHAR_DEVICE_SPICEVMC))
#define RED_CHAR_DEVICE_SPICEVMC_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), RED_TYPE_CHAR_DEVICE_SPICEVMC, RedCharDeviceSpiceVmcClass))

typedef struct RedCharDeviceSpiceVmc RedCharDeviceSpiceVmc;
typedef struct RedCharDeviceSpiceVmcClass RedCharDeviceSpiceVmcClass;

struct RedCharDeviceSpiceVmc {
    RedCharDevice parent;
    RedVmcChannel *channel;
};

struct RedCharDeviceSpiceVmcClass
{
    RedCharDeviceClass parent_class;
};

static GType red_char_device_spicevmc_get_type(void) G_GNUC_CONST;
static RedCharDevice *red_char_device_spicevmc_new(SpiceCharDeviceInstance *sin,
                                                   RedsState *reds,
                                                   void *opaque);

G_DEFINE_TYPE(RedCharDeviceSpiceVmc, red_char_device_spicevmc, RED_TYPE_CHAR_DEVICE)

#define RED_TYPE_VMC_CHANNEL red_vmc_channel_get_type()

#define RED_VMC_CHANNEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), RED_TYPE_VMC_CHANNEL, RedVmcChannel))
#define RED_VMC_CHANNEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), RED_TYPE_VMC_CHANNEL, RedVmcChannelClass))
#define RED_IS_VMC_CHANNEL(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), RED_TYPE_VMC_CHANNEL))
#define RED_IS_VMC_CHANNEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), RED_TYPE_VMC_CHANNEL))
#define RED_VMC_CHANNEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), RED_TYPE_VMC_CHANNEL, RedVmcChannelClass))

struct RedVmcChannel
{
    RedChannel parent;

    RedChannelClient *rcc;
    RedCharDevice *chardev;
    SpiceCharDeviceInstance *chardev_sin;
    RedVmcPipeItem *pipe_item;
    RedCharDeviceWriteBuffer *recv_from_client_buf;
    uint8_t port_opened;
};

struct RedVmcChannelClass
{
    RedChannelClass parent_class;
};

GType red_vmc_channel_get_type(void) G_GNUC_CONST;

G_DEFINE_TYPE(RedVmcChannel, red_vmc_channel, RED_TYPE_CHANNEL)

#define RED_TYPE_VMC_CHANNEL_USBREDIR red_vmc_channel_usbredir_get_type()
typedef struct
{
    RedVmcChannel parent;
} RedVmcChannelUsbredir;

typedef struct
{
    RedVmcChannelClass parent_class;
} RedVmcChannelUsbredirClass;

GType red_vmc_channel_usbredir_get_type(void) G_GNUC_CONST;
static void red_vmc_channel_usbredir_init(RedVmcChannelUsbredir *self)
{
}
G_DEFINE_TYPE(RedVmcChannelUsbredir, red_vmc_channel_usbredir, RED_TYPE_VMC_CHANNEL)


#define RED_TYPE_VMC_CHANNEL_WEBDAV red_vmc_channel_webdav_get_type()
typedef struct
{
    RedVmcChannel parent;
} RedVmcChannelWebdav;

typedef struct
{
    RedVmcChannelClass parent_class;
} RedVmcChannelWebdavClass;

GType red_vmc_channel_webdav_get_type(void) G_GNUC_CONST;
static void red_vmc_channel_webdav_init(RedVmcChannelWebdav *self)
{
}
G_DEFINE_TYPE(RedVmcChannelWebdav, red_vmc_channel_webdav, RED_TYPE_VMC_CHANNEL)


#define RED_TYPE_VMC_CHANNEL_PORT red_vmc_channel_port_get_type()
typedef struct
{
    RedVmcChannel parent;
} RedVmcChannelPort;

typedef struct
{
    RedVmcChannelClass parent_class;
} RedVmcChannelPortClass;

GType red_vmc_channel_port_get_type(void) G_GNUC_CONST;
static void red_vmc_channel_port_init(RedVmcChannelPort *self)
{
}
G_DEFINE_TYPE(RedVmcChannelPort, red_vmc_channel_port, RED_TYPE_VMC_CHANNEL)

enum {
    PROP0,
    PROP_DEVICE_INSTANCE
};

static void
red_vmc_channel_get_property(GObject *object,
                             guint property_id,
                             GValue *value,
                             GParamSpec *pspec)
{
    RedVmcChannel *self = RED_VMC_CHANNEL(object);

    switch (property_id)
    {
        case PROP_DEVICE_INSTANCE:
            g_value_set_pointer(value, self->chardev_sin);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
red_vmc_channel_set_property(GObject *object,
                             guint property_id,
                             const GValue *value,
                             GParamSpec *pspec)
{
    RedVmcChannel *self = RED_VMC_CHANNEL(object);

    switch (property_id)
    {
        case PROP_DEVICE_INSTANCE:
            self->chardev_sin = g_value_get_pointer(value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void spicevmc_connect(RedChannel *channel, RedClient *client,
                             RedsStream *stream, int migration, int num_common_caps,
                             uint32_t *common_caps, int num_caps, uint32_t *caps);

static void
red_vmc_channel_constructed(GObject *object)
{
    RedVmcChannel *self = RED_VMC_CHANNEL(object);
    ClientCbs client_cbs = { NULL, };
    RedsState *reds = red_channel_get_server(RED_CHANNEL(self));

    G_OBJECT_CLASS(red_vmc_channel_parent_class)->constructed(object);

    client_cbs.connect = spicevmc_connect;
    red_channel_register_client_cbs(RED_CHANNEL(self), &client_cbs, NULL);

#ifdef USE_LZ4
    red_channel_set_cap(RED_CHANNEL(self), SPICE_SPICEVMC_CAP_DATA_COMPRESS_LZ4);
#endif

    red_channel_init_outgoing_messages_window(RED_CHANNEL(self));

    self->chardev = red_char_device_spicevmc_new(self->chardev_sin, reds, self);

    reds_register_channel(reds, RED_CHANNEL(self));
}

static void
red_vmc_channel_init(RedVmcChannel *self)
{
}

static RedVmcChannel *red_vmc_channel_new(RedsState *reds, uint8_t channel_type,
                                          SpiceCharDeviceInstance *sin)
{
    GType gtype = G_TYPE_NONE;
    static uint8_t id[256] = { 0, };

    switch (channel_type) {
        case SPICE_CHANNEL_USBREDIR:
            gtype = RED_TYPE_VMC_CHANNEL_USBREDIR;
            break;
        case SPICE_CHANNEL_WEBDAV:
            gtype = RED_TYPE_VMC_CHANNEL_WEBDAV;
            break;
        case SPICE_CHANNEL_PORT:
            gtype = RED_TYPE_VMC_CHANNEL_PORT;
            break;
        default:
            g_error("Unsupported channel_type for red_vmc_channel_new(): %u", channel_type);
    }
    return g_object_new(gtype,
                        "spice-server", reds,
                        "core-interface", reds_get_core_interface(reds),
                        "channel-type", channel_type,
                        "id", id[channel_type]++,
                        "handle-acks", FALSE,
                        "migration-flags",
                        (SPICE_MIGRATE_NEED_FLUSH | SPICE_MIGRATE_NEED_DATA_TRANSFER),
                        "device-instance", sin,
                        NULL);
}

typedef struct RedPortInitPipeItem {
    RedPipeItem base;
    char* name;
    uint8_t opened;
} RedPortInitPipeItem;

typedef struct RedPortEventPipeItem {
    RedPipeItem base;
    uint8_t event;
} RedPortEventPipeItem;

enum {
    RED_PIPE_ITEM_TYPE_SPICEVMC_DATA = RED_PIPE_ITEM_TYPE_CHANNEL_BASE,
    RED_PIPE_ITEM_TYPE_SPICEVMC_MIGRATE_DATA,
    RED_PIPE_ITEM_TYPE_PORT_INIT,
    RED_PIPE_ITEM_TYPE_PORT_EVENT,
};

static void spicevmc_red_channel_release_msg_rcv_buf(RedChannelClient *rcc,
                                                     uint16_t type,
                                                     uint32_t size,
                                                     uint8_t *msg);
/* n is the data size (uncompressed)
 * msg_item -- the current pipe item with the uncompressed data
 * This function returns:
 *  - NULL upon failure.
 *  - a new pipe item with the compressed data in it upon success
 */
#ifdef USE_LZ4
static RedVmcPipeItem* try_compress_lz4(RedVmcChannel *state, int n, RedVmcPipeItem *msg_item)
{
    RedVmcPipeItem *msg_item_compressed;
    int compressed_data_count;

    if (reds_stream_get_family(red_channel_client_get_stream(state->rcc)) == AF_UNIX) {
        /* AF_LOCAL - data will not be compressed */
        return NULL;
    }
    if (n <= COMPRESS_THRESHOLD) {
        /* n <= threshold - data will not be compressed */
        return NULL;
    }
    if (!red_channel_test_remote_cap(RED_CHANNEL(state), SPICE_SPICEVMC_CAP_DATA_COMPRESS_LZ4)) {
        /* Client doesn't have compression cap - data will not be compressed */
        return NULL;
    }
    msg_item_compressed = spice_new0(RedVmcPipeItem, 1);
    red_pipe_item_init(&msg_item_compressed->base, RED_PIPE_ITEM_TYPE_SPICEVMC_DATA);
    compressed_data_count = LZ4_compress_default((char*)&msg_item->buf,
                                                 (char*)&msg_item_compressed->buf,
                                                 n,
                                                 BUF_SIZE);

    if (compressed_data_count > 0 && compressed_data_count < n) {
        msg_item_compressed->type = SPICE_DATA_COMPRESSION_TYPE_LZ4;
        msg_item_compressed->uncompressed_data_size = n;
        msg_item_compressed->buf_used = compressed_data_count;
        free(msg_item);
        return msg_item_compressed;
    }

    /* LZ4 compression failed or did non compress, fallback a non-compressed data is to be sent */
    free(msg_item_compressed);
    return NULL;
}
#endif

static RedPipeItem *spicevmc_chardev_read_msg_from_dev(SpiceCharDeviceInstance *sin,
                                                       void *opaque)
{
    RedVmcChannel *state = opaque;
    SpiceCharDeviceInterface *sif;
    RedVmcPipeItem *msg_item;
    int n;

    sif = spice_char_device_get_interface(sin);

    if (!state->rcc) {
        return NULL;
    }

    if (!state->pipe_item) {
        msg_item = spice_new0(RedVmcPipeItem, 1);
        msg_item->type = SPICE_DATA_COMPRESSION_TYPE_NONE;
        red_pipe_item_init(&msg_item->base, RED_PIPE_ITEM_TYPE_SPICEVMC_DATA);
    } else {
        spice_assert(state->pipe_item->buf_used == 0);
        msg_item = state->pipe_item;
        state->pipe_item = NULL;
    }

    n = sif->read(sin, msg_item->buf,
                  sizeof(msg_item->buf));
    if (n > 0) {
        spice_debug("read from dev %d", n);
#ifdef USE_LZ4
        RedVmcPipeItem *msg_item_compressed;

        msg_item_compressed = try_compress_lz4(state, n, msg_item);
        if (msg_item_compressed != NULL) {
            return &msg_item_compressed->base;
        }
#endif
        msg_item->uncompressed_data_size = n;
        msg_item->buf_used = n;
        return &msg_item->base;
    } else {
        state->pipe_item = msg_item;
        return NULL;
    }
}

static void spicevmc_chardev_send_msg_to_client(RedPipeItem *msg,
                                                RedClient *client,
                                                void *opaque)
{
    RedVmcChannel *state = opaque;

    spice_assert(red_channel_client_get_client(state->rcc) == client);
    red_pipe_item_ref(msg);
    red_channel_client_pipe_add_push(state->rcc, msg);
}

static RedVmcChannel *spicevmc_red_channel_client_get_state(RedChannelClient *rcc)
{
    RedChannel *channel = red_channel_client_get_channel(rcc);
    return RED_VMC_CHANNEL(channel);
}

static void spicevmc_port_send_init(RedChannelClient *rcc)
{
    RedVmcChannel *state = spicevmc_red_channel_client_get_state(rcc);
    SpiceCharDeviceInstance *sin = state->chardev_sin;
    RedPortInitPipeItem *item = spice_malloc(sizeof(RedPortInitPipeItem));

    red_pipe_item_init(&item->base, RED_PIPE_ITEM_TYPE_PORT_INIT);
    item->name = strdup(sin->portname);
    item->opened = state->port_opened;
    red_channel_client_pipe_add_push(rcc, &item->base);
}

static void spicevmc_port_send_event(RedChannelClient *rcc, uint8_t event)
{
    RedPortEventPipeItem *item = spice_malloc(sizeof(RedPortEventPipeItem));

    red_pipe_item_init(&item->base, RED_PIPE_ITEM_TYPE_PORT_EVENT);
    item->event = event;
    red_channel_client_pipe_add_push(rcc, &item->base);
}

static void spicevmc_char_dev_send_tokens_to_client(RedClient *client,
                                                    uint32_t tokens,
                                                    void *opaque)
{
    spice_printerr("Not implemented!");
}

static void spicevmc_char_dev_remove_client(RedClient *client, void *opaque)
{
    RedVmcChannel *state = opaque;

    spice_printerr("vmc state %p, client %p", state, client);
    spice_assert(state->rcc &&
                 red_channel_client_get_client(state->rcc) == client);

    red_channel_client_shutdown(state->rcc);
}

static int spicevmc_red_channel_client_config_socket(RedChannelClient *rcc)
{
    int delay_val = 1;
    RedsStream *stream = red_channel_client_get_stream(rcc);
    RedChannel *channel = red_channel_client_get_channel(rcc);
    uint32_t type;

    g_object_get(channel, "channel-type", &type, NULL);
    if (type == SPICE_CHANNEL_USBREDIR) {
        if (setsockopt(stream->socket, IPPROTO_TCP, TCP_NODELAY,
                &delay_val, sizeof(delay_val)) != 0) {
            if (errno != ENOTSUP && errno != ENOPROTOOPT) {
                spice_printerr("setsockopt failed, %s", strerror(errno));
                return FALSE;
            }
        }
    }

    return TRUE;
}

static void spicevmc_red_channel_client_on_disconnect(RedChannelClient *rcc)
{
    RedVmcChannel *state;
    SpiceCharDeviceInterface *sif;
    RedClient *client = red_channel_client_get_client(rcc);

    if (!rcc) {
        return;
    }

    state = spicevmc_red_channel_client_get_state(rcc);

    /* partial message which wasn't pushed to device */
    red_char_device_write_buffer_release(state->chardev, &state->recv_from_client_buf);

    if (state->chardev) {
        if (red_char_device_client_exists(state->chardev, client)) {
            red_char_device_client_remove(state->chardev, client);
        } else {
            spice_printerr("client %p have already been removed from char dev %p",
                           client, state->chardev);
        }
    }

    /* Don't destroy the rcc if it is already being destroyed, as then
       red_client_destroy/red_channel_client_destroy will already do this! */
    if (!red_channel_client_is_destroying(rcc))
        red_channel_client_destroy(rcc);

    state->rcc = NULL;
    sif = spice_char_device_get_interface(state->chardev_sin);
    if (sif->state) {
        sif->state(state->chardev_sin, 0);
    }
}

static int spicevmc_channel_client_handle_migrate_flush_mark(RedChannelClient *rcc)
{
    red_channel_client_pipe_add_type(rcc, RED_PIPE_ITEM_TYPE_SPICEVMC_MIGRATE_DATA);
    return TRUE;
}

static int spicevmc_channel_client_handle_migrate_data(RedChannelClient *rcc,
                                                       uint32_t size, void *message)
{
    SpiceMigrateDataHeader *header;
    SpiceMigrateDataSpiceVmc *mig_data;
    RedVmcChannel *state;

    state = spicevmc_red_channel_client_get_state(rcc);

    header = (SpiceMigrateDataHeader *)message;
    mig_data = (SpiceMigrateDataSpiceVmc *)(header + 1);
    spice_assert(size >= sizeof(SpiceMigrateDataHeader) + sizeof(SpiceMigrateDataSpiceVmc));

    if (!migration_protocol_validate_header(header,
                                            SPICE_MIGRATE_DATA_SPICEVMC_MAGIC,
                                            SPICE_MIGRATE_DATA_SPICEVMC_VERSION)) {
        spice_error("bad header");
        return FALSE;
    }
    return red_char_device_restore(state->chardev, &mig_data->base);
}

static int handle_compressed_msg(RedVmcChannel *state, RedChannelClient *rcc,
                                 SpiceMsgCompressedData *compressed_data_msg)
{
    /* NOTE: *decompressed is free by the char-device */
    int decompressed_size;
    RedCharDeviceWriteBuffer *write_buf;

    write_buf = red_char_device_write_buffer_get(state->chardev,
                                                 red_channel_client_get_client(rcc),
                                                 compressed_data_msg->uncompressed_size);
    if (!write_buf) {
        return FALSE;
    }

    switch (compressed_data_msg->type) {
#ifdef USE_LZ4
    case SPICE_DATA_COMPRESSION_TYPE_LZ4: {
        uint8_t *decompressed = write_buf->buf;
        decompressed_size = LZ4_decompress_safe ((char *)compressed_data_msg->compressed_data,
                                                 (char *)decompressed,
                                                 compressed_data_msg->compressed_size,
                                                 compressed_data_msg->uncompressed_size);
        break;
    }
#endif
    default:
        spice_warning("Invalid Compression Type");
        red_char_device_write_buffer_release(state->chardev, &write_buf);
        return FALSE;
    }
    if (decompressed_size != compressed_data_msg->uncompressed_size) {
        spice_warning("Decompression Error");
        red_char_device_write_buffer_release(state->chardev, &write_buf);
        return FALSE;
    }
    write_buf->buf_used = decompressed_size;
    red_char_device_write_buffer_add(state->chardev, write_buf);
    return TRUE;
}

static int spicevmc_red_channel_client_handle_message_parsed(RedChannelClient *rcc,
                                                             uint32_t size,
                                                             uint16_t type,
                                                             void *msg)
{
    /* NOTE: *msg free by free() (when cb to spicevmc_red_channel_release_msg_rcv_buf
     * with the compressed msg type) */
    RedVmcChannel *state;
    SpiceCharDeviceInterface *sif;

    state = spicevmc_red_channel_client_get_state(rcc);
    sif = spice_char_device_get_interface(state->chardev_sin);

    switch (type) {
    case SPICE_MSGC_SPICEVMC_DATA:
        spice_assert(state->recv_from_client_buf->buf == msg);
        state->recv_from_client_buf->buf_used = size;
        red_char_device_write_buffer_add(state->chardev, state->recv_from_client_buf);
        state->recv_from_client_buf = NULL;
        break;
    case SPICE_MSGC_SPICEVMC_COMPRESSED_DATA:
        return handle_compressed_msg(state, rcc, (SpiceMsgCompressedData*)msg);
        break;
    case SPICE_MSGC_PORT_EVENT:
        if (size != sizeof(uint8_t)) {
            spice_warning("bad port event message size");
            return FALSE;
        }
        if (sif->base.minor_version >= 2 && sif->event != NULL)
            sif->event(state->chardev_sin, *(uint8_t*)msg);
        break;
    default:
        return red_channel_client_handle_message(rcc, size, type, (uint8_t*)msg);
    }

    return TRUE;
}

static uint8_t *spicevmc_red_channel_alloc_msg_rcv_buf(RedChannelClient *rcc,
                                                       uint16_t type,
                                                       uint32_t size)
{
    RedVmcChannel *state;
    RedClient *client = red_channel_client_get_client(rcc);

    state = spicevmc_red_channel_client_get_state(rcc);

    switch (type) {
    case SPICE_MSGC_SPICEVMC_DATA:
        assert(!state->recv_from_client_buf);

        state->recv_from_client_buf = red_char_device_write_buffer_get(state->chardev,
                                                                       client,
                                                                       size);
        if (!state->recv_from_client_buf) {
            spice_error("failed to allocate write buffer");
            return NULL;
        }
        return state->recv_from_client_buf->buf;

    default:
        return spice_malloc(size);
    }

}

static void spicevmc_red_channel_release_msg_rcv_buf(RedChannelClient *rcc,
                                                     uint16_t type,
                                                     uint32_t size,
                                                     uint8_t *msg)
{
    RedVmcChannel *state;

    state = spicevmc_red_channel_client_get_state(rcc);

    switch (type) {
    case SPICE_MSGC_SPICEVMC_DATA:
        /* buffer wasn't pushed to device */
        red_char_device_write_buffer_release(state->chardev, &state->recv_from_client_buf);
        break;
    default:
        free(msg);
    }
}

static void spicevmc_red_channel_send_data(RedChannelClient *rcc,
                                           SpiceMarshaller *m,
                                           RedPipeItem *item)
{
    RedVmcPipeItem *i = SPICE_UPCAST(RedVmcPipeItem, item);

    /* for compatibility send using not compressed data message */
    if (i->type == SPICE_DATA_COMPRESSION_TYPE_NONE) {
        red_channel_client_init_send_data(rcc, SPICE_MSG_SPICEVMC_DATA, item);
    } else {
        /* send as compressed */
        red_channel_client_init_send_data(rcc, SPICE_MSG_SPICEVMC_COMPRESSED_DATA, item);
        SpiceMsgCompressedData compressed_msg = {
            .type = i->type,
            .uncompressed_size = i->uncompressed_data_size
        };
        spice_marshall_SpiceMsgCompressedData(m, &compressed_msg);
    }
    spice_marshaller_add_ref(m, i->buf, i->buf_used);
}

static void spicevmc_red_channel_send_migrate_data(RedChannelClient *rcc,
                                                   SpiceMarshaller *m,
                                                   RedPipeItem *item)
{
    RedVmcChannel *state;

    state = spicevmc_red_channel_client_get_state(rcc);
    red_channel_client_init_send_data(rcc, SPICE_MSG_MIGRATE_DATA, item);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_SPICEVMC_MAGIC);
    spice_marshaller_add_uint32(m, SPICE_MIGRATE_DATA_SPICEVMC_VERSION);

    red_char_device_migrate_data_marshall(state->chardev, m);
}

static void spicevmc_red_channel_send_port_init(RedChannelClient *rcc,
                                                SpiceMarshaller *m,
                                                RedPipeItem *item)
{
    RedPortInitPipeItem *i = SPICE_UPCAST(RedPortInitPipeItem, item);
    SpiceMsgPortInit init;

    red_channel_client_init_send_data(rcc, SPICE_MSG_PORT_INIT, item);
    init.name = (uint8_t *)i->name;
    init.name_size = strlen(i->name) + 1;
    init.opened = i->opened;
    spice_marshall_msg_port_init(m, &init);
}

static void spicevmc_red_channel_send_port_event(RedChannelClient *rcc,
                                                 SpiceMarshaller *m,
                                                 RedPipeItem *item)
{
    RedPortEventPipeItem *i = SPICE_UPCAST(RedPortEventPipeItem, item);
    SpiceMsgPortEvent event;

    red_channel_client_init_send_data(rcc, SPICE_MSG_PORT_EVENT, item);
    event.event = i->event;
    spice_marshall_msg_port_event(m, &event);
}

static void spicevmc_red_channel_send_item(RedChannelClient *rcc,
                                           RedPipeItem *item)
{
    SpiceMarshaller *m = red_channel_client_get_marshaller(rcc);

    switch (item->type) {
    case RED_PIPE_ITEM_TYPE_SPICEVMC_DATA:
        spicevmc_red_channel_send_data(rcc, m, item);
        break;
    case RED_PIPE_ITEM_TYPE_SPICEVMC_MIGRATE_DATA:
        spicevmc_red_channel_send_migrate_data(rcc, m, item);
        break;
    case RED_PIPE_ITEM_TYPE_PORT_INIT:
        spicevmc_red_channel_send_port_init(rcc, m, item);
        break;
    case RED_PIPE_ITEM_TYPE_PORT_EVENT:
        spicevmc_red_channel_send_port_event(rcc, m, item);
        break;
    default:
        spice_error("bad pipe item %d", item->type);
        return;
    }
    red_channel_client_begin_send_message(rcc);
}


static void
red_vmc_channel_class_init(RedVmcChannelClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    RedChannelClass *channel_class = RED_CHANNEL_CLASS(klass);

    object_class->get_property = red_vmc_channel_get_property;
    object_class->set_property = red_vmc_channel_set_property;
    object_class->constructed = red_vmc_channel_constructed;

    channel_class->handle_parsed = spicevmc_red_channel_client_handle_message_parsed;

    channel_class->config_socket = spicevmc_red_channel_client_config_socket;
    channel_class->on_disconnect = spicevmc_red_channel_client_on_disconnect;
    channel_class->send_item = spicevmc_red_channel_send_item;
    channel_class->alloc_recv_buf = spicevmc_red_channel_alloc_msg_rcv_buf;
    channel_class->release_recv_buf = spicevmc_red_channel_release_msg_rcv_buf;
    channel_class->handle_migrate_flush_mark = spicevmc_channel_client_handle_migrate_flush_mark;
    channel_class->handle_migrate_data = spicevmc_channel_client_handle_migrate_data;

    g_object_class_install_property(object_class,
                                    PROP_DEVICE_INSTANCE,
                                    g_param_spec_pointer("device-instance",
                                                         "device instance",
                                                         "Device instance for this channel",
                                                         G_PARAM_READWRITE |
                                                         G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
}

static void
red_vmc_channel_usbredir_class_init(RedVmcChannelUsbredirClass *klass)
{
    RedChannelClass *channel_class = RED_CHANNEL_CLASS(klass);

    channel_class->parser = spice_get_client_channel_parser(SPICE_CHANNEL_USBREDIR, NULL);
}

static void
red_vmc_channel_webdav_class_init(RedVmcChannelWebdavClass *klass)
{
    RedChannelClass *channel_class = RED_CHANNEL_CLASS(klass);

    channel_class->parser = spice_get_client_channel_parser(SPICE_CHANNEL_WEBDAV, NULL);
}

static void
red_vmc_channel_port_class_init(RedVmcChannelPortClass *klass)
{
    RedChannelClass *channel_class = RED_CHANNEL_CLASS(klass);

    channel_class->parser = spice_get_client_channel_parser(SPICE_CHANNEL_PORT, NULL);
}

static void spicevmc_connect(RedChannel *channel, RedClient *client,
    RedsStream *stream, int migration, int num_common_caps,
    uint32_t *common_caps, int num_caps, uint32_t *caps)
{
    RedChannelClient *rcc;
    RedVmcChannel *state;
    SpiceCharDeviceInstance *sin;
    SpiceCharDeviceInterface *sif;
    uint32_t type, id;

    state = RED_VMC_CHANNEL(channel);
    sin = state->chardev_sin;
    g_object_get(channel, "channel-type", &type, "id", &id, NULL);

    if (state->rcc) {
        spice_printerr("channel client %d:%d (%p) already connected, refusing second connection",
                       type, id, state->rcc);
        // TODO: notify client in advance about the in use channel using
        // SPICE_MSG_MAIN_CHANNEL_IN_USE (for example)
        reds_stream_free(stream);
        return;
    }

    rcc = red_channel_client_create(channel, client, stream, FALSE,
                                    num_common_caps, common_caps, num_caps, caps);
    if (!rcc) {
        return;
    }
    state->rcc = rcc;
    red_channel_client_ack_zero_messages_window(rcc);

    if (strcmp(sin->subtype, "port") == 0) {
        spicevmc_port_send_init(rcc);
    }

    if (!red_char_device_client_add(state->chardev, client, FALSE, 0, ~0, ~0,
                                    red_channel_client_is_waiting_for_migrate_data(rcc))) {
        spice_warning("failed to add client to spicevmc");
        red_channel_client_disconnect(rcc);
        return;
    }

    sif = spice_char_device_get_interface(state->chardev_sin);
    if (sif->state) {
        sif->state(sin, 1);
    }
}

RedCharDevice *spicevmc_device_connect(RedsState *reds,
                                       SpiceCharDeviceInstance *sin,
                                       uint8_t channel_type)
{
    RedCharDeviceSpiceVmc *dev_state;
    RedVmcChannel *state = red_vmc_channel_new(reds, channel_type, sin);

    dev_state = RED_CHAR_DEVICE_SPICEVMC(state->chardev);
    dev_state->channel = state;

    return RED_CHAR_DEVICE(dev_state);
}

/* Must be called from RedClient handling thread. */
void spicevmc_device_disconnect(RedsState *reds, SpiceCharDeviceInstance *sin)
{
    RedVmcChannel *channel;
    RedCharDeviceSpiceVmc *vmc = RED_CHAR_DEVICE_SPICEVMC(sin->st);

    channel = vmc->channel;
    vmc->channel = NULL;

    red_char_device_write_buffer_release(channel->chardev, &channel->recv_from_client_buf);
    /* FIXME */
    red_char_device_destroy(RED_CHAR_DEVICE(vmc));
    channel->chardev = NULL;
    sin->st = NULL;

    reds_unregister_channel(reds, RED_CHANNEL(channel));
    free(channel->pipe_item);
    red_channel_destroy(RED_CHANNEL(channel));
}

SPICE_GNUC_VISIBLE void spice_server_port_event(SpiceCharDeviceInstance *sin, uint8_t event)
{
    RedVmcChannel *state;

    if (sin->st == NULL) {
        spice_warning("no SpiceCharDeviceState attached to instance %p", sin);
        return;
    }

    /* FIXME */
    state = (RedVmcChannel *)red_char_device_opaque_get((RedCharDevice*)sin->st);
    if (event == SPICE_PORT_EVENT_OPENED) {
        state->port_opened = TRUE;
    } else if (event == SPICE_PORT_EVENT_CLOSED) {
        state->port_opened = FALSE;
    }

    if (state->rcc == NULL) {
        return;
    }

    spicevmc_port_send_event(state->rcc, event);
}

static void
red_char_device_spicevmc_dispose(GObject *object)
{
    RedCharDeviceSpiceVmc *self = RED_CHAR_DEVICE_SPICEVMC(object);

    g_clear_object(&self->channel);
}

static void
red_char_device_spicevmc_class_init(RedCharDeviceSpiceVmcClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    RedCharDeviceClass *char_dev_class = RED_CHAR_DEVICE_CLASS(klass);

    object_class->dispose = red_char_device_spicevmc_dispose;

    char_dev_class->read_one_msg_from_device = spicevmc_chardev_read_msg_from_dev;
    char_dev_class->send_msg_to_client = spicevmc_chardev_send_msg_to_client;
    char_dev_class->send_tokens_to_client = spicevmc_char_dev_send_tokens_to_client;
    char_dev_class->remove_client = spicevmc_char_dev_remove_client;
}

static void
red_char_device_spicevmc_init(RedCharDeviceSpiceVmc *self)
{
}

static RedCharDevice *
red_char_device_spicevmc_new(SpiceCharDeviceInstance *sin,
                             RedsState *reds,
                             void *opaque)
{
    return g_object_new(RED_TYPE_CHAR_DEVICE_SPICEVMC,
                        "sin", sin,
                        "spice-server", reds,
                        "client-tokens-interval", 0ULL,
                        "self-tokens", ~0ULL,
                        "opaque", opaque,
                        NULL);
}
