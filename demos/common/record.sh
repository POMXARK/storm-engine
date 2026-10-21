#!/bin/bash
# Records a demo run to a video: starts the demo, captures its window with ffmpeg and plays a
# scripted sequence of inputs, then quits the engine.
# Usage: record.sh <demo dir> <engine executable> <out.mp4> <step>...
# Steps run in order once the window is up and the recording has started:
#   sleep:S          wait S seconds (fractions allowed)
#   key:K            press and release key K (xdotool key names: 1, Escape, space, w...)
#   hold:K:S         hold key K for S seconds
#   type:TEXT        type TEXT as a player would, a letter at a time (no colons in it)
#   move:DX:DY:S     move the mouse by DX, DY pixels evenly over S seconds
#   click[:X:Y]      left click at X, Y pixels of the window, or where the mouse is. The engine keeps the
#                    mouse relative on Linux: once the window has the focus only move: steps move it
#   mousedown:B      press mouse button B (1 left, 3 right) and keep it, till mouseup:B; the editor's
#                    scene camera flies while the right one is held
# The engine is stopped after the last step, so end with the time the result should stay in view.
# Each recording is listed in index.tsv next to the video: date, demo, engine commit, file.
# ENGINE_ARGS are passed to the engine after run.sh (ENGINE_ARGS=--editor records the editor).
# PREVIEW=S also writes the frame at S seconds next to the video as a .jpg. The take shown on GitHub
# goes to demos/<demo>/media/<demo>.{mp4,jpg}, the gallery in demos/README.md links it.
# RECORD_AUDIO=1 records the sound of the engine too: it plays into a null PulseAudio sink of its own,
# so nothing else gets into the recording, ffmpeg captures its monitor and a loopback plays it on the
# default output, so the run is heard as well.
# Needs X11, xdotool and ffmpeg with libx264 (and pactl and the pulse input of ffmpeg for the sound).
set -euo pipefail
DEMO="$(realpath "$1")"
ENGINE="$(realpath "$2")"
OUT="$(realpath -m "$3")"
shift 3
FPS=30
AUDIO_ARGS=()
AUDIO_MODULES=()
if [ "${RECORD_AUDIO:-0}" = 1 ]; then
    OUTPUT=$(pactl get-default-sink)
    AUDIO_MODULES+=("$(pactl load-module module-null-sink sink_name=storm_record \
        sink_properties=device.description=storm_record)")
    AUDIO_MODULES+=("$(pactl load-module module-loopback source=storm_record.monitor sink="$OUTPUT" \
        latency_msec=40)")
    # libpulse (also under the ALSA pulse plugin) plays to this sink instead of the default one
    export PULSE_SINK=storm_record
    # small fragments and deep input queues: with the defaults ffmpeg waits on second-long pulse
    # fragments and grabs a frame of the window between them, the video freezes for a second at a time
    AUDIO_ARGS=(-thread_queue_size 1024 -f pulse -fragment_size 3840 -i storm_record.monitor -c:a aac -b:a 160k)
fi


