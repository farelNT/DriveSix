#ifndef VIDEO_H
#define VIDEO_H

#include <SDL2/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    // Vidéo
    AVFormatContext*   fmt_ctx;
    AVCodecContext*    codec_ctx;
    AVFrame*           frame;
    AVFrame*           frame_rgb;
    AVPacket*          packet;
    struct SwsContext* sws_ctx;
    SDL_Texture*       texture;
    int                video_stream;
    
    // Audio
    int                audio_stream;
    AVCodecContext*    audio_codec_ctx;
    struct SwrContext* swr_ctx;
    SDL_AudioDeviceID  audio_dev;

    // Propriétés
    int                largeur;
    int                hauteur;
    double             fps;
    Uint32             tps_debut;
    int                frame_courant;
    int                termine;
} VideoPlayer;

int video_init(VideoPlayer* vp, SDL_Renderer* ren, const char* chemin) {
    memset(vp, 0, sizeof(VideoPlayer));
    vp->termine = 0;
    vp->video_stream = -1;
    vp->audio_stream = -1;

    if (avformat_open_input(&vp->fmt_ctx, chemin, NULL, NULL) < 0) return 0;
    if (avformat_find_stream_info(vp->fmt_ctx, NULL) < 0) return 0;

    // 1. Recherche des flux vidéo et audio
    for (int i = 0; i < (int)vp->fmt_ctx->nb_streams; i++) {
        if (vp->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO && vp->video_stream < 0)
            vp->video_stream = i;
        if (vp->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO && vp->audio_stream < 0)
            vp->audio_stream = i;
    }

    // 2. Setup Vidéo
    if (vp->video_stream >= 0) {
        AVCodecParameters* v_cp = vp->fmt_ctx->streams[vp->video_stream]->codecpar;
        const AVCodec* v_codec = avcodec_find_decoder(v_cp->codec_id);
        vp->codec_ctx = avcodec_alloc_context3(v_codec);
        avcodec_parameters_to_context(vp->codec_ctx, v_cp);
        avcodec_open2(vp->codec_ctx, v_codec, NULL);
        
        vp->largeur = vp->codec_ctx->width;
        vp->hauteur = vp->codec_ctx->height;
        AVRational fps_r = vp->fmt_ctx->streams[vp->video_stream]->avg_frame_rate;
        vp->fps = (fps_r.den > 0) ? (double)fps_r.num / fps_r.den : 25.0;

        vp->sws_ctx = sws_getContext(vp->largeur, vp->hauteur, vp->codec_ctx->pix_fmt,
                                     vp->largeur, vp->hauteur, AV_PIX_FMT_RGB24,
                                     SWS_BILINEAR, NULL, NULL, NULL);
        vp->texture = SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, vp->largeur, vp->hauteur);
    }

    // 3. Setup Audio (Correction pour entendre le son)
    if (vp->audio_stream >= 0) {
        AVCodecParameters* a_cp = vp->fmt_ctx->streams[vp->audio_stream]->codecpar;
        const AVCodec* a_codec = avcodec_find_decoder(a_cp->codec_id);
        vp->audio_codec_ctx = avcodec_alloc_context3(a_codec);
        avcodec_parameters_to_context(vp->audio_codec_ctx, a_cp);
        avcodec_open2(vp->audio_codec_ctx, a_codec, NULL);

        SDL_AudioSpec wanted, have;
        SDL_memset(&wanted, 0, sizeof(wanted));
        wanted.freq = vp->audio_codec_ctx->sample_rate;
        wanted.format = AUDIO_S16SYS;
        wanted.channels = vp->audio_codec_ctx->ch_layout.nb_channels;
        wanted.samples = 1024;

        vp->audio_dev = SDL_OpenAudioDevice(NULL, 0, &wanted, &have, 0);
        
        // Création du resampler pour convertir le son vers le format SDL
        swr_alloc_set_opts2(&vp->swr_ctx, 
            &vp->audio_codec_ctx->ch_layout, AV_SAMPLE_FMT_S16, vp->audio_codec_ctx->sample_rate,
            &vp->audio_codec_ctx->ch_layout, vp->audio_codec_ctx->sample_fmt, vp->audio_codec_ctx->sample_rate,
            0, NULL);
        swr_init(vp->swr_ctx);
        
        SDL_PauseAudioDevice(vp->audio_dev, 0); // Démarre le son
    }

    vp->frame = av_frame_alloc();
    vp->frame_rgb = av_frame_alloc();
    vp->packet = av_packet_alloc();

    int buf_size = av_image_get_buffer_size(AV_PIX_FMT_RGB24, vp->largeur, vp->hauteur, 1);
    uint8_t* buf = (uint8_t*)av_malloc(buf_size);
    av_image_fill_arrays(vp->frame_rgb->data, vp->frame_rgb->linesize, buf, AV_PIX_FMT_RGB24, vp->largeur, vp->hauteur, 1);

    vp->tps_debut = SDL_GetTicks();
    return 1;
}

