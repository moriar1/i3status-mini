Print cpu usage, current volume, and data.

# Build and run

```sh
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=1 -B build -S .
cmake --build build
./build/i3status-mini
# Example output: cpu: 1% | vol: 20% | 10-29 12:22
```
