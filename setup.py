#!/usr/bin/env python3

from distutils.core import setup

setup(name         = 'fleshutils',
      version      = '0.1',
      description  = 'Collection of scripts that supplement coreutils',
      author       = 'Antti Kervinen',
      author_email = 'antti.kervinen@gmail.com',
      packages     = [],
      package_data = {},
      scripts      = ['bin/bracketshr',
                      'bin/epochfilt',
                      'bin/go-mod-tree',
                      'bin/grepctx',
                      'bin/hashfilt',
                      'bin/igo',
                      'bin/longestcommon',
                      'bin/numdelta',
                      'bin/numhr',
                      'bin/procdata',
                      'bin/tinypwd',
                      'bin/trace-system-execve',
                      'bin/trace-system-open-write',
                      'bin/trace-system-network',
                      'bin/tsl'],
)
