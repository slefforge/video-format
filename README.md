# video-format

Toy project for experimenting with a custom `.es` video format.

## Build
```bash
make
````

## Usage

```bash
scripts/encode.sh v1 media/candles.y4m out/v1/candles.es
scripts/decode.sh v1 out/v1/candles.es out/v1/candles.y4m
scripts/roundtrip.sh v1 media/candles.y4m out/v1/candles-rt.y4m
```
