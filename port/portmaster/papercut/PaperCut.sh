#!/bin/bash
# =============================================================================
# PortMaster Launch Script for Paper Mario: The Thousand-Year Door (Paper Cut)
# Target: Rockchip RK3326 / Mali-G31 MP2 / OpenGL ES 3.0 / KMSDRM
# =============================================================================

# Pre-kill any stale instances to ensure DRM master and inputs are completely free
sudo killall -9 ttyd-eu gptokeyb gptokeyb2 2>/dev/null || true

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

[ -f "$controlfolder/control.txt" ] && source "$controlfolder/control.txt"
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
type get_controls >/dev/null 2>&1 && get_controls

GAMEDIR="/roms/ports/papercut"
cd "$GAMEDIR" || exit 1

# =============================================================================
# 2. AUTO-DETECT MALI GPU DRIVER & CREATE SYMLINKS (needed by both splash & game)
# =============================================================================
MALI_DIR="/tmp/papercut_mali"
rm -rf "$MALI_DIR"
mkdir -p "$MALI_DIR"

if [ -f "$GAMEDIR/libs.aarch64/libmali-bifrost-g31-rxp0-gbm.so" ]; then
  MALI_BLOB="$GAMEDIR/libs.aarch64/libmali-bifrost-g31-rxp0-gbm.so"
elif [ -f "/usr/lib/aarch64-linux-gnu/libmali-bifrost-g31-rxp0-gbm.so" ]; then
  MALI_BLOB="/usr/lib/aarch64-linux-gnu/libmali-bifrost-g31-rxp0-gbm.so"
else
  MALI_BLOB=$(find /usr/lib -name "libmali-bifrost-g31-*.so" -o -name "libmali.so" 2>/dev/null | head -n 1)
fi

if [ -n "$MALI_BLOB" ]; then
  ln -sf "$MALI_BLOB" "$MALI_DIR/libEGL.so.1"
  ln -sf "$MALI_BLOB" "$MALI_DIR/libEGL.so"
  ln -sf "$MALI_BLOB" "$MALI_DIR/libGLESv2.so.2"
  ln -sf "$MALI_BLOB" "$MALI_DIR/libGLESv2.so"
  ln -sf "$MALI_BLOB" "$MALI_DIR/libgbm.so.1"
  ln -sf "$MALI_BLOB" "$MALI_DIR/libgbm.so"
fi

# =============================================================================
# SPLASH / LOADING SCREEN (Resolution-matched, centered with edge replication)
# =============================================================================
SPLASH_PID=""
cleanup() {
  [ -n "$SPLASH_PID" ] && kill -9 "$SPLASH_PID" 2>/dev/null || true
}
trap cleanup EXIT

CUR_RES=""
if [ -x "$controlfolder/sdl_resolution.$DEVICE_ARCH" ]; then
  CUR_RES=$("$controlfolder/sdl_resolution.$DEVICE_ARCH" 2>/dev/null | grep -a 'Current' | awk -F ': ' '{print $2}')
fi
if [ -z "$CUR_RES" ] && [ -n "$DISPLAY_WIDTH" ] && [ -n "$DISPLAY_HEIGHT" ]; then
  CUR_RES="${DISPLAY_WIDTH}x${DISPLAY_HEIGHT}"
fi
if [ -z "$CUR_RES" ] && [ -f "/sys/class/graphics/fb0/virtual_size" ]; then
  CUR_RES=$(cat /sys/class/graphics/fb0/virtual_size 2>/dev/null | tr ',' 'x')
fi
[ -z "$CUR_RES" ] && CUR_RES="640x480"

SPLASH_IMG=""
if [ -f "$GAMEDIR/Logo_${CUR_RES}.jpg" ]; then
  SPLASH_IMG="$GAMEDIR/Logo_${CUR_RES}.jpg"
fi

