#!/usr/bin/env python3
"""Check SDCC maps, or prepare ZDE-only instrumented stage builds."""
import argparse
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
STAGES = ('cc_parse', 'cc_semantic', 'cc_codegen', 'ast_dump')


def check_maps(directory):
    report = {}
    for stage in STAGES:
        text = (directory / f'{stage}.map').read_text()
        values = {m[2]: int(m[1], 16) for m in re.finditer(r'^\s*([0-9A-Fa-f]{8})\s+(s__\w+|l__\w+)\b', text, re.M)}
        heap = values['s__HEAP']
        # Includes initialized/uninitialized storage. The allocator stops at C300;
        # the fixed reader owns C300-C4FF; the private user stack is above C500.
        if not 0x4000 < heap < 0xC300:
            raise ValueError(f'{stage}: image/static storage overlaps the fixed reader buffer')
        for symbol, start in values.items():
            if not symbol.startswith('s__') or symbol in ('s__HEAP', 's__CABS', 's__DABS'):
                continue
            size = values.get('l__' + symbol[3:], 0)
            if size and start + size > heap:
                raise ValueError(f'{stage}: {symbol[3:]} extends beyond the heap start')
        report[stage] = {'heap_start': f'0x{heap:04X}', 'pool_bytes': 0xC300 - heap,
                         'file_buffer': '0xC300-0xC4FF', 'stack_region_bytes': 0xFFFF - 0xC500 + 1}
    return report


def prepare_probe_project():
    text = (ROOT / 'CMakeLists.txt').read_text()
    preamble = text[:text.index('add_program(cc\n')]
    preamble = preamble.replace('${CMAKE_CURRENT_LIST_DIR}/include', '/src/include')
    blocks = [preamble]
    for stage, define in [('cc_parse', 'PROBE_PARSE'), ('cc_semantic', 'PROBE_SEMANTIC'), ('cc_codegen', 'PROBE_CODEGEN'), ('ast_dump', 'PROBE_DUMP')]:
        match = re.search(r'add_program\(' + stage + r'\s+(.*?)\n\)', text, re.S)
        sources = re.findall(r'"(src/[^"\n]+)"', match[1])
        sources = [source for source in sources if not source.endswith('/main.c') and source not in ('src/common/common.c', 'src/tools/ast_dump.c')]
        entries = ['    "/src/tests/infrastructure/stage_memory_probe.c"', *[f'    "/src/{source}"' for source in sources]]
        blocks.append(f'add_program({stage}\n' + '\n'.join(entries) + f'\n    DEFS {define}\n    ILIBS core\n)\n')
    project = ROOT / 'tests/audit/target-probes'
    project.mkdir(parents=True, exist_ok=True)
    (project / 'CMakeLists.txt').write_text('\n'.join(blocks))
    print('Prepared tests/audit/target-probes. Inside zde -i, run:')
    print('cd /src/tests/audit/target-probes')
    print('cmake -B build && cmake --build build')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-probes', action='store_true')
    parser.add_argument('--maps', type=Path, default=ROOT / 'debug')
    args = parser.parse_args()
    if args.prepare_probes:
        prepare_probe_project()
    else:
        print(json.dumps(check_maps(args.maps), indent=2))


if __name__ == '__main__':
    main()
