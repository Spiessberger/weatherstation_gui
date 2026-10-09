# Panomax API notes

Research date: 2026-10-09, 13:55–16:49 UTC. Panomax publishes no API docs. Everything here
comes from live requests and from the code of the official viewer
(`https://client-static.panomax.com/js/pmaxhtmlapp.js`, functions `pmax_server_ApiUrls.*`,
`pmax_server_UrlsStatic.getPanodataUrl/getPanoFileName/getPanoUrl`, `pmax_server_data_SizeData`,
`pmax_server_remoting_DayManager.fetchRecentDay`). Panomax can change it without notice.
"(unverified)" marks claims I could not check. General API behaviour was first explored on
another camera; the camera-specific facts below were checked on camera 9015.

Camera 9015 is "Gipfelstube Patscherkofel" (Patscherkofel summit near Innsbruck, Austria,
47.2083 N 11.4612 E, 2244 m). It belongs to viewer instance 4053005
(`https://patscherkofel.panomax.com/gipfelstube`), uses the time zone `Europe/Vienna`, and has its
first image at 2026-10-01 11:20:00.

## How to get the latest image

1. `GET https://api.panomax.com/1.0/cams/9015/images/day` (no date). It returns the most recent
   day that has images: `date`, `storage`, `sizes`, `images`.
2. Take the last element of `images` that has `"available": true` and read its `time`.
3. Pick the size: the entry in `sizes` with the smallest `rows * tileHeight` that is >= 800.
   For 9015 this is `default`, a single file of 3725x800.
4. Build the URL:
   `https://panodata{storage}.panomax.com/cams/9015/{YYYY}/{MM}/{DD}/{HH}-{mm}-{ss}_{resolution}.jpg`
   If `storage` is 0, the host is `panodata.panomax.com`. Add `_{col}_{row}` before `.jpg` only
   when the size is tiled (`rows > 1 || cols > 1`).
   Example: `date` 2026-10-09, `storage` 42, `time` 18:40:00 gives
   `https://panodata42.panomax.com/cams/9015/2026/10/09/18-40-00_default.jpg` (3725x800, 194 KB).
5. Repeat step 1 every 60 s with `If-None-Match`. The server answers 304 when nothing changed.
   Download only when `date` + `time` differs from the image on screen.

`date` and `time` are camera-local wall-clock time (see section 4). Use them as they are, with no
time zone conversion. The `days` endpoint is not needed for the latest image.

## Endpoint list

| Endpoint | Purpose | Cache-Control |
|---|---|---|
| `GET api.panomax.com/1.0/cams/{cam}/images/day` | latest day with images: sizes, storage, image times | `max-age=30, s-maxage=100` |
| `GET api.panomax.com/1.0/cams/{cam}/images/day/{YYYY-MM-DD}` | same for one given day | same; 422 when there are no images |
| `GET api.panomax.com/1.0/cams/{cam}/days?from=&to=&includeSizes=1` | one entry per day that has images (no image times) | `max-age=600, s-maxage=600` |
| `GET api.panomax.com/1.0/cams/{cam}/weather/data?startDate=&endDate=` | weather readings, not image times | `max-age=600, s-maxage=600` |
| `GET api.panomax.com/1.0/instances/{instance}/config/{culture}` | instance and camera info, including `timezone` | `max-age=1800` |
| `GET api.panomax.com/1.0/instances/thumbnails/{id,id,...}` | instance to camera mapping, `lastPano`, recent image URLs | `max-age=600` |
| `GET panodata{storage}.panomax.com/cams/{cam}/{Y}/{M}/{D}/{file}.jpg` | the images | `max-age=2592000` (30 days) |
| `GET live-image.panomax.com/cams/{cam}/recent_{resolution}.jpg` | newest image, with no time in the URL | `max-age=60` |

`GET /1.0/cams/{cam}` (camera info by camera ID) does not exist: it returns an HTML 404. Other
paths the viewer uses but that are not needed here: `cams/{cam}/images/sequence`, `.../bestshots`,
`.../peaks`, `.../weather/forecast?culture=` (422 "There is no forecast for this cam." for cameras
without a forecast), `instances/lists/{slug}`.

## 1. Response schemas

### `cams/{cam}/images/day` and `cams/{cam}/images/day/{date}`

The JSON object has the same shape with or without the date. Abridged example (fixture
`tests/fixtures/cam9015-images-day-latest.json`):