if [ -z "$SPLASH_IMG" ] && [ -n "$CUR_RES" ]; then
  RES_W=$(echo "$CUR_RES" | cut -d'x' -f 1)
  RES_H=$(echo "$CUR_RES" | cut -d'x' -f 2)
  if [ -n "$RES_W" ] && [ -n "$RES_H" ] && [ "$RES_H" -gt 0 ]; then
    RATIO=$(( (RES_W * 100) / RES_H ))
    if [ "$RATIO" -ge 165 ]; then
      [ -f "$GAMEDIR/Logo_16x9.jpg" ] && SPLASH_IMG="$GAMEDIR/Logo_16x9.jpg"
    elif [ "$RATIO" -ge 140 ] && [ "$RATIO" -le 160 ]; then
      [ -f "$GAMEDIR/Logo_3x2.jpg" ] && SPLASH_IMG="$GAMEDIR/Logo_3x2.jpg"
    elif [ "$RATIO" -ge 120 ] && [ "$RATIO" -le 139 ]; then
      [ -f "$GAMEDIR/Logo_4x3.jpg" ] && SPLASH_IMG="$GAMEDIR/Logo_4x3.jpg"
    elif [ "$RATIO" -le 110 ]; then
      [ -f "$GAMEDIR/Logo_1x1.jpg" ] && SPLASH_IMG="$GAMEDIR/Logo_1x1.jpg"
    fi
  fi
fi

if [ -z "$SPLASH_IMG" ] || [ ! -f "$SPLASH_IMG" ]; then
  if [ -f "$GAMEDIR/Logo_640x480.jpg" ]; then
    SPLASH_IMG="$GAMEDIR/Logo_640x480.jpg"
  else
    SPLASH_IMG="$GAMEDIR/Logo.jpg"
  fi
fi

if [ -f "$SPLASH_IMG" ]; then
  if [ -x "$controlfolder/sdl2imgshow.$DEVICE_ARCH" ]; then
    echo "[port] Showing loading splash $SPLASH_IMG for $CUR_RES ($controlfolder/sdl2imgshow.$DEVICE_ARCH)"
    LD_LIBRARY_PATH="$MALI_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$controlfolder/sdl2imgshow.$DEVICE_ARCH" -i "$SPLASH_IMG" -S fit -c "000000" -b ttyd-eu &
    SPLASH_PID=$!
  elif [ -x "$controlfolder/sdl2imgshow" ]; then
    echo "[port] Showing loading splash $SPLASH_IMG for $CUR_RES ($controlfolder/sdl2imgshow)"
    LD_LIBRARY_PATH="$MALI_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" "$controlfolder/sdl2imgshow" -i "$SPLASH_IMG" -S fit -c "000000" -b ttyd-eu &
    SPLASH_PID=$!
  fi
fi

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

echo "=================================================="
echo "Starting Paper Mario: TTYD - Paper Cut (RK3326)"
echo "Date: $(date)"
echo "Target: RK3326 Mali-G31 MP2 KMSDRM (OpenGL ES 3.0)"
echo "=================================================="

# =============================================================================
# 1. HARDWARE AUDIO SETUP
# =============================================================================
ES_VOL=$(/usr/local/bin/current_volume 2>/dev/null | tr -dc '0-9')
if [ -z "$ES_VOL" ]; then
  ES_VOL=$(amixer sget Playback 2>/dev/null | awk -F'[][]' '/%/ {print $2; exit}' | tr -dc '0-9')
fi
if [ -z "$ES_VOL" ]; then
  ES_VOL=$(amixer sget Master 2>/dev/null | awk -F'[][]' '/%/ {print $2; exit}' | tr -dc '0-9')
fi
if [ -n "$ES_VOL" ]; then
  amixer -c 0 sset 'Playback Path' 'SPK' >/dev/null 2>&1 || true
  amixer -c 0 -q sset 'Playback' "${ES_VOL}%" >/dev/null 2>&1 || true
  echo "[audio] Preserved EmulationStation volume: ${ES_VOL}%"
else
  amixer -c 0 sset 'Playback Path' 'SPK' >/dev/null 2>&1 || true
fi

# =============================================================================
# 2. LIBRARY PATH (MALI_DIR already populated before splash)
# =============================================================================
echo "[mali] Using Mali blob: $MALI_BLOB"

