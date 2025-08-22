# video-format

Toy project for experimenting with a custom `.es` video format.

## Build
```bash
make
````

## Usage

```bash
# Encode v1
bin/v1/myenc -i media/candles.y4m -o out/v1/candles.es

# Decode v1
bin/v1/mydec -i out/v1/candles.es -o out/v1/candles.y4m

# Roundtrip without temp files (sanity test)
bin/v1/myenc -i media/candles.y4m | bin/v1/mydec > /dev/null
```
