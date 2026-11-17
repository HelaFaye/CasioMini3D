# cglink: the calculator <-> ESP32 link library

Shared by the launcher (fx-CG50, gint) and the ESP32-S3 firmware (ESP-IDF),
and testable on a PC. No allocation and no floating point.

| Part | Files | Role |
|---|---|---|
| Framing | `frame.{h,c}` | SLIP around `[payload][CRC-16/CCITT-FALSE]`, payloads up to 4352 bytes (a whole compressed page). Same conventions as HelaFaye/gbagate's `link_proto` |
| Pak index | `pak.{h,c}` | Reads the page pak built by HelaFaye/tmc's `scripts/fxcg/build_page_pak.py`: pages compressed one by one, a resident baseline and a manifest per room |
| Decoder | `inflate.{h,c}` | Raw DEFLATE (RFC 1951) into a caller buffer |
| Cache | `cache.{h,c}` | RAM page cache: fixed slots, O(1) lookup, LRU eviction, pinning |
| Streaming | `pages.{h,c}` | `PAGE_REQUEST` (an ordered list, replacing the previous one) and `PAGE_DATA`; the server runs on the ESP32, the client in the launcher |

```sh
cmake -S lib/cglink -B build/cglink && cmake --build build/cglink
ctest --test-dir build/cglink --output-on-failure
build/cglink/cglink_sim page.pak route.txt 115200 921600 -v
```

`cglink_sim` replays a route of room changes over a simulated serial link
with the real server, client, framing, decoder and cache, and reports how
long the game stalls waiting for pages under three preload policies. A route
file has one `area room dwell_seconds` line per room (area and room in hex).
The pak contains ROM data: build it from your own ROM and keep it out of the
repository.
