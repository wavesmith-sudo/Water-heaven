#include "playbackprofile.h"

#include <QJsonArray>
#include <QUrl>
#include <QUrlQuery>

namespace PlaybackProfile {
namespace {

QJsonObject condition(const char *cond, const char *property, const QString &value)
{
    // IsRequired=false: a file missing this metadata is still allowed to direct play.
    return {{"Condition", cond}, {"Property", property}, {"Value", value}, {"IsRequired", false}};
}

QJsonObject videoLimits(const char *codec, int maxBitDepth, const QString &profiles)
{
    QJsonArray conditions{condition("LessThanEqual", "Width", QString::number(MaxWidth)),
                          condition("LessThanEqual", "Height", QString::number(MaxHeight)),
                          condition("LessThanEqual", "VideoBitDepth", QString::number(maxBitDepth))};
    if (!profiles.isEmpty())
        conditions.append(condition("EqualsAny", "VideoProfile", profiles));
    return {{"Type", "Video"}, {"Codec", codec}, {"Conditions", conditions}};
}

QJsonObject directPlay(const char *type, const char *container, const char *video, const char *audio)
{
    QJsonObject o{{"Type", type}, {"Container", container}};
    if (*video)
        o["VideoCodec"] = video;
    if (*audio)
        o["AudioCodec"] = audio;
    return o;
}

} // namespace

QJsonObject deviceProfile()
{
    const QJsonArray direct{
        directPlay("Video", "mp4,m4v,mov", "h264,hevc,mpeg4,mpeg2video", "aac,mp3,ac3,eac3,flac,opus,alac"),
        directPlay("Video", "mkv,webm", "h264,hevc,vp8,vp9,mpeg4,mpeg2video", "aac,mp3,ac3,eac3,flac,opus,vorbis,dts"),
        directPlay("Video", "ts,mpegts", "h264,hevc,mpeg2video", "aac,mp3,ac3,eac3"),
        directPlay("Audio", "mp3", "", "mp3"),
        directPlay("Audio", "flac", "", "flac"),
        directPlay("Audio", "m4a,mp4", "", "aac,alac"),
    };
    const QJsonArray transcoding{
        // H.264 is the cheapest codec for the CM5 to decode in software, so it is the target.
        QJsonObject{{"Type", "Video"}, {"Container", "ts"}, {"Protocol", "hls"}, {"Context", "Streaming"},
                    {"VideoCodec", "h264"}, {"AudioCodec", "aac,mp3,ac3"},
                    {"MaxAudioChannels", QString::number(MaxAudioChannels)},
                    {"MinSegments", 1}, {"BreakOnNonKeyFrames", true}},
        QJsonObject{{"Type", "Audio"}, {"Container", "mp3"}, {"Protocol", "http"}, {"Context", "Streaming"},
                    {"AudioCodec", "mp3"}, {"MaxAudioChannels", "2"}},
    };
    const QJsonArray codecs{
        videoLimits("h264", 8, QStringLiteral("high|main|baseline|constrained baseline")),
        videoLimits("hevc", 10, QStringLiteral("main|main 10")),
        videoLimits("vp9", 10, {}),
        videoLimits("mpeg4", 8, {}),
        videoLimits("mpeg2video", 8, {}),
        QJsonObject{{"Type", "VideoAudio"},
                    {"Conditions", QJsonArray{condition("LessThanEqual", "AudioChannels", QString::number(MaxAudioChannels))}}},
    };
    return {{"Name", "Retro Arcade CM5"},
            {"MaxStreamingBitrate", MaxStreamingBitrate},
            {"MaxStaticBitrate", MaxStreamingBitrate * 2},
            {"MusicStreamingTranscodingBitrate", 192000},
            {"DirectPlayProfiles", direct},
            {"TranscodingProfiles", transcoding},
            {"CodecProfiles", codecs},
            {"ContainerProfiles", QJsonArray{}},
            // The player does not render subtitles yet; none are requested (SubtitleStreamIndex -1).
            {"SubtitleProfiles", QJsonArray{}}};
}

QJsonObject playbackInfoRequest(const QString &userId, bool allowDirect)
{
    return {{"UserId", userId},
            {"DeviceProfile", deviceProfile()},
            {"MaxStreamingBitrate", MaxStreamingBitrate},
            {"MaxAudioChannels", MaxAudioChannels},
            {"SubtitleStreamIndex", -1},
            {"EnableDirectPlay", allowDirect},
            {"EnableDirectStream", allowDirect},
            {"EnableTranscoding", true},
            {"AllowVideoStreamCopy", allowDirect},
            {"AllowAudioStreamCopy", true},
            {"AutoOpenLiveStream", true}};
}

Decision decide(const QJsonObject &info, const QString &server, const QString &itemId, const QString &kind,
                const QString &token, const QString &deviceId)
{
    Decision d;
    d.playSessionId = info.value("PlaySessionId").toString();
    const QString errorCode = info.value("ErrorCode").toString();
    const QJsonArray sources = info.value("MediaSources").toArray();
    if (!errorCode.isEmpty() || sources.isEmpty()) {
        d.error = errorCode == "NotAllowed" ? QStringLiteral("This account is not allowed to play this item")
                                            : QStringLiteral("The server has no playable version of this item");
        return d;
    }
    // Jellyfin orders sources by preference; the first one is the default version.
    const QJsonObject source = sources.first().toObject();
    d.mediaSourceId = source.value("Id").toString();
    d.container = source.value("Container").toString();
    d.transcodeReasons = source.value("TranscodeReasons").toVariant().toStringList().join(',');
    if (d.transcodeReasons.isEmpty())
        d.transcodeReasons = source.value("TranscodeReasons").toString();

    const QString transcodingUrl = source.value("TranscodingUrl").toString();
    if (source.value("SupportsDirectPlay").toBool() || (source.value("SupportsDirectStream").toBool() && transcodingUrl.isEmpty())) {
        d.method = source.value("SupportsDirectPlay").toBool() ? QStringLiteral("DirectPlay") : QStringLiteral("DirectStream");
        // Qt Multimedia's MediaPlayer cannot send request headers, so the stream URL is the one
        // place the token travels as a query parameter. Never log it.
        QUrlQuery q;
        q.addQueryItem("static", "true");
        if (!d.mediaSourceId.isEmpty())
            q.addQueryItem("MediaSourceId", d.mediaSourceId);
        if (!d.playSessionId.isEmpty())
            q.addQueryItem("PlaySessionId", d.playSessionId);
        q.addQueryItem("DeviceId", deviceId);
        q.addQueryItem("api_key", token);
        d.url = server + "/" + kind + "/" + QUrl::toPercentEncoding(itemId) + "/stream?" + q.toString(QUrl::FullyEncoded);
        return d;
    }
    if (source.value("SupportsTranscoding").toBool(true) && !transcodingUrl.isEmpty()) {
        d.method = QStringLiteral("Transcode");
        // TranscodingUrl is relative to the server root, so a reverse-proxy base path kept in
        // `server` (https://host/jellyfin) is preserved.
        QUrl u(server + (transcodingUrl.startsWith('/') ? transcodingUrl : "/" + transcodingUrl));
        QUrlQuery q(u);
        if (!q.hasQueryItem("api_key") && !q.hasQueryItem("ApiKey"))
            q.addQueryItem("api_key", token);
        if (q.queryItemValue("DeviceId").isEmpty() && q.queryItemValue("deviceId").isEmpty()) {
            q.removeAllQueryItems("DeviceId");
            q.removeAllQueryItems("deviceId");
            q.addQueryItem("DeviceId", deviceId);
        }
        u.setQuery(q);
        d.url = u.toString(QUrl::FullyEncoded);
        return d;
    }
    d.error = QStringLiteral("This file can't be played on this device and the server won't transcode it");
    return d;
}

} // namespace PlaybackProfile
