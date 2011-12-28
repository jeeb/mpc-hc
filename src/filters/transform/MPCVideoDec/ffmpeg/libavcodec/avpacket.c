/*
 * AVPacket functions for libavcodec
 * Copyright (c) 2000, 2001, 2002 Fabrice Bellard
 *
 * This file is part of Libav.
 *
 * Libav is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Libav is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with Libav; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "avcodec.h"
#include "libavutil/avassert.h"


void av_destruct_packet_nofree(AVPacket *pkt)
{
    pkt->data = NULL; pkt->size = 0;
}

void av_destruct_packet(AVPacket *pkt)
{
    av_free(pkt->data);
    pkt->data = NULL; pkt->size = 0;
}

void av_init_packet(AVPacket *pkt)
{
    pkt->pts   = AV_NOPTS_VALUE;
    pkt->dts   = AV_NOPTS_VALUE;
    pkt->pos   = -1;
    pkt->duration = 0;
    pkt->convergence_duration = 0;
    pkt->flags = 0;
    pkt->stream_index = 0;
    pkt->destruct= NULL;
}

int av_new_packet(AVPacket *pkt, int size)
{
    uint8_t *data= NULL;
    if((unsigned)size < (unsigned)size + FF_INPUT_BUFFER_PADDING_SIZE)
        data = av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE);
    if (data){
        memset(data + size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    }else
        size=0;

    av_init_packet(pkt);
    pkt->data = data;
    pkt->size = size;
    pkt->destruct = av_destruct_packet;
    if(!data)
        return AVERROR(ENOMEM);
    return 0;
}

void av_shrink_packet(AVPacket *pkt, int size)
{
    if (pkt->size <= size) return;
    pkt->size = size;
    memset(pkt->data + size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
}

int av_grow_packet(AVPacket *pkt, int grow_by)
{
    void *new_ptr;
    av_assert0((unsigned)pkt->size <= INT_MAX - FF_INPUT_BUFFER_PADDING_SIZE);
    if (!pkt->size)
        return av_new_packet(pkt, grow_by);
    if ((unsigned)grow_by > INT_MAX - (pkt->size + FF_INPUT_BUFFER_PADDING_SIZE))
        return -1;
    new_ptr = av_realloc(pkt->data, pkt->size + grow_by + FF_INPUT_BUFFER_PADDING_SIZE);
    if (!new_ptr)
        return AVERROR(ENOMEM);
    pkt->data = new_ptr;
    pkt->size += grow_by;
    memset(pkt->data + pkt->size, 0, FF_INPUT_BUFFER_PADDING_SIZE);
    return 0;
}

#define DUP_DATA(dst, src, size, padding) \
    do { \
        void *data; \
        if (padding) { \
            if ((unsigned)(size) > (unsigned)(size) + FF_INPUT_BUFFER_PADDING_SIZE) \
                goto failed_alloc; \
            data = av_malloc(size + FF_INPUT_BUFFER_PADDING_SIZE); \
        } else { \
            data = av_malloc(size); \
        } \
        if (!data) \
            goto failed_alloc; \
        memcpy(data, src, size); \
        if (padding) \
            memset((uint8_t*)data + size, 0, FF_INPUT_BUFFER_PADDING_SIZE); \
        dst = data; \
    } while(0)

int av_dup_packet(AVPacket *pkt)
{
    AVPacket tmp_pkt;

    if (((pkt->destruct == av_destruct_packet_nofree) || (pkt->destruct == NULL)) && pkt->data) {
        tmp_pkt = *pkt;

        pkt->data      = NULL;
        DUP_DATA(pkt->data, tmp_pkt.data, pkt->size, 1);
        pkt->destruct = av_destruct_packet;
    }
    return 0;
failed_alloc:
    av_destruct_packet(pkt);
    return AVERROR(ENOMEM);
}

void av_free_packet(AVPacket *pkt)
{
    if (pkt) {
        if (pkt->destruct) pkt->destruct(pkt);
        pkt->data = NULL; pkt->size = 0;
    }
}
