import sys
import struct
import os

def main():
    if len(sys.argv) != 3:
        sys.stderr.write("usage: embed_spirv.py <input.spv> <symbol_name>\n")
        sys.exit(1)

    spv_path = sys.argv[1]
    sym = sys.argv[2]

    with open(spv_path, "rb") as f:
        data = f.read()

    if len(data) % 4 != 0:
        sys.stderr.write("spv size not multiple of 4: " + spv_path + "\n")
        sys.exit(1)

    n = len(data) // 4
    words = struct.unpack("<" + str(n) + "I", data)

    out = []
    out.append("#include <stdint.h>")
    out.append("#include <stddef.h>")
    out.append("const uint32_t " + sym + "[] = {")

    row = []
    for i, w in enumerate(words):
        row.append("0x{:08x}u".format(w))
        if len(row) == 8:
            out.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        out.append("    " + ", ".join(row))
    out.append("};")
    out.append("const size_t " + sym + "_size = sizeof(" + sym + ");")
    out.append("")

    sys.stdout.write("\n".join(out))

if __name__ == "__main__":
    main()