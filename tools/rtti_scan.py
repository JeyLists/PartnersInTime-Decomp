#!/usr/bin/env python3

import sys
import argparse
from dataclasses import dataclass
from pathlib import Path
import re
import struct

parser = argparse.ArgumentParser(description="Lists RTTI classes and their binary locations")
parser.add_argument('extract_dir', type=Path, help="Input extraction directory")
parser.add_argument('-f', '-o', '--output', type=Path, dest='out_file', required=False, help='Output RTTI overview file')

# Finds scalar fields in dsd's generated layout YAML
LAYOUT_REGEX = r'^\s*([a-z_]+):\s*([^\r\n]+)'
# Finds length prefixes, identifiers and complete binary name strings
LENGTH_REGEX = r'[1-9][0-9]*'
IDENTIFIER_REGEX = r'[A-Za-z_][A-Za-z_0-9]*'
STRING_REGEX = rb'[\x20-\x7e]+\x00'


def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)


@dataclass(eq=False)
class Module:
    name: str
    cpu: str
    base: int
    data: bytes

    def contains(self, address):
        return self.base <= address < self.base + len(self.data)


# Read the scalar fields needed from the extraction layout
def fields(text):
    return dict(re.findall(LAYOUT_REGEX, text, flags=re.MULTILINE))


def load_modules(folder):
    modules = []

    def add(binary, cpu, layout):
        modules.append(Module(binary.relative_to(folder).as_posix(), cpu,
                              int(layout["base_address"], 0), binary.read_bytes()))

    for cpu in ("arm9", "arm7"):
        for config in sorted((folder / cpu).glob("*.yaml")):
            layout = fields(config.read_text())
            if "base_address" in layout:
                add(config.with_suffix(".bin"), cpu, layout)
        overlay_dir = folder / (cpu + "_overlays")
        config = overlay_dir / "overlays.yaml"
        if config.exists():
            for block in config.read_text().split("- id:")[1:]:
                layout = fields(block)
                add(overlay_dir / layout["file_name"].strip("'\""), cpu, layout)
    if not modules:
        raise ValueError(f"No extracted modules found in {folder}")
    return modules


# Decode simple length-prefixed names; keep other encodings as written
def decode_name(raw):
    if raw.startswith("N") and not raw.endswith("E"):
        return raw
    text = raw[1:-1] if raw.startswith("N") else raw
    parts = []
    if text.startswith("St"):
        parts.append("std")
        text = text[2:]
    while text:
        match = re.match(LENGTH_REGEX, text)
        if not match:
            return raw
        length = int(match[0])
        text = text[len(match[0]):]
        part, text = text[:length], text[length:]
        if len(part) != length or not re.fullmatch(IDENTIFIER_REGEX, part):
            return raw
        parts.append(part)
        if text and not raw.startswith("N"):
            return raw
    return "::".join(parts) or raw