# shellcheck disable=SC2086 # the options are split into words on purpose
"$DEMO/run.sh" "$ENGINE" ${ENGINE_ARGS:-} > "$DEMO/engine.out" 2>&1 &
ENGINE_PID=$!
FFMPEG_PID=
cleanup() {
    if [ -n "$FFMPEG_PID" ]; then kill -INT "$FFMPEG_PID" 2>/dev/null || true; wait "$FFMPEG_PID" 2>/dev/null || true; fi
    # SDL turns SIGTERM into a quit event the scripts do not handle, so the engine gets a moment
    # and is then killed
    kill "$ENGINE_PID" 2>/dev/null || true
    for _ in $(seq 10); do kill -0 "$ENGINE_PID" 2>/dev/null || break; sleep 0.2; done
    kill -KILL "$ENGINE_PID" 2>/dev/null || true
    wait "$ENGINE_PID" 2>/dev/null || true
    # the loopback goes before the sink it plays from; a pulse server busy with the killed client answers
    # "Connection failure: Timeout" once, and a loopback left behind hangs the sound: tried again
    for ((m = ${#AUDIO_MODULES[@]} - 1; m >= 0; m--)); do
        for _ in 1 2 3; do timeout 5 pactl unload-module "${AUDIO_MODULES[m]}" && break; sleep 1; done
    done
}
trap cleanup EXIT

# run.sh execs the engine, so the window belongs to this pid; asset generation may take a while
WID=
for _ in $(seq 120); do
    WID=$(xdotool search --onlyvisible --pid "$ENGINE_PID" 2>/dev/null | tail -1 || true)
    [ -n "$WID" ] && break
    sleep 0.5
done
[ -n "$WID" ] || { echo "record.sh: no engine window" >&2; exit 1; }
xdotool windowactivate --sync "$WID" 2>/dev/null || true
# the engine keeps the mouse relative: where the pointer is as it takes it, the game's cursor starts.
# The centre of the window, so that move: steps from it reach the same place every take
eval "$(xdotool getwindowgeometry --shell "$WID")"
xdotool mousemove --window "$WID" $((WIDTH / 2)) $((HEIGHT / 2))
# the first frames are spent loading the scene
sleep 3

mkdir -p "$(dirname "$OUT")"
eval "$(xdotool getwindowgeometry --shell "$WID")"
# x264 needs even sizes
WIDTH=$((WIDTH / 2 * 2))
HEIGHT=$((HEIGHT / 2 * 2))
ffmpeg -loglevel error -y -thread_queue_size 1024 -f x11grab -draw_mouse 0 -framerate "$FPS" -window_id "$WID" \
    -video_size "${WIDTH}x${HEIGHT}" -i "$DISPLAY" "${AUDIO_ARGS[@]}" \
    -c:v libx264 -preset veryfast -crf 23 -pix_fmt yuv420p -movflags +faststart "$OUT" &
FFMPEG_PID=$!

for step in "$@"; do
    IFS=: read -r action a b c <<< "$step"
    case "$action" in
        sleep) sleep "$a" ;;
        # a short hold: a press and release within one frame may be missed
        key) xdotool keydown "$a"; sleep 0.15; xdotool keyup "$a" ;;
        type) xdotool type --delay 150 "$a" ;;
        hold) xdotool keydown "$a"; sleep "$b"; xdotool keyup "$a" ;;
        # the press is held over a few frames, like the keys
        click)
            [ -z "$a" ] || { xdotool mousemove --window "$WID" "$a" "$b"; sleep 0.15; }
            xdotool mousedown 1; sleep 0.15; xdotool mouseup 1 ;;
        mousedown) xdotool mousedown "$a" ;;
        mouseup) xdotool mouseup "$a" ;;
        move)
            n=$(awk -v s="$c" 'BEGIN { n = int(s * 30 + 0.5); print (n < 1 ? 1 : n) }')
            for i in $(seq "$n"); do
                xdotool mousemove_relative -- $((a * i / n - a * (i - 1) / n)) $((b * i / n - b * (i - 1) / n))
                sleep 0.033
            done ;;
        *) echo "record.sh: unknown step $step" >&2; exit 1 ;;
    esac
done
# an index next to the videos tells which engine build each shows
COMMIT=$(git -C "$DEMO" describe --always --dirty 2>/dev/null || echo unknown)
printf '%s\t%s\t%s\t%s\n' "$(date -Iseconds)" "$(basename "$DEMO")" "$COMMIT" "$(basename "$OUT")" \
    >> "$(dirname "$OUT")/index.tsv"
if [ -n "${PREVIEW:-}" ]; then
    # the frame is read from a finished file: ffmpeg writes the index (moov) only when it stops
    kill -INT "$FFMPEG_PID" 2>/dev/null || true
    wait "$FFMPEG_PID" 2>/dev/null || true
    FFMPEG_PID=
    ffmpeg -v error -y -ss "$PREVIEW" -i "$OUT" -frames:v 1 -q:v 4 "${OUT%.*}.jpg"
fi
echo "record.sh: $OUT"