```json
{"date":"2026-10-09","storage":42,
 "sizes":[{"resolution":"thumb","rows":1,"cols":1,"tileWidth":349,"tileHeight":75},
          {"resolution":"default","rows":1,"cols":1,"tileWidth":3725,"tileHeight":800},
          {"resolution":"hd","rows":1,"cols":4,"tileWidth":1257,"tileHeight":1080}, "..."],
 "images":[{"time":"00:00:00","latitude":null,"longitude":null,"elevation":null,"zeroDirection":null,"available":true},
           "...",
           {"time":"18:40:00","latitude":null,"longitude":null,"elevation":null,"zeroDirection":null,"available":true}],
 "imageCount":113,"imageAvailableCount":113}
```

| Field | Type | Meaning |
|---|---|---|
| `date` | string `YYYY-MM-DD` | camera-local date of the day |
| `storage` | int | storage server number, used as the image host `panodata{storage}`. Changes from day to day (9015: 41, 43, 42 within one week) |
| `sizes[]` | array | size variants of this day, in no particular order (see section 3) |
| `sizes[].resolution` | string | variant name: `thumb`, `small`, `optimized`, `reduced`, `default`, `hd`, `h2800`, `full`, `original` |
| `sizes[].rows`, `.cols` | int | tile grid. Full image = `cols*tileWidth` x `rows*tileHeight` |
| `sizes[].tileWidth`, `.tileHeight` | int | pixel size of one tile |
| `imageCount`, `imageAvailableCount` | int | number of images, and how many of them are available |
| `images[]` | array, ascending by time | one entry per image of the day |
| `images[].time` | string `HH:MM:SS` | camera-local capture time. Usually on a 10-minute grid, but not always (`16:48:19` on 2026-10-05) |
| `images[].available` | bool | the viewer skips `false` entries when it looks for the newest image. All entries I saw were `true` |
| `images[].latitude`, `.longitude`, `.zeroDirection` | number or null | per-image position, for moving "tour" cameras (null for 9015) |
| `images[].elevation` | int or null | as above |

Without a date, the endpoint returns the newest day that has images, not "today". Camera 1583,
disconnected since 2025-01-31, still returns `"date":"2025-01-31"` with its single image (fixture
`tests/fixtures/cam1583-images-day-latest-disconnected.json`). So a camera that does not shoot at
night returns the previous day at night.

A date with no images (missing day, or a future date) returns HTTP 422 with
`Cache-Control: no-cache, private`:
`{"title":"Date not found","status":422,"detail":"The requested date is not available."}`

### `cams/{cam}/days?from=YYYY-MM-DD&to=YYYY-MM-DD&includeSizes=1`

A JSON array with one object per day that has images, in ascending date order. Days without
images are left out (an older camera has 42 missing days since 2015).

```json
[{"date":"2026-10-08","time":"13:50:00","storage":41,"sizes":[{"resolution":"default","rows":1,"cols":1,"tileWidth":3725,"tileHeight":800}, "..."],"allImages":false},
 {"date":"2026-10-09","time":"12:00:00","storage":42,"sizes":["..."],"allImages":false}]
```

| Field | Type | Meaning |
|---|---|---|
| `date` | string | camera-local date |
| `time` | string, optional | the day's representative image. The viewer calls it `representingImageTime`. For 9015 it is usually `12:00:00`, but `16:48:19` on 2026-10-05 and `13:50:00` (the first image) on 2026-10-08. Missing on some days of other cameras. It is **not** the newest image |
| `storage` | int | as above |
| `sizes` | array | as above. It is `[]` unless `includeSizes=1` is sent |
| `allImages` | bool | always `false` for 9015. Meaning unverified (in the viewer code it marks day objects that carry a full image list) |

Range behaviour (all HTTP 200):
- `from=to=2026-10-09`: exactly one entry (today).
- `to` in the future (`from=2026-10-09&to=2026-10-12`): the same response as `to=2026-10-09`. Future days are simply missing.
- Range with no days (`from=2026-10-10&to=2026-10-12`), or `from` after `to`: `[]`.
- Long ranges are large (11 years of an older camera: 4057 entries, 2.8 MB), so query short ranges.
- What happens without `from`/`to` was not tested.

### `cams/{cam}/weather/data?startDate=YYYY-MM-DD&endDate=YYYY-MM-DD`

