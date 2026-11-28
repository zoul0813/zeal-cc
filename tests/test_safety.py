"""Sanitizer regressions for allocator, lexer, cleanup and binary AST boundaries."""
import os
from pathlib import Path
import re
import runpy
import struct
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
U16 = lambda n: struct.pack('<H', n)


def ast_program(expression=b'\x12\x0c\x00', expression_nodes=1, strings=(b'main',)):
    body = b'\x01\x01\x00\x02\x00\x00\x81\x00\x00\x00\x00\x04\x01\x00\x05\x01' + expression
    header = b'ZAST' + bytes([1, 0]) + U16(0) + U16(4 + expression_nodes) + U16(len(strings)) + struct.pack('<I', 16 + len(body))
    return header + body + b''.join(U16(len(s)) + s for s in strings)


class SafetyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory(prefix='safety-', dir=ROOT / 'tests')
        cls.work = Path(cls.directory.name)
        cls.addClassCleanup(cls.directory.cleanup)
        cls.env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0:halt_on_error=1', 'UBSAN_OPTIONS': 'halt_on_error=1:print_stacktrace=1'}
        makefile = (ROOT / 'Makefile').read_text().replace('\\\n', '')
        cls.sources = {}
        for stage, key in [('parse', 'PARSE_SRCS'), ('semantic', 'SEMANTIC_SRCS'), ('codegen', 'CODEGEN_SRCS'), ('dump', 'AST_DUMP_SRCS')]:
            sources = re.search(r'^' + key + r' = (.*)$', makefile, re.M).group(1).split()
            cls.sources[stage] = sources
            cls.build(stage, sources)
        cls.build('poison', ['tests/infrastructure/poison_parser.c', *[s for s in cls.sources['parse'] if s != 'src/parser/main.c']])
        cls.build('allocator', ['tests/infrastructure/allocator_test.c'])

    @classmethod
    def build(cls, name, sources):
        proc = subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-g', '-O1', '-Iinclude', '-DCC_POOL_SIZE=32768', '-fsanitize=address,undefined', '-fno-omit-frame-pointer', *sources, '-o', str(cls.work / name)], cwd=ROOT, capture_output=True, text=True)
        if proc.returncode:
            raise AssertionError(proc.stdout + proc.stderr)

    def run_stage(self, name, *args, expected=0):
        proc = subprocess.run([str(self.work / name), *map(str, args)], cwd=ROOT, env=self.env, capture_output=True, text=True, timeout=10)
        self.assertEqual(proc.returncode, expected, proc.stdout + proc.stderr)
        self.assertNotIn('runtime error:', proc.stdout + proc.stderr)
        self.assertNotIn('AddressSanitizer', proc.stdout + proc.stderr)
        return proc

    def parse(self, source, expected=0, stage='parse'):
        path = self.work / 'input.c'
        path.write_text(source)
        ast = self.work / 'output.ast'
        ast.unlink(missing_ok=True)
        proc = self.run_stage(stage, path, ast, expected=expected)
        if expected:
            self.assertNotIn(' -> ', proc.stdout)
        return ast

    def test_allocator_alignment_splitting_and_overflow(self):
        self.run_stage('allocator')
        self.run_stage('allocator', 'overflow', expected=1)
        self.run_stage('allocator', 'oversize', expected=1)

    def test_target_map_guard(self):
        checker = runpy.run_path(str(ROOT / 'tests/check_target_memory.py'))
        maps = self.work / 'maps'
        maps.mkdir(exist_ok=True)
        for stage in checker['STAGES']:
            (maps / f'{stage}.map').write_text('     00009000  s__HEAP\n')
        report = checker['check_maps'](maps)
        self.assertEqual(report['cc_parse']['pool_bytes'], 0x3300)
        (maps / 'cc_parse.map').write_text('     00009000  s__HEAP\n     00008FFF s__BSS\n     00000010 l__BSS\n')
        with self.assertRaises(ValueError):
            checker['check_maps'](maps)
        (maps / 'cc_parse.map').write_text('     0000C300  s__HEAP\n')
        with self.assertRaises(ValueError):
            checker['check_maps'](maps)

    def test_valid_integer_boundaries(self):
        for text, value in [('0', 0), ('010', 8), ('0177777', 65535), ('65535u', 65535), ('0XFFFFU', 65535), ('32768', 32768), ('0x7fff', 32767)]:
            with self.subTest(literal=text):
                ast = self.parse(f'int main(){{return {text};}}')
                data = ast.read_bytes()
                end = struct.unpack_from('<I', data, 12)[0]
                self.assertTrue(data[16:end].endswith(b'\x12' + U16(value)))
                self.run_stage('semantic', ast)
                self.run_stage('codegen', ast, self.work / 'out.asm')

    def test_malformed_or_unsupported_literals(self):
        cases = ['65536', '0x10000', '0200000', '08', '0x', '0xg', '1.9', '1e+2', '1uU', '1L', '1f', '123abc', '9'*46+'e+', '9'*200, '0'*100]
        for literal in cases:
            with self.subTest(literal=literal[:55]):
                self.parse(f'int main(){{return {literal};}}', expected=1)
        for source in ["int main(){return 'a;}", "int main(){return '';}", "int main(){return 'ab';}", "int main(){return '\\x20';}", 'int main(){return 0;} /* unterminated', 'char* s="unterminated;', 'char* s="line\nbreak";', 'char* s="a\\0b";', 'int '+'a'*32+';', 'char* s="'+'a'*96+'";', 'int main(){return 0;}\x00int x;']:
            with self.subTest(source=source[:55]):
                self.parse(source, expected=1)

    def test_poisoned_partial_nodes(self):
        for source in ['int main(){if(', 'int main(){while(', 'int main(){for(', 'int helper(int']:
            with self.subTest(source=source):
                self.parse(source, expected=1, stage='poison')

    def test_source_nesting_is_bounded(self):
        self.parse('int main(){return ' + '!'*27 + '0;}')
        for source in ['int main(){return ' + '!'*200 + '0;}', 'int main(){return ' + '('*100 + '0' + ')'*100 + ';}', 'int main(){'+'if(1)'*100+'return 0;}', 'int '+'*'*100+'p;', 'int helper(int '+'*'*8+'p[2]){return 0;}', 'int main(){return '+'+'.join(['1']*150)+';}']:
            with self.subTest(source=source[:50]):
                self.parse(source, expected=1)

    def test_exact_binary_and_statement_depth_boundaries(self):
        self.parse('int main(){return ' + '+'.join(['1']*28) + ';}')
        self.parse('int main(){return ' + '+'.join(['1']*29) + ';}', expected=1)
        self.parse('int main(){' + 'if(0)return 1; else '*27 + 'return 0;}')
        self.parse('int main(){' + 'if(0)return 1; else '*28 + 'return 0;}', expected=1)

    def test_semantic_invalid_codegen_inputs_remain_memory_safe(self):
        cases = [
            'int main(){return missing;}',
            'int main(){return absent(1);}',
            'int main(){break;return 0;}',
            'int main(){goto absent;return 0;}',
            'int main(){return 1[0];}',
            'int main(){int x=1;return *x;}',
            ''.join(f'char* p{i}="s{i}";' for i in range(65))+'int main(){return 0;}',
        ]
        for source in cases:
            with self.subTest(source=source[:55]):
                ast = self.parse(source)
                self.run_stage('semantic', ast, expected=1)
                # Generation is not language validation; only the semantic-accepted
                # pipeline is supported. Direct invalid inputs must still not crash.
                proc = subprocess.run([self.work/'codegen', ast, self.work/'invalid-semantic.asm'], cwd=ROOT, env=self.env, capture_output=True, text=True, timeout=10)
                self.assertIn(proc.returncode, (0,1), proc.stdout+proc.stderr)
                self.assertNotIn('runtime error:', proc.stderr)
                self.assertNotIn('AddressSanitizer', proc.stderr)

    def test_codegen_over_capacity_tables_remain_memory_safe(self):
        # Two structurally valid 33-declaration blocks exceed semantic scope
        # capacity (32) and the flattened codegen local table (64).
        declarations = [b'\x03'+U16(i+1)+b'\x81\x00\x00\x00\x00' for i in range(66)]
        blocks = [b'\x04'+U16(33)+b''.join(declarations[i:i+33]) for i in (0,33)]
        body = b'\x01'+U16(1)+b'\x02'+U16(0)+b'\x81\x00\x00\x00\x00'
        body += b'\x04'+U16(3)+b''.join(blocks)+b'\x05\x01\x12'+U16(0)
        strings = [b'main', *[f'v{i}'.encode() for i in range(66)]]
        data = b'ZAST'+b'\x01\x00'+U16(0)+U16(73)+U16(len(strings))+struct.pack('<I',16+len(body))
        data += body+b''.join(U16(len(value))+value for value in strings)
        path = self.work/'capacity.ast'
        path.write_bytes(data)
        proc = self.run_stage('semantic',path,expected=1)
        self.assertIn('Too many',proc.stdout+proc.stderr)
        proc = subprocess.run([self.work/'codegen',path,self.work/'capacity.asm'],cwd=ROOT,env=self.env,capture_output=True,text=True,timeout=10)
        self.assertIn(proc.returncode,(0,1),proc.stdout+proc.stderr)
        self.assertNotIn('runtime error:',proc.stderr)
        self.assertNotIn('AddressSanitizer',proc.stderr)

    def assert_bad_ast(self, data):
        path = self.work / 'bad.ast'
        path.write_bytes(data)
        self.run_stage('semantic', path, expected=1)
        self.run_stage('dump', path, expected=1)
        self.run_stage('codegen', path, self.work / 'bad.asm', expected=1)

    def test_ast_header_and_string_allocation_boundaries(self):
        valid = ast_program()
        for offset, replacement in [(4,b'\xff'), (5,b'\x01'), (6,U16(1)), (8,U16(0)), (8,U16(65535)), (10,U16(32769)), (10,U16(513)), (12,struct.pack('<I', 0)), (12,struct.pack('<I',0xffffffff)), (12,struct.pack('<I', len(valid)))]:
            with self.subTest(offset=offset, replacement=replacement):
                self.assert_bad_ast(valid[:offset]+replacement+valid[offset+len(replacement):])
        for data in [valid+b'junk', valid[:-1], valid[:35]+U16(65535), ast_program(strings=(b'x'*96,)), ast_program(strings=(b'a\0b',)), ast_program(strings=(b'bad:name',))]:
            self.assert_bad_ast(data)
        for length in range(len(valid)):
            with self.subTest(truncated=length):
                self.assert_bad_ast(valid[:length])

    def test_ast_tags_types_indices_flags_and_depth(self):
        valid = ast_program()
        for offset, replacement in [(16,b'\x00'), (19,b'\x00'), (20,U16(1)), (22,b'\xff'), (23,b'\xff'), (24,U16(1)), (26,b'\x09'), (27,b'\x12'), (28,U16(65)), (31,b'\x02'), (32,b'\x00')]:
            with self.subTest(offset=offset):
                self.assert_bad_ast(valid[:offset]+replacement+valid[offset+len(replacement):])
        for expression in [b'\x13'+U16(1), b'\x11'+U16(1), b'\x10\xff\x12\x00\x00', b'\x0f\xff\x12\x00\x00\x12\x00\x00']:
            self.assert_bad_ast(ast_program(expression, 2 if expression[0] == 16 else 1))
        self.assert_bad_ast(ast_program(b'\x10\x02'*28+b'\x12\x00\x00',29))
        path = self.work / 'deep.ast'
        path.write_bytes(ast_program(b'\x10\x02'*27+b'\x12\x00\x00',28))
        self.run_stage('semantic', path)
        self.run_stage('dump', path)
        self.run_stage('codegen', path, self.work / 'deep.asm')

    def test_deterministic_ast_mutations_never_crash(self):
        valid = ast_program()
        path = self.work / 'mutation.ast'
        for i in range(len(valid)):
            path.write_bytes(valid[:i]+bytes([valid[i]^0xff])+valid[i+1:])
            for stage in ('semantic','codegen','dump'):
                args = [path] if stage != 'codegen' else [path, self.work/'mutation.asm']
                proc = subprocess.run([str(self.work/stage), *map(str,args)], cwd=ROOT, env=self.env, capture_output=True, text=True, timeout=3)
                self.assertIn(proc.returncode, (0,1), proc.stderr)
                self.assertNotIn('runtime error:', proc.stderr)
                self.assertNotIn('AddressSanitizer', proc.stderr)


if __name__ == '__main__':
    unittest.main()