export LD_LIBRARY_PATH="$MALI_DIR:$GAMEDIR/libs.aarch64:$GAMEDIR:$LD_LIBRARY_PATH"
unset LD_PRELOAD

# =============================================================================
# 3. SDL2 / KMSDRM / AUDIO CONFIGURATION
# =============================================================================
export SDL_VIDEODRIVER="sdl2"
export SDL3SHIM_SDL2_VIDEODRIVER="kmsdrm"
export SDL_VIDEO_GL_DRIVER="/usr/lib/aarch64-linux-gnu/libmali-bifrost-g31-rxp0-gbm.so"
export SDL_AUDIODRIVER="sdl2"
export SDL3SHIM_SDL2_AUDIODRIVER="alsa"
export SDL_AUDIO_ALSA_SET_BUFFER_SIZE=1
export SDL_AUDIO_ALSA_BUFFER_SIZE=4096
export SDL_AUDIO_ALSA_PERIOD_SIZE=1024

# =============================================================================
# 4. HARDWARE PERFORMANCE GOVERNORS & MEMORY TUNING
# =============================================================================
# Online all 4 CPU cores
for cpu in /sys/devices/system/cpu/cpu[0-3]/online; do
  [ -f "$cpu" ] && echo 1 | sudo tee "$cpu" >/dev/null 2>&1 || true
done