`{"weatherData":[{"date":"2026-10-09","time":"00:03:00","tempC":1.4,"tempF":34.52,"humidityPc":99.5,"windDirectionDeg":6,"windSpeedKmh":15,"windSpeedKn":8.099352051905825,"windSpeedMph":9.322560596643878,"rainMm":0,"airpressureHpa":1019.7}, ...]}`

`date` and `time` are strings in camera-local time. All other fields are numbers. Readings come
about every 12 minutes (00:03, 00:17, 00:29, 00:41, ...), independent of image times. **This
endpoint does not help find images.** It only served as extra time zone evidence.

### `instances/{instance}/config/{culture}` (e.g. `instances/4053005/config/en-GB`)

Top-level keys are `cultures`, `features`, `instance`, `mode`. The useful part is `instance.cam`:

```json
{"id":9015,"latitude":47.208314,"longitude":11.461224,"zeroDirection":310,"viewAngle":360,"elevation":2244,
 "firstPano":"2026-10-01 11:20:00","nightVision":true,"timezone":"Europe/Vienna","timezoneOffset":7200, "...": "..."}
```

`timezone` is an IANA zone name. `timezoneOffset` is in seconds east of UTC and includes the
current DST (+7200 for CEST in October; camera 1583 in Rio reports -10800). The endpoint takes the
**instance** ID, not the camera ID.

### `instances/thumbnails/{instance ids, comma-separated}`

Returns an array with one object per instance. Each object has keys such as `mode`, `name`, `url`
(the viewer URL), `images` (map from `"YYYY-MM-DD HH-MM-SS"` to an object of
`thumb/small/optimized/reduced/default` URLs, for every image of the latest day), `weather`
(`tempC`, `windDirectionDeg`, `windSpeedKmh`), and `instance`. `instance` has `id`, `name`,
`frontentryUrl` and `cam`; `cam` has `id`, lat/lon, `country`, `countryName`, `firstPano`,
`lastPano` (camera-local `YYYY-MM-DD HH:MM:SS`) and, for dead cameras, `"disconnected":true`.
Apart from this, the only way to map instance to camera that I found is the viewer page itself.
The list of all public instances (with names and positions, but no camera IDs) is at
`https://explore.panomax.com/api/panomax-search`. This endpoint is slow: 5 IDs took 1.7 s, 50 IDs
took 33 s, and 160 IDs had not finished after 4 minutes. The official thumbnail widget sends at
most 50 IDs per request. Requests of 10 IDs, 8 in parallel, covered all 634 public instances in a
few minutes.

### Errors

- Unknown API route: HTTP 404 with an HTML page.
- Known route, data not available: HTTP 422, `application/json` `{"title","status","detail"}`.
- Missing image file: HTTP 404, `text/html`, 146 bytes.

## 2. Newest image: request sequence

There is a single request: `cams/{cam}/images/day` already contains the latest date, its
`storage`, its `sizes` and all image times. This is what the official viewer does at startup
(`DayManager.fetchRecentDay`). It then calls `days?from={firstPano}&to={recent date}&includeSizes=1`
only to learn which days exist (calendar and archive). For a given day (browser feature), use `images/day/{date}`. To list the
days that exist, use `days`.

URL construction, as in the viewer's `getPanoUrl`:

```
https://panodata{storage}.panomax.com/cams/{cam}/{YYYY}/{MM}/{DD}/{HH}-{mm}-{ss}_{resolution}[_{col}_{row}].jpg
```

- `{YYYY}/{MM}/{DD}` comes from `date`, zero-padded. `{HH}-{mm}-{ss}` is `time` with `:` replaced by `-`.
- The viewer config has `panodataUrl = "https://panodata[x].panomax.com"`, where `[x]` is `storage`. `storage == 0` becomes an empty string.
- `https://panodata.panomax.com/...` (without a number) also serves every path I tried. For files a few minutes old it answers **302** to `panodata{storage}`: `_reduced.jpg` and `_default.jpg` files did this about 15 min after capture, while `thumb/small/optimized` of the same image were served directly. Older files are served with 200. Either way the client must follow redirects. Qt 6 does this by default.

## 3. Filename parts, size variants, tiles

