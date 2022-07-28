# libffmpeg-zmq-streaming

Originally, this code would encode camera or directory images to h264 and just send
those packets via zmq to a subscriber.

I have since enabled RTP transport, so the stream can be played with ffplay and VLC
(with h264 only).

## Binaries

`encode_video_fromdir` reads images from a directory and sends it to an rtp host and port. An SDP file is written and
needs to be used by the client (does not change unless you change parameters).

Usage is

```
./build/encode_video_fromdir ~/Downloads/images/ jpeg 127.0.0.1 5006
```

Beware that **an even-numbered RTP port** is necessary otherwise VLC will not receive
packets. This is because the live555 library VLC used discards the last bit of the port
number, so the port gets changed when odd (wtf).

`encode_from_zmq` receives images via zmq and sends it to an rtp host and port. Check [below](#Encode-From-ZMQ) for details. 

# Streaming to VLC

The current code uses VP9, which won't work with VLC, but the h264 stuff is still there
commented out.

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

This streaming process has a delay of at least 0.5s, which I could not get down, even with
`--network-caching=0`

## Streaming to `ffplay`

The lowest-latency invocation I have found is

```
ffplay -probesize 32 -analyzeduration 0 -fflags nobuffer -fflags discardcorrupt -flags low_delay -sync ext -framedrop -avioflags direct -protocol_whitelist "file,udp,rtp" test.sdp
```

And that also has 200ms delay.

You can use `decode_rtp <sdpfile>` binary to be a little better. You can change the
`max_delay` to adjust reorder tolerance in the code, or disable reordering by setting it to 0. This doesn't work over lossy links though.

# Dependencies

Unfortunately this is a bit shitty because there is no cmake support for libffmpeg. I pilfered a cmake script for finding ffmpeg from VTK (i think),
but they do not include a bunch of library dependencies (not sure if forgotten or not necessary for certain versions of libffmpeg), so I hacked them in there until it worked.

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

This code needs ffmpeg version 4.4 right now, because it would need to update for their
breaking API changes. If you have that installed to some prefix that's not a system
prefix, use `-DFFMPEG_ROOT=<prefix>` during `cmake`.

# Performance

When sender and receiver run on the same host, no streaming delay is observed, save for
the time it takes to encode and decode. There is not a single frame of delay, so the
method can be considered to be optimal on a lossless link.

Over a VPN connection via Azure to the same country (germany), performance is still very
good, the loss is low enough that only rarely a frame is missed.

Over a VPN connection via Azure US and a much more delayed and lossly link, this is
unuseable, as over UDP seemingly too few packets get lost to even decode a single frame
in time. We would need to try TCP for this, which `libavcodec` does not support for RTP.
RTSP would have to be investigated.

Previously, I was sending plain h264 packets over Zmq, which can use TCP. Performance
for this was tolerable for high resolution images, with some artifacts and jankiness,
but might be improved with smaller image size. Special consideration must be taken with
setting `conflate`, `sendhwm` and `recvhwm` options in senders/receivers to avoid
unintentional buffering and delays.

# Encode From ZMQ

Example usage is

```
./build/encode_from_zmq --host 192.168.101.10 --port 6021 --reciever 192.168.19.202 --stream-port 8000
```

```
USAGE: 

   ./encode_from_zmq  [-z] [--zoom-factor <zoom-factor as Float>] [-b
                      <Bitrate as Integer>] [-f <fps as Integer>]
                      [--stream-port <Port as Integer>] [-R <Reciever as
                      String>] [-t <topic as string>] [-p <password as
                      string>] [-u <user as string>] [--port <Port as
                      Integer>] [-H <Host as String>] [--] [--version]
                      [-h]


Where: 

   -z,  --zoom
     Enable Zoom.

   --zoom-factor <zoom-factor as Float>
     zoom-factor of stream

   -b <Bitrate as Integer>,  --bitrate <Bitrate as Integer>
     bitrate of stream

   -f <fps as Integer>,  --fps <fps as Integer>
     fps of stream

   --stream-port <Port as Integer>
     port of stream

   -R <Reciever as String>,  --reciever <Reciever as String>
     reciever of the rtp stream

   -t <topic as string>,  --topic <topic as string>
     topic to filter zmq messages

   -p <password as string>,  --password <password as string>
     password to authenticate at zmq

   -u <user as string>,  --user <user as string>
     user to authenticate at zmq

   --port <Port as Integer>
     port of incoming zmq messages

   -H <Host as String>,  --host <Host as String>
     host of incoming zmq messages

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.


   Recieve zmq from code, send via RTP
```