# libav-zmq-streaming
Originally, this code would encode camera or directory images to h264 and just send
those packets via zmq to a subscriber.

I have since enabled RTP transport, so the stream can be played with ffplay and VLC.

## Binaries

`encode_video_zmq` is no longer aptly named, as it does not use zmq. Instead it reads
images from a directory and sends it to an rtp host and port. An SDP file is written and
needs to be used by the client (does not change unless you change parameters).

Usage is

```
./build/encode_video_zmq ~/Downloads/images/ jpeg 127.0.0.1 5006
```

Beware that **an even-numbered RTP port** is necessary otherwise VLC will not receive
packets. This is because the live555 library VLC used discards the last bit of the port
number, so the port gets changed when odd (wtf).


# Streaming to VLC

VLC can stream this with

```
/Applications/VLC.app/Contents/MacOS/VLC -vvv test.sdp
```

On ios, you can also use the VLC app and then follow these steps.

1. Serve the SDP file over HTTP somehow – there is no way to open a file on the device
   itself. For test purposes you can run `python3 -m http.server` from the directory
   where the SDP file is located
2. Open the VLC app, go to **Network**, then enter `http://<host ip>:8000/test.sdp` and
   tap **Open Network Stream**
3. Start the stream on the host with `./build/encode_video_zmq ~/Downloads/images/ jpeg <ios ip> 5006` or similar
4. The stream should now appear


# Dependencies
Unfortunately this is a bit shitty because there is no cmake support for libav. I pilfered a cmake script for finding ffmpeg from VTK (i think),
but they do not include a bunch of library dependencies (not sure if forgotten or not necessary for certain versions of libav), so I hacked them in there until it worked.

Basically you need most (but strangely not all) thats outlined here: https://trac.ffmpeg.org/wiki/CompilationGuide/Ubuntu

Depending on which codecs you have enabled during ffmpeg install
(`--enable-libtheora`, `--enable-libvpx`, `--enable-libx264`), you need to adapt the
`FindFFMPEG.cmake` script and add to `libdeps`.

- Theora: `theora;theoraenc;vorbis;vorbisenc`
- x264: `x264`
- vpx: `vpx`

Theora I actually could not get to work because libvacodec complains that `Configuration
is missing` or something. Not sure if I accidentally fixed this by now.

VP9 packetization for RTP transport is experimental, so you need

```
this->ofmt_ctx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
```

However, VLC does not seem to support that so the point is moot, unless you can use
ffplay or write your own receiver with libavcodec.
