#!/usr/bin/env python3
"""Finishes what `agk release` makes, for ROTOR RAIDER: run it after every
`agk release`.

  - Requirements: 1 MB (the kit's release tool always writes 512 KB)
  - the zip gets the demo GIF and the disk's data files (the logo, the
    theatres, the BOBs: the program loads them, also from a hard disk), and
    its README.txt the copyright and the engine's licences

    agk release && python3 tools/finish_release.py
"""
import glob
import os
import shutil
import zipfile

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
OLD_REQ = 'Any Amiga with 512 KB of chip RAM and Kickstart 1.3 or later (OCS, PAL).'
NEW_REQ = ('An Amiga 500 with 1 MB of RAM (512 KB chip + 512 KB expansion)\n'
           'and Kickstart 1.3 or later, PAL.')


def readme_txt(t):
    assert OLD_REQ in t, 'the requirements line has changed: update tools/finish_release.py'
    t = t.replace(OLD_REQ, NEW_REQ)
    t = t.replace('anywhere and run it from the Shell or Workbench.\n',
                  'into a drawer of its own together with the *.pk files (the theatres, the\n'
                  'graphics, the logo), and run it from there.\n'
                  'rotor-raider-demo.gif is a 12-second clip of the game in action.\n')
    t = t.replace('By Paul Ross Seibert.\n',
                  'Authored by Paul Ross Seibert. Published by Desert Palms Software.\n'
                  '(c) 2026 Desert Palms Software. All rights reserved.\n\n'
                  'Built with the ACE game engine (Mozilla Public License 2.0,\n'
                  'https://github.com/AmigaPorts/ACE), including its ProTracker player, and the\n'
                  'Amiga Game Kit runtime (MIT License).\n\n'
                  'Music on Amigas with 1 MB of chip RAM or more (A500+, A600, A1200): dog68\n'
                  'and dog70 by Songerson, from The Mod Archive (modarchive.org), licensed\n'
                  'CC BY, used unchanged - see CREDITS.txt.\n')
    return t


def main():
    rel = os.path.join(ROOT, 'build', 'release')
    (zpath,) = glob.glob(os.path.join(rel, '*.zip'))
    tmp = zpath + '.tmp'
    with zipfile.ZipFile(zpath) as a, zipfile.ZipFile(tmp, 'w', zipfile.ZIP_DEFLATED) as b:
        names = a.namelist()
        assert 'rotor-raider-demo.gif' not in names, 'already finished: run agk release first'
        for i in a.infolist():
            d = a.read(i.filename)
            if i.filename == 'README.txt':
                d = readme_txt(d.decode()).encode()
            b.writestr(i, d)
        b.write(os.path.join(ROOT, 'demo', 'rotor-raider.gif'), 'rotor-raider-demo.gif')
        for pk in sorted(glob.glob(os.path.join(ROOT, 'build', 'disk', '*.pk'))):
            b.write(pk, os.path.basename(pk))
    shutil.move(tmp, zpath)
    (rpath,) = glob.glob(os.path.join(rel, '*.readme'))
    t = open(rpath).read()
    assert OLD_REQ in t, 'the .readme requirements line has changed'
    t = t.replace(OLD_REQ, NEW_REQ)
    t += ('\nMusic on Amigas with 1 MB of chip RAM or more: dog68 and dog70 by Songerson\n'
          '(The Mod Archive, modarchive.org), CC BY - see CREDITS.txt in the zip.\n')
    open(rpath, 'w').write(t)
    print(f'finished {os.path.basename(zpath)} and {os.path.basename(rpath)}')


if __name__ == '__main__':
    main()