int video_update(VideoPlayer* vp, SDL_Renderer* ren, int screen_w, int screen_h) {
    if (vp->termine) return 0;

    Uint32 elapsed = SDL_GetTicks() - vp->tps_debut;
    int frame_target = (int)((elapsed / 1000.0) * vp->fps);

    int video_ready = 0;
    while (!video_ready) {
        if (av_read_frame(vp->fmt_ctx, vp->packet) < 0) {
            vp->termine = 1;
            return 0;
        }

        // Gérer l'AUDIO
        if (vp->packet->stream_index == vp->audio_stream) {
            if (avcodec_send_packet(vp->audio_codec_ctx, vp->packet) >= 0) {
                AVFrame* a_frame = av_frame_alloc();
                if (avcodec_receive_frame(vp->audio_codec_ctx, a_frame) >= 0) {
                    uint8_t* out_data[1];
                    int out_samples = av_rescale_rnd(a_frame->nb_samples, vp->audio_codec_ctx->sample_rate, vp->audio_codec_ctx->sample_rate, AV_ROUND_UP);
                    av_samples_alloc(out_data, NULL, vp->audio_codec_ctx->ch_layout.nb_channels, out_samples, AV_SAMPLE_FMT_S16, 0);
                    
                    int converted = swr_convert(vp->swr_ctx, out_data, out_samples, (const uint8_t**)a_frame->data, a_frame->nb_samples);
                    int out_size = converted * vp->audio_codec_ctx->ch_layout.nb_channels * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                    
                    SDL_QueueAudio(vp->audio_dev, out_data[0], out_size);
                    av_freep(&out_data[0]);
                }
                av_frame_free(&a_frame);
            }
        } 
        // Gérer la VIDÉO
        else if (vp->packet->stream_index == vp->video_stream) {
            if (avcodec_send_packet(vp->codec_ctx, vp->packet) >= 0) {
                if (avcodec_receive_frame(vp->codec_ctx, vp->frame) >= 0) {
                    if (vp->frame_courant >= frame_target - 1) {
                        sws_scale(vp->sws_ctx, (const uint8_t* const*)vp->frame->data, vp->frame->linesize, 0, vp->hauteur, vp->frame_rgb->data, vp->frame_rgb->linesize);
                        SDL_UpdateTexture(vp->texture, NULL, vp->frame_rgb->data[0], vp->frame_rgb->linesize[0]);
                        video_ready = 1;
                    }
                    vp->frame_courant++;
                }
            }
        }
        av_packet_unref(vp->packet);
    }

    SDL_Rect dst = {0, 0, screen_w, screen_h};
    SDL_RenderCopy(ren, vp->texture, NULL, &dst);
    return 1;
}

void video_free(VideoPlayer* vp) {
    if (vp->audio_dev) SDL_CloseAudioDevice(vp->audio_dev);
    if (vp->swr_ctx)   swr_free(&vp->swr_ctx);
    if (vp->texture)   SDL_DestroyTexture(vp->texture);
    if (vp->sws_ctx)   sws_freeContext(vp->sws_ctx);
    if (vp->frame)     av_frame_free(&vp->frame);
    if (vp->frame_rgb) av_frame_free(&vp->frame_rgb);
    if (vp->packet)    av_packet_free(&vp->packet);
    if (vp->codec_ctx) avcodec_free_context(&vp->codec_ctx);
    if (vp->audio_codec_ctx) avcodec_free_context(&vp->audio_codec_ctx);
    if (vp->fmt_ctx)   avformat_close_input(&vp->fmt_ctx);
    memset(vp, 0, sizeof(VideoPlayer));
}

#endif
