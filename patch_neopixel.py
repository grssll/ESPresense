# Aligns Adafruit NeoPixel esp.c espShow() signature with the header
# (uint8_t pin -> uint16_t pin) to fix the LTO type-mismatch link error.
Import("env")
import os, glob

libdeps = env.subst("$PROJECT_LIBDEPS_DIR")
envname = env.subst("$PIOENV")
pattern = os.path.join(libdeps, envname, "Adafruit NeoPixel", "esp.c")

for path in glob.glob(pattern):
    with open(path) as f:
        content = f.read()
    patched = content.replace("void espShow(uint8_t pin",
                              "void espShow(uint16_t pin")
    if patched != content:
        with open(path, "w") as f:
            f.write(patched)
        print(f"[patch_neopixel] Patched {path}")
    else:
        print(f"[patch_neopixel] Already matches / not found: {path}")