def find_rtti(modules):
    # Overlays overlap in RAM: prefer the current module, then require a unique
    # owner. ARM7 and ARM9 pointers are resolved in separate address spaces.
    def owner(context, address):
        if context.contains(address):
            return context
        matches = [m for m in modules if m.cpu == context.cpu and m.contains(address)]
        return matches[0] if len(matches) == 1 else None

    def lookup(table, context, address):
        return table.get((owner(context, address), address))

    words = {m: [w[0] for w in struct.iter_unpack("<I", m.data[:len(m.data) // 4 * 4])]
             for m in modules}
    strings = {}
    for module in modules:
        for match in re.finditer(STRING_REGEX, module.data):
            if len(match[0]) <= 513:
                strings[module, module.base + match.start()] = match[0][:-1].decode("ascii")

    # A type_info record starts with [RTTI-kind vtable pointer, name pointer].
    candidates = {}
    for module, data in words.items():
        for i in range(1, len(data)):
            raw = lookup(strings, module, data[i]) if data[i] else None
            if raw and data[i - 1] and data[i - 1] % 4 == 0:
                candidates[module, module.base + (i - 1) * 4] = (decode_name(raw), data[i - 1], i - 1)

    # Discover the RTTI-kind vtables from the runtime's own type_info names.
    # No game class names or addresses are hardcoded.
    kinds = {}
    abi_names = {f"{namespace}::{kind}" for namespace in ("abi", "__cxxabiv1")
                 for kind in ("__class_type_info", "__si_class_type_info", "__vmi_class_type_info")}
    for module, data in words.items():
        for i in range(1, len(data) - 2):
            info = lookup(candidates, module, data[i]) if data[i] else None
            if info and info[0] in abi_names and data[i - 1] == 0:
                if all((pointer & 1 or pointer % 4 == 0) and owner(module, pointer & ~1)
                       for pointer in data[i + 1:i + 3]):
                    kinds[module, module.base + (i + 1) * 4] = (info[0].split("::")[-1], info[1])
    kinds = {key: value for key, value in kinds.items() if lookup(kinds, key[0], value[1])}

    records = {}
    for (module, address), (name, vptr, index) in candidates.items():
        kind = lookup(kinds, module, vptr)
        if not kind:
            continue
        data = words[module]
        bases = []
        if kind[0] == "__si_class_type_info":
            if index + 2 >= len(data) or not data[index + 2] or data[index + 2] % 4:
                continue
            bases = [data[index + 2]]
        elif kind[0] == "__vmi_class_type_info":
            if (index + 3 >= len(data) or data[index + 2] & ~3 or
                    not 0 < data[index + 3] <= (len(data) - index - 4) // 2):
                continue
            bases = data[index + 4:index + 4 + data[index + 3] * 2:2]
            if any(not base or base % 4 for base in bases):
                continue
        records[module, address] = (name, bases)

    # Find vtables through their type_info pointer. Keep them as candidates:
    # a mapped function pointer alone does not prove the table's full extent.
    vtables = {key: [] for key in records}
    addresses = {address for _, address in records}
    for module, data in words.items():
        for i in range(1, len(data) - 1):
            if data[i] not in addresses:
                continue
            key = (owner(module, data[i]), data[i])
            offset_to_top = data[i - 1] if data[i - 1] < 0x80000000 else data[i - 1] - 0x100000000
            pointer = data[i + 1]
            if (key in vtables and -0x1000000 <= offset_to_top <= 0 and
                    (pointer & 1 or pointer % 4 == 0) and owner(module, pointer & ~1)):
                vtables[key].append((module, module.base + (i - 1) * 4))

    def location(context, address):
        module = owner(context, address)
        if module is None:
            return f"0x{address:08X} (binary unresolved)"
        return f"0x{address:08X} ({module.name} +0x{address - module.base:X})"

    lines = [f"RTTI in {len(modules)} extracted modules: {len(records)} records", "",
             "Addresses are RAM addresses; file offsets are relative to extracted .bin files.",
             "RTTI address  Class : Base class", ""]
    for module in modules:
        found = [(address, name, bases) for (m, address), (name, bases) in records.items() if m is module]
        if not found:
            continue
        lines.append(module.name)
        for address, name, bases in sorted(found):
            names = [lookup(records, module, base) for base in bases]
            inheritance = " : " + ", ".join(info[0] if info else f"?0x{base:08X}"
                                             for info, base in zip(names, bases)) if bases else ""
            lines.append(f"  0x{address:08X}  {name}{inheritance}  (file +0x{address - module.base:X})")
            _, vptr, index = candidates[module, address]
            name_address = words[module][index + 1]
            raw_name = lookup(strings, module, name_address)
            lines.append(f"    Name: {location(module, name_address)}  \"{raw_name}\"")
            lines.append(f"    RTTI-kind vtable address point: {location(module, vptr)}")
            for base in bases:
                lines.append(f"    Base RTTI: {location(module, base)}")
            for source, start in vtables[module, address]:
                lines.append(f"    Vtable candidate: {location(source, start)}; address point 0x{start + 8:08X}")
            lines.append("")
        lines.append("")
    return "\n".join(lines) + "\n"


def main():
    args = parser.parse_args()

    try:
        report = find_rtti(load_modules(args.extract_dir))
    except (OSError, ValueError, KeyError) as error:
        eprint(f"Failed to scan '{args.extract_dir}'")
        eprint(error)
        sys.exit(1)

    if args.out_file:
        try:
            if not args.out_file.exists() or args.out_file.read_text(encoding="utf-8") != report:
                with args.out_file.open('w', encoding="utf-8") as file:
                    file.write(report)
        except OSError as error:
            eprint(f"Failed to write file '{args.out_file}'")
            eprint(error)
            sys.exit(1)
    else:
        print(report, end="")


if __name__ == "__main__":
    main()
