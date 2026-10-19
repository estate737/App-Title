#!/usr/bin/env python3
from pathlib import Path
import sys
src, dst, name = sys.argv[1:]
data = Path(src).read_bytes()
Path(dst).parent.mkdir(parents=True, exist_ok=True)
with Path(dst).open('w') as out:
    out.write('#include <cstddef>\nextern "C" {\n')
    out.write('extern const unsigned char '+name+'[] = {\n')
    for i in range(0,len(data),20):
        out.write(','.join(str(x) for x in data[i:i+20])+',\n')
    out.write('};\nextern const std::size_t '+name+'_size = '+str(len(data))+';\n}\n')