`18-30-00_full_3_0.jpg` means capture time 18:30:00 (camera-local), variant `full`, tile column 3,
tile row 0. Column and row indexes are 0-based, the column comes first, and columns run left to
right. A variant is tiled when `rows > 1 || cols > 1`, which is `SizeData.get_tiled()` in the
viewer. Only tiled variants carry the `_{col}_{row}` suffix. A single-file variant is plain
`18-30-00_default.jpg`. Tiled names without the suffix, and `_default_0_0.jpg`, return 404.

Verification:
- I stitched the 4 `hd` tiles of 9015 at 18:30 in column order 0 to 3 and scaled them to
  3725x800. They match `_default.jpg` with RMSE 2.5%. Reversed order gives 19%.
- On camera 10035 (full = 24 cols x 4 rows), `_full_23_3.jpg` returns 200 and `_full_3_23.jpg`
  returns 404, so the suffix is `col_row`. That rows run top to bottom is assumed (unverified).

**Remainder tiles.** The real image is slightly larger than `cols*tileWidth` x `rows*tileHeight`.
One extra sliver tile exists at index `cols` (and at `rows`):
- `18-30-00_hd_4_0.jpg` is 1x1080, so the true `hd` width is 4*1257+1 = 5029.
- `18-30-00_full_15_0.jpg` is 13x940 and `_full_0_4.jpg` is 1167x2, so the true `full` size is
  15*1167+13 x 4*940+2 = 17518x3762, the `original` size. The corner tile `_full_15_4.jpg` is 13x2.
- `18-30-00_h2800_10_0.jpg` is 8x1400.
- Index `cols+1` returns 404.

The viewer ignores the slivers and uses `cols*tileWidth`. A gap of a few pixels in a 360°
panorama is not visible.

### Camera 9015 sizes (2026-10-09 18:30:00)

Dimensions were checked with `file` on the downloaded files.

| resolution | tiles (cols x rows) | tile px | image px (from API) | checked | bytes |
|---|---|---|---|---|---|
| thumb | 1x1 | 349x75 | 349x75 | yes | 4,286 |
| small | 1x1 | 698x150 | 698x150 | yes | 12,441 |
| optimized | 1x1 | 1397x300 | 1397x300 | yes | 38,718 |
| reduced | 1x1 | 2095x450 | 2095x450 | yes | 76,491 |
| **default** | **1x1** | **3725x800** | **3725x800** | yes | **196,756** |
| hd | 4x1 | 1257x1080 | 5028x1080 (+1 px sliver) | all 4 tiles + sliver | 52–78 KB per tile |
| h2800 | 10x2 | 1303x1400 | 13030x2800 (+8 px sliver) | tile 9,1 + sliver | 107 KB (tile 9,1) |
| full | 15x4 | 1167x940 | 17505x3760 (+13/+2 px slivers) | tiles 0,0 and 14,3 + slivers | 8–52 KB per tile |
| original | 1x1 | 17518x3762 | listed by the API, but `_original.jpg` returns 404 | not served | – |

These are evening images; daylight images are larger. The viewer code also knows the names
`h5400`, `h572` and `h320`. I never saw them in API data.

### Differences per camera and per day

Variants and dimensions differ per camera. The heights of the small variants are the same
everywhere; widths follow each camera's field of view:

| camera | default | hd | h2800 | full |
|---|---|---|---|---|
| 9015 (360°) | 1x1 of 3725x800 | 4x1 of 1257x1080 | 10x2 of 1303x1400 | 15x4 of 1167x940 (= 17505x3760) |
| 10035 Turku | 1x1 of 5077x800 (file checked) | 4x1 of 1713x1080 | 13x3 of 1366x933 | 24x4 of 1226x1159 (= 29424x4636) |
| 4480 Canmore | 1x1 of 3931x800 | 4x1 of 1326x1080 | 10x2 of 1375x1400 | 15x3 of 1179x1199 (= 17685x3597) |

Heights seen on all 3 cameras: thumb 75, small 150, optimized 300, reduced 450, default 800,
hd 1080. `full`, `h2800` and `original` depend on the camera.

Variants also change over time for the same camera. On an older camera, `default` was 4 tiles
until 2020 and has been one file since, and thumb/small/reduced were 1 px narrower before 2016.
So read `sizes` from each day's own data, not once per camera.

## 4. Time zone: camera-local wall-clock time

The `date` and `time` fields in the API, `firstPano`/`lastPano`, the image path
`YYYY/MM/DD/HH-MM-SS` and the weather times are all **camera-local time without an offset**.
Evidence:

