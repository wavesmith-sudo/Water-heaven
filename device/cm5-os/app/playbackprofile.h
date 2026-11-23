#pragma once
#include <QJsonObject>
#include <QString>

// What the CM5 image can play, described as a Jellyfin DeviceProfile, and the client-side
// half of PlaybackInfo negotiation.
//
// The profile mirrors the GStreamer plugins in br2-external/configs/cm5_media_defconfig:
//   containers  mp4/m4v/mov (isomp4), mkv/webm (matroska), ts (mpegtsdemux), mp3/flac/m4a
//   video       h264, hevc, vp8, vp9, mpeg4, mpeg2video: all software-decoded by gst1-libav,
//               because the image does not ship v4l2codecs yet (CM5 has an HEVC block, no H.264 block)
//   audio       aac, mp3, ac3, eac3, flac, opus, vorbis, alac, dts (gst1-libav)
// Video is capped at 1920x1080 so software decode has headroom; anything larger, deeper or in
// another codec is transcoded by the server to H.264/AAC over HLS (hlsdemux + mpegtsdemux).
// None of these limits have been measured on CM5 hardware yet (docs/MEDIA_CENTER.md).
namespace PlaybackProfile {

constexpr int MaxWidth = 1920;
constexpr int MaxHeight = 1080;
constexpr qint64 MaxStreamingBitrate = 40000000; // bits/s, for remote/cloud servers
constexpr int MaxAudioChannels = 6;

QJsonObject deviceProfile();

// The POST /Items/{id}/PlaybackInfo body. allowDirect=false asks the server to transcode,
// which is the fallback after direct play fails on the device.
QJsonObject playbackInfoRequest(const QString &userId, bool allowDirect);

struct Decision {
    QString url;            // what Qt Multimedia should open (carries the token, never log it)
    QString method;         // DirectPlay | DirectStream | Transcode (Jellyfin PlayMethod)
    QString mediaSourceId;
    QString playSessionId;
    QString container;
    QString transcodeReasons;
    QString error;          // non-empty when nothing on the device or server can play the item
};

// Picks the playback URL from a PlaybackInfo response. Pure function so it is unit tested.
// kind is "Audio" or "Videos" (the stream endpoint prefix).
Decision decide(const QJsonObject &playbackInfo, const QString &server, const QString &itemId,
                const QString &kind, const QString &token, const QString &deviceId);

} // namespace PlaybackProfile
