#!/usr/bin/env python3
"""Create a separate, auditable ARM C++ compatibility include tree.

The original source checkout is read-only input. This does not provide platform
implementations or make the whole game runnable.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import shutil


def digest(blob):
    return hashlib.sha256(blob).hexdigest()


def prepare(source, output):
    source = Path(source).resolve()
    output = Path(output).resolve()
    if output == source or output.is_relative_to(source / 'include'):
        raise ValueError('Output must be outside the source include tree')
    overlay = output / 'overlay'
    output.mkdir(parents=True, exist_ok=True)
    before = {p.relative_to(source).as_posix(): digest(p.read_bytes())
              for p in (source / 'include').rglob('*') if p.is_file()}
    shutil.copytree(source / 'include', overlay, dirs_exist_ok=True)
    changes = {}

    def edit(name, transform, reason):
        p = overlay / name
        original = p.read_bytes()
        updated = transform(original.decode()).encode()
        if updated != original:
            p.write_bytes(updated)
            changes[name] = dict(before_sha256=digest(original), after_sha256=digest(updated), reason=reason)

    def tree_names(s):
        for old, new in [
            ('(JSUTree<T>*)getFirstLink()', '(JSUTree<T>*)this->getFirstLink()'),
            ('(JSUTree<T>*)getLast()', '(JSUTree<T>*)this->getLast()'),
            ('(JSUTree<T>*)mNext', '(JSUTree<T>*)this->mNext'),
            ('(JSUTree<T>*)getPrev()', '(JSUTree<T>*)this->getPrev()'),
            ('return mLinkCount;', 'return this->mLinkCount;'),
            ('return (T*)mValue;', 'return (T*)this->mValue;'),
            ('(JSUTree<T>*)getList()', '(JSUTree<T>*)this->getList()'),
        ]:
            s = s.replace(old, new)
        return s

    edit('JSystem/JSupport/JSUList.h', tree_names, 'Qualify dependent base members for standard C++ lookup')
    edit('types.h', lambda s: s.replace('typedef u32 size_t;', 'typedef __SIZE_TYPE__ size_t;'),
         'Use the compiler size_t type for C++ operator new; ARM width remains 32 bits')
    edit('Dolphin/__start.h', lambda s: s.replace('u16 Pad3Button : PAD3_BUTTON_ADDR;', 'extern u16 Pad3Button;'),
         'Replace Metrowerks absolute-address syntax with declaration requiring a platform implementation')
    edit('JSystem/JMath.h', lambda s: re.sub(r'(?m)^(f32 TSinCosTable<\d+, f32>::(?:sinShort|cosShort)\()', r'template<>\n\1', s),
         'Add required explicit-specialization syntax')
    edit('Matrixf.h', lambda s: s.replace('inline void Vector3f::operator*=(Matrixf& other)', 'template<>\ninline void Vector3f::operator*=(Matrixf& other)'),
         'Add required explicit-specialization syntax')
    edit('PowerPC_EABI_Support/MSL_C++/MSL_Common/Include/msl_memory.h', lambda s: s.replace('class __uninitialized_copy_helper {', 'class __uninitialized_copy_helper {\npublic:'),
         'Expose helper member called by existing free function')
    edit('JSystem/JKernel/JKRAram.h', lambda s: s.replace('virtual size_t getExpandedResSize(', 'virtual u32 getExpandedResSize('),
         'Match original base-class return type after standardizing size_t')
    edit('Container.h', lambda s: s.replace('_18      = 1;', 'this->_18 = 1;').replace('const_cast<char*>(mName)', 'const_cast<char*>(this->mName)'),
         'Qualify dependent base members')
    edit('JSystem/JGadget/linklist.h', lambda s: s.replace('TLinkList<T, Offset>::iterator Erase_destroy', 'typename TLinkList<T, Offset>::iterator Erase_destroy').replace('while (!empty())', 'while (!this->empty())').replace('T* item = &front();', 'T* item = &this->front();'),
         'Add dependent typename and member qualification')
    edit('JSystem/JGeometry.h', lambda s: re.sub(r'(?<![.\w>])([if])(?=\.)', r'this->\1', s),
         'Qualify inherited box endpoint members')
    edit('Game/StateMachine.h', lambda s: s.replace('T::StateType*', 'typename T::StateType*'),
         'Add required dependent typename')
    # Correct spelling only in the generated tree; no original headers change.
    for name, target in [('JSYstem', 'JSystem'), ('Stream.h', 'stream.h'), ('sys', 'Sys')]:
        p = overlay / name
        if not p.exists():
            p.symlink_to(target)
    (output / 'intrinsics.h').write_text('''#ifndef P2_ENGINE_PROBE_INTRINSICS_H
#define P2_ENGINE_PROBE_INTRINSICS_H
// Declarations only. Actual ARM implementations are required before linking.
extern "C" double __frsqrte(double);
extern "C" float __fabsf(float);
#endif
''')
    for name, expected in before.items():
        if digest((source / name).read_bytes()) != expected:
            raise RuntimeError('Input changed during overlay creation: ' + name)
    report = dict(source=str(source), changes=changes, original_header_sha256=before,
                  originals_unchanged=True, overlay_only=True)
    (output / 'overlay-manifest.json').write_text(json.dumps(report, indent=2) + '\n')
    return overlay


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    print(prepare(args.source, args.output))