# CPU governor: prefer schedutil to scale dynamic clock (408MHz-1.2GHz) on demand
# while allowing CPU sleep states during vsync (averts 78C thermal throttling).
if [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
  if grep -q "schedutil" /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_governors 2>/dev/null; then
    echo schedutil | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true
    echo 816000 | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq >/dev/null 2>&1 || true
    echo 1200000 | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq >/dev/null 2>&1 || true
  else
    echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor >/dev/null 2>&1 || true
  fi
fi

# Mali GPU devfreq governor -> performance (~520 MHz)
for gpu_gov in /sys/devices/platform/*.gpu/devfreq/*.gpu/governor /sys/class/devfreq/*gpu*/governor; do
  [ -f "$gpu_gov" ] && echo performance | sudo tee "$gpu_gov" >/dev/null 2>&1 || true
done

# DMC (DRAM Controller) governor -> performance
for dmc_gov in /sys/devices/platform/dmc/devfreq/dmc/governor /sys/class/devfreq/*dmc*/governor; do
  [ -f "$dmc_gov" ] && echo performance | sudo tee "$dmc_gov" >/dev/null 2>&1 || true
done

# Ensure zram swap is at least 512MB
if [ -b /dev/zram0 ]; then
  ZRAM_SIZE=$(cat /sys/block/zram0/disksize 2>/dev/null || echo 0)
  if [ "$ZRAM_SIZE" -lt 536870912 ]; then
    echo "[swap] Setting /dev/zram0 to 512MB..."
    echo ark | sudo -S /sbin/swapoff -a >/dev/null 2>&1 || true
    echo 1 | sudo tee /sys/block/zram0/reset >/dev/null 2>&1 || true
    echo 536870912 | sudo tee /sys/block/zram0/disksize >/dev/null 2>&1 || true
    echo ark | sudo -S /sbin/mkswap /dev/zram0 >/dev/null 2>&1 || true
    echo ark | sudo -S /sbin/swapon -p 10 /dev/zram0 >/dev/null 2>&1 || true
  fi
fi

# Kernel virtual memory tunings & cache drop
echo ark | sudo -S /sbin/sysctl -w vm.overcommit_memory=1 >/dev/null 2>&1 || true
echo ark | sudo -S /sbin/sysctl -w vm.min_free_kbytes=16384 >/dev/null 2>&1 || true
echo ark | sudo -S /sbin/sysctl -w vm.vfs_cache_pressure=200 >/dev/null 2>&1 || true
echo ark | sudo -S /sbin/sysctl -w vm.swappiness=60 >/dev/null 2>&1 || true
echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null 2>&1 || true

# Auto-protect ttyd-eu process from OOM killer
(
  for _ in {1..30}; do
    PID=$(pgrep -x ttyd-eu | head -n 1)
    if [ -n "$PID" ]; then
      echo ark | sudo -S sh -c "echo -500 > /proc/$PID/oom_score_adj" 2>/dev/null
      echo "[oom] Protected ttyd-eu (PID $PID) with oom_score_adj=-500"
      break
    fi
    sleep 0.5
  done
) &

# =============================================================================
# 5. GAMEPAD CONFIGURATION
# =============================================================================
MAP_GO="190000004b4800000011000000010000,GO-Super Gamepad,a:b1,b:b0,back:b12,dpdown:b9,dpleft:b10,dpright:b11,dpup:b8,guide:b16,leftshoulder:b4,leftstick:b14,lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b15,righttrigger:b7,rightx:a2,righty:a3,start:b13,x:b2,y:b3,platform:Linux,"
MAP_GO_DEB="1900bb3e4b4800000011000000010000,GO-Super Gamepad,a:b1,b:b0,back:b12,dpdown:b9,dpleft:b10,dpright:b11,dpup:b8,guide:b16,leftshoulder:b4,leftstick:b14,lefttrigger:b6,leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b15,righttrigger:b7,rightx:a2,righty:a3,start:b13,x:b2,y:b3,platform:Linux,"

if [ -n "$sdl_controllerconfig" ]; then
  cfg_deb=$(echo "$sdl_controllerconfig" | sed 's/190000004b4800000011000000010000/1900bb3e4b4800000011000000010000/g')
  export SDL_GAMECONTROLLERCONFIG="${sdl_controllerconfig}"$'\n'"${cfg_deb}"$'\n'"${MAP_GO}"$'\n'"${MAP_GO_DEB}"
else
  export SDL_GAMECONTROLLERCONFIG="${MAP_GO}"$'\n'"${MAP_GO_DEB}"
fi

BIN="$GAMEDIR/ttyd-eu"
chmod +x "$BIN"

# Controller hotkey daemon (Select + Start to exit)
if [ -f "$GAMEDIR/papercut.gptk" ]; then
  if [ -n "$GPTOKEYB2" ]; then
    $GPTOKEYB2 "$(basename "$BIN")" -c "$GAMEDIR/papercut.gptk" &
  elif [ -n "$GPTOKEYB" ]; then
    $GPTOKEYB "$(basename "$BIN")" -c "$GAMEDIR/papercut.gptk" &
  fi
fi

type pm_platform_helper >/dev/null 2>&1 && pm_platform_helper "$BIN"

# =============================================================================
# 6. LOCATE DISC IMAGE & SETUP DIRECTORIES
# =============================================================================
DISC_PATH=""
for candidate in "$GAMEDIR"/game.iso "$GAMEDIR"/"Paper Mario - The Thousand-Year Door.iso" "$GAMEDIR"/*.iso "$GAMEDIR"/*.ISO "$GAMEDIR"/*.gcm; do
  if [ -f "$candidate" ]; then
    DISC_PATH="$candidate"
    break
  fi
done

if [ -z "$DISC_PATH" ]; then
  echo "ERROR: No GameCube disc image (.iso / .gcm) found in $GAMEDIR"
  type pm_finish >/dev/null 2>&1 && pm_finish
  exit 1
fi

echo "[disc] Using game disc: $DISC_PATH"

mkdir -p "$GAMEDIR/cache"
mkdir -p "$GAMEDIR/saves"

export TTYD_DISC="$DISC_PATH"
export TTYD_GPU="opengles"
export TTYD_MSAA=0
export TTYD_CACHE_DIR="$GAMEDIR/cache"
export TTYD_USER_DIR="$GAMEDIR/saves"

# =============================================================================
# 7. EXECUTION
# =============================================================================
echo "=== Launching Paper Cut ($BIN) ==="
"$BIN" "$DISC_PATH"
ret=$?

echo "=== Paper Cut exited with code $ret ==="
type pm_finish >/dev/null 2>&1 && pm_finish
exit $ret