1. At 16:46:20 UTC, `images/day` for 9015 listed 18:40:00 as the newest image. Read as UTC, that
   would be 1 h 54 min in the future.
2. File `Last-Modified` times are capture time minus 2 h plus a minute or two:
   00:00:00 gives 22:01:33 GMT the day before, 07:00:00 gives 05:01:19, 12:00:00 gives 10:01:52,
   18:30:00 gives 16:32:00, 18:40:00 gives 16:41:40.
3. Camera 9015's config says `"timezone":"Europe/Vienna","timezoneOffset":7200`. Vienna is UTC+2
   (CEST) on this date.
4. It is not one server-wide zone. At 14:07 UTC, `lastPano` of cameras in other zones matched
   each one's own local time: Canmore, CA (UTC-6) 07:55; Parque do Caracol, BR (UTC-3) 11:05;
   Angra do Heroísmo, Azores (UTC+0) 14:00; Turku, FI (UTC+3) 16:50.
5. The newest weather reading in a response from about 16:28 UTC was 18:23.

For "age of the image" or "now" on screen, convert with `QTimeZone("Europe/Vienna")`, or use the
camera's `timezone` from the instance config. DST fall-back hour (2026-10-25, 02:00–03:00 happens
twice): how the API labels images in that hour is unverified. 9015 shoots at night, so this hour
matters for it: if the labels repeat, images of the second 02:xx hour sort before the 02:50 image
already shown, and the app skips them until 03:00.

## 5. Update interval and latency

- 9015 takes an image every 10 minutes, around the clock (`nightVision` is true): 2026-10-09 had
  113 images from 00:00 to 18:40 with no gaps.
- Gaps happen. 2026-10-08 has 61 images, from 13:50 to 23:50 only, and 2026-10-05 has 43.
  Off-grid times occur too (16:48:19 on 2026-10-05).
- Other cameras differ. Canmore shoots hourly at night and every 5 min by day. Turku shoots every
  10 min around the clock. Some cameras shoot in daylight only.
- An image file appears about **1.5–2 minutes after its timestamp** for 9015 (Last-Modified values
  above). Another camera took 4.5–6 minutes. The listing in `images/day` can lag up to another
  100 s because of `s-maxage=100`. At 16:46:20 UTC the API listed 18:40 as the newest image; its
  file was written at 16:41:40.

## 6. Headers, caching, rate limits

- No API key, token, cookie, Referer, Origin or browser User-Agent is needed. Everything worked
  with curl's default UA and with no `User-Agent` header at all, on both the API and the image
  hosts.
- No rate-limit headers (`X-RateLimit-*`, `Retry-After`) were seen. The real limits are unknown.
  A CDN sits in front of the API (`x-server: <CACHE>`, `x-cache-status: HIT/MISS`).
- CORS: `access-control-allow-origin: *` on the API and image responses.
- Conditional requests work. The API returns `ETag`, and `If-None-Match` gives 304. The image
  hosts return `Last-Modified`/`ETag`, and `If-Modified-Since` gives 304. Image responses are
  cacheable for 30 days.
- Compression: the API sent `Content-Encoding: gzip` on a cached weather response even though the
  request had no `Accept-Encoding` header. Qt's `QNetworkAccessManager` sends
  `Accept-Encoding: gzip` itself and decompresses transparently. Do not set that header manually,
  or you have to inflate the body yourself.
