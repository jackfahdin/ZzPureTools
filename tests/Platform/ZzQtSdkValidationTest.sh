#!/usr/bin/env bash
set -euo pipefail
repo=$(realpath "${1:?repository root required}")
helper="$repo/scripts/sdk/ZzQtSdkValidation.sh"
[[ -f "$helper" ]] || { echo 'FAIL: runtime SDK validation is missing'; exit 1; }
source "$helper"
scratch=$(mktemp -d -t zz-sdk-runtime-contract-XXXXXXXX)
trap 'rm -rf -- "$scratch"' EXIT
mkdir -p "$scratch/sdk/lib" "$scratch/alien/lib"
printf 'int qt_fixture(void) { return 0; }\n' >"$scratch/lib.c"
printf 'int qt_fixture(void); int main(void) { return qt_fixture(); }\n' >"$scratch/main.c"
cc -shared -fPIC "$scratch/lib.c" -Wl,-soname,libQt6Fixture.so -o "$scratch/sdk/lib/libQt6Fixture.so"
cp "$scratch/sdk/lib/libQt6Fixture.so" "$scratch/alien/lib/"
for variant in sdk alien; do
    cc "$scratch/main.c" "-L$scratch/$variant/lib" -lQt6Fixture "-Wl,-rpath,$scratch/$variant/lib" -o "$scratch/$variant/app"
done
sdk_assert_runtime "$scratch/sdk" "$scratch/sdk/app" "$scratch/good.log"
LD_LIBRARY_PATH="$scratch/alien/lib" sdk_assert_runtime "$scratch/sdk" "$scratch/sdk/app" "$scratch/isolated.log"
if (sdk_assert_runtime "$scratch/sdk" "$scratch/alien/app" "$scratch/bad.log"); then
    echo 'FAIL: foreign Qt runtime accepted'; exit 1
fi
# A successful Qt Test process can still mean the only playback test was skipped.
printf '%s\n' 'SKIP   : ZzExampleAudioMeterSourceTest::playsIndependentChannelsAndPausesWhenHidden() no backend' \
    'Totals: 2 passed, 0 failed, 1 skipped, 0 blacklisted' >"$scratch/skipped.log"
if (sdk_require_audio_playback "$scratch/skipped.log"); then
    echo 'FAIL: skipped playback accepted'; exit 1
fi
printf '%s\n' '321: PASS   : ZzExampleAudioMeterSourceTest::playsIndependentChannelsAndPausesWhenHidden()' \
    '321: Totals: 4 passed, 0 failed, 0 skipped, 0 blacklisted' >"$scratch/passed.log"
sdk_require_audio_playback "$scratch/passed.log"
printf 'SDK runtime identity and skipped-playback rejection checks passed.\n'
