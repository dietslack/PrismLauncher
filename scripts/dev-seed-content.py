#!/usr/bin/env python3
"""Adds sample mods, packs, a world and servers to one instance made by dev-seed.sh (for UI screenshots)."""
import base64, gzip, io, json, os, struct, sys, zipfile

game = sys.argv[1]  # <instance>/minecraft


def png(color, size=16):
    """A tiny solid-colour PNG with a darker checker, without external libraries."""
    import zlib
    rows = b""
    for y in range(size):
        row = b"\x00"
        for x in range(size):
            c = color if (x // 4 + y // 4) % 2 else tuple(max(0, v - 40) for v in color)
            row += bytes(c) + b"\xff"
        rows += row
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 6, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def write_zip(path, files):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with zipfile.ZipFile(path, "w") as z:
        for name, data in files.items():
            z.writestr(name, data)


mods = [("sodium", "Sodium", "0.6.5", (90, 160, 230)), ("lithium", "Lithium", "0.14.3", (120, 200, 120)),
        ("iris", "Iris Shaders", "1.8.1", (200, 120, 220)), ("modmenu", "Mod Menu", "13.0.2", (230, 180, 60)),
        ("fabric-api", "Fabric API", "0.110.5", (210, 190, 150))]
for mod_id, name, version, color in mods:
    meta = {"schemaVersion": 1, "id": mod_id, "name": name, "version": version, "icon": "icon.png",
            "authors": ["Someone"], "description": f"{name} for testing."}
    write_zip(f"{game}/mods/{mod_id}-{version}.jar", {"fabric.mod.json": json.dumps(meta), "icon.png": png(color)})
os.rename(f"{game}/mods/modmenu-13.0.2.jar", f"{game}/mods/modmenu-13.0.2.jar.disabled")

write_zip(f"{game}/resourcepacks/Faithful 32x.zip",
          {"pack.mcmeta": json.dumps({"pack": {"pack_format": 42, "description": "Faithful"}}), "pack.png": png((180, 140, 90))})
write_zip(f"{game}/shaderpacks/ComplementaryReimagined.zip", {"shaders/composite.fsh": "void main(){}"})
write_zip(f"{game}/datapacks/terralith.zip",
          {"pack.mcmeta": json.dumps({"pack": {"pack_format": 48, "description": "Terralith"}}), "pack.png": png((90, 170, 120))})


# --- minimal NBT writer
def nbt_string(s):
    b = s.encode()
    return struct.pack(">H", len(b)) + b


def tag(kind, name, payload):
    return bytes([kind]) + nbt_string(name) + payload


def compound(items):
    return b"".join(items) + b"\x00"


def world(folder, name, game_type, last_played_ms, color):
    data = compound([tag(8, "LevelName", nbt_string(name)), tag(3, "GameType", struct.pack(">i", game_type)),
                     tag(4, "LastPlayed", struct.pack(">q", last_played_ms)), tag(4, "RandomSeed", struct.pack(">q", 12345))])
    root = tag(10, "", compound([tag(10, "Data", data)]))
    os.makedirs(f"{game}/saves/{folder}", exist_ok=True)
    with gzip.open(f"{game}/saves/{folder}/level.dat", "wb") as f:
        f.write(root)
    with open(f"{game}/saves/{folder}/icon.png", "wb") as f:
        f.write(png(color, 64))


import time
now = int(time.time() * 1000)
world("New World", "Survival Base", 0, now - 3600_000 * 30, (110, 170, 80))
world("Creative Test", "Redstone Lab", 1, now - 3600_000 * 24 * 9, (200, 80, 80))
write_zip(f"{game}/saves/New World/datapacks/coordinates-hud.zip",
          {"pack.mcmeta": json.dumps({"pack": {"pack_format": 61, "description": "Coordinates HUD"}}), "pack.png": png((70, 120, 200))})

servers = []
for name, ip, color in [("Hypixel", "mc.hypixel.net", (240, 180, 40)), ("Friends SMP", "smp.example.org", (80, 140, 220))]:
    entry = compound([tag(8, "name", nbt_string(name)), tag(8, "ip", nbt_string(ip)),
                      tag(8, "icon", nbt_string(base64.b64encode(png(color, 64)).decode()))])
    servers.append(entry)
servers_list = bytes([10]) + struct.pack(">i", len(servers)) + b"".join(servers)
with open(f"{game}/servers.dat", "wb") as f:
    f.write(tag(10, "", compound([tag(9, "servers", servers_list)])))
print("Added sample content to", game)
