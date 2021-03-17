# freelist-experiment
malloc hook LD_PRELOAD experiment

## Usage
The following should be enough to compile and witness the breakage:
```bash
$> make
$> LD_PRELOAD=./malloc.so /bin/ls -l
```
