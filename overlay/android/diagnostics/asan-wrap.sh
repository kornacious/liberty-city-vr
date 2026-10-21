#!/system/bin/sh
# Loaded only from an explicitly requested, debuggable ASan APK.
HERE="$(dirname "$0")"
export ASAN_OPTIONS="detect_leaks=0:allow_user_segv_handler=1:abort_on_error=1"
export LD_PRELOAD="$HERE/libclang_rt.asan-aarch64-android.so $HERE/libc++_shared.so"
exec "$@"
