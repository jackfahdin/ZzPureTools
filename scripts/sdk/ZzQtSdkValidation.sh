#!/usr/bin/env bash
# Sourced by the SDK verifier and its lightweight behavior tests.
sdk_assert_runtime() {
    local sdk=$1 executable=$2 output=$3 line resolved found=0
    local pattern='libQt6[^[:space:]]+[[:space:]]+=>[[:space:]]+(.+) \(0x[[:xdigit:]]+\)'
    sdk=$(realpath -e "$sdk") || return 1
    env -u LD_PRELOAD -u LD_LIBRARY_PATH ldd "$executable" >"$output" 2>&1 || return 1
    if grep -Fq 'not found' "$output"; then
        printf 'Missing runtime dependency: %s\n' "$executable" >&2; return 1
    fi
    while IFS= read -r line; do
        if [[ "$line" =~ $pattern ]]; then
            found=1
            resolved=$(realpath -e "${BASH_REMATCH[1]}") || return 1
            if [[ "$resolved" != "$sdk/"* ]]; then
                printf 'Foreign Qt runtime: %s loads %s\n' "$executable" "$resolved" >&2; return 1
            fi
        elif [[ "$line" == *libQt6* ]]; then
            printf 'Unrecognized Qt runtime mapping: %s\n' "$line" >&2; return 1
        fi
    done <"$output"
    ((found)) || { printf 'No Qt runtime found: %s\n' "$executable" >&2; return 1; }
}

sdk_require_audio_playback() {
    local output=$1
    if grep -Eq 'SKIP[[:space:]]+:' "$output" ||
        ! grep -Eq 'PASS[[:space:]]+: ZzExampleAudioMeterSourceTest::playsIndependentChannelsAndPausesWhenHidden\(\)' "$output"; then
        printf 'Real audio playback did not pass without skips: %s\n' "$output" >&2
        return 1
    fi
}