- The ETag differs between the gzip variant (`W/"..."`) and the plain variant (`"..."`).
- All hosts use HTTP/2 with HSTS. Error responses (404/422) are `no-cache, private`.
- `live-image.panomax.com/cams/9015/recent_default.jpg` (also `recent_reduced.jpg`) always serves
  the newest image (`max-age=60`). The JPEGs carry no EXIF or comment, so the capture time is
  unknown. Only `Last-Modified` (upload time) is available. It could serve as a fallback.
  `panodata-alternate.panomax.com` (the viewer's failover host) did not resolve in DNS.

## 7. Recommendations for the app

**(a) Polling every 60 s.** Send one request,
`GET https://api.panomax.com/1.0/cams/9015/images/day` with `If-None-Match`. The response is about
13 KB raw or about 0.7 KB gzipped, and mostly 304. Compare `date` + last available `time`
(camera-local strings sort correctly) with what is on screen. If it is newer, download one image
(about 0.2 MB, 144 times a day).
- `days` is not needed for polling. A short range works (`from=to=today` gives one entry, a future
  `to` is harmless, an empty range gives `[]`). But it is cached for 600 s and has no image times.
- For cameras that do not shoot at night, the same request keeps returning the previous day at
  night, which is correct behaviour.
- Polling faster than about 30–100 s gains nothing because of caching. New images arrive about
  every 10 min.

**(b) Size.** For 9015 the smallest variant with height >= 800 is `default`: one file, 3725x800,
about 0.2 MB. Yes, this can be chosen per camera from API data. `sizes` comes with every
`images/day` response (and with `days?includeSizes=1`). Compute `height = rows*tileHeight`,
`width = cols*tileWidth`, take the smallest height >= 800, and choose per day because sizes change
over time. `default` is 800 px high on all 3 cameras checked, but take it from the data instead of
hard-coding the name. Never pick `original` (404).

Risk (not checked): many embedded GPUs have a maximum texture size of 4096–8192 px. `default`
(3725 px) fits, but stitched `hd` (5028 px) and larger variants, or `default` of other cameras,
may not. The display code may have to split or scale the image.

## Fixtures (`tests/fixtures/`)

Real, unmodified response bodies. gzip transfer encoding is decoded. Fetched 2026-10-09 (server
`Date` header).

| File | Request | Status, fetched (UTC) |
|---|---|---|
| `tests/fixtures/cam9015-images-day-latest.json` | `https://api.panomax.com/1.0/cams/9015/images/day` | 200, 16:46:20 (CDN copy, age 78 s) |
| `tests/fixtures/cam9015-images-day-2026-10-08.json` | `https://api.panomax.com/1.0/cams/9015/images/day/2026-10-08` | 200, 16:47:39 |
| `tests/fixtures/cam9015-images-day-2026-10-10-unavailable.json` | `https://api.panomax.com/1.0/cams/9015/images/day/2026-10-10` | **422**, 16:47:39 |
| `tests/fixtures/cam9015-days-2026-10-01-to-2026-10-09.json` | `https://api.panomax.com/1.0/cams/9015/days?from=2026-10-01&to=2026-10-09&includeSizes=1` | 200, 16:47:39 |
| `tests/fixtures/cam9015-days-2026-10-09-to-2026-10-12-future.json` | `https://api.panomax.com/1.0/cams/9015/days?from=2026-10-09&to=2026-10-12&includeSizes=1` | 200, 16:47:40 |
| `tests/fixtures/cam9015-days-2026-10-10-to-2026-10-12-empty.json` | `https://api.panomax.com/1.0/cams/9015/days?from=2026-10-10&to=2026-10-12&includeSizes=1` | 200 (`[]`), 16:47:40 |
| `tests/fixtures/cam9015-weather-data-2026-10-09.json` | `https://api.panomax.com/1.0/cams/9015/weather/data?startDate=2026-10-09&endDate=2026-10-09` | 200, 16:32:37 (CDN copy, age 303 s) |
| `tests/fixtures/instance4053005-config-en-GB.json` | `https://api.panomax.com/1.0/instances/4053005/config/en-GB` | 200, 16:47:42 |
| `tests/fixtures/instance4053005-thumbnails.json` | `https://api.panomax.com/1.0/instances/thumbnails/4053005` | 200, 16:42:44 (CDN copy, age 298 s) |
| `tests/fixtures/cam10035-images-day-latest.json` | `https://api.panomax.com/1.0/cams/10035/images/day` | 200, about 14:09 |
| `tests/fixtures/cam4480-images-day-latest.json` | `https://api.panomax.com/1.0/cams/4480/images/day` | 200, about 14:09 |
| `tests/fixtures/cam1583-images-day-latest-disconnected.json` | `https://api.panomax.com/1.0/cams/1583/images/day` | 200, about 14:11 |

Full-history `days` responses are not stored. The 2026-10-01..09 range is all of 9015's history.

## Not verified / open

- The meaning of `allImages` and of `available: false`. I never saw an unavailable image.
- The row order of multi-row tiles (assumed top to bottom). `default` is untiled, so this matters
  only for `h2800`/`full` and for other cameras.
- How images in the DST fall-back hour are labelled. 9015 shoots at night (see section 4).
- The real rate limits. `days` without `from`/`to`.
- Whether the target GPU can hold textures wider than 4096 px (see 7b).
- A direct camera-ID to instance-ID lookup. I found none; `instances/thumbnails` and the viewer
  page are the only links seen.
