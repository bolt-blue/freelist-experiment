# freelist-experiment
malloc hook LD_PRELOAD experiment - not to be taken seriously.

## Usage
The following should be enough to compile and witness the breakage (before reaching `main`):
```bash
$> make
$> LD_PRELOAD=./malloc.so /bin/ls -l > dump
```
The `> dump` may not include the most crucial last printed lines. I presume to do with cache not being flushed appropriately.

## Disclaimer
Nothing untoward should happen from running this code, but use at your own risk.
