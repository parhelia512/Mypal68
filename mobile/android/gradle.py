# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function

import buildconfig
import os
import subprocess
import sys

from mozbuild.util import (
    ensureParentDir,
    lock_file,
)
import mozpack.path as mozpath


def android(verb, *args):
    # Building the same Gradle root project with multiple concurrent processes
    # is not well supported, so we use a simple lock file to serialize build
    # steps.
    lock_path = '{}/gradle/mach_android.lockfile'.format(buildconfig.topobjdir)
    ensureParentDir(lock_path)
    lock_instance = lock_file(lock_path)
    try:
        cmd = [
            sys.executable,
            mozpath.join(buildconfig.topsrcdir, 'mach'),
            'android',
            verb,
        ]
        cmd.extend(args)
        env = dict(os.environ)
        # Confusingly, `MACH` is set only within `mach build`.
        if env.get('MACH'):
            env['GRADLE_INVOKED_WITHIN_MACH_BUILD'] = '1'
        subprocess.check_call(cmd, env=env)

        return 0
    finally:
        del lock_instance


def assemble_app(dummy_output_file, *inputs):
    return android('assemble-app')


def generate_sdk_bindings(dummy_output_file, *args):
    return android('generate-sdk-bindings', *args)


def generate_generated_jni_wrappers(dummy_output_file, *args):
    return android('generate-generated-jni-wrappers', *args)
